#pragma once

#include <app.hpp>
#include <dbus_utility.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/json_utils.hpp>
#include "health.hpp"

namespace redfish
{

std::map<std::string, std::string> modes = {
    {"xyz.openbmc_project.Control.Power.Mode.PowerMode.MaximumPerformance",
     "Automatic"},
    {"xyz.openbmc_project.Control.Power.Mode.PowerMode.OEM", "Override"},
    {"xyz.openbmc_project.Control.Power.Mode.PowerMode.PowerSaving", "Manual"}};
const std::array<const char*, 2> powerinterfaces = {
    "xyz.openbmc_project.Control.Power.Cap",
    "xyz.openbmc_project.Control.Power.Mode"};
inline void
    getPowercontrolObjects(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& chassisID,
                           const std::string& chassisPath)
{
    nlohmann::json& members = asyncResp->res.jsonValue["Members"];
    nlohmann::json& count = asyncResp->res.jsonValue["Members@odata.count"];
    members = nlohmann::json::array();
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisID, &members,
         &count](const boost::system::error_code,
                 std::variant<std::vector<std::string>>& resp) {
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        for (const auto& object : *data)
        {
            sdbusplus::message::object_path objPath(object);
            members.push_back(
                {{"@odata.id", "/redfish/v1/Chassis/" + chassisID +
                                   "/Controls/" + objPath.filename()}});
        }
        asyncResp->res.jsonValue["Members@odata.count"] = members.size();
    },
        "xyz.openbmc_project.ObjectMapper", chassisPath + "/power_controls",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getChassisPower(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& path,
                            const std::string& chassisPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, path](
            const boost::system::error_code errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
        if (errorno)
        {
            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", errorno);
            messages::internalError(asyncResp->res);
            return;
        }

        for (const auto& element : objInfo)
        {
            for (const auto& interface : element.second)
            {
                if ((interface == "xyz.openbmc_project.Control.Power.Cap") ||
                    (interface == "xyz.openbmc_project.Control.Power.Mode") ||
                    (interface ==
                     "xyz.openbmc_project.Inventory.Decorator.Area"))
                {
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, path](
                            const boost::system::error_code errorno,
                            const std::vector<
                                std::pair<std::string,
                                          std::variant<size_t, std::string>>>&
                                propertiesList) {
                        if (errorno)
                        {
                            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed:{}", errorno);
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        for (const std::pair<std::string,
                                             std::variant<size_t, std::string>>&
                                 property : propertiesList)
                        {
                            std::string propertyName = property.first;
                            if (propertyName == "MaxPowerCapValue")
                            {
                                propertyName = "AllowableMax";
                            }
                            else if (propertyName == "MinPowerCapValue")
                            {
                                propertyName = "AllowableMin";
                            }
                            else if (propertyName == "PowerCap")
                            {
                                propertyName = "SetPoint";
                            }
                            else if (propertyName == "PhysicalContext")
                            {
                                const auto* physicalcontext =
                                    std::get_if<std::string>(&property.second);
                                asyncResp->res.jsonValue[propertyName] =
                                    redfish::dbus_utils::toPhysicalContext(
                                        *physicalcontext);
                                continue;
                            }
                            else if (propertyName == "PowerMode")
                            {
                                propertyName = "ControlMode";
                                const std::string* mode =
                                    std::get_if<std::string>(&property.second);
                                std::map<std::string, std::string>::iterator
                                    itr;
                                for (auto& itr : modes)
                                {
                                    if (*mode == itr.first)
                                    {
                                        asyncResp->res.jsonValue[propertyName] =
                                            itr.second;
                                        break;
                                    }
                                }

                                continue;
                            }
                            const auto* value =
                                std::get_if<size_t>(&property.second);
                            if (value == nullptr)
                            {
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            asyncResp->res.jsonValue[propertyName] = *value;
                        }
                    },
                        element.first, path, "org.freedesktop.DBus.Properties",
                        "GetAll", interface);
                }
            }
        }
    },

        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path, powerinterfaces);

    auto health = std::make_shared<HealthPopulate>(asyncResp);
    sdbusplus::asio::getProperty<std::vector<std::string>>(
        *crow::connections::systemBus, "xyz.openbmc_project.ObjectMapper",
        chassisPath + "/all_sensors", "xyz.openbmc_project.Association",
        "endpoints",
        [health](const boost::system::error_code ec2,
                 const std::vector<std::string>& resp) {
        if (ec2)
        {
            return; // no sensors = no failures
        }
        health->inventory = resp;
    });
    health->populate();
}

