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

#include "health.hpp"
#include "led.hpp"

#include <app.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_utility.hpp>
#include <erot_chassis.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/collection.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/json_utils.hpp>

namespace redfish
{

/**
 * @brief Fill out links association to underneath chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getChassisLinksContains(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get underneath chassis links";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no chassis = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& linksArray =
                aResp->res.jsonValue["Links"]["Contains"];
            linksArray = nlohmann::json::array();
            for (const std::string& chassisPath : *data)
            {
                sdbusplus::message::object_path objectPath(chassisPath);
                std::string chassisName = objectPath.filename();
                if (chassisName.empty())
                {
                    messages::internalError(aResp->res);
                    return;
                }
                linksArray.push_back(
                    {{"@odata.id", "/redfish/v1/Chassis/" + chassisName}});
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out processor links association by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getChassisProcessorLinks(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get underneath processor links";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no processors = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& linksArray =
                aResp->res.jsonValue["Links"]["Processors"];
            linksArray = nlohmann::json::array();
            for (const std::string& processorPath : *data)
            {
                sdbusplus::message::object_path objectPath(processorPath);
                std::string processorName = objectPath.filename();
                if (processorName.empty())
                {
                    messages::internalError(aResp->res);
                    return;
                }
                linksArray.push_back(
                    {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                   "/Processors/" +
                                       processorName}});
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_processors",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out fabric switches links association by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getChassisFabricSwitchesLinks(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get fabric switches links";
    crow::connections::systemBus->async_method_call(
        [aResp, objPath](const boost::system::error_code ec2,
                         std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no fabric = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr || data->size() > 1)
            {
                // There must be single fabric
                return;
            }
            const std::string& fabricPath = data->front();
            sdbusplus::message::object_path objectPath(fabricPath);
            std::string fabricId = objectPath.filename();
            if (fabricId.empty())
            {
                messages::internalError(aResp->res);
                return;
            }
            // Get the switches
            crow::connections::systemBus->async_method_call(
                [aResp,
                 fabricId](const boost::system::error_code ec1,
                           std::variant<std::vector<std::string>>& resp1) {
                    if (ec1)
                    {
                        return; // no switches = no failures
                    }
                    std::vector<std::string>* data1 =
                        std::get_if<std::vector<std::string>>(&resp1);
                    if (data1 == nullptr)
                    {
                        return;
                    }
                    // Sort the switches links
                    std::sort(data1->begin(), data1->end());
                    nlohmann::json& linksArray =
                        aResp->res.jsonValue["Links"]["Switches"];
                    linksArray = nlohmann::json::array();
                    for (const std::string& switchPath : *data1)
                    {
                        sdbusplus::message::object_path objectPath1(switchPath);
                        std::string switchId = objectPath1.filename();
                        if (switchId.empty())
                        {
                            messages::internalError(aResp->res);
                            return;
                        }
                        linksArray.push_back(
                            {{"@odata.id", std::string("/redfish/v1/Fabrics/")
                                               .append(fabricId)
                                               .append("/Switches/")
                                               .append(switchId)}});
                    }
                },
                "xyz.openbmc_project.ObjectMapper", objPath + "/all_switches",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/fabrics",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
/**
 * @brief Fill out chassis nvidia specific info by
 * requesting data from the associated D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getOemBaseboardChassisAssert(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& objPath)

{
    BMCWEB_LOG_DEBUG << "Get chassis OEM info";
    dbus::utility::findAssociations(objPath + "/associated_fru", 
        [aResp{std::move(aResp)}](const boost::system::error_code ec, std::variant<std::vector<std::string>>& assoc){
            if(ec)
            {
                BMCWEB_LOG_DEBUG << "Cannot get association";
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&assoc);
            if (data == nullptr)
            {
                return;
            }
            const std::string& fruPath = data->front();
            crow::connections::systemBus->async_method_call(
                [aResp{std::move(aResp)}, fruPath](
                    const boost::system::error_code ec,
                    const std::vector<std::pair<std::string, std::vector<std::string>>>& objects) {
                    if(ec || objects.size() <= 0)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for serial number";
                        messages::internalError(aResp->res);
                        return;
                    }
                    const std::string& fruObject = objects[0].first;
                    crow::connections::systemBus->async_method_call(
                        [aResp{std::move(aResp)}](
                            const boost::system::error_code ec,
                            const std::vector<std::pair<
                                std::string, std::variant<std::string, bool, uint64_t>>>&
                                propertiesList) {
                        if(ec || propertiesList.size() <= 0)
                        {
                            messages::internalError(aResp->res);
                            return;
                        }
                        for (const auto& property : propertiesList)
                        {
                            if (property.first == "CHASSIS_PART_NUMBER")
                            {
                                const std::string* value =
                                    std::get_if<std::string>(&property.second);
                                if (value == nullptr)
                                {
                                    BMCWEB_LOG_DEBUG << "Null value returned "
                                                        "Part number";
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                aResp->res.jsonValue["Oem"]["Nvidia"]["PartNumber"] =
                                    *value;
                            }
                            else if (property.first == "CHASSIS_SERIAL_NUMBER")
                            {
                                const std::string* value =
                                    std::get_if<std::string>(&property.second);
                                if (value == nullptr)
                                {
                                    BMCWEB_LOG_DEBUG << "Null value returned "
                                                        "for serial number";
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                aResp->res.jsonValue["Oem"]["Nvidia"]["SerialNumber"] =
                                    *value;
                            }
                        }
                    },
                    fruObject, fruPath, "org.freedesktop.DBus.Properties", "GetAll",
                    "xyz.openbmc_project.FruDevice");
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject", fruPath,
                std::array<std::string, 1>({"xyz.openbmc_project.FruDevice"}));
        });
}

/**
 * @brief Write chassis nvidia specific info to eeprom by
 * setting data to the associated Fru D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    setOemBaseboardChassisAssert(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& objPath,
                                 const std::string& prop,
                                 const std::string& value)
{
    BMCWEB_LOG_DEBUG << "Set chassis OEM info: \n";
    dbus::utility::findAssociations(objPath + "/associated_fru",
        [aResp{std::move(aResp)}, prop, value](const boost::system::error_code ec, std::variant<std::vector<std::string>>& assoc){
            if(ec)
            {
                messages::internalError(aResp->res);
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&assoc);
            if (data == nullptr)
            {
                return;
            }
            const std::string& fruPath = data->front();
            crow::connections::systemBus->async_method_call(
                [aResp{std::move(aResp)}, fruPath, prop, value](
                    const boost::system::error_code ec,
                    const std::vector<std::pair<std::string, std::vector<std::string>>>& objects) {
                    if(ec || objects.size() <= 0)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    const std::string& fruObject = objects[0].first;
                    if(prop == "PartNumber")
                    {
                        crow::connections::systemBus->async_method_call(
                            [aResp](const boost::system::error_code ec) {
                                if (ec)
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "DBUS response error: Set CHASSIS_PART_NUMBER"
                                        << ec;
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                messages::success(aResp->res);
                                BMCWEB_LOG_DEBUG << "Set CHASSIS_PART_NUMBER done.";
                            },
                            fruObject, fruPath, "org.freedesktop.DBus.Properties", "Set",
                            "xyz.openbmc_project.FruDevice", "CHASSIS_PART_NUMBER",
                            dbus::utility::DbusVariantType(value));
                    }
                    else if(prop == "SerialNumber")
                    {
                        crow::connections::systemBus->async_method_call(
                            [aResp](const boost::system::error_code ec) {
                                if (ec)
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "DBUS response error: Set CHASSIS_SERIAL_NUMBER"
                                        << ec;
                                    messages::internalError(aResp->res);
                                    return;
                                }
                                messages::success(aResp->res);
                                BMCWEB_LOG_DEBUG << "Set CHASSIS_SERIAL_NUMBER done.";
                            },
                            fruObject, fruPath, "org.freedesktop.DBus.Properties", "Set",
                            "xyz.openbmc_project.FruDevice", "CHASSIS_SERIAL_NUMBER",
                            dbus::utility::DbusVariantType(value));
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject", fruPath,
                std::array<std::string, 1>({"xyz.openbmc_project.FruDevice"}));
        });
}

/**
 * @brief Fill out chassis nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getOemHdwWriteProtectInfo(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                      const std::string& service,
                                      const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Baseboard Hardware write protect info";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::variant<std::string, bool, uint64_t>>>&
                propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Baseboard Hardware write protect info";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : propertiesList)
            {
                if (property.first == "WriteProtected")
                {
                    const bool* value = std::get_if<bool>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for hardware write protected";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res
                        .jsonValue["Oem"]["Nvidia"]["HardwareWriteProtected"] =
                        *value;
                }

                if (property.first == "WriteProtectedControl")
                {
                    const bool* value = std::get_if<bool>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG
                            << "Null value returned "
                               "for hardware write protected control";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["Oem"]["Nvidia"]
                                        ["HardwareWriteProtectedControl"] =
                        *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Settings");
}

/**
 * @brief Fill out chassis nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getOemPCIeDeviceClockReferenceInfo(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                       const std::string& service,
                                       const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Baseboard PCIeReference clock count";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::variant<std::string, bool, uint64_t>>>&
                propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Baseboard PCIeReference clock count";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : propertiesList)
            {
                if (property.first == "PCIeReferenceClockCount")
                {
                    const uint64_t* value =
                        std::get_if<uint64_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for pcie refernce clock count";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["Oem"]["Nvidia"][property.first] =
                        *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.PCIeRefClock");
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * @brief Fill out chassis power limits info of a chassis by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getChassisPowerLimits(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                  const std::string& service,
                                  const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get chassis power limits";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::variant<size_t>>>&
                propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Chassis power limits";
                messages::internalError(aResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<size_t>>& property :
                 propertiesList)
            {
                const std::string& propertyName = property.first;
                if ((propertyName == "MinPowerWatts") ||
                    (propertyName == "MaxPowerWatts"))
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for power limits";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue[propertyName] = *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.PowerLimit");
}

/**
 * @brief Retrieves chassis state properties over dbus
 *
 * @param[in] aResp - Shared pointer for completing asynchronous calls.
 *
 * @return None.
 */
inline void getChassisState(std::shared_ptr<bmcweb::AsyncResp> aResp)
{
    // crow::connections::systemBus->async_method_call(
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, "xyz.openbmc_project.State.Chassis",
        "/xyz/openbmc_project/state/chassis0",
        "xyz.openbmc_project.State.Chassis", "CurrentPowerState",
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const std::string& chassisState) {
            if (ec)
            {
                if (ec == boost::system::errc::host_unreachable)
                {
                    // Service not available, no error, just don't return
                    // chassis state info
                    BMCWEB_LOG_DEBUG << "Service not available " << ec;
                    return;
                }
                BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                messages::internalError(aResp->res);
                return;
            }

            BMCWEB_LOG_DEBUG << "Chassis state: " << chassisState;
            // Verify Chassis State
            if (chassisState ==
                "xyz.openbmc_project.State.Chassis.PowerState.On")
            {
                aResp->res.jsonValue["PowerState"] = "On";
                aResp->res.jsonValue["Status"]["State"] = "Enabled";
            }
            else if (chassisState ==
                     "xyz.openbmc_project.State.Chassis.PowerState.Off")
            {
                aResp->res.jsonValue["PowerState"] = "Off";
                aResp->res.jsonValue["Status"]["State"] = "StandbyOffline";
            }
        });
}

