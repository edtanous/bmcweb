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
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <sdbusplus/utility/dedup_variant.hpp>
#include <utils/collection.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/json_utils.hpp>
#include <utils/port_utils.hpp>
#include <utils/processor_utils.hpp>

namespace redfish
{
using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;
using GetManagedPropertyType =
    boost::container::flat_map<std::string, dbus::utility::DbusVariantType>;
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

inline std::string getProcessorFpgaType(const std::string& processorFpgaType)
{
    if (processorFpgaType ==
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete")
    {
        return "Discrete";
    }
    if (processorFpgaType ==
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Integrated")
    {
        return "Integrated";
    }
    // Unknown or others
    return "";
}

inline std::string getProcessorResetType(const std::string& processorType)
{
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.ForceOff")
    {
        return "ForceOff";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.ForceOn")
    {
        return "ForceOn";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.ForceRestart")
    {
        return "ForceRestart";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.GracefulRestart")
    {
        return "GracefulRestart";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.GracefulShutdown")
    {
        return "GracefulShutdown";
    }
    // Unknown or others
    return "";
}

/**
 * @brief Fill out pcie interface properties by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out] asyncResp       Async HTTP response.
 * @param[in]     objPath         D-Bus object to query.
 */
inline void getSystemPCIeInterfaceProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get processor system pcie interface properties";
    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath](
            const boost::system::error_code errorCode,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) mutable {
            if (errorCode)
            {
                BMCWEB_LOG_ERROR << "error_code = " << errorCode;
                BMCWEB_LOG_ERROR << "error msg = " << errorCode.message();
                if (asyncResp)
                {
                    messages::internalError(asyncResp->res);
                }
                return;
            }
            if (objInfo.empty())
            {
                BMCWEB_LOG_ERROR << "Empty Object Size";
                if (asyncResp)
                {
                    messages::internalError(asyncResp->res);
                }
                return;
            }
            // Get all properties
            crow::connections::systemBus->async_method_call(
                [asyncResp](const boost::system::error_code ec,
                            GetManagedPropertyType& resp) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "error_code = " << ec;
                        BMCWEB_LOG_ERROR << "error msg = " << ec.message();
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    asyncResp->res
                        .jsonValue["SystemInterface"]["InterfaceType"] = "PCIe";
                    for (auto& property : resp)
                    {
                        const std::string& propertyName = property.first;
                        if (propertyName == "CurrentSpeed")
                        {
                            const size_t* value =
                                std::get_if<size_t>(&property.second);
                            if (value == nullptr)
                            {
                                BMCWEB_LOG_DEBUG << "Null value returned "
                                                    "for CurrentSpeed";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            asyncResp->res.jsonValue["SystemInterface"]["PCIe"]
                                                    ["PCIeType"] =
                                redfish::port_utils::getLinkSpeedGeneration(
                                    *value);
                        }
                        else if (propertyName == "ActiveWidth")
                        {
                            const size_t* value =
                                std::get_if<size_t>(&property.second);
                            if (value == nullptr)
                            {
                                BMCWEB_LOG_DEBUG << "Null value returned "
                                                    "for Width";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            asyncResp->res.jsonValue["SystemInterface"]["PCIe"]
                                                    ["LanesInUse"] =
                                redfish::port_utils::getLinkWidth(*value);
                        }
                    }
                    return;
                },
                objInfo[0].first, objPath, "org.freedesktop.DBus.Properties",
                "GetAll", "");
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", objPath,
        std::array<const char*, 0>());
}

/**
 * @brief Fill out pcie interface properties by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out] asyncResp       Async HTTP response.
 * @param[in]     objPath         D-Bus object to query.
 */
inline void getFPGAPCIeInterfaceProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get processor fpga pcie interface properties";
    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath](
            const boost::system::error_code errorCode,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) mutable {
            if (errorCode)
            {
                BMCWEB_LOG_ERROR << "error_code = " << errorCode;
                BMCWEB_LOG_ERROR << "error msg = " << errorCode.message();
                if (asyncResp)
                {
                    messages::internalError(asyncResp->res);
                }
                return;
            }
            if (objInfo.empty())
            {
                BMCWEB_LOG_ERROR << "Empty Object Size";
                if (asyncResp)
                {
                    messages::internalError(asyncResp->res);
                }
                return;
            }
            // Get all properties
            crow::connections::systemBus->async_method_call(
                [asyncResp](const boost::system::error_code ec,
                            GetManagedPropertyType& resp) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "error_code = " << ec;
                        BMCWEB_LOG_ERROR << "error msg = " << ec.message();
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    std::string speed;
                    size_t width = 0;
                    for (auto& property : resp)
                    {
                        const std::string& propertyName = property.first;
                        if (propertyName == "CurrentSpeed")
                        {
                            const size_t* value =
                                std::get_if<size_t>(&property.second);
                            if (value == nullptr)
                            {
                                BMCWEB_LOG_DEBUG << "Null value returned "
                                                    "for CurrentSpeed";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            speed = redfish::port_utils::getLinkSpeedGeneration(
                                *value);
                        }
                        else if (propertyName == "Width")
                        {
                            const size_t* value =
                                std::get_if<size_t>(&property.second);
                            if (value == nullptr)
                            {
                                BMCWEB_LOG_DEBUG << "Null value returned "
                                                    "for Width";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            width = redfish::port_utils::getLinkWidth(*value);
                        }
                    }
                    nlohmann::json& fpgaIfaceArray =
                        asyncResp->res.jsonValue["FPGA"]["ExternalInterfaces"];
                    fpgaIfaceArray = nlohmann::json::array();
                    fpgaIfaceArray.push_back(
                        {{"InterfaceType", "PCIe"},
                         {"PCIe",
                          {{"PCIeType", speed}, {"LanesInUse", width}}}});
                    return;
                },
                objInfo[0].first, objPath, "org.freedesktop.DBus.Properties",
                "GetAll", "");
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", objPath,
        std::array<const char*, 0>());
}

/**
 * @brief Fill out system PCIe interface properties by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getProcessorSystemPCIeInterface(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get underneath system interface pcie link";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no system interface = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            for (const std::string& linkPath : *data)
            {
                getSystemPCIeInterfaceProperties(aResp, linkPath);
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/system_interface",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out fpga PCIe interface properties by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getProcessorFPGAPCIeInterface(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get underneath fpga interface pcie link";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no fpga interface = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            for (const std::string& linkPath : *data)
            {
                getFPGAPCIeInterfaceProperties(aResp, linkPath);
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/fpga_interface",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
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
                    {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                   "/Memory/" +
                                       memoryName}});
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
                 parentChassisName](const boost::system::error_code ec1,
                                    const MapperGetSubTreeResponse& subtree) {
                    if (ec1)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    for (const auto& [objectPath1, serviceMap] : subtree)
                    {
                        // Process same device
                        if (!boost::ends_with(objectPath1, chassisName))
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
                            aResp, serviceName, objectPath1, pcieDeviceLink);
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
                            const std::string& objPath, const std::string& service)
{
    BMCWEB_LOG_DEBUG << "Get parent chassis link";
    crow::connections::systemBus->async_method_call(
        [aResp, objPath, service](const boost::system::error_code ec,
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

            // Get PCIeDevice on this chassis
            crow::connections::systemBus->async_method_call(
                [aResp, chassisName, chassisPath,  
                service](const boost::system::error_code ec,
                        std::variant<std::vector<std::string>>& resp) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << "Chassis has no connected PCIe devices";
                    return; // no pciedevices = no failures
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr && data->size() > 1)
                {
                    // Chassis must have single pciedevice
                    BMCWEB_LOG_ERROR << "chassis must have single pciedevice";
                    return;
                }
                const std::string& pcieDevicePath = data->front();
                sdbusplus::message::object_path objectPath(pcieDevicePath);
                std::string pcieDeviceName = objectPath.filename();
                if (pcieDeviceName.empty())
                {
                    BMCWEB_LOG_ERROR << "chassis pciedevice name empty";
                    messages::internalError(aResp->res);
                    return;
                }
                            std::string pcieDeviceLink = "/redfish/v1/Chassis/";
                            pcieDeviceLink += chassisName;
                            pcieDeviceLink += "/PCIeDevices/";
                            pcieDeviceLink += chassisName;
                            aResp->res.jsonValue["Links"]["PCIeDevice"] = {
                                {"@odata.id", pcieDeviceLink}};

                // Get PCIeFunctions Link
                getProcessorPCIeFunctionsLinks(
                       aResp, service, pcieDevicePath, pcieDeviceLink);

                },
                "xyz.openbmc_project.ObjectMapper", chassisPath + "/pciedevice",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");

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
 * @brief Fill out fpgsType info of a processor by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getFpgaTypeData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                             const std::string& service,
                             const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Processor fpgatype";
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Inventory.Decorator.FpgaType", "FpgaType",
        [objPath, aResp{std::move(aResp)}](const boost::system::error_code ec,
                                           const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }
            std::string fpgaType = getProcessorFpgaType(property);
            aResp->res.jsonValue["FPGA"]["FpgaType"] = fpgaType;
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
                if (!*cpuPresent)
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
                if (!*cpuFunctional)
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
                if (value != nullptr && *value != 2)
                {
                    aResp->res.jsonValue["ProcessorId"]["EffectiveFamily"] =
                        "0x" + intToHexString(*value, 4);
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
                if (*value != 0)
                {
                    aResp->res.jsonValue["ProcessorId"]["EffectiveModel"] =
                        "0x" + intToHexString(*value, 4);
                }
            }
            else if (property.first == "Id")
            {
                const uint64_t* value = std::get_if<uint64_t>(&property.second);
                if (value != nullptr && *value != 0)
                {
                    aResp->res
                        .jsonValue["ProcessorId"]["IdentificationRegisters"] =
                        "0x" + intToHexString(*value, 16);
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
                if (*value != 0)
                {
                    aResp->res.jsonValue["ProcessorId"]["MicrocodeInfo"] =
                        "0x" + intToHexString(*value, 8);
                }
            }
            else if (property.first == "Step")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                if (*value != 0)
                {
                    aResp->res.jsonValue["ProcessorId"]["Step"] =
                        "0x" + intToHexString(*value, 4);
                }
            }
        }
    }
}

inline void getCpuDataByService(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                const std::string& cpuId,
                                const std::string& service,
                                const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get available system cpu resources by service.";

    crow::connections::systemBus->async_method_call(
        [cpuId, service, objPath, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
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
                else if (object.first.str.starts_with(corePath))
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
                                        if (*present)
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

inline void getCpuAssetData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                            const std::string& service,
                            const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Cpu Asset Data";
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Inventory.Decorator.Asset",
        [objPath, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            const std::string* serialNumber = nullptr;
            const std::string* model = nullptr;
            const std::string* manufacturer = nullptr;
            const std::string* partNumber = nullptr;
            const std::string* sparePartNumber = nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), properties, "SerialNumber",
                serialNumber, "Model", model, "Manufacturer", manufacturer,
                "PartNumber", partNumber, "SparePartNumber", sparePartNumber);

            if (!success)
            {
                messages::internalError(aResp->res);
                return;
            }

            if (serialNumber != nullptr && !serialNumber->empty())
            {
                aResp->res.jsonValue["SerialNumber"] = *serialNumber;
            }

            if ((model != nullptr) && !model->empty())
            {
                aResp->res.jsonValue["Model"] = *model;
            }

            if (manufacturer != nullptr)
            {
                aResp->res.jsonValue["Manufacturer"] = *manufacturer;

                // Otherwise would be unexpected.
                if (manufacturer->find("Intel") != std::string::npos)
                {
                    aResp->res.jsonValue["ProcessorArchitecture"] = "x86";
                    aResp->res.jsonValue["InstructionSet"] = "x86-64";
                }
                else if (manufacturer->find("IBM") != std::string::npos)
                {
                    aResp->res.jsonValue["ProcessorArchitecture"] = "Power";
                    aResp->res.jsonValue["InstructionSet"] = "PowerISA";
                }
            }

            if (partNumber != nullptr)
            {
                aResp->res.jsonValue["PartNumber"] = *partNumber;
            }

            if (sparePartNumber != nullptr && !sparePartNumber->empty())
            {
                aResp->res.jsonValue["SparePartNumber"] = *sparePartNumber;
            }
        });
}

inline void getCpuRevisionData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                               const std::string& service,
                               const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Cpu Revision Data";
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Inventory.Decorator.Revision",
        [objPath, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            const std::string* version = nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), properties, "Version",
                version);

            if (!success)
            {
                messages::internalError(aResp->res);
                return;
            }

            if (version != nullptr)
            {
                aResp->res.jsonValue["Version"] = *version;
            }
        });
}

inline void getAcceleratorDataByService(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& acclrtrId,
    const std::string& service, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG
        << "Get available system Accelerator resources by service.";

#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
    std::shared_ptr<HealthRollup> health = std::make_shared<HealthRollup>(objPath,
        [aResp](const std::string& rootHealth,
                const std::string& healthRollup) {
            aResp->res.jsonValue["Status"]["Health"] = rootHealth;
            aResp->res.jsonValue["Status"]["HealthRollup"] = healthRollup;
        });
    health->start();
#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, objPath, "",
        [acclrtrId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            const bool* functional = nullptr;
            const bool* present = nullptr;
            const std::string* accType = nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), properties, "Functional",
                functional, "Present", present, "Type", accType);

            if (!success)
            {
                messages::internalError(aResp->res);
                return;
            }

            std::string state = "Enabled";
#ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
            std::string health = "OK";
#endif // ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

            if (present != nullptr && !*present)
            {
                state = "Absent";
            }
#ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
            if (functional != nullptr && !*functional)
            {
                if (state == "Enabled")
                {
                    health = "Critical";
                }
            }
#else  // ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
            (void)functional;
#endif // ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

            aResp->res.jsonValue["Id"] = acclrtrId;
            aResp->res.jsonValue["Name"] = "Processor";
            aResp->res.jsonValue["Status"]["State"] = state;
#ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
            aResp->res.jsonValue["Status"]["Health"] = health;
#endif // ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

            if (accType != nullptr && !accType->empty())
            {
                aResp->res.jsonValue["ProcessorType"] =
                    getProcessorType(*accType);
            }
        });
}

// OperatingConfig D-Bus Types
using TurboProfileProperty = std::vector<std::tuple<uint32_t, size_t>>;
using BaseSpeedPrioritySettingsProperty =
    std::vector<std::tuple<uint32_t, std::vector<uint32_t>>>;
using OperatingConfigProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

// uint32_t and size_t may or may not be the same type, requiring a dedup'd
// variant

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
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig",
        [aResp, cpuId,
         service](const boost::system::error_code ec,
                  const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                   << ec.message();
                messages::internalError(aResp->res);
                return;
            }

            nlohmann::json& json = aResp->res.jsonValue;

            const sdbusplus::message::object_path* appliedConfig = nullptr;
            const bool* baseSpeedPriorityEnabled = nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), properties, "AppliedConfig",
                appliedConfig, "BaseSpeedPriorityEnabled",
                baseSpeedPriorityEnabled);

            if (!success)
            {
                messages::internalError(aResp->res);
                return;
            }

            if (appliedConfig != nullptr)
            {
                const std::string& dbusPath = appliedConfig->str;
                std::string uri = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                  "/Processors/" +
                                  cpuId + "/OperatingConfigs";
                nlohmann::json::object_t operatingConfig;
                operatingConfig["@odata.id"] = uri;
                json["OperatingConfigs"] = std::move(operatingConfig);

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
                    return;
                }
                uri += '/';
                uri += dbusPath.substr(baseNamePos + 1);
                nlohmann::json::object_t appliedOperatingConfig;
                appliedOperatingConfig["@odata.id"] = uri;
                json["AppliedOperatingConfig"] =
                    std::move(appliedOperatingConfig);

                // Once we found the current applied config, queue another
                // request to read the base freq core ids out of that
                // config.
                sdbusplus::asio::getProperty<BaseSpeedPrioritySettingsProperty>(
                    *crow::connections::systemBus, service, dbusPath,
                    "xyz.openbmc_project.Inventory.Item.Cpu."
                    "OperatingConfig",
                    "BaseSpeedPrioritySettings",
                    [aResp](const boost::system::error_code ec2,
                            const BaseSpeedPrioritySettingsProperty&
                                baseSpeedList) {
                        if (ec2)
                        {
                            BMCWEB_LOG_WARNING << "D-Bus Property Get error: "
                                               << ec2;
                            messages::internalError(aResp->res);
                            return;
                        }

                        highSpeedCoreIdsHandler(aResp, baseSpeedList);
                    });
            }

            if (baseSpeedPriorityEnabled != nullptr)
            {
                json["BaseSpeedPriorityState"] =
                    *baseSpeedPriorityEnabled ? "Enabled" : "Disabled";
            }
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
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
        [aResp](const boost::system::error_code ec,
                const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                   << ec.message();
                messages::internalError(aResp->res);
                return;
            }

            using SpeedConfigProperty = std::tuple<bool, uint32_t>;
            const SpeedConfigProperty* speedConfig;
            const size_t* availableCoreCount = nullptr;
            const uint32_t* baseSpeed = nullptr;
            const uint32_t* maxJunctionTemperature = nullptr;
            const uint32_t* maxSpeed = nullptr;
            const uint32_t* minSpeed = nullptr;
            const uint32_t* operatingSpeed = nullptr;
            const uint32_t* powerLimit = nullptr;
            const TurboProfileProperty* turboProfile = nullptr;
            const BaseSpeedPrioritySettingsProperty* baseSpeedPrioritySettings =
                nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), properties,
                "AvailableCoreCount", availableCoreCount, "BaseSpeed",
                baseSpeed, "MaxJunctionTemperature", maxJunctionTemperature,
                "MaxSpeed", maxSpeed, "PowerLimit", powerLimit, "TurboProfile",
                turboProfile, "BaseSpeedPrioritySettings",
                baseSpeedPrioritySettings, "MinSpeed", minSpeed,
                "OperatingSpeed", operatingSpeed, "SpeedConfig", speedConfig);

            if (!success)
            {
                messages::internalError(aResp->res);
                return;
            }

            nlohmann::json& json = aResp->res.jsonValue;

            if (availableCoreCount != nullptr)
            {
                json["TotalAvailableCoreCount"] = *availableCoreCount;
            }

            if (baseSpeed != nullptr)
            {
                json["BaseSpeedMHz"] = *baseSpeed;
            }

            if (maxJunctionTemperature != nullptr)
            {
                json["MaxJunctionTemperatureCelsius"] = *maxJunctionTemperature;
            }

            if (maxSpeed != nullptr)
            {
                json["MaxSpeedMHz"] = *maxSpeed;
            }

            if (minSpeed != nullptr)
            {
                json["MinSpeedMHz"] = *minSpeed;
            }

            if (operatingSpeed != nullptr)
            {
                json["OperatingSpeedMHz"] = *operatingSpeed;
            }

            if (operatingSpeed != nullptr)
            {
                json["OperatingSpeedMHz"] = *operatingSpeed;
            }

            if (speedConfig != nullptr)
            {
                const auto& [speedLock, speed] = *speedConfig;
                json["SpeedLocked"] = speedLock;
                json["SpeedLimitMHz"] = speed;
            }

            if (turboProfile != nullptr)
            {
                nlohmann::json& turboArray = json["TurboProfile"];
                turboArray = nlohmann::json::array();
                for (const auto& [turboSpeed, coreCount] : *turboProfile)
                {
                    nlohmann::json::object_t turbo;
                    turbo["ActiveCoreCount"] = coreCount;
                    turbo["MaxSpeedMHz"] = turboSpeed;
                    turboArray.push_back(std::move(turbo));
                }
            }

            if (baseSpeedPrioritySettings != nullptr)
            {
                nlohmann::json& baseSpeedArray =
                    json["BaseSpeedPrioritySettings"];
                baseSpeedArray = nlohmann::json::array();
                for (const auto& [baseSpeedMhz, coreList] :
                     *baseSpeedPrioritySettings)
                {
                    nlohmann::json::object_t speed;
                    speed["CoreCount"] = coreList.size();
                    speed["CoreIDs"] = coreList;
                    speed["BaseSpeedMHz"] = baseSpeedMhz;
                    baseSpeedArray.push_back(std::move(speed));
                }
            }
        });
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
            std::string metricsURI =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
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
    std::string metricsURI =
        "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
    metricsURI += cpuId;
    metricsURI += "/MemorySummary/MemoryMetrics";
    json["MemorySummary"]["Metrics"]["@odata.id"] = metricsURI;
    getEccModeData(aResp, cpuId, service, objPath);
}

inline void getProcessorResetTypeData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& cpuId, const std::string& service,
                           const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId](const boost::system::error_code ec,
                       const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error on reset interface";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "ResetType")
                {
                    const std::string* processorResetType =
                        std::get_if<std::string>(&property.second);
                    if (processorResetType == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Property processorResetType is null";
                        messages::internalError(aResp->res);
                        return;
                    }
                    const std::string processorResetTypeValue =
                        getProcessorResetType(*processorResetType);
                    aResp->res.jsonValue["Actions"]["#Processor.Reset"] = {
                        {"target", "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/"
                            + cpuId + "/Actions/Processor.Reset"},
                        {"ResetType@Redfish.AllowableValues", {processorResetTypeValue}}};
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Control.Processor.Reset");
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void
    getProcessorThrottleData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& service,
                             const std::string& objPath)
{

    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
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
                if (property.first == "ThrottleReason")
                {
                    std::string reason;
                    const std::vector<std::string>* throttleReasons =
                        std::get_if<std::vector<std::string>>(&property.second);
                    std::vector<std::string> formattedThrottleReasons{};

                    if (throttleReasons == nullptr)
                    {
                        BMCWEB_LOG_ERROR
                            << "Get Throttle reasons property failed";
                        messages::internalError(aResp->res);
                        return;
                    }

                    for (auto val : *throttleReasons)
                    {
                        reason = redfish::dbus_utils::toReasonType(val);
                        if (!reason.empty())
                        {
                            formattedThrottleReasons.push_back(reason);
                        }
                    }

                    json["Oem"]["Nvidia"]["@odata.type"] =
                        "#NvidiaProcessorMetrics.v1_0_0.NvidiaProcessorMetrics";
                    json["Oem"]["Nvidia"]["ThrottleReasons"] =
                        formattedThrottleReasons;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.State.ProcessorPerformance");
}

inline void getMigModeData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
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
                if (property.first == "MIGModeEnabled")
                {
                    const bool* migModeEnabled =
                        std::get_if<bool>(&property.second);
                    if (migModeEnabled == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    json["Oem"]["Nvidia"]["@odata.type"] =
                        "#NvidiaProcessor.v1_0_0.NvidiaProcessor";
                    json["Oem"]["Nvidia"]["MIGModeEnabled"] = *migModeEnabled;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.MigMode");
}

inline void getProcessorMigModeData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& cpuId,
    const std::string& service, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << " get GpuMIGMode data";
    getMigModeData(aResp, cpuId, service, objPath);
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getProcessorData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& processorId,
                             const std::string& objectPath,
                             const dbus::utility::MapperServiceMap& serviceMap)
{
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        for (const auto& interface : interfaceList)
        {
            if (interface == "xyz.openbmc_project.Inventory.Decorator.Asset")
            {
                getCpuAssetData(aResp, serviceName, objectPath);
            }
            else if (interface ==
                     "xyz.openbmc_project.Inventory.Decorator.Revision")
            {
                getCpuRevisionData(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Inventory.Item.Cpu")
            {
                getCpuDataByService(aResp, processorId, serviceName,
                                    objectPath);
            }
            else if (interface ==
                     "xyz.openbmc_project.Inventory.Item.Accelerator")
            {
                getAcceleratorDataByService(aResp, processorId, serviceName,
                                            objectPath);
            }
            else if (
                interface ==
                "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig")
            {
                getCpuConfigData(aResp, processorId, serviceName, objectPath);
            }
            else if (interface ==
                     "xyz.openbmc_project.Inventory.Decorator.LocationCode")
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
            else if (interface ==
                     "xyz.openbmc_project.Inventory.Decorator.UniqueIdentifier")
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
            else if (interface == "xyz.openbmc_project.Inventory."
                                  "Decorator.FpgaType")
            {
                getFpgaTypeData(aResp, serviceName, objectPath);
            }
            else if (interface == "xyz.openbmc_project.Control.Processor.Reset")
            {
                getProcessorResetTypeData(aResp, processorId, serviceName,
                                        objectPath);
            }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            else if (interface == "com.nvidia.MigMode")
            {
                getProcessorMigModeData(aResp, processorId, serviceName,
                                        objectPath);
            }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
        }
    }
    aResp->res.jsonValue["EnvironmentMetrics"] = {
        {"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/" +
                          processorId + "/EnvironmentMetrics"}};
    aResp->res.jsonValue["@Redfish.Settings"]["@odata.type"] =
        "#Settings.v1_3_3.Settings";
    aResp->res.jsonValue["@Redfish.Settings"]["SettingsObject"] = {
        {"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/" +
                          processorId + "/Settings"}};

    aResp->res.jsonValue["Ports"] = {
        {"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/" +
                          processorId + "/Ports"}};
    // Links association to underneath memory
    getProcessorMemoryLinks(aResp, objectPath);
    // Link association to parent chassis
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        getProcessorChassisLink(aResp, objectPath, serviceName);
    }
    // Get system and fpga interfaces properties
    getProcessorSystemPCIeInterface(aResp, objectPath);
    getProcessorFPGAPCIeInterface(aResp, objectPath);
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * Handle the PATCH operation of the MIG Mode Property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       migMode         New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchMigMode(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                         const std::string& processorId, const bool migMode,
                         const std::string& cpuObjectPath,
                         const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.MigMode") != interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        BMCWEB_LOG_DEBUG << " GpuMIGMode interface not found ";
        messages::internalError(resp->res);
        return;
    }

    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, migMode](boost::system::error_code ec,
                                     sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set MIG Mode property succeeded";
                return;
            }

            BMCWEB_LOG_DEBUG << "CPU:" << processorId
                             << " set MIG Mode  property failed: " << ec;
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
        "Set", "com.nvidia.MigMode", "MIGModeEnabled",
        std::variant<bool>(migMode));
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * Handle the PATCH operation of the speed config property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       speedConfig     New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchSpeedConfig(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                             const std::string& processorId,
                             const std::tuple<bool, uint32_t>& reqSpeedConfig,
                             const std::string& cpuObjectPath,
                             const MapperServiceMap& serviceMap)
{
    BMCWEB_LOG_DEBUG << "Setting SpeedConfig";
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
    BMCWEB_LOG_DEBUG << "patchSpeedConfig";
    crow::connections::systemBus->async_method_call(
        [resp, processorId, reqSpeedConfig](boost::system::error_code ec,
                                            sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set speed config property succeeded";
                return;
            }

            BMCWEB_LOG_DEBUG << "CPU:" << processorId
                             << " set speed config property failed: " << ec;
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
                uint32_t speedLimit = std::get<1>(reqSpeedConfig);
                messages::propertyValueIncorrect(resp->res, "SpeedLimitMHz",
                                                 std::to_string(speedLimit));
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
        "SpeedConfig",
        std::variant<std::tuple<bool, uint32_t>>(reqSpeedConfig));
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
    const std::string conName = *inventoryService;
    sdbusplus::asio::getProperty<std::tuple<bool, uint32_t>>(
        *crow::connections::systemBus, conName, cpuObjectPath,
        "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig", "SpeedConfig",
        [resp, processorId, conName, cpuObjectPath, serviceMap,
         speedLocked](const boost::system::error_code ec,
                      const std::tuple<bool, uint32_t>& speedConfig) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for SpeedConfig";
                messages::internalError(resp->res);
                return;
            }
            std::tuple<bool, uint32_t> reqSpeedConfig;
            uint32_t cachedSpeedLimit = std::get<1>(speedConfig);
            reqSpeedConfig = std::make_tuple(speedLocked, cachedSpeedLimit);
            patchSpeedConfig(resp, processorId, reqSpeedConfig, cpuObjectPath,
                             serviceMap);
        });
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
    const std::string conName = *inventoryService;
    BMCWEB_LOG_DEBUG << "patchSpeedLimit";
    // Set the property, with handler to check error responses
    sdbusplus::asio::getProperty<std::tuple<bool, uint32_t>>(
        *crow::connections::systemBus, conName, cpuObjectPath,
        "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig", "SpeedConfig",
        [resp, processorId, conName, cpuObjectPath, serviceMap,
         speedLimit](const boost::system::error_code ec,
                     const std::tuple<bool, uint32_t>& speedConfig) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for SpeedConfig";
                messages::internalError(resp->res);
                return;
            }
            std::tuple<bool, uint32_t> reqSpeedConfig;
            bool cachedSpeedLocked = std::get<0>(speedConfig);
            reqSpeedConfig = std::make_tuple(cachedSpeedLocked,
                                             static_cast<uint32_t>(speedLimit));
            patchSpeedConfig(resp, processorId, reqSpeedConfig, cpuObjectPath,
                             serviceMap);
        });
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
                                const sdbusplus::message_t& msg)
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
    const std::string& cpuObjectPath,
    const dbus::utility::MapperServiceMap& serviceMap)
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
    std::string expectedPrefix("/redfish/v1/Systems/" PLATFORMSYSTEMID
                               "/Processors/");
    expectedPrefix += processorId;
    expectedPrefix += "/OperatingConfigs/";
    if (!appliedConfigUri.starts_with(expectedPrefix) ||
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
                                 const sdbusplus::message_t& msg) {
            handleAppliedConfigResponse(resp, appliedConfigUri, ec, msg);
        },
        *controlService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "xyz.openbmc_project.Control.Processor.CurrentOperatingConfig",
        "AppliedConfig", dbus::utility::DbusVariantType(std::move(configPath)));
}

inline void requestRoutesOperatingConfigCollection(App& app)
{

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Processors/<str>/OperatingConfigs/")
        .privileges(redfish::privileges::getOperatingConfigCollection)
        .methods(
            boost::beast::http::verb::
                get)([&app](const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& cpuName) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] =
                "#OperatingConfigCollection.OperatingConfigCollection";
            asyncResp->res.jsonValue["@odata.id"] = req.url;
            asyncResp->res.jsonValue["Name"] = "Operating Config Collection";

            // First find the matching CPU object so we know how to
            // constrain our search for related Config objects.
            crow::connections::systemBus->async_method_call(
                [asyncResp,
                 cpuName](const boost::system::error_code ec,
                          const dbus::utility::MapperGetSubTreePathsResponse&
                              objects) {
                    if (ec)
                    {
                        BMCWEB_LOG_WARNING << "D-Bus error: " << ec << ", "
                                           << ec.message();
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    for (const std::string& object : objects)
                    {
                        if (!object.ends_with(cpuName))
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
                            "/redfish/v1/Systems/" PLATFORMSYSTEMID
                            "/Processors/" +
                                cpuName + "/OperatingConfigs",
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
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Processors/<str>/OperatingConfigs/<str>/")
        .privileges(redfish::privileges::getOperatingConfig)
        .methods(
            boost::beast::http::verb::
                get)([&app](const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& cpuName,
                            const std::string& configName) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            // Ask for all objects implementing OperatingConfig so we can search
            // for one with a matching name
            crow::connections::systemBus->async_method_call(
                [asyncResp, cpuName, configName, reqUrl{req.url}](
                    boost::system::error_code ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
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
                        if (!objectPath.ends_with(expectedEnding) ||
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
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Processors/")
        .privileges(redfish::privileges::getProcessorCollection)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& systemName) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                if (systemName != PLATFORMSYSTEMID)
                {
                    messages::resourceNotFound(asyncResp->res, "ComputerSystem",
                                               systemName);
                    return;
                }

                asyncResp->res.jsonValue["@odata.type"] =
                    "#ProcessorCollection.ProcessorCollection";
                asyncResp->res.jsonValue["Name"] = "Processor Collection";

                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors";

                collection_util::getCollectionMembers(
                    asyncResp,
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors",
                    std::vector<const char*>(processorInterfaces.begin(),
                                             processorInterfaces.end()));
            });
}

inline void requestRoutesProcessor(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Processors/<str>/")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& systemName,
                   const std::string& processorId) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                if (systemName != PLATFORMSYSTEMID)
                {
                    messages::resourceNotFound(asyncResp->res, "ComputerSystem",
                                               systemName);
                    return;
                }

                asyncResp->res.jsonValue["@odata.type"] =
                    "#Processor.v1_13_0.Processor";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/" +
                    processorId;
                std::string processorMetricsURI =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
                processorMetricsURI += processorId;
                processorMetricsURI += "/ProcessorMetrics";
                asyncResp->res.jsonValue["Metrics"]["@odata.id"] =
                    processorMetricsURI;

                redfish::processor_utils::getProcessorObject(
                    asyncResp, processorId, getProcessorData);
                redfish::conditions_utils::populateServiceConditions(
                    asyncResp, processorId);
            });

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Processors/<str>/")
        .privileges(redfish::privileges::patchProcessor)
        .methods(
            boost::beast::http::verb::
                patch)([&app](
                           const crow::Request& req,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& systemName,
                           const std::string& processorId) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            if (systemName != PLATFORMSYSTEMID)
            {
                messages::resourceNotFound(asyncResp->res, "ComputerSystem",
                                           systemName);
                return;
            }

            std::optional<int> speedLimit;
            std::optional<bool> speedLocked;
            std::optional<nlohmann::json> oemObject;
            std::optional<nlohmann::json> appliedConfigJson;
            if (!redfish::json_util::readJsonAction(
                    req, asyncResp->res, "SpeedLimitMHz", speedLimit,
                    "SpeedLocked", speedLocked, "AppliedOperatingConfig",
                    appliedConfigJson, "Oem", oemObject))
            {
                return;
            }

            // Update speed limit
            if (speedLimit && speedLocked)
            {
                std::tuple<bool, uint32_t> reqSpeedConfig;
                reqSpeedConfig = std::make_tuple(
                    *speedLocked, static_cast<uint32_t>(*speedLimit));
                redfish::processor_utils::getProcessorObject(
                    asyncResp, processorId,
                    [reqSpeedConfig](
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp1,
                        const std::string& processorId1,
                        const std::string& objectPath,
                        const MapperServiceMap& serviceMap) {
                        patchSpeedConfig(asyncResp1, processorId1,
                                         reqSpeedConfig, objectPath,
                                         serviceMap);
                    });
            }
            else if (speedLimit)
            {
                redfish::processor_utils::getProcessorObject(
                    asyncResp, processorId,
                    [speedLimit](
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp1,
                        const std::string& processorId1,
                        const std::string& objectPath,
                        const MapperServiceMap& serviceMap) {
                        patchSpeedLimit(asyncResp1, processorId1, *speedLimit,
                                        objectPath, serviceMap);
                    });
            }
            else if (speedLocked)
            {
                // Update speed locked
                redfish::processor_utils::getProcessorObject(
                    asyncResp, processorId,
                    [speedLocked](
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp1,
                        const std::string& processorId1,
                        const std::string& objectPath,
                        const MapperServiceMap& serviceMap) {
                        patchSpeedLocked(asyncResp1, processorId1, *speedLocked,
                                         objectPath, serviceMap);
                    });
            }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            // Update migMode
            if (std::optional<nlohmann::json> oemNvidiaObject;
                oemObject &&
                redfish::json_util::readJson(*oemObject, asyncResp->res,
                                             "Nvidia", oemNvidiaObject))
            {
                std::optional<bool> migMode;
                if (oemNvidiaObject && redfish::json_util::readJson(
                                           *oemNvidiaObject, asyncResp->res,
                                           "MIGModeEnabled", migMode))
                {
                    if (migMode)
                    {
                        redfish::processor_utils::getProcessorObject(
                            asyncResp, processorId,
                            [migMode](const std::shared_ptr<bmcweb::AsyncResp>&
                                          asyncResp1,
                                      const std::string& processorId1,
                                      const std::string& objectPath,
                                      const MapperServiceMap& serviceMap) {
                                patchMigMode(asyncResp1, processorId1, *migMode,
                                             objectPath, serviceMap);
                            });
                    }
                }
            }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            std::string appliedConfigUri;
            if (appliedConfigJson)
            {
                if (!redfish::json_util::readJson(*appliedConfigJson,
                                                  asyncResp->res, "@odata.id",
                                                  appliedConfigUri))
                {
                    return;
                }
                // Check for 404 and find matching D-Bus object, then run
                // property patch handlers if that all succeeds.
                redfish::processor_utils::getProcessorObject(
                    asyncResp, processorId,
                    [appliedConfigUri = std::move(appliedConfigUri)](
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp1,
                        const std::string& processorId1,
                        const std::string& objectPath,
                        const MapperServiceMap& serviceMap) {
                        patchAppliedOperatingConfig(asyncResp1, processorId1,
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

inline void getVoltageData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& service,
                           const std::string& chassisId,
                           const std::string& sensorPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, chassisId, sensorPath](
            const boost::system::error_code ec,
            const std::vector<
                std::pair<std::string, std::variant<std::string, double>>>&
                propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Can't get sensor reading";
                return;
            }
            sdbusplus::message::object_path objectPath(sensorPath);
            const std::string& sensorName = objectPath.filename();
            std::string sensorURI =
                (boost::format("/redfish/v1/Chassis/%s/Sensors/%s") %
                 chassisId % sensorName)
                    .str();
            aResp->res.jsonValue["CoreVoltage"]["DataSourceUri"] = sensorURI;
            const double* attributeValue = nullptr;
            for (const std::pair<std::string,
                                 std::variant<std::string, double>>& property :
                 propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Value")
                {
                    attributeValue = std::get_if<double>(&property.second);
                    if (attributeValue != nullptr)
                    {
                        aResp->res.jsonValue["CoreVoltage"]["Reading"] =
                            *attributeValue;
                    }
                }
            }
        },
        service, sensorPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Sensor.Value");
}

inline void getSensorMetric(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& service,
                            const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, service,
         objPath](const boost::system::error_code ec,
                  std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no chassis = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr && data->size() > 1)
            {
                // Object must have single parent chassis
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
            const std::string& chassisId = chassisName;
            crow::connections::systemBus->async_method_call(
                [aResp, service, objPath,
                 chassisId](const boost::system::error_code& e,
                            std::variant<std::vector<std::string>>& resp1) {
                    if (e)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    std::vector<std::string>* data1 =
                        std::get_if<std::vector<std::string>>(&resp1);
                    if (data1 == nullptr)
                    {
                        return;
                    }
                    for (const std::string& sensorPath : *data1)
                    {
                        std::vector<std::string> split;
                        // Reserve space for
                        // /xyz/openbmc_project/sensors/<name>/<subname>
                        split.reserve(6);
                        boost::algorithm::split(split, sensorPath,
                                                boost::is_any_of("/"));
                        if (split.size() < 6)
                        {
                            BMCWEB_LOG_ERROR
                                << "Got path that isn't long enough "
                                << objPath;
                            continue;
                        }
                        const std::string& sensorType = split[4];
                        if (sensorType == "voltage")
                        {
                            getVoltageData(aResp, service, chassisId,
                                           sensorPath);
                        }
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                chassisPath + "/all_sensors", "org.freedesktop.DBus.Properties",
                "Get", "xyz.openbmc_project.Association", "endpoints");
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
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
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
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
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.PCIe.PCIeECC") !=
                        interfaces.end())
                    {
                        redfish::processor_utils::getPCIeErrorData(
                            aResp, service, path);
                    }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                    if (std::find(
                            interfaces.begin(), interfaces.end(),
                            "xyz.openbmc_project.State.ProcessorPerformance") !=
                        interfaces.end())
                    {
                        getProcessorThrottleData(aResp, service, path);
                    }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                    getSensorMetric(aResp, service, path);
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
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Processors/<str>/ProcessorMetrics")
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
            const boost::system::error_code ec, GetSubTreeType& subtree) {
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
                        const boost::system::error_code ec1,
                        const OperatingConfigProperties& properties) {
                        if (ec1)
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
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
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
                                const boost::system::error_code ec1,
                                const OperatingConfigProperties& properties) {
                                if (ec1)
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
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
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
                nlohmann::json& json = aResp->res.jsonValue;
                json["@odata.id"] = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                    "/Processors/" +
                                    processorId + "/Settings";
                json["@odata.type"] = "#Processor.v1_13_0.Processor";
                json["Id"] = "Settings";
                json["Name"] = processorId + "PendingSettings";
                for (const auto& [service, interfaces] : object)
                {
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Memory.MemoryECC") !=
                        interfaces.end())
                    {
                        getEccModeData(aResp, processorId, service, path);
                    }
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Software.ApplyTime") !=
                        interfaces.end())
                    {
                        crow::connections::systemBus->async_method_call(
                            [aResp](
                                const boost::system::error_code ec1,
                                const OperatingConfigProperties& properties) {
                                if (ec1)
                                {
                                    BMCWEB_LOG_DEBUG << "DBUS response error";
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                nlohmann::json& json1 = aResp->res.jsonValue;
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
                                        json1
                                            ["@Redfish.SettingsApplyTime"]
                                            ["@odata.type"] =
                                                "#Settings.v1_3_3.PreferredApplyTime";
                                        json1["@Redfish.SettingsApplyTime"]
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
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
                 "Settings")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                getProcessorSettingsData(asyncResp, processorId);
            });

    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
                 "Settings")
        .privileges(redfish::privileges::patchProcessor)
        .methods(boost::beast::http::verb::patch)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                std::optional<nlohmann::json> memSummary;
                if (!redfish::json_util::readJsonAction(
                        req, asyncResp->res, "MemorySummary", memSummary))
                {
                    return;
                }
                std::optional<bool> eccModeEnabled;
                if (memSummary)
                {
                    if (redfish::json_util::readJson(
                            *memSummary, asyncResp->res, "ECCModeEnabled",
                            eccModeEnabled))
                    {
                        redfish::processor_utils::getProcessorObject(
                            asyncResp, processorId,
                            [eccModeEnabled](
                                const std::shared_ptr<bmcweb::AsyncResp>&
                                    asyncResp1,
                                const std::string& processorId1,
                                const std::string& objectPath,
                                const MapperServiceMap& serviceMap) {
                                patchEccMode(asyncResp1, processorId1,
                                             *eccModeEnabled, objectPath,
                                             serviceMap);
                            });
                    }
                }
            });
}

inline void postResetType(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                          const std::string& processorId,
                          const std::string& cpuObjectPath,
                          const std::string& resetType,
                          const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "xyz.openbmc_project.Control.Processor.Reset") !=
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
    const std::string conName = *inventoryService;
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, conName, cpuObjectPath,
        "xyz.openbmc_project.Control.Processor.Reset", "ResetType",
        [resp, resetType, processorId, conName, cpuObjectPath](
            const boost::system::error_code ec, const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "DBus response, error for ResetType ";
                BMCWEB_LOG_ERROR << ec.message();
                messages::internalError(resp->res);
                return;
            }

            const std::string processorResetType =
                getProcessorResetType(property);
            if (processorResetType != resetType)
            {
                BMCWEB_LOG_DEBUG << "Property Value Incorrect";
                messages::actionParameterNotSupported(resp->res, "ResetType",
                                                      resetType);
                return;
            }
            // Set the property, with handler to check error responses
            crow::connections::systemBus->async_method_call(
                [resp, processorId](boost::system::error_code ec1,
                                    const int retValue) {
                    if (!ec1)
                    {

                        if (retValue != 0)
                        {
                            BMCWEB_LOG_ERROR << retValue;
                            messages::internalError(resp->res);
                        }
                        BMCWEB_LOG_DEBUG << "CPU:" << processorId
                                         << " Reset Succeded";
                        messages::success(resp->res);
                        return;
                    }
                    BMCWEB_LOG_DEBUG << ec1;
                    messages::internalError(resp->res);
                    return;
                },
                conName, cpuObjectPath,
                "xyz.openbmc_project.Control.Processor.Reset", "Reset");
        });
}

inline void requestRoutesProcessorReset(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
                 "Actions/Processor.Reset")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                std::optional<std::string> resetType;
                if (!json_util::readJsonAction(req, asyncResp->res, "ResetType",
                                               resetType))
                {
                    return;
                }
                if (resetType)
                {
                    redfish::processor_utils::getProcessorObject(
                        asyncResp, processorId,
                        [resetType](const std::shared_ptr<bmcweb::AsyncResp>&
                                        asyncResp1,
                                    const std::string& processorId1,
                                    const std::string& objectPath,
                                    const MapperServiceMap& serviceMap) {
                            postResetType(asyncResp1, processorId1, objectPath,
                                          *resetType, serviceMap);
                        });
                }
            });
}

inline void requestRoutesProcessorPortCollection(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
                 "Ports")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                BMCWEB_LOG_DEBUG << "Get available system processor resource";
                crow::connections::systemBus->async_method_call(
                    [processorId,
                     asyncResp](const boost::system::error_code ec,
                                const boost::container::flat_map<
                                    std::string,
                                    boost::container::flat_map<
                                        std::string, std::vector<std::string>>>&
                                    subtree) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error";
                            messages::internalError(asyncResp->res);

                            return;
                        }
                        for (const auto& [path, object] : subtree)
                        {
                            if (!boost::ends_with(path, processorId))
                            {
                                continue;
                            }
                            asyncResp->res.jsonValue["@odata.id"] =
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Processors/" +
                                processorId + "/Ports";
                            asyncResp->res.jsonValue["@odata.type"] =
                                "#PortCollection.PortCollection";
                            asyncResp->res.jsonValue["Name"] =
                                "NVLink Port Collection";
                            collection_util::getCollectionMembers(
                                asyncResp,
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Processors/" +
                                    processorId + "/Ports",
                                {"xyz.openbmc_project.Inventory.Item.Port"},
                                path.c_str());

                            return;
                        }
                        // Object not found
                        messages::resourceNotFound(
                            asyncResp->res, "#Processor.v1_13_0.Processor",
                            processorId);
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                    "/xyz/openbmc_project/inventory", 0,
                    std::array<const char*, 2>{
                        "xyz.openbmc_project.Inventory.Item.Cpu",
                        "xyz.openbmc_project.Inventory.Item.Accelerator"});
            });
}

inline void
    getConnectedSwitchPorts(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& portPath,
                            const std::string& fabricId,
                            const std::string& switchName)
{
    BMCWEB_LOG_DEBUG << "Get connected switch ports on " << switchName;
    crow::connections::systemBus->async_method_call(
        [asyncResp, portPath, fabricId,
         switchName](const boost::system::error_code ec,
                     std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Get connected switch failed on"
                                 << switchName;
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& switchlinksArray =
                asyncResp->res.jsonValue["Links"]["ConnectedSwitchPorts"];
            for (const std::string& portPath1 : *data)
            {
                sdbusplus::message::object_path objectPath(portPath1);
                std::string portId = objectPath.filename();
                if (portId.empty())
                {
                    BMCWEB_LOG_DEBUG << "Unable to fetch port";
                    messages::internalError(asyncResp->res);
                    return;
                }
                nlohmann::json thisPort = nlohmann::json::object();
                std::string portUri = "/redfish/v1/Fabrics/" + fabricId;
                portUri += "/Switches/" + switchName + "/Ports/";
                portUri += portId;
                thisPort["@odata.id"] = portUri;
                switchlinksArray.push_back(std::move(thisPort));
            }
        },
        "xyz.openbmc_project.ObjectMapper", portPath + "/switch_port",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void
    getConnectedSwitches(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& switchPath,
                         const std::string& portPath,
                         const std::string& switchName)
{
    BMCWEB_LOG_DEBUG << "Get connected switch on" << switchName;
    crow::connections::systemBus->async_method_call(
        [asyncResp, switchPath, portPath,
         switchName](const boost::system::error_code ec,
                     std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_DEBUG << "Get connected switch failed on: "
                                 << switchName;
                return;
            }
            for (const std::string& fabricPath : *data)
            {
                sdbusplus::message::object_path objectPath(fabricPath);
                std::string fabricId = objectPath.filename();
                if (fabricId.empty())
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                nlohmann::json& switchlinksArray =
                    asyncResp->res.jsonValue["Links"]["ConnectedSwitches"];
                nlohmann::json thisSwitch = nlohmann::json::object();
                std::string switchUri = "/redfish/v1/Fabrics/";
                switchUri += fabricId;
                switchUri += "/Switches/";
                switchUri += switchName;
                thisSwitch["@odata.id"] = switchUri;
                switchlinksArray.push_back(std::move(thisSwitch));
                getConnectedSwitchPorts(asyncResp, portPath, fabricId,
                                        switchName);
            }
        },
        "xyz.openbmc_project.ObjectMapper", switchPath + "/fabrics",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void
    getProcessorPortLinks(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& portPath,
                          const std::string& processorId,
                          const std::string& port)
{
    BMCWEB_LOG_DEBUG << "Get associated switch on" << port;
    crow::connections::systemBus->async_method_call(
        [asyncResp, portPath, processorId,
         port](const boost::system::error_code ec,
               std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Get associated switch failed on: " << port;
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& switchlinksArray =
                asyncResp->res.jsonValue["Links"]["ConnectedSwitches"];
            switchlinksArray = nlohmann::json::array();
            nlohmann::json& portlinksArray =
                asyncResp->res.jsonValue["Links"]["ConnectedSwitchPorts"];
            portlinksArray = nlohmann::json::array();
            for (const std::string& switchPath : *data)
            {
                sdbusplus::message::object_path objectPath(switchPath);
                std::string switchName = objectPath.filename();
                if (switchName.empty())
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                getConnectedSwitches(asyncResp, switchPath, portPath,
                                     switchName);
            }
        },
        "xyz.openbmc_project.ObjectMapper", portPath + "/associated_switch",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void requestRoutesProcessorPort(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
                 "Ports/<str>")
        .privileges(redfish::privileges::getProcessor)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& processorId,
                                              const std::string& port) {
            BMCWEB_LOG_DEBUG << "Get available system processor resource";
            crow::connections::systemBus->async_method_call(
                [processorId, port, asyncResp](
                    const boost::system::error_code ec,
                    const boost::container::flat_map<
                        std::string,
                        boost::container::flat_map<
                            std::string, std::vector<std::string>>>& subtree) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "DBUS response error";
                        messages::internalError(asyncResp->res);

                        return;
                    }
                    for (const auto& [path, object] : subtree)
                    {
                        if (!boost::ends_with(path, processorId))
                        {
                            continue;
                        }
                        crow::connections::systemBus->async_method_call(
                            [processorId, port, asyncResp](
                                const boost::system::error_code ec1,
                                const boost::container::flat_map<
                                    std::string,
                                    boost::container::flat_map<
                                        std::string, std::vector<std::string>>>&
                                    subtree1) {
                                if (ec1)
                                {
                                    BMCWEB_LOG_DEBUG << "DBUS response error";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                for (const auto& [portPath, object1] : subtree1)
                                {
                                    if (!boost::ends_with(portPath, port))
                                    {
                                        continue;
                                    }
                                    std::string portUri =
                                        "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                        "/Processors/";
                                    portUri += processorId;
                                    portUri += "/Ports/";
                                    portUri += port;
                                    asyncResp->res.jsonValue["@odata.id"] =
                                        portUri;
                                    asyncResp->res.jsonValue["@odata.type"] =
                                        "#Port.v1_4_0.Port";
                                    asyncResp->res.jsonValue["Name"] =
                                        port + " Resource";
                                    asyncResp->res.jsonValue["Id"] = port;
                                    std::string metricsURI =
                                        "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                        "/Processors/";
                                    metricsURI += processorId + "/Ports/";
                                    metricsURI += port + "/Metrics";
                                    asyncResp->res
                                        .jsonValue["Metrics"]["@odata.id"] =
                                        metricsURI;
                                    for (const auto& [service, interfaces] :
                                         object1)
                                    {
                                        redfish::port_utils::getPortData(
                                            asyncResp, service, portPath);
                                        getProcessorPortLinks(
                                            asyncResp, portPath, processorId,
                                            port);
                                    }

                                    redfish::conditions_utils::
                                        populateServiceConditions(asyncResp,
                                                                  port);
                                    return;
                                }
                                // Object not found
                                messages::resourceNotFound(
                                    asyncResp->res, "#Port.v1_4_0.Port", port);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            path, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Port"});
                        return;
                    }
                    // Object not found
                    messages::resourceNotFound(asyncResp->res,
                                               "#Processor.v1_13_0.Processor",
                                               processorId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 2>{
                    "xyz.openbmc_project.Inventory.Item.Cpu",
                    "xyz.openbmc_project.Inventory.Item.Accelerator"});
        });
}

inline void
    getPortMetricsData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& service, const std::string& path)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, service,
         path](const boost::system::error_code ec,
               // const boost::container::flat_map<std::string,
               // std::variant<size_t,uint16_t,uint32_t,uint64_t>>&
               const boost::container::flat_map<
                   std::string, dbus::utility::DbusVariantType>& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(asyncResp->res);
                return;
            }
            for (const auto& property : properties)
            {
                if ((property.first == "TXBytes") ||
                    (property.first == "RXBytes"))
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for TX/RX bytes";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue[property.first] = *value;
                }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                else if (property.first == "RXNoProtocolBytes")
                {
                    const uint64_t* value =
                        std::get_if<uint64_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for RXNoProtocolBytes";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                        "#NvidiaPortMetrics.v1_0_0.NvidiaPortMetrics";
                    asyncResp->res
                        .jsonValue["Oem"]["Nvidia"]["RXNoProtocolBytes"] =
                        *value;
                }
                else if (property.first == "TXNoProtocolBytes")
                {
                    const uint64_t* value =
                        std::get_if<uint64_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for TXNoProtocolBytes";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res
                        .jsonValue["Oem"]["Nvidia"]["TXNoProtocolBytes"] =
                        *value;
                }
                else if (property.first == "DataCRCCount")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for DataCRCCount";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["DataCRCCount"] = *value;
                }
                else if (property.first == "FlitCRCCount")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for FlitCRCCount";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["FlitCRCCount"] = *value;
                }
                else if (property.first == "RecoveryCount")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for RecoveryCount";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["RecoveryCount"] = *value;
                }
                else if (property.first == "ReplayCount")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for ReplayCount";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["ReplayCount"] = *value;
                }
                else if (property.first == "RuntimeError")
                {
                    const uint16_t* value =
                        std::get_if<uint16_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG
                            << "Null value returned for RuntimeError";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    if (*value != 0)
                    {
                        asyncResp->res
                            .jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                      ["RuntimeError"] = true;
                    }
                    else
                    {
                        asyncResp->res
                            .jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                      ["RuntimeError"] = false;
                    }
                }
                else if (property.first == "TrainingError")
                {
                    const uint16_t* value =
                        std::get_if<uint16_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG
                            << "Null value returned for TrainingError";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    if (*value != 0)
                    {
                        asyncResp->res
                            .jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                      ["TrainingError"] = true;
                    }
                    else
                    {
                        asyncResp->res
                            .jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                      ["TrainingError"] = false;
                    }
                }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            }
        },
        service, path, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Port");
}

inline void requestRoutesProcessorPortMetrics(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
                 "Ports/<str>/Metrics")
        .privileges(redfish::privileges::getProcessor)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& processorId,
                                              const std::string& port) {
            BMCWEB_LOG_DEBUG << "Get available system processor resource";
            crow::connections::systemBus->async_method_call(
                [processorId, port, asyncResp](
                    const boost::system::error_code ec,
                    const boost::container::flat_map<
                        std::string,
                        boost::container::flat_map<
                            std::string, std::vector<std::string>>>& subtree) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "DBUS response error";
                        messages::internalError(asyncResp->res);

                        return;
                    }
                    for (const auto& [path, object] : subtree)
                    {
                        if (!boost::ends_with(path, processorId))
                        {
                            continue;
                        }
                        crow::connections::systemBus->async_method_call(
                            [processorId, port, asyncResp](
                                const boost::system::error_code ec1,
                                const boost::container::flat_map<
                                    std::string,
                                    boost::container::flat_map<
                                        std::string, std::vector<std::string>>>&
                                    subtree1) {
                                if (ec1)
                                {
                                    BMCWEB_LOG_DEBUG << "DBUS response error";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                for (const auto& [portPath, object1] : subtree1)
                                {
                                    if (!boost::ends_with(portPath, port))
                                    {
                                        continue;
                                    }
                                    asyncResp->res.jsonValue["@odata.type"] =
                                        "#PortMetrics.v1_1_0.PortMetrics";
                                    std::string portMetricUri =
                                        "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                        "/Processors/";
                                    portMetricUri += processorId + "/Ports/";
                                    portMetricUri += port + "/Metrics";
                                    asyncResp->res.jsonValue["@odata.id"] =
                                        portMetricUri;
                                    asyncResp->res.jsonValue["Id"] = "Metrics";
                                    asyncResp->res.jsonValue["Name"] =
                                        port + " Port Metrics";
                                    for (const auto& [service, interfaces] :
                                         object1)
                                    {
                                        getPortMetricsData(asyncResp, service,
                                                           portPath);

                                        
                                        if (std::find(interfaces.begin(), 
                                         interfaces.end(),
                                         "xyz.openbmc_project.PCIe.PCIeECC") !=
                                         interfaces.end())
                                        {
                                            redfish::processor_utils::
                                                    getPCIeErrorData(
                                                    asyncResp, service, 
                                                     portPath);
                                        }
                                    }

                                    return;
                                }
                                // Object not found
                                messages::resourceNotFound(
                                    asyncResp->res, "#Port.v1_4_0.Port", port);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            path, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Port"});
                        return;
                    }
                    // Object not found
                    messages::resourceNotFound(asyncResp->res,
                                               "#Processor.v1_13_0.Processor",
                                               processorId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 2>{
                    "xyz.openbmc_project.Inventory.Item.Cpu",
                    "xyz.openbmc_project.Inventory.Item.Accelerator"});
        });
}

} // namespace redfish