inline void getTotalPower(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisID)
{
    const std::string& sensorName = platformPowerControlSensorName;

    crow::connections::systemBus->async_method_call(
        [asyncResp, sensorName, chassisID](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
        if (ec)
        {
            // do not add err msg in redfish response, because this is not
            //     mandatory property
            BMCWEB_LOG_DEBUG("DBUS error: no matched iface {}", ec);
            return;
        }
        // Iterate over all retrieved ObjectPaths.
        for (const std::pair<
                 std::string,
                 std::vector<std::pair<std::string, std::vector<std::string>>>>&
                 object : subtree)
        {
            const std::string& path = object.first;
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                connectionNames = object.second;

            sdbusplus::message::object_path objPath(path);
            if (objPath.filename() != sensorName)
            {
                continue;
            }

            if (connectionNames.size() < 1)
            {
                BMCWEB_LOG_ERROR("Got 0 Connection names");
                continue;
            }
            const std::string& serviceName = connectionNames[0].first;

            // Read Sensor value
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisID, sensorName, serviceName,
                 path](const boost::system::error_code ec,
                       const std::variant<double>& totalPower) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("Get Sensor value failed: {}", ec);
                    messages::internalError(asyncResp->res);
                    return;
                }

                const auto* value = std::get_if<double>(&totalPower);
                asyncResp->res.jsonValue["Sensor"]["Reading"] = *value;
                asyncResp->res.jsonValue["Sensor"]["DataSourceUri"] =
                    ("/redfish/v1/Chassis/" + chassisID + "/Sensors/")
                        .append(sensorName);
            },
                serviceName, path, "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Sensor.Value", "Value");

            // Read related items
            crow::connections::systemBus->async_method_call(
                [asyncResp,
                 chassisID](const boost::system::error_code errCode,
                            std::variant<std::vector<std::string>>& resp) {
                if (errCode)
                {
                    BMCWEB_LOG_DEBUG("Get Related Items failed: {}", errCode);
                    return; // no gpus = no failures
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned for Related Items ");
                    return;
                }
                nlohmann::json& relatedItemsArray =
                    asyncResp->res.jsonValue["RelatedItem"];
                relatedItemsArray = nlohmann::json::array();
                for (const std::string& gpuPath : *data)
                {
                    sdbusplus::message::object_path objectPath(gpuPath);
                    std::string gpuName = objectPath.filename();
                    if (gpuName.empty())
                    {
                        return;
                    }
                    relatedItemsArray.push_back(
                        {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                       "/Processors/" +
                                           gpuName}});
                }
            },
                "xyz.openbmc_project.ObjectMapper", path + "/all_processors",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/sensors", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Sensor.Value"});
}