/**
 * @brief Fill out chassis physical dimensions info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getChassisDimensions(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& service,
                                 const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get chassis dimensions";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, dbus::utility::DbusVariantType>>& propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Chassis dimensions";
                messages::internalError(aResp->res);
                return;
            }
            for (const std::pair<std::string, dbus::utility::DbusVariantType>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Height")
                {
                    const double* value = std::get_if<double>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for Height";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["HeightMm"] = *value;
                }
                else if (propertyName == "Width")
                {
                    const double* value = std::get_if<double>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for Width";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["WidthMm"] = *value;
                }
                else if (propertyName == "Depth")
                {
                    const double* value = std::get_if<double>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for Depth";
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["DepthMm"] = *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.Dimension");
}

inline void getIntrusionByService(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                  const std::string& service,
                                  const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get intrusion status by service \n";

    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, objPath,
        "xyz.openbmc_project.Chassis.Intrusion", "Status",
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const std::string& value) {
            if (ec)
            {
                // do not add err msg in redfish response, because this is not
                //     mandatory property
                BMCWEB_LOG_ERROR << "DBUS response error " << ec << "\n";
                return;
            }

            aResp->res.jsonValue["PhysicalSecurity"]["IntrusionSensorNumber"] =
                1;
            aResp->res.jsonValue["PhysicalSecurity"]["IntrusionSensor"] = value;
        });
}

/**
 * Retrieves physical security properties over dbus
 */
