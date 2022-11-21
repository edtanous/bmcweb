#pragma once

#include <app.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/json_utils.hpp>

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
                asyncResp->res.jsonValue["Members@odata.count"] =
                    members.size();
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
                BMCWEB_LOG_ERROR << "ObjectMapper::GetObject call failed: "
                                 << errorno;
                messages::internalError(asyncResp->res);
                return;
            }
            
            for (const auto& element : objInfo)
            {
                for (const auto& interface : element.second)
                {
                    if ((interface ==
                         "xyz.openbmc_project.Control.Power.Cap") ||
                        (interface ==
                         "xyz.openbmc_project.Control.Power.Mode") ||
                        (interface ==
                         "xyz.openbmc_project.Inventory.Decorator.Area"))
                    {
                        crow::connections::systemBus->async_method_call(
                            [asyncResp,
                             path](const boost::system::error_code errorno,
                                   const std::vector<std::pair<
                                       std::string,
                                       std::variant<size_t, std::string>>>&
                                       propertiesList) {
                                if (errorno)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "ObjectMapper::GetObject call failed:"
                                        << errorno;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                for (const std::pair<
                                         std::string,
                                         std::variant<size_t, std::string>>&
                                         property : propertiesList)
                                {
                                    std::string propertyName = property.first;
                                        if (propertyName == "MaxPowerCapValue")
                                        {
                                            propertyName = "AllowableMax";
                                        }
                                        else if (propertyName ==
                                                 "MinPowerCapValue")
                                        {
                                            propertyName = "AllowableMin";
                                        }
                                        else if (propertyName == "PowerCap")
                                        {
                                            propertyName = "SetPoint";
                                        }
                                        else if (propertyName ==
                                                 "PhysicalContext")
                                        {
                                            const auto* physicalcontext =
                                                std::get_if<std::string>(
                                                    &property.second);
                                            asyncResp->res
                                                .jsonValue[propertyName] =
                                                redfish::dbus_utils::
                                                    toPhysicalContext(
                                                        *physicalcontext);
                                            continue;
                                        }
                                        else if (propertyName == "PowerMode")
                                        {
                                            propertyName = "ControlMode";
                                            const std::string* mode =
                                                std::get_if<std::string>(
                                                    &property.second);
                                            std::map<std::string,
                                                     std::string>::iterator itr;
                                            for (auto& itr : modes)
                                            {
                                                if (*mode == itr.first)
                                                {
                                                    asyncResp->res.jsonValue
                                                        [propertyName] =
                                                        itr.second;
                                                    break;
                                                }
                                            }

                                            continue;
                                        }
                                        const auto* value = std::get_if<size_t>(
                                            &property.second);
                                        if (value == nullptr)
                                        {
                                            messages::internalError(
                                                asyncResp->res);
                                            return;
                                        }
                                        asyncResp->res.jsonValue[propertyName] =
                                            *value;
                                }
                            },
                            element.first, path,
                            "org.freedesktop.DBus.Properties", "GetAll",
                            interface);
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
    const std::string& sensorName = platformTotalPowerSensorName;

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
                BMCWEB_LOG_DEBUG << "DBUS error: no matched iface " << ec
                                 << "\n";
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
                if (objPath.filename() != sensorName)
                {
                    continue;
                }

                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
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
                            BMCWEB_LOG_ERROR << "Get Sensor value failed: "
                                             << ec;
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
                            BMCWEB_LOG_DEBUG << "Get Related Items failed: "
                                             << errCode;
                            return; // no gpus = no failures
                        }
                        std::vector<std::string>* data =
                            std::get_if<std::vector<std::string>>(&resp);
                        if (data == nullptr)
                        {
                            BMCWEB_LOG_DEBUG
                                << "Null value returned for Related Items ";
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
                                {{"@odata.id",
                                  "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                  "/Processors/" +
                                      gpuName}});
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    path + "/all_processors", "org.freedesktop.DBus.Properties",
                    "Get", "xyz.openbmc_project.Association", "endpoints");
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/sensors", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Sensor.Value"});
}