inline void
    getControlSettings(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& path)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, path](
            const boost::system::error_code errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
        if (errorno)
        {
            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", errorno);
            messages::internalError(asyncResp->res);
            return;
        }

        for (const auto& element : objInfo)
        {
            crow::connections::systemBus->async_method_call(
                [asyncResp, path](
                    const boost::system::error_code errorno,
                    const std::vector<
                        std::pair<std::string, dbus::utility::DbusVariantType>>&
                        propertiesList) {
                if (errorno)
                {
                    BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed:{}", errorno);
                    messages::internalError(asyncResp->res);
                    return;
                }
                for (const auto& [propertyName, value] : propertiesList)
                {
                    if (propertyName == "MaxPowerCapValue" &&
                        std::holds_alternative<uint32_t>(value))
                    {
                        asyncResp->res.jsonValue["AllowableMax"] =
                            std::get<uint32_t>(value);
                    }
                    else if (propertyName == "MinPowerCapValue" &&
                             std::holds_alternative<uint32_t>(value))
                    {
                        asyncResp->res.jsonValue["AllowableMin"] =
                            std::get<uint32_t>(value);
                    }
                    else if (propertyName == "PowerCap" &&
                             std::holds_alternative<uint32_t>(value))
                    {
                        asyncResp->res.jsonValue["SetPoint"] =
                            std::get<uint32_t>(value);
                    }
                    else if (propertyName == "PowerCapEnable" &&
                             std::holds_alternative<bool>(value))
                    {
                        if (std::get<bool>(value))
                        {
                            asyncResp->res.jsonValue["ControlMode"] =
                                "Automatic";
                        }
                        else
                        {
                            asyncResp->res.jsonValue["ControlMode"] =
                                "Disabled";
                        }
                        asyncResp->res.jsonValue["Status"]["Health"] = "OK";
                    }
                }
            },
                element.first, path, "org.freedesktop.DBus.Properties",
                "GetAll", "xyz.openbmc_project.Control.Power.Cap");

            crow::connections::systemBus->async_method_call(
                [asyncResp, path](
                    const boost::system::error_code errorno,
                    const std::vector<
                        std::pair<std::string, dbus::utility::DbusVariantType>>&
                        propertiesList) {
                if (errorno)
                {
                    return;
                }

                for (const auto& [propertyName, value] : propertiesList)
                {
                    if (propertyName == "PhysicalContext")
                    {
                        const auto* physicalcontext =
                            std::get_if<std::string>(&value);
                        asyncResp->res.jsonValue[propertyName] =
                            redfish::dbus_utils::toPhysicalContext(
                                *physicalcontext);
                        return;
                    }
                }
            },
                element.first, path, "org.freedesktop.DBus.Properties",
                "GetAll", "xyz.openbmc_project.Inventory.Decorator.Area");

            // Read related items
            crow::connections::systemBus->async_method_call(
                [asyncResp](const boost::system::error_code errCode,
                            std::variant<std::vector<std::string>>& resp) {
                if (errCode)
                {
                    BMCWEB_LOG_DEBUG("Get Related Items failed: {}", errCode);
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned for Related Items ");
                    return;
                }
                nlohmann::json& relatedItemsArray =
                    asyncResp->res.jsonValue["RelatedItem"];
                relatedItemsArray = nlohmann::json::array();
                for (const std::string& chassisPath : *data)
                {
                    sdbusplus::message::object_path objectPath(chassisPath);
                    std::string chassisName = objectPath.filename();
                    if (chassisName.empty())
                    {
                        return;
                    }
                    relatedItemsArray.push_back(
                        {{"@odata.id", "/redfish/v1/Chassis/" + chassisName}});
                }
            },
                "xyz.openbmc_project.ObjectMapper", path + "/chassis",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path, powerinterfaces);
}

inline void getPowerReading(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& chassisID,
                            const std::string& chassisPath)
{
    sdbusplus::asio::getProperty<std::vector<std::string>>(
        *crow::connections::systemBus, "xyz.openbmc_project.ObjectMapper",
        chassisPath + "/all_sensors", "xyz.openbmc_project.Association",
        "endpoints",
        [asyncResp, chassisID,
         chassisPath](const boost::system::error_code ec,
                      const std::vector<std::string>& resp) {
        if (ec)
        {
            return; // no sensors = no failures
        }
        for (const auto& sensorPath : resp)
        {
            sdbusplus::message::object_path objPath(sensorPath);
            std::string prefix = "/xyz/openbmc_project/sensors/power/" +
                                 chassisID + "_Power";
            if (sensorPath.find(prefix) == std::string::npos)
            {
                continue;
            }

            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisPath, sensorPath](
                    const boost::system::error_code ec2,
                    const std::vector<std::pair<
                        std::string, std::vector<std::string>>>& objInfo) {
                if (ec2)
                {
                    BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", ec2.what());
                    messages::internalError(asyncResp->res);
                    return;
                }

                for (const auto& [service, interfaces] : objInfo)
                {
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, chassisPath, sensorPath](
                            const boost::system::error_code ec3,
                            const std::vector<std::pair<
                                std::string, dbus::utility::DbusVariantType>>&
                                propertiesList) {
                        if (ec3)
                        {
                            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed:{}", ec3.what());
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        for (const auto& [propertyName, val] : propertiesList)
                        {
                            if (propertyName == "Value" &&
                                std::holds_alternative<double>(val))
                            {
                                const auto value = std::get<double>(val);
                                sdbusplus::message::object_path
                                    chassisObjectPath(chassisPath);
                                sdbusplus::message::object_path
                                    sensorObjectPath(sensorPath);
                                asyncResp->res.jsonValue["Sensor"]["Reading"] =
                                    value;
                                asyncResp->res
                                    .jsonValue["Sensor"]["DataSourceUri"] =
                                    ("/redfish/v1/Chassis/" +
                                     chassisObjectPath.filename() +
                                     "/Sensors/" + sensorObjectPath.filename());
                                return;
                            }
                        }
                    },
                        service, sensorPath, "org.freedesktop.DBus.Properties",
                        "GetAll", "xyz.openbmc_project.Sensor.Value");
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject", sensorPath,
                std::array<const char*, 1>{"xyz.openbmc_project.Sensor.Value"});
        }
    });
}