inline void getPhysicalSecurityData(std::shared_ptr<bmcweb::AsyncResp> aResp)
{
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                // do not add err msg in redfish response, because this is not
                //     mandatory property
                BMCWEB_LOG_INFO << "DBUS error: no matched iface " << ec
                                << "\n";
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const auto& object : subtree)
            {
                for (const auto& service : object.second)
                {
                    getIntrusionByService(aResp, service.first, object.first);
                    return;
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/Intrusion", 1,
        std::array<const char*, 1>{"xyz.openbmc_project.Chassis.Intrusion"});
}

inline void handleChassisCollectionGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] =
        "#ChassisCollection.ChassisCollection";
    asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Chassis";
    asyncResp->res.jsonValue["Name"] = "Chassis Collection";

    collection_util::getCollectionMembers(
        asyncResp, "/redfish/v1/Chassis",
        {"xyz.openbmc_project.Inventory.Item.Board",
         "xyz.openbmc_project.Inventory.Item.Chassis"});
}

/**
 * ChassisCollection derived class for delivering Chassis Collection Schema
 *  Functions triggers appropriate requests on DBus
 */
inline void requestRoutesChassisCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/")
        .privileges(redfish::privileges::getChassisCollection)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleChassisCollectionGet, std::ref(app)));
}

