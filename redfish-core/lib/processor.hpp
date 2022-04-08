/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "dbus_singleton.hpp"
#include "error_messages.hpp"
#include "health.hpp"

#include <app.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_utility.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <sdbusplus/utility/dedup_variant.hpp>
#include <utils/collection.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/json_utils.hpp>
namespace redfish
{

using InterfacesProperties = boost::container::flat_map<
    std::string,
    boost::container::flat_map<std::string, dbus::utility::DbusVariantType>>;

// Map of service name to list of interfaces
using MapperServiceMap =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

// Map of object paths to MapperServiceMaps
using MapperGetSubTreeResponse =
    std::vector<std::pair<std::string, MapperServiceMap>>;

// Interfaces which imply a D-Bus object represents a Processor
constexpr std::array<const char*, 2> processorInterfaces = {
    "xyz.openbmc_project.Inventory.Item.Cpu",
    "xyz.openbmc_project.Inventory.Item.Accelerator"};

inline std::string getProcessorType(const std::string& processorType)
{
    if (processorType == "xyz.openbmc_project.Inventory.Item.Accelerator."
                         "AcceleratorType.Accelerator")
    {
        return "Accelerator";
    }
    if (processorType ==
        "xyz.openbmc_project.Inventory.Item.Accelerator.AcceleratorType.FPGA")
    {
        return "FPGA";
    }
    if (processorType ==
        "xyz.openbmc_project.Inventory.Item.Accelerator.AcceleratorType.GPU")
    {
        return "GPU";
    }
    // Unknown or others
    return "";
}

/**
 * @brief Fill out memory links association by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getProcessorMemoryLinks(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get underneath memory links";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no memory = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& linksArray =
                aResp->res.jsonValue["Links"]["Memory"];
            linksArray = nlohmann::json::array();
            for (const std::string& memoryPath : *data)
            {
                sdbusplus::message::object_path objectPath(memoryPath);
                std::string memoryName = objectPath.filename();
                if (memoryName.empty())
                {
                    messages::internalError(aResp->res);
                    return;
                }
                linksArray.push_back(
                    {{"@odata.id",
                      "/redfish/v1/Systems/system/Memory/" + memoryName}});
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_memory",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out pcie functions links association by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out] aResp           Async HTTP response.
 * @param[in]     objPath         D-Bus object to query.
 * @param[in]     service         D-Bus service to query.
 * @param[in]     pcieDeviceLink  D-Bus service to query.
 */
inline void getProcessorPCIeFunctionsLinks(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& service,
    const std::string& objPath, const std::string& pcieDeviceLink)
{
    BMCWEB_LOG_DEBUG << "Get processor pcie functions links";
    crow::connections::systemBus->async_method_call(
        [aResp, pcieDeviceLink](
            const boost::system::error_code ec,
            boost::container::flat_map<std::string,
                                       std::variant<std::string, size_t>>&
                pcieDevProperties) {
            if (ec)
            {
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["SystemInterface"]["InterfaceType"] = "PCIe";
            // PCIe interface properties
            for (const std::pair<std::string,
                                 std::variant<std::string, size_t>>& property :
                 pcieDevProperties)
            {
                const std::string& propertyName = property.first;
                if ((propertyName == "LanesInUse") ||
                    (propertyName == "MaxLanes"))
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value != nullptr)
                    {
                        aResp->res.jsonValue["SystemInterface"]["PCIe"]
                                            [propertyName] = *value;
                    }
                }
                else if ((propertyName == "PCIeType") ||
                         (propertyName == "MaxPCIeType"))
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value != nullptr)
                    {
                        aResp->res.jsonValue["SystemInterface"]["PCIe"]
                                            [propertyName] =
                            getPCIeType(*value);
                    }
                }
            }
            // PCIe functions properties
            nlohmann::json& pcieFunctionList =
                aResp->res.jsonValue["Links"]["PCIeFunctions"];
            pcieFunctionList = nlohmann::json::array();
            static constexpr const int maxPciFunctionNum = 8;
            for (int functionNum = 0; functionNum < maxPciFunctionNum;
                 functionNum++)
            {
                // Check if this function exists by looking for a device
                // ID
                std::string devIDProperty =
                    "Function" + std::to_string(functionNum) + "DeviceId";
                std::string* property =
                    std::get_if<std::string>(&pcieDevProperties[devIDProperty]);
                if (property && !property->empty())
                {
                    pcieFunctionList.push_back(
                        {{"@odata.id", pcieDeviceLink + "/PCIeFunctions/" +
                                           std::to_string(functionNum)}});
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.PCIeDevice");
}

/**
 * @brief Fill out links for parent chassis PCIeDevice by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       chassisName D-Bus object chassisName.
 */
inline void getParentChassisPCIeDeviceLink(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath,
    const std::string& chassisName)
{
    crow::connections::systemBus->async_method_call(
        [aResp, chassisName](const boost::system::error_code ec,
                             std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no chassis = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr && data->size() > 1)
            {
                // Chassis must have single parent chassis
                return;
            }
            const std::string& parentChassisPath = data->front();
            sdbusplus::message::object_path objectPath(parentChassisPath);
            std::string parentChassisName = objectPath.filename();
            if (parentChassisName.empty())
            {
                messages::internalError(aResp->res);
                return;
            }
            crow::connections::systemBus->async_method_call(
                [aResp, chassisName,
                 parentChassisName](const boost::system::error_code ec,
                                    const MapperGetSubTreeResponse& subtree) {
                    if (ec)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    for (const auto& [objectPath, serviceMap] : subtree)
                    {
                        // Process same device
                        if (!boost::ends_with(objectPath, chassisName))
                        {
                            continue;
                        }
                        std::string pcieDeviceLink = "/redfish/v1/Chassis/";
                        pcieDeviceLink += parentChassisName;
                        pcieDeviceLink += "/PCIeDevices/";
                        pcieDeviceLink += chassisName;
                        aResp->res.jsonValue["Links"]["PCIeDevice"] = {
                            {"@odata.id", pcieDeviceLink}};
                        if (serviceMap.size() < 1)
                        {
                            BMCWEB_LOG_ERROR << "Got 0 service "
                                                "names";
                            messages::internalError(aResp->res);
                            return;
                        }
                        const std::string& serviceName = serviceMap[0].first;
                        // Get PCIeFunctions Link
                        getProcessorPCIeFunctionsLinks(
                            aResp, serviceName, objectPath, pcieDeviceLink);
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                parentChassisPath, 0,
                std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item."
                                           "PCIeDevice"});
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out links association to parent chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getProcessorChassisLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get parent chassis link";
    crow::connections::systemBus->async_method_call(
        [aResp, objPath](const boost::system::error_code ec,
                         std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no chassis = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr && data->size() > 1)
            {
                // Processor must have single parent chassis
                return;
            }
            const std::string& chassisPath = data->front();
            sdbusplus::message::object_path objectPath(chassisPath);
            std::string chassisName = objectPath.filename();
            if (chassisName.empty())
            {
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["Links"]["Chassis"] = {
                {"@odata.id", "/redfish/v1/Chassis/" + chassisName}};

            // Check if PCIeDevice on this chassis
            crow::connections::systemBus->async_method_call(
                [aResp, chassisName,
                 chassisPath](const boost::system::error_code ec,
                              const MapperGetSubTreeResponse& subtree) {
                    if (ec)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    // If PCIeDevice doesn't exists on this chassis
                    // Check PCIeDevice on its parent chassis
                    if (subtree.empty())
                    {
                        getParentChassisPCIeDeviceLink(aResp, chassisPath,
                                                       chassisName);
                    }
                    else
                    {
                        for (const auto& [objectPath, serviceMap] : subtree)
                        {
                            // Process same device
                            if (!boost::ends_with(objectPath, chassisName))
                            {
                                continue;
                            }
                            std::string pcieDeviceLink = "/redfish/v1/Chassis/";
                            pcieDeviceLink += chassisName;
                            pcieDeviceLink += "/PCIeDevices/";
                            pcieDeviceLink += chassisName;
                            aResp->res.jsonValue["Links"]["PCIeDevice"] = {
                                {"@odata.id", pcieDeviceLink}};
                            if (serviceMap.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 service names";
                                messages::internalError(aResp->res);
                                return;
                            }
                            const std::string& serviceName =
                                serviceMap[0].first;
                            // Get PCIeFunctions Link
                            getProcessorPCIeFunctionsLinks(
                                aResp, serviceName, objectPath, pcieDeviceLink);
                        }
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree", chassisPath,
                0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice"});
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out uuid info of a processor by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getProcessorUUID(std::shared_ptr<bmcweb::AsyncResp> aResp,
                             const std::string& service,
                             const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Processor UUID";
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Common.UUID", "UUID",
        [objPath, aResp{std::move(aResp)}](const boost::system::error_code ec,
                                           const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["UUID"] = property;
        });
}

/**
 * @brief Fill out firmware version info of a accelerator by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getProcessorFirmwareVersion(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                const std::string& service,
                                const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Processor firmware version";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const std::variant<std::string>& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Processor firmware version";
                messages::internalError(aResp->res);
                return;
            }
            const std::string* value = std::get_if<std::string>(&property);
            if (value == nullptr)
            {
                BMCWEB_LOG_DEBUG << "Null value returned for Version";
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["FirmwareVersion"] = *value;
        },
        service, objPath, "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Software.Version", "Version");
}

inline void getCpuDataByInterface(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const dbus::utility::DBusInteracesMap& cpuInterfacesProperties)
{
    BMCWEB_LOG_DEBUG << "Get CPU resources by interface.";

    // Set the default value of state
    aResp->res.jsonValue["Status"]["State"] = "Enabled";
    aResp->res.jsonValue["Status"]["Health"] = "OK";

    for (const auto& interface : cpuInterfacesProperties)
    {
        for (const auto& property : interface.second)
        {
            if (property.first == "Present")
            {
                const bool* cpuPresent = std::get_if<bool>(&property.second);
                if (cpuPresent == nullptr)
                {
                    // Important property not in desired type
                    messages::internalError(aResp->res);
                    return;
                }
                if (*cpuPresent == false)
                {
                    // Slot is not populated
                    aResp->res.jsonValue["Status"]["State"] = "Absent";
                }
            }
            else if (property.first == "Functional")
            {
                const bool* cpuFunctional = std::get_if<bool>(&property.second);
                if (cpuFunctional == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                if (*cpuFunctional == false)
                {
                    aResp->res.jsonValue["Status"]["Health"] = "Critical";
                }
            }
            else if (property.first == "CoreCount")
            {
                const uint16_t* coresCount =
                    std::get_if<uint16_t>(&property.second);
                if (coresCount == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["TotalCores"] = *coresCount;
            }
            else if (property.first == "MaxSpeedInMhz")
            {
                const uint32_t* value = std::get_if<uint32_t>(&property.second);
                if (value != nullptr)
                {
                    aResp->res.jsonValue["MaxSpeedMHz"] = *value;
                }
            }
            else if (property.first == "Socket")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    aResp->res.jsonValue["Socket"] = *value;
                }
            }
            else if (property.first == "ThreadCount")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value != nullptr)
                {
                    aResp->res.jsonValue["TotalThreads"] = *value;
                }
            }
            else if (property.first == "EffectiveFamily")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value != nullptr)
                {
                    aResp->res.jsonValue["ProcessorId"]["EffectiveFamily"] =
                        "0x" + intToHexString(*value);
                }
            }
            else if (property.first == "EffectiveModel")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["ProcessorId"]["EffectiveModel"] =
                    "0x" + intToHexString(*value);
            }
            else if (property.first == "Id")
            {
                const uint64_t* value = std::get_if<uint64_t>(&property.second);
                if (value != nullptr && *value != 0)
                {
                    aResp->res
                        .jsonValue["ProcessorId"]["IdentificationRegisters"] =
                        "0x" + intToHexString(*value);
                }
            }
            else if (property.first == "Microcode")
            {
                const uint32_t* value = std::get_if<uint32_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["ProcessorId"]["MicrocodeInfo"] =
                    "0x" + intToHexString(*value);
            }
            else if (property.first == "Step")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["ProcessorId"]["Step"] =
                    "0x" + intToHexString(*value);
            }
        }
    }

    return;
}

inline void getCpuDataByService(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& cpuId,
                                const std::string& service,
                                const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get available system cpu resources by service.";

    crow::connections::systemBus->async_method_call(
        [cpuId, service, objPath,
         aResp{aResp}](const boost::system::error_code ec,
                       const dbus::utility::ManagedObjectType& dbusData) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["Id"] = cpuId;
            aResp->res.jsonValue["Name"] = "Processor";
            aResp->res.jsonValue["ProcessorType"] = "CPU";

            bool slotPresent = false;
            std::string corePath = objPath + "/core";
            size_t totalCores = 0;
            for (const auto& object : dbusData)
            {
                if (object.first.str == objPath)
                {
                    getCpuDataByInterface(aResp, object.second);
                }
                else if (boost::starts_with(object.first.str, corePath))
                {
                    for (const auto& interface : object.second)
                    {
                        if (interface.first ==
                            "xyz.openbmc_project.Inventory.Item")
                        {
                            for (const auto& property : interface.second)
                            {
                                if (property.first == "Present")
                                {
                                    const bool* present =
                                        std::get_if<bool>(&property.second);
                                    if (present != nullptr)
                                    {
                                        if (*present == true)
                                        {
                                            slotPresent = true;
                                            totalCores++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // In getCpuDataByInterface(), state and health are set
            // based on the present and functional status. If core
            // count is zero, then it has a higher precedence.
            if (slotPresent)
            {
                if (totalCores == 0)
                {
                    // Slot is not populated, set status end return
                    aResp->res.jsonValue["Status"]["State"] = "Absent";
                    aResp->res.jsonValue["Status"]["Health"] = "OK";
                }
                aResp->res.jsonValue["TotalCores"] = totalCores;
            }
            return;
        },
        service, "/xyz/openbmc_project/inventory",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
}

inline void getCpuAssetData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& service,
                            const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Cpu Asset Data";
    crow::connections::systemBus->async_method_call(
        [objPath, aResp{aResp}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "SerialNumber")
                {
                    const std::string* sn =
                        std::get_if<std::string>(&property.second);
                    if (sn != nullptr && !sn->empty())
                    {
                        aResp->res.jsonValue["SerialNumber"] = *sn;
                    }
                }
                else if (property.first == "Model")
                {
                    const std::string* model =
                        std::get_if<std::string>(&property.second);
                    if (model != nullptr && !model->empty())
                    {
                        aResp->res.jsonValue["Model"] = *model;
                    }
                }
                else if (property.first == "Manufacturer")
                {

                    const std::string* mfg =
                        std::get_if<std::string>(&property.second);
                    if (mfg != nullptr)
                    {
                        aResp->res.jsonValue["Manufacturer"] = *mfg;

                        // Otherwise would be unexpected.
                        if (mfg->find("Intel") != std::string::npos)
                        {
                            aResp->res.jsonValue["ProcessorArchitecture"] =
                                "x86";
                            aResp->res.jsonValue["InstructionSet"] = "x86-64";
                        }
                        else if (mfg->find("IBM") != std::string::npos)
                        {
                            aResp->res.jsonValue["ProcessorArchitecture"] =
                                "Power";
                            aResp->res.jsonValue["InstructionSet"] = "PowerISA";
                        }
                    }
                }
                else if (property.first == "PartNumber")
                {
                    const std::string* partNumber =
                        std::get_if<std::string>(&property.second);

                    if (partNumber == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PartNumber"] = *partNumber;
                }
                else if (property.first == "SparePartNumber")
                {
                    const std::string* sparePartNumber =
                        std::get_if<std::string>(&property.second);

                    if (sparePartNumber == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["SparePartNumber"] = *sparePartNumber;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.Asset");
}

inline void getCpuRevisionData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                               const std::string& service,
                               const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Cpu Revision Data";
    crow::connections::systemBus->async_method_call(
        [objPath, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "Version")
                {
                    const std::string* ver =
                        std::get_if<std::string>(&property.second);
                    if (ver != nullptr)
                    {
                        aResp->res.jsonValue["Version"] = *ver;
                    }
                    break;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.Revision");
}

inline void getAcceleratorDataByService(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& acclrtrId,
    const std::string& service, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG
        << "Get available system Accelerator resources by service.";
    crow::connections::systemBus->async_method_call(
        [acclrtrId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["Id"] = acclrtrId;
            aResp->res.jsonValue["Name"] = "Processor";
            const bool* accPresent = nullptr;
            const bool* accFunctional = nullptr;

            for (const auto& property : properties)
            {
                if (property.first == "Functional")
                {
                    accFunctional = std::get_if<bool>(&property.second);
                }
                else if (property.first == "Present")
                {
                    accPresent = std::get_if<bool>(&property.second);
                }
                else if (property.first == "Type")
                {
                    const std::string* accType =
                        std::get_if<std::string>(&property.second);
                    if (accType == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(aResp->res);
                        return;
                    }
                    std::string processorType = getProcessorType(*accType);
                    aResp->res.jsonValue["ProcessorType"] = processorType;
                }
            }

            std::string state = "Enabled";
            std::string health = "OK";

            if (accPresent != nullptr && *accPresent == false)
            {
                state = "Absent";
            }

            if ((accFunctional != nullptr) && (*accFunctional == false))
            {
                if (state == "Enabled")
                {
                    health = "Critical";
                }
            }

            aResp->res.jsonValue["Status"]["State"] = state;
            aResp->res.jsonValue["Status"]["Health"] = health;
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

// OperatingConfig D-Bus Types
using TurboProfileProperty = std::vector<std::tuple<uint32_t, size_t>>;
using BaseSpeedPrioritySettingsProperty =
    std::vector<std::tuple<uint32_t, std::vector<uint32_t>>>;
// uint32_t and size_t may or may not be the same type, requiring a dedup'd
// variant
using OperatingConfigProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

/**
 * Fill out the HighSpeedCoreIDs in a Processor resource from the given
 * OperatingConfig D-Bus property.
 *
 * @param[in,out]   aResp               Async HTTP response.
 * @param[in]       baseSpeedSettings   Full list of base speed priority groups,
 *                                      to use to determine the list of high
 *                                      speed cores.
 */
inline void highSpeedCoreIdsHandler(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const BaseSpeedPrioritySettingsProperty& baseSpeedSettings)
{
    // The D-Bus property does not indicate which bucket is the "high
    // priority" group, so let's discern that by looking for the one with
    // highest base frequency.
    auto highPriorityGroup = baseSpeedSettings.cend();
    uint32_t highestBaseSpeed = 0;
    for (auto it = baseSpeedSettings.cbegin(); it != baseSpeedSettings.cend();
         ++it)
    {
        const uint32_t baseFreq = std::get<uint32_t>(*it);
        if (baseFreq > highestBaseSpeed)
        {
            highestBaseSpeed = baseFreq;
            highPriorityGroup = it;
        }
    }

    nlohmann::json& jsonCoreIds = aResp->res.jsonValue["HighSpeedCoreIDs"];
    jsonCoreIds = nlohmann::json::array();

    // There may not be any entries in the D-Bus property, so only populate
    // if there was actually something there.
    if (highPriorityGroup != baseSpeedSettings.cend())
    {
        jsonCoreIds = std::get<std::vector<uint32_t>>(*highPriorityGroup);
    }
}

/**
 * Fill out OperatingConfig related items in a Processor resource by requesting
 * data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       cpuId       CPU D-Bus name.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getCpuConfigData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& cpuId,
                             const std::string& service,
                             const std::string& objPath)
{
    BMCWEB_LOG_INFO << "Getting CPU operating configs for " << cpuId;

    // First, GetAll CurrentOperatingConfig properties on the object
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId, service](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, dbus::utility::DbusVariantType>>& properties) {
            if (ec)
            {
                BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                   << ec.message();
                messages::internalError(aResp->res);
                return;
            }

            nlohmann::json& json = aResp->res.jsonValue;

            for (const auto& [dbusPropName, variantVal] : properties)
            {
                if (dbusPropName == "AppliedConfig")
                {
                    const sdbusplus::message::object_path* dbusPathWrapper =
                        std::get_if<sdbusplus::message::object_path>(
                            &variantVal);
                    if (dbusPathWrapper == nullptr)
                    {
                        continue;
                    }

                    const std::string& dbusPath = dbusPathWrapper->str;
                    std::string uri = "/redfish/v1/Systems/system/Processors/" +
                                      cpuId + "/OperatingConfigs";
                    json["OperatingConfigs"] = {{"@odata.id", uri}};

                    // Reuse the D-Bus config object name for the Redfish
                    // URI
                    size_t baseNamePos = dbusPath.rfind('/');
                    if (baseNamePos == std::string::npos ||
                        baseNamePos == (dbusPath.size() - 1))
                    {
                        // If the AppliedConfig was somehow not a valid path,
                        // skip adding any more properties, since everything
                        // else is tied to this applied config.
                        messages::internalError(aResp->res);
                        break;
                    }
                    uri += '/';
                    uri += dbusPath.substr(baseNamePos + 1);
                    json["AppliedOperatingConfig"] = {{"@odata.id", uri}};

                    // Once we found the current applied config, queue another
                    // request to read the base freq core ids out of that
                    // config.
                    sdbusplus::asio::getProperty<
                        BaseSpeedPrioritySettingsProperty>(
                        *crow::connections::systemBus, service, dbusPath,
                        "xyz.openbmc_project.Inventory.Item.Cpu."
                        "OperatingConfig",
                        "BaseSpeedPrioritySettings",
                        [aResp](const boost::system::error_code ec,
                                const BaseSpeedPrioritySettingsProperty&
                                    baseSpeedList) {
                            if (ec)
                            {
                                BMCWEB_LOG_WARNING
                                    << "D-Bus Property Get error: " << ec;
                                messages::internalError(aResp->res);
                                return;
                            }

                            highSpeedCoreIdsHandler(aResp, baseSpeedList);
                        });
                }
                else if (dbusPropName == "BaseSpeedPriorityEnabled")
                {
                    const bool* state = std::get_if<bool>(&variantVal);
                    if (state != nullptr)
                    {
                        json["BaseSpeedPriorityState"] =
                            *state ? "Enabled" : "Disabled";
                    }
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig");
}

/**
 * @brief Fill out location info of a processor by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getCpuLocationCode(std::shared_ptr<bmcweb::AsyncResp> aResp,
                               const std::string& service,
                               const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Cpu Location Data";
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Inventory.Decorator.LocationCode", "LocationCode",
        [objPath, aResp{std::move(aResp)}](const boost::system::error_code ec,
                                           const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            aResp->res.jsonValue["Location"]["PartLocation"]["ServiceLabel"] =
                property;
        });
}

/**
 * @brief Fill out location info of a processor by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getCpuLocationType(std::shared_ptr<bmcweb::AsyncResp> aResp,
                               const std::string& service,
                               const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Cpu LocationType Data";
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Inventory.Decorator.Location", "LocationType",
        [objPath, aResp{std::move(aResp)}](const boost::system::error_code ec,
                                           const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            aResp->res.jsonValue["Location"]["PartLocation"]["LocationType"] =
                redfish::dbus_utils::toLocationType(property);
        });
}

/**
 * Populate the unique identifier in a Processor resource by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getCpuUniqueId(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& service,
                           const std::string& objectPath)
{
    BMCWEB_LOG_DEBUG << "Get CPU UniqueIdentifier";
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, objectPath,
        "xyz.openbmc_project.Inventory.Decorator.UniqueIdentifier",
        "UniqueIdentifier",
        [aResp](boost::system::error_code ec, const std::string& id) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Failed to read cpu unique id: " << ec;
                messages::internalError(aResp->res);
                return;
            }
            aResp->res
                .jsonValue["ProcessorId"]["ProtectedIdentificationNumber"] = id;
        });
}

/**
 * Find the D-Bus object representing the requested Processor, and call the
 * handler with the results. If matching object is not found, add 404 error to
 * response and don't call the handler.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Redfish Processor Id.
 * @param[in]       handler         Callback to continue processing request upon
 *                                  successfully finding object.
 */
template <typename Handler>
inline void getProcessorObject(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                               const std::string& processorId,
                               Handler&& handler)
{
    BMCWEB_LOG_DEBUG << "Get available system processor resources.";

    // GetSubTree on all interfaces which provide info about a Processor
    crow::connections::systemBus->async_method_call(
        [resp, processorId, handler = std::forward<Handler>(handler)](
            boost::system::error_code ec,
            const MapperGetSubTreeResponse& subtree) mutable {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                messages::internalError(resp->res);
                return;
            }
            for (const auto& [objectPath, serviceMap] : subtree)
            {
                // Ignore any objects which don't end with our desired cpu name
                if (!boost::ends_with(objectPath, processorId))
                {
                    continue;
                }

                bool found = false;
                // Filter out objects that don't have the CPU-specific
                // interfaces to make sure we can return 404 on non-CPUs
                // (e.g. /redfish/../Processors/dimm0)
                for (const auto& [serviceName, interfaceList] : serviceMap)
                {
                    if (std::find_first_of(
                            interfaceList.begin(), interfaceList.end(),
                            processorInterfaces.begin(),
                            processorInterfaces.end()) != interfaceList.end())
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    continue;
                }

                // Process the first object which does match our cpu name and
                // required interfaces, and potentially ignore any other
                // matching objects. Assume all interfaces we want to process
                // must be on the same object path.

                handler(resp, processorId, objectPath, serviceMap);

                return;
            }
            messages::resourceNotFound(resp->res, "Processor", processorId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 9>{
            "xyz.openbmc_project.Common.UUID",
            "xyz.openbmc_project.Inventory.Decorator.Asset",
            "xyz.openbmc_project.Inventory.Decorator.Revision",
            "xyz.openbmc_project.Inventory.Item.Cpu",
            "xyz.openbmc_project.Inventory.Decorator.LocationCode",
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Software.Version",
            "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig",
            "xyz.openbmc_project.Inventory.Decorator.UniqueIdentifier"});
}

/**
 * Request all the properties for the given D-Bus object and fill out the
 * related entries in the Redfish OperatingConfig response.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service name to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getOperatingConfigData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& service,
                           const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                   << ec.message();
                messages::internalError(aResp->res);
                return;
            }

            nlohmann::json& json = aResp->res.jsonValue;
            for (const auto& [key, variant] : properties)
            {
                if (key == "AvailableCoreCount")
                {
                    const size_t* cores = std::get_if<size_t>(&variant);
                    if (cores != nullptr)
                    {
                        json["TotalAvailableCoreCount"] = *cores;
                    }
                }
                else if (key == "BaseSpeed")
                {
                    const uint32_t* speed = std::get_if<uint32_t>(&variant);
                    if (speed != nullptr)
                    {
                        json["BaseSpeedMHz"] = *speed;
                    }
                }
                else if (key == "MaxJunctionTemperature")
                {
                    const uint32_t* temp = std::get_if<uint32_t>(&variant);
                    if (temp != nullptr)
                    {
                        json["MaxJunctionTemperatureCelsius"] = *temp;
                    }
                }
                else if (key == "MaxSpeed")
                {
                    const uint32_t* speed = std::get_if<uint32_t>(&variant);
                    if (speed != nullptr)
                    {
                        json["MaxSpeedMHz"] = *speed;
                    }
                }
                else if (key == "MinSpeed")
                {
                    const uint32_t* speed = std::get_if<uint32_t>(&variant);
                    if (speed != nullptr)
                    {
                        json["MinSpeedMHz"] = *speed;
                    }
                }
                else if (key == "OperatingSpeed")
                {
                    const uint32_t* speed = std::get_if<uint32_t>(&variant);
                    if (speed != nullptr)
                    {
                        json["OperatingSpeedMHz"] = *speed;
                    }
                }
                else if (key == "PowerLimit")
                {
                    const uint32_t* tdp = std::get_if<uint32_t>(&variant);
                    if (tdp != nullptr)
                    {
                        json["TDPWatts"] = *tdp;
                    }
                }
                else if (key == "SpeedLimit")
                {
                    const uint32_t* speed = std::get_if<uint32_t>(&variant);
                    if (speed != nullptr)
                    {
                        json["SpeedLimitMHz"] = *speed;
                    }
                }
                else if (key == "SpeedLocked")
                {
                    const bool* speedLock = std::get_if<bool>(&variant);
                    if (speedLock != nullptr)
                    {
                        json["SpeedLocked"] = *speedLock;
                    }
                }
                else if (key == "TurboProfile")
                {
                    const auto* turboList =
                        std::get_if<TurboProfileProperty>(&variant);
                    if (turboList == nullptr)
                    {
                        continue;
                    }

                    nlohmann::json& turboArray = json["TurboProfile"];
                    turboArray = nlohmann::json::array();
                    for (const auto& [turboSpeed, coreCount] : *turboList)
                    {
                        turboArray.push_back({{"ActiveCoreCount", coreCount},
                                              {"MaxSpeedMHz", turboSpeed}});
                    }
                }
                else if (key == "BaseSpeedPrioritySettings")
                {
                    const auto* baseSpeedList =
                        std::get_if<BaseSpeedPrioritySettingsProperty>(
                            &variant);
                    if (baseSpeedList == nullptr)
                    {
                        continue;
                    }

                    nlohmann::json& baseSpeedArray =
                        json["BaseSpeedPrioritySettings"];
                    baseSpeedArray = nlohmann::json::array();
                    for (const auto& [baseSpeed, coreList] : *baseSpeedList)
                    {
                        baseSpeedArray.push_back(
                            {{"CoreCount", coreList.size()},
                             {"CoreIDs", coreList},
                             {"BaseSpeedMHz", baseSpeed}});
                    }
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig");
}

/**
 * Request all the properties for the given D-Bus object and fill out the
 * related entries in the Redfish processor response.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       cpuId       CPU D-Bus name.
 * @param[in]       service     D-Bus service name to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getProcessorMemoryData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& cpuId, const std::string& service,
                           const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId](boost::system::error_code ec,
                       const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                   << ec.message();
                messages::internalError(aResp->res);
                return;
            }
            nlohmann::json& json = aResp->res.jsonValue;
            std::string metricsURI = "/redfish/v1/Systems/system/Processors/";
            metricsURI += cpuId;
            metricsURI += "/MemorySummary/MemoryMetrics";
            json["MemorySummary"]["Metrics"]["@odata.id"] = metricsURI;
            for (const auto& [key, variant] : properties)
            {
                if (key == "CacheSizeInKiB")
                {
                    const uint64_t* value = std::get_if<uint64_t>(&variant);
                    if (value != nullptr)
                    {
                        json["MemorySummary"]["TotalCacheSizeMiB"] =
                            (*value) >> 10;
                    }
                }
                else if (key == "VolatileSizeInKiB")
                {
                    const uint64_t* value = std::get_if<uint64_t>(&variant);
                    if (value != nullptr)
                    {
                        json["MemorySummary"]["TotalMemorySizeMiB"] =
                            (*value) >> 10;
                    }
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.PersistentMemory");
}

inline void getEccModeData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& cpuId, const std::string& service,
                           const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId](const boost::system::error_code ec,
                       const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }
            nlohmann::json& json = aResp->res.jsonValue;
            for (const auto& property : properties)
            {
                if (property.first == "ECCModeEnabled")
                {
                    const bool* eccModeEnabled =
                        std::get_if<bool>(&property.second);
                    if (eccModeEnabled == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    json["MemorySummary"]["ECCModeEnabled"] = *eccModeEnabled;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Memory.MemoryECC");
}

inline void getProcessorEccModeData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& cpuId,
    const std::string& service, const std::string& objPath)
{
    nlohmann::json& json = aResp->res.jsonValue;
    std::string metricsURI = "/redfish/v1/Systems/system/Processors/";
    metricsURI += cpuId;
    metricsURI += "/MemorySummary/MemoryMetrics";
    json["MemorySummary"]["Metrics"]["@odata.id"] = metricsURI;
    getEccModeData(aResp, cpuId, service, objPath);
}

inline void getProcessorData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& processorId,
                             const std::string& objectPath,
                             const MapperServiceMap& serviceMap)
{
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        for (const auto& interface : interfaceList)
        {
            if (interface == "xyz.openbmc_project.Inventory.Decorator.Asset")
            {
                getCpuAssetData(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory."
                                  "Decorator.Revision")
            {
                getCpuRevisionData(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory.Item.Cpu")
            {
                getCpuDataByService(aResp, processorId, serviceName,
                                    objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory."
                                  "Item.Accelerator")
            {
                getAcceleratorDataByService(aResp, processorId, serviceName,
                                            objectPath);
            }
            else if (interface == "xyz.openbmc_project.Control.Processor."
                                  "CurrentOperatingConfig")
            {
                getCpuConfigData(aResp, processorId, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory."
                                  "Decorator.LocationCode")
            {
                getCpuLocationCode(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory."
                                  "Decorator.Location")
            {
                getCpuLocationType(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Software.Version")
            {
                getProcessorFirmwareVersion(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Common.UUID")
            {
                getProcessorUUID(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory."
                                  "Decorator.UniqueIdentifier")
            {
                getCpuUniqueId(aResp, serviceName, objectPath);
            }
            else if (interface ==
                     "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig")
            {
                getOperatingConfigData(aResp, serviceName, objectPath);
            }
            else if (interface ==
                     "xyz.openbmc_project.Inventory.Item.PersistentMemory")
            {
                getProcessorMemoryData(aResp, processorId, serviceName,
                                       objectPath);
            }
            else if (interface == "xyz.openbmc_project.Memory.MemoryECC")
            {
                getProcessorEccModeData(aResp, processorId, serviceName,
                                        objectPath);
            }
        }
    }
    aResp->res.jsonValue["EnvironmentMetrics"] = {
        {"@odata.id", "/redfish/v1/Systems/system/Processors/" + processorId +
                          "/EnvironmentMetrics"}};
    aResp->res.jsonValue["@Redfish.Settings"]["@odata.type"] =
        "#Settings.v1_3_3.Settings";
    aResp->res.jsonValue["@Redfish.Settings"]["SettingsObject"] = {
        {"@odata.id",
         "/redfish/v1/Systems/system/Processors/" + processorId + "/Settings"}};
    // Links association to underneath memory
    getProcessorMemoryLinks(aResp, objectPath);
    // Link association to parent chassis
    getProcessorChassisLink(aResp, objectPath);
}

/**
 * Handle the PATCH operation of the speed locked property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       speedLocked     New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchSpeedLocked(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                             const std::string& processorId,
                             const bool speedLocked,
                             const std::string& cpuObjectPath,
                             const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(
                interfaceList.begin(), interfaceList.end(),
                "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig") !=
            interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        messages::internalError(resp->res);
        return;
    }
    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, speedLocked](boost::system::error_code ec,
                                         sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set speed locked property succeeded";
                return;
            }

            BMCWEB_LOG_DEBUG << "CPU:" << processorId
                             << " set speed locked property failed: " << ec;
            // Read and convert dbus error message to redfish error
            const sd_bus_error* dbusError = msg.get_error();
            if (dbusError == nullptr)
            {
                messages::internalError(resp->res);
                return;
            }

            if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                        "Device.Error.WriteFailure") == 0)
            {
                // Service failed to change the config
                messages::operationFailed(resp->res);
            }
            else
            {
                messages::internalError(resp->res);
            }
        },
        *inventoryService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
        "SpeedLocked", std::variant<bool>(speedLocked));
}

/**
 * Handle the PATCH operation of the speed limit property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       speedLimit      New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchSpeedLimit(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                            const std::string& processorId,
                            const int speedLimit,
                            const std::string& cpuObjectPath,
                            const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(
                interfaceList.begin(), interfaceList.end(),
                "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig") !=
            interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        messages::internalError(resp->res);
        return;
    }
    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, speedLimit](boost::system::error_code ec,
                                        sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set speed limit property succeeded";
                return;
            }

            BMCWEB_LOG_DEBUG << "CPU:" << processorId
                             << " set speed limit property failed: " << ec;
            // Read and convert dbus error message to redfish error
            const sd_bus_error* dbusError = msg.get_error();
            if (dbusError == nullptr)
            {
                messages::internalError(resp->res);
                return;
            }
            if (strcmp(dbusError->name,
                       "xyz.openbmc_project.Common.Error.InvalidArgument") == 0)
            {
                // Invalid value
                messages::propertyValueIncorrect(resp->res, "SpeedLimit",
                                                 std::to_string(speedLimit));
            }
            else if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                             "Device.Error.WriteFailure") == 0)
            {
                // Service failed to change the config
                messages::operationFailed(resp->res);
            }
            else
            {
                messages::internalError(resp->res);
            }
        },
        *inventoryService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
        "SpeedLimit",
        std::variant<uint32_t>(static_cast<uint32_t>(speedLimit)));
}

/**
 * Handle the D-Bus response from attempting to set the CPU's AppliedConfig
 * property. Main task is to translate error messages into Redfish errors.
 *
 * @param[in,out]   resp    HTTP response.
 * @param[in]       setPropVal  Value which we attempted to set.
 * @param[in]       ec      D-Bus response error code.
 * @param[in]       msg     D-Bus response message.
 */
inline void
    handleAppliedConfigResponse(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                                const std::string& setPropVal,
                                boost::system::error_code ec,
                                const sdbusplus::message::message& msg)
{
    if (!ec)
    {
        BMCWEB_LOG_DEBUG << "Set Property succeeded";
        return;
    }

    BMCWEB_LOG_DEBUG << "Set Property failed: " << ec;

    const sd_bus_error* dbusError = msg.get_error();
    if (dbusError == nullptr)
    {
        messages::internalError(resp->res);
        return;
    }

    // The asio error code doesn't know about our custom errors, so we have to
    // parse the error string. Some of these D-Bus -> Redfish translations are a
    // stretch, but it's good to try to communicate something vaguely useful.
    if (strcmp(dbusError->name,
               "xyz.openbmc_project.Common.Error.InvalidArgument") == 0)
    {
        // Service did not like the object_path we tried to set.
        messages::propertyValueIncorrect(
            resp->res, "AppliedOperatingConfig/@odata.id", setPropVal);
    }
    else if (strcmp(dbusError->name,
                    "xyz.openbmc_project.Common.Error.NotAllowed") == 0)
    {
        // Service indicates we can never change the config for this processor.
        messages::propertyNotWritable(resp->res, "AppliedOperatingConfig");
    }
    else if (strcmp(dbusError->name,
                    "xyz.openbmc_project.Common.Error.Unavailable") == 0)
    {
        // Service indicates the config cannot be changed right now, but maybe
        // in a different system state.
        messages::resourceInStandby(resp->res);
    }
    else if (strcmp(dbusError->name,
                    "xyz.openbmc_project.Common.Device.Error.WriteFailure") ==
             0)
    {
        // Service tried to change the config, but it failed.
        messages::operationFailed(resp->res);
    }
    else
    {
        messages::internalError(resp->res);
    }
}

/**
 * Handle the PATCH operation of the AppliedOperatingConfig property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       appliedConfigUri    New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchAppliedOperatingConfig(
    const std::shared_ptr<bmcweb::AsyncResp>& resp,
    const std::string& processorId, const std::string& appliedConfigUri,
    const std::string& cpuObjectPath, const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* controlService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "xyz.openbmc_project.Control.Processor."
                      "CurrentOperatingConfig") != interfaceList.end())
        {
            controlService = &serviceName;
            break;
        }
    }

    if (controlService == nullptr)
    {
        messages::internalError(resp->res);
        return;
    }

    // Check that the config URI is a child of the cpu URI being patched.
    std::string expectedPrefix("/redfish/v1/Systems/system/Processors/");
    expectedPrefix += processorId;
    expectedPrefix += "/OperatingConfigs/";
    if (!boost::starts_with(appliedConfigUri, expectedPrefix) ||
        expectedPrefix.size() == appliedConfigUri.size())
    {
        messages::propertyValueIncorrect(
            resp->res, "AppliedOperatingConfig/@odata.id", appliedConfigUri);
        return;
    }

    // Generate the D-Bus path of the OperatingConfig object, by assuming it's a
    // direct child of the CPU object.
    // Strip the expectedPrefix from the config URI to get the "filename", and
    // append to the CPU's path.
    std::string configBaseName = appliedConfigUri.substr(expectedPrefix.size());
    sdbusplus::message::object_path configPath(cpuObjectPath);
    configPath /= configBaseName;

    BMCWEB_LOG_INFO << "Setting config to " << configPath.str;

    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, appliedConfigUri](const boost::system::error_code ec,
                                 const sdbusplus::message::message& msg) {
            handleAppliedConfigResponse(resp, appliedConfigUri, ec, msg);
        },
        *controlService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig",
        "AppliedConfig", dbus::utility::DbusVariantType(std::move(configPath)));
}

inline void requestRoutesOperatingConfigCollection(App& app)
{

    BMCWEB_ROUTE(
        app, "/redfish/v1/Systems/system/Processors/<str>/OperatingConfigs/")
        .privileges(redfish::privileges::getOperatingConfigCollection)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request& req,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& cpuName) {
            asyncResp->res.jsonValue["@odata.type"] =
                "#OperatingConfigCollection.OperatingConfigCollection";
            asyncResp->res.jsonValue["@odata.id"] = req.url;
            asyncResp->res.jsonValue["Name"] = "Operating Config Collection";

            // First find the matching CPU object so we know how to
            // constrain our search for related Config objects.
            crow::connections::systemBus->async_method_call(
                [asyncResp, cpuName](const boost::system::error_code ec,
                                     const std::vector<std::string>& objects) {
                    if (ec)
                    {
                        BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                           << ec.message();
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    for (const std::string& object : objects)
                    {
                        if (!boost::ends_with(object, cpuName))
                        {
                            continue;
                        }

                        // Not expected that there will be multiple matching
                        // CPU objects, but if there are just use the first
                        // one.

                        // Use the common search routine to construct the
                        // Collection of all Config objects under this CPU.
                        collection_util::getCollectionMembers(
                            asyncResp,
                            "/redfish/v1/Systems/system/Processors/" + cpuName +
                                "/OperatingConfigs",
                            {"xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig"},
                            object.c_str());
                        return;
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig"});
        });
}

inline void requestRoutesOperatingConfig(App& app)
{
    BMCWEB_ROUTE(
        app,
        "/redfish/v1/Systems/system/Processors/<str>/OperatingConfigs/<str>/")
        .privileges(redfish::privileges::getOperatingConfig)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request& req,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& cpuName,
                                              const std::string& configName) {
            // Ask for all objects implementing OperatingConfig so we can search
            // for one with a matching name
            crow::connections::systemBus->async_method_call(
                [asyncResp, cpuName, configName,
                 reqUrl{req.url}](boost::system::error_code ec,
                                  const MapperGetSubTreeResponse& subtree) {
                    if (ec)
                    {
                        BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                           << ec.message();
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    const std::string expectedEnding =
                        cpuName + '/' + configName;
                    for (const auto& [objectPath, serviceMap] : subtree)
                    {
                        // Ignore any configs without matching cpuX/configY
                        if (!boost::ends_with(objectPath, expectedEnding) ||
                            serviceMap.empty())
                        {
                            continue;
                        }

                        nlohmann::json& json = asyncResp->res.jsonValue;
                        json["@odata.type"] =
                            "#OperatingConfig.v1_0_0.OperatingConfig";
                        json["@odata.id"] = reqUrl;
                        json["Name"] = "Processor Profile";
                        json["Id"] = configName;

                        // Just use the first implementation of the object - not
                        // expected that there would be multiple matching
                        // services
                        getOperatingConfigData(
                            asyncResp, serviceMap.begin()->first, objectPath);
                        return;
                    }
                    messages::resourceNotFound(asyncResp->res,
                                               "OperatingConfig", configName);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig"});
        });
}

inline void requestRoutesProcessorCollection(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/system/Processors/")
        .privileges(redfish::privileges::getProcessorCollection)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#ProcessorCollection.ProcessorCollection";
                asyncResp->res.jsonValue["Name"] = "Processor Collection";

                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/system/Processors";

                collection_util::getCollectionMembers(
                    asyncResp, "/redfish/v1/Systems/system/Processors",
                    std::vector<const char*>(processorInterfaces.begin(),
                                             processorInterfaces.end()));
            });
}

inline void requestRoutesProcessor(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/system/Processors/<str>/")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#Processor.v1_13_0.Processor";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/system/Processors/" + processorId;
                std::string processorMetricsURI =
                    "/redfish/v1/Systems/system/Processors/";
                processorMetricsURI += processorId;
                processorMetricsURI += "/ProcessorMetrics";
                asyncResp->res.jsonValue["Metrics"]["@odata.id"] =
                    processorMetricsURI;

                getProcessorObject(asyncResp, processorId, getProcessorData);
            });

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/system/Processors/<str>/")
        .privileges(redfish::privileges::patchProcessor)
        .methods(boost::beast::http::verb::patch)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                std::optional<int> speedLimit;
                std::optional<bool> speedLocked;
                std::optional<nlohmann::json> appliedConfigJson;
                // Read json request
                if (!json_util::readJson(req, asyncResp->res, "SpeedLimitMHz",
                                         speedLimit, "SpeedLocked", speedLocked,
                                         "AppliedOperatingConfig",
                                         appliedConfigJson))
                {
                    return;
                }
                // Update speed limit
                if (speedLimit)
                {
                    getProcessorObject(
                        asyncResp, processorId,
                        [speedLimit](
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& processorId,
                            const std::string& objectPath,
                            const MapperServiceMap& serviceMap) {
                            patchSpeedLimit(asyncResp, processorId, *speedLimit,
                                            objectPath, serviceMap);
                        });
                }
                // Update speed locked
                if (speedLocked)
                {
                    getProcessorObject(
                        asyncResp, processorId,
                        [speedLocked](
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& processorId,
                            const std::string& objectPath,
                            const MapperServiceMap& serviceMap) {
                            patchSpeedLocked(asyncResp, processorId,
                                             *speedLocked, objectPath,
                                             serviceMap);
                        });
                }

                std::string appliedConfigUri;
                if (appliedConfigJson)
                {
                    if (!json_util::readJson(*appliedConfigJson, asyncResp->res,
                                             "@odata.id", appliedConfigUri))
                    {
                        return;
                    }
                    // Check for 404 and find matching D-Bus object, then run
                    // property patch handlers if that all succeeds.
                    getProcessorObject(
                        asyncResp, processorId,
                        [appliedConfigUri = std::move(appliedConfigUri)](
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& processorId,
                            const std::string& objectPath,
                            const MapperServiceMap& serviceMap) {
                            patchAppliedOperatingConfig(asyncResp, processorId,
                                                        appliedConfigUri,
                                                        objectPath, serviceMap);
                        });
                }
            });
}

inline void getProcessorDataByService(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                      const std::string& service,
                                      const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get processor metrics data.";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "OperatingSpeed")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["OperatingSpeedMHz"] = *value;
                }
                else if (property.first == "Utilization")
                {
                    const double* value = std::get_if<double>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["BandwidthPercent"] = *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig");
}

inline void getProcessorMemoryECCData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                      const std::string& service,
                                      const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get processor memory ecc data.";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "ceCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["CacheMetricsTotal"]["LifeTime"]
                                        ["CorrectableECCErrorCount"] = *value;
                }
                else if (property.first == "ueCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["CacheMetricsTotal"]["LifeTime"]
                                        ["UncorrectableECCErrorCount"] = *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Memory.MemoryECC");
}

inline void getProcessorMetricsData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                    const std::string& processorId)
{
    BMCWEB_LOG_DEBUG << "Get available system processor resource";
    crow::connections::systemBus->async_method_call(
        [processorId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);

                return;
            }
            for (const auto& [path, object] : subtree)
            {
                if (!boost::ends_with(path, processorId))
                {
                    continue;
                }
                std::string processorMetricsURI =
                    "/redfish/v1/Systems/system/Processors/";
                processorMetricsURI += processorId;
                processorMetricsURI += "/ProcessorMetrics";
                aResp->res.jsonValue["@odata.type"] =
                    "#ProcessorMetrics.v1_4_0.ProcessorMetrics";
                aResp->res.jsonValue["@odata.id"] = processorMetricsURI;
                aResp->res.jsonValue["Id"] = "ProcessorMetrics";
                aResp->res.jsonValue["Name"] =
                    processorId + " Processor Metrics";
                for (const auto& [service, interfaces] : object)
                {
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Inventory.Item.Cpu."
                                  "OperatingConfig") != interfaces.end())
                    {
                        getProcessorDataByService(aResp, service, path);
                    }
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Memory.MemoryECC") !=
                        interfaces.end())
                    {
                        getProcessorMemoryECCData(aResp, service, path);
                    }
                }
                return;
            }
            // Object not found
            messages::resourceNotFound(
                aResp->res, "#Processor.v1_13_0.Processor", processorId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.Accelerator"});
}

inline void requestRoutesProcessorMetrics(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/system/Processors/<str>/ProcessorMetrics")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                getProcessorMetricsData(asyncResp, processorId);
            });
}

inline void getProcessorMemoryDataByService(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath,
    const std::string& memoryPath, const int64_t& processorCECount,
    const int64_t& processorUECount)
{
    BMCWEB_LOG_DEBUG << "Get processor memory data";
    crow::connections::systemBus->async_method_call(
        [aResp, memoryPath, processorCECount, processorUECount](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(aResp->res);
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                // Get the processor memory
                if (object.first != memoryPath)
                {
                    continue;
                }
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;
                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }
                const std::string& connectionName = connectionNames[0].first;
                crow::connections::systemBus->async_method_call(
                    [aResp{aResp}, processorCECount, processorUECount](
                        const boost::system::error_code ec,
                        const OperatingConfigProperties& properties) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error";
                            messages::internalError(aResp->res);

                            return;
                        }
                        for (const auto& property : properties)
                        {
                            if (property.first == "MemoryConfiguredSpeedInMhz")
                            {
                                const uint16_t* value =
                                    std::get_if<uint16_t>(&property.second);
                                if (value == nullptr)
                                {
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                aResp->res.jsonValue["OperatingSpeedMHz"] =
                                    *value;
                            }
                            else if (property.first == "Utilization")
                            {
                                const double* value =
                                    std::get_if<double>(&property.second);
                                if (value == nullptr)
                                {
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                aResp->res.jsonValue["BandwidthPercent"] =
                                    *value;
                            }
                            else if (property.first == "ceCount")
                            {
                                const int64_t* value =
                                    std::get_if<int64_t>(&property.second);
                                if (value == nullptr)
                                {
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                aResp->res
                                    .jsonValue["LifeTime"]
                                              ["CorrectableECCErrorCount"] =
                                    *value + processorCECount;
                            }
                            else if (property.first == "ueCount")
                            {
                                const int64_t* value =
                                    std::get_if<int64_t>(&property.second);
                                if (value == nullptr)
                                {
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                aResp->res
                                    .jsonValue["LifeTime"]
                                              ["UncorrectableECCErrorCount"] =
                                    *value + processorUECount;
                            }
                        }
                    },
                    connectionName, memoryPath,
                    "org.freedesktop.DBus.Properties", "GetAll", "");
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", objPath, 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Dimm"});
}

inline void getProcessorMemorySummary(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath,
    const int64_t& processorCECount, const int64_t& processorUECount)
{
    BMCWEB_LOG_DEBUG << "Get available system processor resource";
    // Get processor memory
    crow::connections::systemBus->async_method_call(
        [aResp, processorCECount,
         processorUECount](const boost::system::error_code ec,
                           std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no memory = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            for (const std::string& memoryPath : *data)
            {
                // Get subtree for memory parent path
                size_t separator = memoryPath.rfind('/');
                if (separator == std::string::npos)
                {
                    BMCWEB_LOG_ERROR << "Invalid memory path";
                    continue;
                }
                std::string parentPath = memoryPath.substr(0, separator);
                // Get entity subtree
                getProcessorMemoryDataByService(aResp, parentPath, memoryPath,
                                                processorCECount,
                                                processorUECount);
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_memory",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getProcessorMemoryMetricsData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const std::string& processorId)
{
    BMCWEB_LOG_DEBUG << "Get available system processor resource";
    crow::connections::systemBus->async_method_call(
        [processorId, aResp{aResp}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);

                return;
            }
            for (const auto& [path, object] : subtree)
            {
                if (!boost::ends_with(path, processorId))
                {
                    continue;
                }
                std::string memoryMetricsURI =
                    "/redfish/v1/Systems/system/Processors/";
                memoryMetricsURI += processorId;
                memoryMetricsURI += "/MemorySummary/MemoryMetrics";
                aResp->res.jsonValue["@odata.type"] =
                    "#MemoryMetrics.v1_4_1.MemoryMetrics";
                aResp->res.jsonValue["@odata.id"] = memoryMetricsURI;
                aResp->res.jsonValue["Id"] = "MemoryMetrics";
                aResp->res.jsonValue["Name"] =
                    processorId + " Memory Summary Metrics";
                // Get processor cache memory ECC counts
                for (const auto& [service, interfaces] : object)
                {
                    const std::string memoryECCInterface =
                        "xyz.openbmc_project.Memory.MemoryECC";
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  memoryECCInterface) != interfaces.end())
                    {
                        crow::connections::systemBus->async_method_call(
                            [path = path, aResp{aResp}](
                                const boost::system::error_code ec,
                                const OperatingConfigProperties& properties) {
                                if (ec)
                                {
                                    BMCWEB_LOG_DEBUG << "DBUS response error";
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                // Get processor memory error counts to combine
                                // to memory summary error counts
                                int64_t processorCECount = 0;
                                int64_t processorUECount = 0;
                                for (const auto& property : properties)
                                {
                                    if (property.first == "ceCount")
                                    {
                                        const int64_t* value =
                                            std::get_if<int64_t>(
                                                &property.second);
                                        if (value == nullptr)
                                        {
                                            messages::internalError(aResp->res);
                                            return;
                                        }
                                        processorCECount = *value;
                                    }
                                    else if (property.first == "ueCount")
                                    {
                                        const int64_t* value =
                                            std::get_if<int64_t>(
                                                &property.second);
                                        if (value == nullptr)
                                        {
                                            messages::internalError(aResp->res);
                                            return;
                                        }
                                        processorUECount = *value;
                                    }
                                }
                                // Get processor memory summary data
                                getProcessorMemorySummary(aResp, path,
                                                          processorCECount,
                                                          processorUECount);
                            },
                            service, path, "org.freedesktop.DBus.Properties",
                            "GetAll", memoryECCInterface);
                    }
                }
                return;
            }
            // Object not found
            messages::resourceNotFound(
                aResp->res, "#Processor.v1_13_0.Processor", processorId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.Accelerator"});
}

inline void requestRoutesProcessorMemoryMetrics(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/system/Processors/<str>/"
                      "MemorySummary/MemoryMetrics")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                getProcessorMemoryMetricsData(asyncResp, processorId);
            });
}

inline std::string toRequestedApplyTime(const std::string& applyTime)
{
    if (applyTime ==
        "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.Immediate")
    {
        return "Immediate";
    }
    if (applyTime ==
        "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.OnReset")
    {
        return "OnReset";
    }
    // Unknown or others
    return "";
}

inline void
    getProcessorSettingsData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& processorId)
{
    BMCWEB_LOG_DEBUG << "Get available system processor resource";
    crow::connections::systemBus->async_method_call(
        [aResp, processorId](boost::system::error_code ec,
                             const MapperGetSubTreeResponse& subtree) mutable {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                messages::internalError(aResp->res);
                return;
            }
            for (const auto& [path, object] : subtree)
            {
                if (!boost::ends_with(path, processorId))
                {
                    continue;
                }
                for (const auto& [service, interfaces] : object)
                {
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Memory.MemoryECC") !=
                        interfaces.end())
                    {
                        nlohmann::json& json = aResp->res.jsonValue;
                        json["@odata.id"] =
                            "/redfish/v1/Systems/system/Processors/" +
                            processorId + "/Settings";
                        json["@odata.type"] = "#Processor.v1_13_0.Processor";
                        json["Id"] = "Settings";
                        json["Name"] = processorId + "PendingSettings";
                        getEccModeData(aResp, processorId, service, path);
                    }
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Software.ApplyTime") !=
                        interfaces.end())
                    {
                        crow::connections::systemBus->async_method_call(
                            [aResp](
                                const boost::system::error_code ec,
                                const OperatingConfigProperties& properties) {
                                if (ec)
                                {
                                    BMCWEB_LOG_DEBUG << "DBUS response error";
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                nlohmann::json& json = aResp->res.jsonValue;
                                for (const auto& property : properties)
                                {
                                    if (property.first == "RequestedApplyTime")
                                    {
                                        const std::string* applyTime =
                                            std::get_if<std::string>(
                                                &property.second);
                                        if (applyTime == nullptr)
                                        {
                                            messages::internalError(aResp->res);
                                            return;
                                        }
                                        json["@Redfish.SettingsApplyTime"]
                                            ["@odata.type"] =
                                                "#Settings.v1_3_3.PreferredApplyTime";
                                        json["@Redfish.SettingsApplyTime"]
                                            ["ApplyTime"] =
                                                toRequestedApplyTime(
                                                    *applyTime);
                                    }
                                }
                            },
                            service, path, "org.freedesktop.DBus.Properties",
                            "GetAll", "xyz.openbmc_project.Software.ApplyTime");
                    }
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Cpu",
            "xyz.openbmc_project.Inventory.Item.Accelerator"});
}

inline void patchEccMode(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                         const std::string& processorId,
                         const bool eccModeEnabled,
                         const std::string& cpuObjectPath,
                         const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "xyz.openbmc_project.Memory.MemoryECC") !=
            interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        messages::internalError(resp->res);
        return;
    }
    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, eccModeEnabled](boost::system::error_code ec,
                                            sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set eccModeEnabled succeeded";
                messages::success(resp->res);
                return;
            }

            BMCWEB_LOG_DEBUG << "CPU:" << processorId
                             << " set eccModeEnabled property failed: " << ec;
            // Read and convert dbus error message to redfish error
            const sd_bus_error* dbusError = msg.get_error();
            if (dbusError == nullptr)
            {
                messages::internalError(resp->res);
                return;
            }

            if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                        "Device.Error.WriteFailure") == 0)
            {
                // Service failed to change the config
                messages::operationFailed(resp->res);
            }
            else
            {
                messages::internalError(resp->res);
            }
        },
        *inventoryService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "xyz.openbmc_project.Memory.MemoryECC", "ECCModeEnabled",
        std::variant<bool>(eccModeEnabled));
}

inline void requestRoutesProcessorSettings(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/system/Processors/<str>/"
                      "Settings")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                getProcessorSettingsData(asyncResp, processorId);
            });

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/system/Processors/<str>/"
                      "Settings")
        .privileges(redfish::privileges::patchProcessor)
        .methods(boost::beast::http::verb::patch)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                std::optional<nlohmann::json> memSummary;
                if (!json_util::readJson(req, asyncResp->res, "MemorySummary",
                                         memSummary))
                {
                    return;
                }
                std::optional<bool> eccModeEnabled;
                if (memSummary)
                {
                    if (json_util::readJson(*memSummary, asyncResp->res,
                                            "ECCModeEnabled", eccModeEnabled))
                    {
                        getProcessorObject(
                            asyncResp, processorId,
                            [eccModeEnabled](
                                const std::shared_ptr<bmcweb::AsyncResp>&
                                    asyncResp,
                                const std::string& processorId,
                                const std::string& objectPath,
                                const MapperServiceMap& serviceMap) {
                                patchEccMode(asyncResp, processorId,
                                             *eccModeEnabled, objectPath,
                                             serviceMap);
                            });
                    }
                }
            });
}

} // namespace redfish