inline void changepowercap(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& path, size_t setpoint)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, setpoint, path](
            const boost::system::error_code errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
        if (errorno)
        {
            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", errorno);
            messages::internalError(asyncResp->res);
            return;
        }
        for (const auto& element : objInfo)
        {
            crow::connections::systemBus->async_method_call(
                [asyncResp, path, setpoint,
                 element](const boost::system::error_code ec2,
                          sdbusplus::message::message& msg) {
                if (!ec2)
                {
                    BMCWEB_LOG_DEBUG("Set power limit property succeeded");
                    messages::success(asyncResp->res);
                    return;
                }
                // Read and convert dbus error message to redfish error
                const sd_bus_error* dbusError = msg.get_error();
                if (dbusError == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                if (strcmp(
                        dbusError->name,
                        "xyz.openbmc_project.Common.Error.InvalidArgument") ==
                    0)
                {
                    // Invalid value
                    messages::propertyValueIncorrect(asyncResp->res, "setpoint",
                                                     std::to_string(setpoint));
                }
                else if (strcmp(dbusError->name,
                                "xyz.openbmc_project.Common."
                                "Device.Error.WriteFailure") == 0)
                {
                    // Service failed to change the config
                    messages::operationFailed(asyncResp->res);
                }
                else if (strcmp(
                             dbusError->name,
                             "xyz.openbmc_project.Common.Error.Unavailable") ==
                         0)
                {
                    std::string errBusy = "0x50A";
                    std::string errBusyResolution =
                        "SMBPBI Command failed with error busy, please try after 60 seconds";
                    // busy error
                    messages::asyncError(asyncResp->res, errBusy,
                                         errBusyResolution);
                }
                else if (strcmp(dbusError->name,
                                "xyz.openbmc_project.Common.Error.Timeout") ==
                         0)
                {
                    std::string errTimeout = "0x600";
                    std::string errTimeoutResolution =
                        "Settings may/maynot have applied, please check get response before patching";
                    // timeout error
                    messages::asyncError(asyncResp->res, errTimeout,
                                         errTimeoutResolution);
                }
                else
                {
                    messages::internalError(asyncResp->res);
                }
            },
                element.first, path, "org.freedesktop.DBus.Properties", "Set",
                "xyz.openbmc_project.Control.Power.Cap", "PowerCap",
                dbus::utility::DbusVariantType(setpoint));
        }
    },

        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 1>{"xyz.openbmc_project.Control.Power.Cap"});
}

inline void
    changePowerCapEnable(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& path, const bool& enabled)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, enabled, path](
            const boost::system::error_code errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
        if (errorno)
        {
            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", errorno);
            messages::internalError(asyncResp->res);
            return;
        }
        for (const auto& element : objInfo)
        {
            crow::connections::systemBus->async_method_call(
                [asyncResp, path, enabled,
                 element](const boost::system::error_code ec2,
                          sdbusplus::message::message& msg) {
                if (!ec2)
                {
                    BMCWEB_LOG_DEBUG("Set power cap enable property succeeded");
                    messages::success(asyncResp->res);
                    return;
                }
                // Read and convert dbus error message to redfish error
                const sd_bus_error* dbusError = msg.get_error();
                if (dbusError == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                            "Device.Error.WriteFailure") == 0)
                {
                    // Service failed to change the config
                    messages::operationFailed(asyncResp->res);
                }
                else if (strcmp(dbusError->name,
                                "org.freedesktop.DBus.Error.UnknownProperty") ==
                         0)
                {
                    // Some implementation does not have PowerCapEnable
                    return;
                }
                else
                {
                    messages::internalError(asyncResp->res);
                }
            },
                element.first, path, "org.freedesktop.DBus.Properties", "Set",
                "xyz.openbmc_project.Control.Power.Cap", "PowerCapEnable",
                dbus::utility::DbusVariantType(enabled));
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 1>{"xyz.openbmc_project.Control.Power.Cap"});
}

inline void requestRoutesChassisControlsCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Controls/")
        .privileges(redfish::privileges::getControl)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID) {
        auto getChassisPath =
            [asyncResp,
             chassisID](const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            asyncResp->res.jsonValue = {
                {"@odata.type", "#ControlCollection.ControlCollection"},
                {"@odata.id", "/redfish/v1/Chassis/" + chassisID + "/Controls"},
                {"Name", "Controls"},
                {"Description",
                 "The collection of Controlable resource instances " +
                     chassisID}};
            getPowercontrolObjects(asyncResp, chassisID, *validChassisPath);
        };
        redfish::chassis_utils::getValidChassisPath(asyncResp, chassisID,
                                                    std::move(getChassisPath));
    });
}
inline void requestRoutesChassisControls(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Controls/<str>")
        .privileges(redfish::privileges::getControl)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID, const std::string& controlID) {
        auto getControlSystem =
            [asyncResp, chassisID,
             controlID](const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] = "#Control.v1_3_0.Control";
            asyncResp->res.jsonValue["SetPointUnits"] = "W";
            asyncResp->res.jsonValue["Id"] = controlID;
            asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/Chassis/" + chassisID + "/Controls/" + controlID;
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisID, controlID, validChassisPath](
                    const boost::system::error_code ec,
                    std::variant<std::vector<std::string>>& resp) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", ec);
                    messages::internalError(asyncResp->res);
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                    return;
                }

                auto validendpoint = false;
                for (const auto& object : *data)
                {
                    sdbusplus::message::object_path objPath(object);
                    if (objPath.filename() == controlID)
                    {
                        asyncResp->res.jsonValue["Name"] =
                            "System Power Control";
                        asyncResp->res.jsonValue["ControlType"] = "Power";
                        getChassisPower(asyncResp, object, *validChassisPath);
                        getTotalPower(asyncResp, chassisID);
                        validendpoint = true;
                        break;
                    }
                }
                if (!validendpoint)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                *validChassisPath + "/power_controls",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        };

        auto getControlCpu =
            [asyncResp, chassisID,
             controlID](const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] = "#Control.v1_3_0.Control";
            asyncResp->res.jsonValue["SetPointUnits"] = "W";
            asyncResp->res.jsonValue["Id"] = controlID;
            asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/Chassis/" + chassisID + "/Controls/" + controlID;
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisID, controlID, validChassisPath](
                    const boost::system::error_code ec,
                    std::variant<std::vector<std::string>>& resp) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", ec);
                    messages::internalError(asyncResp->res);
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                    return;
                }

                auto validendpoint = false;
                for (const auto& object : *data)
                {
                    sdbusplus::message::object_path objPath(object);
                    if (objPath.filename() == controlID)
                    {
                        if (controlID.find("_CPU_") != std::string::npos)
                        {
                            asyncResp->res.jsonValue["Name"] =
                                "Cpu Power Control";
                        }
                        else
                        {
                            asyncResp->res.jsonValue["Name"] =
                                "Module Power Control";
                            // Automatic mode from H100 8-GPU
                            // Redfish SMBPBI Supplement
                            asyncResp->res.jsonValue["ControlMode"] =
                                "Automatic";
                        }
                        asyncResp->res.jsonValue["ControlType"] = "Power";
                        getControlSettings(asyncResp, object);
                        getPowerReading(asyncResp, chassisID,
                                        *validChassisPath);
                        validendpoint = true;
                        break;
                    }
                }
                if (!validendpoint)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                *validChassisPath + "/power_controls",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        };

        auto getControl =
            [asyncResp, chassisID, getControlSystem, getControlCpu](
                const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            crow::connections::systemBus->async_method_call(
                [asyncResp, getControlSystem, getControlCpu, validChassisPath](
                    const boost::system::error_code ec,
                    const dbus::utility::MapperGetObject& objType) {
                if (ec || objType.empty())
                {
                    BMCWEB_LOG_ERROR("GetObject for path {}", (*validChassisPath).c_str());
                    return;
                }
                for (auto [service, interfaces] : objType)
                {
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Inventory.Item.Cpu") !=
                            interfaces.end() ||
                        std::find(
                            interfaces.begin(), interfaces.end(),
                            "xyz.openbmc_project.Inventory.Item.ProcessorModule") !=
                            interfaces.end())
                    {
                        getControlCpu(validChassisPath);
                        return;
                    }
                }
                // Not a CPU
                getControlSystem(validChassisPath);
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject",
                *validChassisPath, std::array<const char*, 0>{});
        };
        redfish::chassis_utils::getValidChassisPath(asyncResp, chassisID,
                                                    std::move(getControl));
    });

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Controls/<str>")
        .privileges(redfish::privileges::patchControl)
        .methods(boost::beast::http::verb::patch)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID, const std::string& controlID) {
        auto patchControlSystem =
            [asyncResp, chassisID, controlID,
             req](const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisID, controlID,
                 req](const boost::system::error_code ec,
                      std::variant<std::vector<std::string>>& resp) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", ec);
                    messages::internalError(asyncResp->res);
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                    return;
                }
                auto validendpoint = false;
                for (const auto& object : *data)
                {
                    sdbusplus::message::object_path objPath(object);
                    if (objPath.filename() == controlID)
                    {
                        validendpoint = true;
                        std::string mode;
                        std::optional<uint32_t> setpoint;

                        if (!json_util::readJsonAction(req, asyncResp->res,
                                                       "SetPoint", setpoint))
                        {
                            return;
                        }
                        if (setpoint)
                        {
                            changepowercap(asyncResp, object, *setpoint);
                        }

                        break;
                    }
                }
                if (!validendpoint)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                *validChassisPath + "/power_controls",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        };

        auto patchControlCpu =
            [asyncResp, chassisID, controlID,
             req](const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisID, controlID,
                 req](const boost::system::error_code ec,
                      std::variant<std::vector<std::string>>& resp) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}", ec);
                    messages::internalError(asyncResp->res);
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                    return;
                }
                auto validendpoint = false;
                for (const auto& object : *data)
                {
                    sdbusplus::message::object_path objPath(object);
                    if (objPath.filename() == controlID)
                    {
                        validendpoint = true;
                        std::optional<std::string> mode;
                        std::optional<uint32_t> setpoint;
                        std::string controlMode;
                        if (!json_util::readJsonPatch(req, asyncResp->res,
                                                      "ControlMode", mode,
                                                      "SetPoint", setpoint))
                        {
                            return;
                        }

                        if (mode)
                        {
                            if (controlID.find("_CPU_") != std::string::npos)
                            {
                                if ((*mode == "Automatic") ||
                                    (*mode == "Override") ||
                                    (*mode == "Manual"))
                                {
                                    changePowerCapEnable(asyncResp, object,
                                                         true);
                                }
                                else if (*mode == "Disabled")
                                {
                                    changePowerCapEnable(asyncResp, object,
                                                         false);
                                }
                                else
                                {
                                    BMCWEB_LOG_ERROR("invalid input");
                                    messages::actionParameterUnknown(
                                        asyncResp->res, "ControlMode", *mode);
                                }
                            }
                            else
                            {
                                messages::actionParameterNotSupported(
                                    asyncResp->res, "ControlMode", *mode);
                            }
                        }

                        if (setpoint)
                        {
                            changepowercap(asyncResp, object, *setpoint);
                        }
                        break;
                    }
                }
                if (!validendpoint)
                {
                    BMCWEB_LOG_ERROR("control id resource not found");
                    messages::resourceNotFound(asyncResp->res, "ControlID",
                                               controlID);
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                *validChassisPath + "/power_controls",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        };

        auto patchControl =
            [asyncResp, chassisID, patchControlSystem, patchControlCpu](
                const std::optional<std::string>& validChassisPath) {
            if (!validChassisPath)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }
            crow::connections::systemBus->async_method_call(
                [asyncResp, patchControlSystem, patchControlCpu,
                 validChassisPath](
                    const boost::system::error_code ec,
                    const dbus::utility::MapperGetObject& objType) {
                if (ec || objType.empty())
                {
                    BMCWEB_LOG_ERROR("GetObject for path {}", (*validChassisPath).c_str());
                    return;
                }
                for (auto [service, interfaces] : objType)
                {
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Inventory.Item.Cpu") !=
                            interfaces.end() ||
                        std::find(
                            interfaces.begin(), interfaces.end(),
                            "xyz.openbmc_project.Inventory.Item.ProcessorModule") !=
                            interfaces.end())
                    {
                        patchControlCpu(validChassisPath);
                        return;
                    }
                }
                // Not a CPU
                patchControlSystem(validChassisPath);
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject",
                *validChassisPath, std::array<const char*, 0>{});
        };
        redfish::chassis_utils::getValidChassisPath(asyncResp, chassisID,
                                                    std::move(patchControl));
    });
}
} // namespace redfish