inline void
    getChassisLocationCode(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.LocationCode", "LocationCode",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for Location";
                messages::internalError(asyncResp->res);
                return;
            }

            asyncResp->res
                .jsonValue["Location"]["PartLocation"]["ServiceLabel"] =
                property;
        });
}

inline void getChassisData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& chassisId)
{
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};

    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisId(std::string(chassisId))](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;

                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }
#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                std::shared_ptr<HealthRollup> health =
                    std::make_shared<HealthRollup>(
                        crow::connections::systemBus, path,
                        [asyncResp](const std::string& rootHealth,
                                    const std::string& healthRollup) {
                            asyncResp->res.jsonValue["Status"]["Health"] =
                                rootHealth;
                            asyncResp->res.jsonValue["Status"]["HealthRollup"] =
                                healthRollup;
                        });
                health->start();
#else  // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                auto health = std::make_shared<HealthPopulate>(asyncResp);

                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus,
                    "xyz.openbmc_project.ObjectMapper", path + "/all_sensors",
                    "xyz.openbmc_project.Association", "endpoints",
                    [health](const boost::system::error_code ec2,
                             const std::vector<std::string>& resp) {
                        if (ec2)
                        {
                            return; // no sensors = no failures
                        }
                        health->inventory = resp;
                    });

                health->populate();
#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

                if (connectionNames.empty())
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

                asyncResp->res.jsonValue["@odata.type"] =
                    "#Chassis.v1_16_0.Chassis";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Chassis/" + chassisId;
                asyncResp->res.jsonValue["Name"] = "Chassis Collection";
                asyncResp->res.jsonValue["ChassisType"] = "RackMount";
                asyncResp->res
                    .jsonValue["Actions"]["#Chassis.Reset"]["target"] =
                    "/redfish/v1/Chassis/" + chassisId +
                    "/Actions/Chassis.Reset";
                asyncResp->res.jsonValue["Actions"]["#Chassis.Reset"]
                                        ["@Redfish.ActionInfo"] =
                    "/redfish/v1/Chassis/" + chassisId + "/ResetActionInfo";
                asyncResp->res.jsonValue["PCIeDevices"] = {
                    {"@odata.id",
                     "/redfish/v1/Chassis/" + chassisId + "/PCIeDevices"}};

                redfish::conditions_utils::populateServiceConditions(asyncResp,
                                                                     chassisId);
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_LOGSERVICES

                asyncResp->res.jsonValue["LogServices"] = {
                    {"@odata.id",
                     "/redfish/v1/Chassis/" + chassisId + "/LogServices"}};