inline void changemode(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& path, const std::string& controlMode)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, controlMode, path](
            const boost::system::error_code errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
            if (errorno)
            {
                BMCWEB_LOG_ERROR << "ObjectMapper::GetObject call failed: "
                                 << errorno;
                messages::internalError(asyncResp->res);
                return;
            }
            for (const auto& element : objInfo)
            {
                for (const auto& interface : element.second)
                {
                    if (interface == "xyz.openbmc_project.Control.Power.Mode")
                    {
                        crow::connections::systemBus->async_method_call(
                            [asyncResp, path, controlMode,
                             objInfo](const boost::system::error_code ec2) {
                                if (ec2)
                                {
                                    BMCWEB_LOG_ERROR << "DBUS response error "
                                                     << ec2;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                    messages::success(asyncResp->res);
                                    return;
                            },
                            element.first, path,
                            "org.freedesktop.DBus.Properties", "Set",
                            "xyz.openbmc_project.Control.Power.Mode",
                            "PowerMode",
                            dbus::utility::DbusVariantType(controlMode));
                    }
                }
            }
        },

        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 1>{"xyz.openbmc_project.Control.Power.Mode"});
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
                BMCWEB_LOG_ERROR << "ObjectMapper::GetObject call failed: "
                                 << errorno;
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
                            BMCWEB_LOG_DEBUG
                                << "Set power limit property succeeded";
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
                            messages::propertyValueIncorrect(
                                asyncResp->res, "setpoint",
                                std::to_string(setpoint));
                        }
                        else if (strcmp(dbusError->name,
                                        "xyz.openbmc_project.Common."
                                        "Device.Error.WriteFailure") == 0)
                        {
                            // Service failed to change the config
                            messages::operationFailed(asyncResp->res);
                        }
                        else
                        {
                            messages::internalError(asyncResp->res);
                        }
                    },
                    element.first, path, "org.freedesktop.DBus.Properties",
                    "Set", "xyz.openbmc_project.Control.Power.Cap", "PowerCap",
                    dbus::utility::DbusVariantType(setpoint));
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
                auto getChassisPath = [asyncResp, chassisID](
                                          const std::optional<std::string>&
                                              validChassisPath) {
                    if (!validChassisPath)
                    {
                        BMCWEB_LOG_ERROR << "Not a valid chassis ID:"
                                         << chassisID;
                        messages::resourceNotFound(asyncResp->res, "Chassis",
                                                   chassisID);
                        return;
                    }
                    asyncResp->res.jsonValue = {
                        {"@odata.type", "#Controls"},
                        {"@odata.id",
                         "/redfish/v1/Chassis/" + chassisID + "/Controls"},
                        {"Name", "Controls"},
                        {"Description",
                         "The collection of Controlable resource instances " +
                             chassisID}};
                    getPowercontrolObjects(asyncResp, chassisID,
                                           *validChassisPath);
                    
                };
                redfish::chassis_utils::getValidChassisPath(
                    asyncResp, chassisID, std::move(getChassisPath));
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
                auto getChassisPath =
                    [asyncResp, chassisID, controlID](
                        const std::optional<std::string>& validChassisPath) {
                        if (!validChassisPath)
                        {
                            BMCWEB_LOG_ERROR << "Not a valid chassis ID:"
                                             << chassisID;
                            messages::resourceNotFound(asyncResp->res,
                                                       "Chassis", chassisID);
                            return;
                        }
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#Control.v1_0_0.Control";
                        asyncResp->res.jsonValue["SetPointUnits"] = "W";
                        asyncResp->res.jsonValue["Id"] = controlID;
                        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisID + "/Controls/" +
                            controlID;
                        crow::connections::systemBus
                            ->async_method_call(
                                [asyncResp, chassisID, controlID,
                                 validChassisPath](
                                    const boost::system::error_code ec,
                                    std::variant<std::vector<std::string>>&
                                        resp) {
                                    if (ec)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "ObjectMapper::GetObject call failed: "
                                            << ec;
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    std::vector<std::string>* data =
                                        std::get_if<std::vector<std::string>>(
                                            &resp);
                                    if (data == nullptr)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "control id resource not found";
                                        messages::resourceNotFound(
                                            asyncResp->res, "ControlID",
                                            controlID);
                                    }

                                    auto validendpoint = false;
                                    for (const auto& object : *data)
                                    {
                                        sdbusplus::message::object_path objPath(
                                            object);
                                        if (objPath.filename() == controlID)
                                        {
                                            asyncResp->res.jsonValue["Name"] =
                                                "System Power Control";
                                            asyncResp->res
                                                .jsonValue["ControlType"] =
                                                "Power";
                                            getChassisPower(asyncResp, object,
                                                            *validChassisPath);
                                            getTotalPower(asyncResp, chassisID);
                                            validendpoint = true;
                                            break;
                                        }
                                    }
                                    if (!validendpoint)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "control id resource not found";
                                        messages::resourceNotFound(
                                            asyncResp->res, "ControlID",
                                            controlID);
                                    }
                                },
                                "xyz.openbmc_project.ObjectMapper",
                                *validChassisPath + "/power_controls",
                                "org.freedesktop.DBus.Properties", "Get",
                                "xyz.openbmc_project.Association", "endpoints");
                    };
                redfish::chassis_utils::getValidChassisPath(
                    asyncResp, chassisID, std::move(getChassisPath));
            });
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Controls/<str>")
        .privileges(redfish::privileges::postControl)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID, const std::string& controlID) {
                auto getChassisPath =
                    [asyncResp, chassisID, controlID,
                     req](const std::optional<std::string>& validChassisPath) {
                        if (!validChassisPath)
                        {
                            BMCWEB_LOG_ERROR << "Not a valid chassis ID:"
                                             << chassisID;
                            messages::resourceNotFound(asyncResp->res,
                                                       "Chassis", chassisID);
                            return;
                        }
                        crow::connections::systemBus->async_method_call(
                            [asyncResp, chassisID, controlID, req](
                                const boost::system::error_code ec,
                                std::variant<std::vector<std::string>>& resp) {
                                if (ec)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "ObjectMapper::GetObject call failed: "
                                        << ec;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                std::vector<std::string>* data =
                                    std::get_if<std::vector<std::string>>(
                                        &resp);
                                if (data == nullptr)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "control id resource not found";
                                    messages::resourceNotFound(
                                        asyncResp->res, "ControlID", controlID);
                                }
                                auto validendpoint = false;
                                for (const auto& object : *data)
                                {
                                    sdbusplus::message::object_path objPath(
                                        object);
                                    if (objPath.filename() == controlID)
                                    {
                                        validendpoint = true;
                                        std::optional<std::string> mode;
                                        std::optional<uint32_t> setpoint;
                                        std::string controlMode;
                                        if (!json_util::readJsonAction(
                                                req, asyncResp->res,
                                                "ControlMode", mode, "SetPoint",
                                                setpoint))
                                        {
                                            return;
                                        }

                                        for (auto& itr : modes)
                                        {
                                            if (itr.second == *mode)
                                            {
                                                controlMode = itr.first;
                                                break;
                                            }
                                        }
                                        if ((mode == "Automatic") ||
                                            (mode == "Manual"))
                                        {
                                            changemode(asyncResp, object,
                                                       controlMode);
                                        }
                                        else if (mode == "Override")
                                        {
                                            changemode(asyncResp, object,
                                                       controlMode);
                                            if (setpoint)
                                            {
                                                changepowercap(asyncResp,
                                                               object,
                                                               *setpoint);
                                            }
                                        }
                                        else if (setpoint)
                                        {
                                            changepowercap(asyncResp, object,
                                                           *setpoint);
                                        }
                                        else
                                        {
                                            BMCWEB_LOG_ERROR
                                                << "invalid input";
                                            messages::resourceNotFound(
                                                asyncResp->res, "ControlID",
                                                controlID);
                                        }
                                        break;
                                    }
                                }
                                if (!validendpoint)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "control id resource not found";
                                    messages::resourceNotFound(
                                        asyncResp->res, "ControlID", controlID);
                                }
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            *validChassisPath + "/power_controls",
                            "org.freedesktop.DBus.Properties", "Get",
                            "xyz.openbmc_project.Association", "endpoints");
                    };
                redfish::chassis_utils::getValidChassisPath(
                    asyncResp, chassisID, std::move(getChassisPath));
            });
}
} // namespace redfish