#endif // BMCWEB_ENABLE_NVIDIA_OEM_LOGSERVICES

                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus,
                    "xyz.openbmc_project.ObjectMapper", path + "/drive",
                    "xyz.openbmc_project.Association", "endpoints",
                    [asyncResp,
                     chassisId](const boost::system::error_code ec3,
                                const std::vector<std::string>& resp) {
                        if (ec3 || resp.empty())
                        {
                            return; // no drives = no failures
                        }

                        nlohmann::json reference;
                        reference["@odata.id"] = crow::utility::urlFromPieces(
                            "redfish", "v1", "Chassis", chassisId, "Drives");
                        asyncResp->res.jsonValue["Drives"] =
                            std::move(reference);
                    });

                const std::string& connectionName = connectionNames[0].first;

                const std::vector<std::string>& interfaces2 =
                    connectionNames[0].second;
                const std::array<const char*, 2> hasIndicatorLed = {
                    "xyz.openbmc_project.Inventory.Item.Panel",
                    "xyz.openbmc_project.Inventory.Item.Board.Motherboard"};

                const std::string assetTagInterface =
                    "xyz.openbmc_project.Inventory.Decorator.AssetTag";
                if (std::find(interfaces2.begin(), interfaces2.end(),
                              assetTagInterface) != interfaces2.end())
                {
                    sdbusplus::asio::getProperty<std::string>(
                        *crow::connections::systemBus, connectionName, path,
                        assetTagInterface, "AssetTag",
                        [asyncResp, chassisId(std::string(chassisId))](
                            const boost::system::error_code ec2,
                            const std::string& property) {
                            if (ec2)
                            {
                                BMCWEB_LOG_DEBUG
                                    << "DBus response error for AssetTag";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            asyncResp->res.jsonValue["AssetTag"] = property;
                        });
                }

                for (const char* interface : hasIndicatorLed)
                {
                    if (std::find(interfaces2.begin(), interfaces2.end(),
                                  interface) != interfaces2.end())
                    {
                        getIndicatorLedState(asyncResp);
                        getLocationIndicatorActive(asyncResp);
                        break;
                    }
                }

                sdbusplus::asio::getAllProperties(
                    *crow::connections::systemBus, connectionName, path,
                    "xyz.openbmc_project.Inventory.Decorator.Asset",
                    [asyncResp, chassisId(std::string(chassisId))](
                        const boost::system::error_code /*ec2*/,
                        const dbus::utility::DBusPropertiesMap&
                            propertiesList) {
                        const std::string* partNumber = nullptr;
                        const std::string* serialNumber = nullptr;
                        const std::string* manufacturer = nullptr;
                        const std::string* model = nullptr;
                        const std::string* sparePartNumber = nullptr;

                        const std::string* sku = nullptr;

                        const bool success = sdbusplus::unpackPropertiesNoThrow(
                            dbus_utils::UnpackErrorPrinter(), propertiesList,
                            "PartNumber", partNumber, "SerialNumber",
                            serialNumber, "Manufacturer", manufacturer, "Model",
                            model, "SparePartNumber", sparePartNumber, "SKU",
                            sku);

                        if (!success)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        if (partNumber != nullptr)
                        {
                            asyncResp->res.jsonValue["PartNumber"] =
                                *partNumber;
                        }

                        if (serialNumber != nullptr)
                        {
                            asyncResp->res.jsonValue["SerialNumber"] =
                                *serialNumber;
                        }

                        if (manufacturer != nullptr)
                        {
                            asyncResp->res.jsonValue["Manufacturer"] =
                                *manufacturer;
                        }

                        if (model != nullptr)
                        {
                            asyncResp->res.jsonValue["Model"] = *model;
                        }

                        // SparePartNumber is optional on D-Bus
                        // so skip if it is empty
                        if (sparePartNumber != nullptr &&
                            !sparePartNumber->empty())
                        {
                            asyncResp->res.jsonValue["SparePartNumber"] =
                                *sparePartNumber;
                        }

                        if (sku != nullptr)
                        {
                            asyncResp->res.jsonValue["SKU"] = *sku;
                        }

                        asyncResp->res.jsonValue["Name"] = chassisId;
                        asyncResp->res.jsonValue["Id"] = chassisId;
#ifdef BMCWEB_ALLOW_DEPRECATED_POWER_THERMAL
                        asyncResp->res.jsonValue["Thermal"]["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisId + "/Thermal";
                        asyncResp->res
                            .jsonValue["ThermalSubsystem"]["@odata.id"] =
                            crow::utility::urlFromPieces("redfish", "v1",
                                                         "Chassis", chassisId,
                                                         "ThermalSubsystem");
                        asyncResp->res.jsonValue["EnvironmentMetrics"] = {
                            {"@odata.id", "/redfish/v1/Chassis/" + chassisId +
                                              "/EnvironmentMetrics"}};
                        // Power object
                        asyncResp->res.jsonValue["Power"]["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisId + "/Power";

#endif
#ifdef BMCWEB_NEW_POWERSUBSYSTEM_THERMALSUBSYSTEM
                        asyncResp->res
                            .jsonValue["PowerSubsystem"]["@odata.id"] =
                            crow::utility::urlFromPieces("redfish", "v1",
                                                         "Chassis", chassisId,
                                                         "PowerSubsystem");
#endif
                        // SensorCollection
                        asyncResp->res.jsonValue["Sensors"]["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisId + "/Sensors";
                        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";

                        // Assembly collection
                        asyncResp->res.jsonValue["Assembly"] = {
                            {"@odata.id",
                             "/redfish/v1/Chassis/" + chassisId + "/Assembly"}};

                        // PCIeSlots collection
                        asyncResp->res.jsonValue["PCIeSlots"] = {
                            {"@odata.id", "/redfish/v1/Chassis/" + chassisId +
                                              "/PCIeSlots"}};

                        // Controls Collection
                        asyncResp->res.jsonValue["Controls"] = {
                            {"@odata.id",
                             "/redfish/v1/Chassis/" + chassisId + "/Controls"}};
                        asyncResp->res.jsonValue["Links"]["ComputerSystems"] = {
                            {{"@odata.id",
                              "/redfish/v1/Systems/" PLATFORMSYSTEMID}}};

                        asyncResp->res.jsonValue["Links"]["ComputerSystems"] = {
                            {{"@odata.id",
                              "/redfish/v1/Systems/" PLATFORMSYSTEMID}}};
                        asyncResp->res.jsonValue["Links"]["ManagedBy"] = {
                            {{"@odata.id",
                              "/redfish/v1/Managers/" PLATFORMBMCID}}};

                        getChassisState(asyncResp);
                        redfish::conditions_utils::populateServiceConditions(
                            asyncResp, chassisId);
                    });

                for (const auto& interface : interfaces2)
                {
                    if (interface == "xyz.openbmc_project.Common.UUID")
                    {
                        redfish::chassis_utils::getChassisUUID(
                            asyncResp, connectionName, path);
                    }
                    else if (
                        interface ==
                        "xyz.openbmc_project.Inventory.Decorator.LocationCode")
                    {
                        getChassisLocationCode(asyncResp, connectionName, path);
                    }
                    else if (interface ==
                             "xyz.openbmc_project.Inventory.Decorator.Location")
                    {
                        redfish::chassis_utils::getChassisLocationType(
                            asyncResp, connectionName, path);
                    }
                    else if (interface == "xyz.openbmc_project.Inventory.Item")
                    {
                        redfish::chassis_utils::getChassisName(
                            asyncResp, connectionName, path);
                    }
                }

                // Chassis item properties
                crow::connections::systemBus->async_method_call(
                    [asyncResp](
                        const boost::system::error_code ec1,
                        const std::vector<std::pair<
                            std::string, dbus::utility::DbusVariantType>>&
                            propertiesList) {
                        if (ec1)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error for "
                                                "Chassis properties";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        for (const std::pair<std::string,
                                             dbus::utility::DbusVariantType>&
                                 property : propertiesList)
                        {
                            const std::string& propertyName = property.first;
                            if (propertyName == "Type")
                            {
                                const std::string* value =
                                    std::get_if<std::string>(&property.second);
                                if (value == nullptr)
                                {
                                    BMCWEB_LOG_DEBUG << "Null value returned "
                                                        "for type";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                std::string chassisType =
                                    redfish::chassis_utils::getChassisType(
                                        *value);
                                asyncResp->res.jsonValue["ChassisType"] =
                                    chassisType;
                            }
                        }
                    },
                    connectionName, path, "org.freedesktop.DBus.Properties",
                    "GetAll", "xyz.openbmc_project.Inventory.Item.Chassis");
                // Chassis physical dimensions
                const std::string dimensionInterface =
                    "xyz.openbmc_project.Inventory.Decorator.Dimension";
                if (std::find(interfaces2.begin(), interfaces2.end(),
                              dimensionInterface) != interfaces2.end())
                {
                    getChassisDimensions(asyncResp, connectionName, path);
                }

                // Chassis power limits
                const std::string powerInterface =
                    "xyz.openbmc_project.Inventory.Decorator."
                    "PowerLimit";
                if (std::find(interfaces2.begin(), interfaces2.end(),
                              powerInterface) != interfaces2.end())
                {
                    getChassisPowerLimits(asyncResp, connectionName, path);
                }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                // default oem data
                nlohmann::json& oem = asyncResp->res.jsonValue["Oem"]["Nvidia"];
                oem["@odata.type"] = "#NvidiaChassis.v1_0_0.NvidiaChassis";

                // Baseboard Chassis OEM properties if exist, search by association
                getOemBaseboardChassisAssert(asyncResp, path);

                // Baseboard Chassis hardware write protect info
                const std::string hdwWriteProtectInterface =
                    "xyz.openbmc_project.Software.Settings";
                if (std::find(interfaces2.begin(), interfaces2.end(),
                              hdwWriteProtectInterface) != interfaces2.end())
                {
                    getOemHdwWriteProtectInfo(asyncResp, connectionName, path);
                }

                // Baseboard Chassis PCIe reference clock count
                const std::string PCIeDeviceclkRefInterface =
                    "xyz.openbmc_project.Inventory.Decorator.PCIeRefClock";
                if (std::find(interfaces2.begin(), interfaces2.end(),
                              PCIeDeviceclkRefInterface) != interfaces2.end())
                {
                    getOemPCIeDeviceClockReferenceInfo(asyncResp,
                                                       connectionName, path);
                }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

                // Links association to underneath chassis
                getChassisLinksContains(asyncResp, path);
                // Links association to underneath processors
                getChassisProcessorLinks(asyncResp, path);
                // Links association to connected fabric switches
                getChassisFabricSwitchesLinks(asyncResp, path);
                // Link association to parent chassis
                redfish::chassis_utils::getChassisLinksContainedBy(asyncResp,
                                                                   path);

                return;
            }

            // Couldn't find an object with that name.  return an error
            messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);

    getPhysicalSecurityData(asyncResp);
}

inline void getChassisUUID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Common.UUID", "UUID",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& chassisUUID) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for UUID";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["UUID"] = chassisUUID;
        });
}

inline void
    handleChassisGet(App& app, const crow::Request& req,
                     const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& chassisId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    redfish::chassis_utils::isEROTChassis(
        chassisId, [req, asyncResp, chassisId](bool isEROT) {
            if (isEROT)
            {
                BMCWEB_LOG_DEBUG << " EROT chassis";
                getEROTChassis(req, asyncResp, chassisId);
            }
            else
            {
                getChassisData(asyncResp, chassisId);
            }
        });
}
inline void
    handleChassisPatch(const crow::Request& req, const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& param)
{
    nlohmann::json reqJson;
    std::optional<bool> locationIndicatorActive;
    std::optional<std::string> indicatorLed;
    std::optional<nlohmann::json> oemJsonObj;
    std::optional<std::string> partNumber;
    std::optional<std::string> serialNumber;
    if (param.empty())
    {
        return;
    }
    if(!json_util::processJsonFromRequest(req, reqJson))
    {
        return;
    }
    if(reqJson.find("LocationIndicatorActive") != reqJson.end())
    {
        json_util::readJsonAction(
            req, asyncResp->res, "LocationIndicatorActive",
            locationIndicatorActive);
    }
    if(reqJson.find("IndicatorLED") != reqJson.end())
    {
        json_util::readJsonAction(
            req, asyncResp->res, "IndicatorLED",
            indicatorLed);
    }
    if(reqJson.find("Oem") != reqJson.end())
    {
        if (json_util::readJsonAction(req, asyncResp->res, "Oem", oemJsonObj))
        {
            std::optional<nlohmann::json> nvidiaJsonObj;
            if (json_util::readJson(*oemJsonObj, asyncResp->res, "Nvidia", nvidiaJsonObj))
            {
                json_util::readJson(
                    *nvidiaJsonObj, asyncResp->res, "PartNumber",
                    partNumber, "SerialNumber", serialNumber);
            }
        }
    }
    // TODO (Gunnar): Remove IndicatorLED after enough time has passed
    if (!locationIndicatorActive && !indicatorLed)
    {
        //return; // delete this when we support more patch properties
    }
    if (indicatorLed)
    {
        asyncResp->res.addHeader(
            boost::beast::http::field::warning,
            "299 - \"IndicatorLED is deprecated. Use LocationIndicatorActive instead.\"");
    }
    
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};

    const std::string& chassisId = param;

    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisId, locationIndicatorActive, indicatorLed, partNumber, serialNumber](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;

                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }

                if (connectionNames.empty())
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

                const std::vector<std::string>& interfaces3 =
                    connectionNames[0].second;

                const std::array<const char*, 2> hasIndicatorLed = {
                    "xyz.openbmc_project.Inventory.Item.Panel",
                    "xyz.openbmc_project.Inventory.Item.Board.Motherboard"};
                bool indicatorChassis = false;
                for (const char* interface : hasIndicatorLed)
                {
                    if (std::find(interfaces3.begin(), interfaces3.end(),
                                  interface) != interfaces3.end())
                    {
                        indicatorChassis = true;
                        break;
                    }
                }
                if (locationIndicatorActive)
                {
                    if (indicatorChassis)
                    {
                        setLocationIndicatorActive(asyncResp,
                                                   *locationIndicatorActive);
                    }
                    else
                    {
                        messages::propertyUnknown(asyncResp->res,
                                                  "LocationIndicatorActive");
                    }
                }
                if (indicatorLed)
                {
                    if (indicatorChassis)
                    {
                        setIndicatorLedState(asyncResp, *indicatorLed);
                    }
                    else
                    {
                        messages::propertyUnknown(asyncResp->res,
                                                  "IndicatorLED");
                    }
                }
                if(partNumber)
                {
                    setOemBaseboardChassisAssert(asyncResp, path, "PartNumber", *partNumber);
                }
                if(serialNumber)
                {
                    setOemBaseboardChassisAssert(asyncResp, path, "SerialNumber", *serialNumber);
                }
                return;
            }

            messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

inline void
    handleChassisPatchReq(App& app, const crow::Request& req,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& param)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    redfish::chassis_utils::isEROTChassis(
        param, [req, asyncResp, param](bool isEROT) {
            if (isEROT)
            {
                BMCWEB_LOG_DEBUG << " EROT chassis";
                handleEROTChassisPatch(req, asyncResp, param);
            }
            else
            {
                handleChassisPatch(req, asyncResp, param);
            }
    });
}

/**
 * Chassis override class for delivering Chassis Schema
 * Functions triggers appropriate requests on DBus
 */
inline void requestRoutesChassis(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/")
        .privileges(redfish::privileges::getChassis)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleChassisGet, std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/")
        .privileges(redfish::privileges::patchChassis)
        .methods(boost::beast::http::verb::patch)(
            std::bind_front(handleChassisPatchReq, std::ref(app)));
}

inline void
    doChassisPowerCycle(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const char* busName = "xyz.openbmc_project.ObjectMapper";
    const char* path = "/xyz/openbmc_project/object_mapper";
    const char* interface = "xyz.openbmc_project.ObjectMapper";
    const char* method = "GetSubTreePaths";

    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.State.Chassis"};

    // Use mapper to get subtree paths.
    crow::connections::systemBus->async_method_call(
        [asyncResp](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& chassisList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "[mapper] Bad D-Bus request error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }

            const char* processName = "xyz.openbmc_project.State.Chassis";
            const char* interfaceName = "xyz.openbmc_project.State.Chassis";
            const char* destProperty = "RequestedPowerTransition";
            const std::string propertyValue =
                "xyz.openbmc_project.State.Chassis.Transition.PowerCycle";
            std::string objectPath =
                "/xyz/openbmc_project/state/chassis_system0";

            /* Look for system reset chassis path */
            if ((std::find(chassisList.begin(), chassisList.end(),
                           objectPath)) == chassisList.end())
            {
                /* We prefer to reset the full chassis_system, but if it doesn't
                 * exist on some platforms, fall back to a host-only power reset
                 */
                objectPath = "/xyz/openbmc_project/state/chassis0";
            }

            crow::connections::systemBus->async_method_call(
                [asyncResp](const boost::system::error_code ec2) {
                    // Use "Set" method to set the property value.
                    if (ec2)
                    {
                        BMCWEB_LOG_DEBUG << "[Set] Bad D-Bus request error: "
                                         << ec2;
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    messages::success(asyncResp->res);
                },
                processName, objectPath, "org.freedesktop.DBus.Properties",
                "Set", interfaceName, destProperty,
                dbus::utility::DbusVariantType{propertyValue});
        },
        busName, path, interface, method, "/", 0, interfaces);
}

inline void handleChassisResetActionInfoPost(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& /*chassisId*/)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    BMCWEB_LOG_DEBUG << "Post Chassis Reset.";

    std::string resetType;

    if (!json_util::readJsonAction(req, asyncResp->res, "ResetType", resetType))
    {
        return;
    }

    if (resetType != "PowerCycle")
    {
        BMCWEB_LOG_DEBUG << "Invalid property value for ResetType: "
                         << resetType;
        messages::actionParameterNotSupported(asyncResp->res, resetType,
                                              "ResetType");

        return;
    }
    doChassisPowerCycle(asyncResp);
}

/**
 * ChassisResetAction class supports the POST method for the Reset
 * action.
 * Function handles POST method request.
 * Analyzes POST body before sending Reset request data to D-Bus.
 */

inline void requestRoutesChassisResetAction(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Actions/Chassis.Reset/")
        .privileges(redfish::privileges::postChassis)
        .methods(boost::beast::http::verb::post)(
            std::bind_front(handleChassisResetActionInfoPost, std::ref(app)));
}

inline void handleChassisResetActionInfoGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] = "#ActionInfo.v1_1_2.ActionInfo";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/Chassis/" + chassisId + "/ResetActionInfo";
    asyncResp->res.jsonValue["Name"] = "Reset Action Info";

    asyncResp->res.jsonValue["Id"] = "ResetActionInfo";
    nlohmann::json::array_t parameters;
    nlohmann::json::object_t parameter;
    parameter["Name"] = "ResetType";
    parameter["Required"] = true;
    parameter["DataType"] = "String";
    nlohmann::json::array_t allowed;
    allowed.push_back("PowerCycle");
    parameter["AllowableValues"] = std::move(allowed);
    parameters.push_back(std::move(parameter));

    asyncResp->res.jsonValue["Parameters"] = std::move(parameters);
}

/**
 * ChassisResetActionInfo derived class for delivering Chassis
 * ResetType AllowableValues using ResetInfo schema.
 */
inline void requestRoutesChassisResetActionInfo(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/ResetActionInfo/")
        .privileges(redfish::privileges::getActionInfo)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleChassisResetActionInfoGet, std::ref(app)));
}

} // namespace redfish
