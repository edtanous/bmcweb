#pragma once

#include <app.hpp>
#include <boost/format.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/json_utils.hpp>
#include <utils/processor_utils.hpp>

#include <tuple>

namespace redfish
{

using SetPointProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

// Map of service name to list of interfaces
using MapperServiceMap =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

inline void
    getfanSpeedsPercent(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& chassisID)
{
    BMCWEB_LOG_DEBUG << "Get properties for getFan associated to chassis = "
                     << chassisID;
    const std::array<std::string, 1> sensorInterfaces = {
        "xyz.openbmc_project.Sensor.Value"};
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisID](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                sensorsubtree) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "D-Bus response error on GetSubTree " << ec;
                if (ec.value() == boost::system::errc::io_error)
                {
                    messages::resourceNotFound(asyncResp->res, "Chassis",
                                               chassisID);
                    return;
                }
                messages::internalError(asyncResp->res);
                return;
            }
            nlohmann::json& fanList =
                asyncResp->res.jsonValue["FanSpeedsPercent"];
            fanList = nlohmann::json::array();

            for (const auto& [objectPath, serviceName] : sensorsubtree)
            {
                if (objectPath.empty() || serviceName.size() != 1)
                {
                    BMCWEB_LOG_DEBUG << "Error getting D-Bus object!";
                    messages::internalError(asyncResp->res);
                    return;
                }
                const std::string& validPath = objectPath;
                const std::string& connectionName = serviceName[0].first;
                std::vector<std::string> split;
                // Reserve space for
                // /xyz/openbmc_project/sensors/<name>/<subname>
                split.reserve(6);
                boost::algorithm::split(split, validPath,
                                        boost::is_any_of("/"));
                if (split.size() < 6)
                {
                    BMCWEB_LOG_ERROR << "Got path that isn't long enough "
                                     << validPath;
                    continue;
                }
                // These indexes aren't intuitive, as boost::split puts an empty
                // string at the beginning
                const std::string& sensorType = split[4];
                const std::string& sensorName = split[5];
                BMCWEB_LOG_DEBUG << "sensorName " << sensorName
                                 << " sensorType " << sensorType;
                if (sensorType == "fan" || sensorType == "fan_tach" ||
                    sensorType == "fan_pwm")
                {
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, chassisID, &fanList,
                         sensorName](const boost::system::error_code ec,
                                     const std::variant<double>& value) {
                            if (ec)
                            {
                                BMCWEB_LOG_DEBUG << "Can't get Fan speed!";
                                messages::internalError(asyncResp->res);
                                return;
                            }

                            const double* attributeValue =
                                std::get_if<double>(&value);
                            if (attributeValue == nullptr)
                            {
                                // illegal property
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            std::string tempPath = "/redfish/v1/Chassis/" +
                                                   chassisID + "/Sensors/";
                            fanList.push_back(
                                {{"DeviceName", "Chassis Fan #" + sensorName},
                                 {"SpeedRPM", *attributeValue},
                                 {"DataSourceUri", tempPath + sensorName},
                                 {"@odata.id", tempPath + sensorName}});
                        },
                        connectionName, validPath,
                        "org.freedesktop.DBus.Properties", "Get",
                        "xyz.openbmc_project.Sensor.Value", "Value");
                }
                else
                {
                    BMCWEB_LOG_DEBUG
                        << "This is not a fan-related sensor,sensortype = "
                        << sensorType;
                    continue;
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/sensors", 0, sensorInterfaces);
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * Handle the PATCH operation of the Edpp Scale limit property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp                Async HTTP response.
 * @param[in]       processorId         prcoessor Id.
 * @param[in]       setPoint            New property value to apply.
 * @param[in]       patchEdppSetPoint   Path of CPU object to modify.
 * @param[in]       serviceMap          Service map for CPU object.
 */
inline void patchEdppSetPoint(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                              const std::string& processorId,
                              const size_t setPoint, const bool persistency,
                              const std::string& cpuObjectPath,
                              const MapperServiceMap& serviceMap)
{

    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.Edpp") != interfaceList.end())
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

    std::tuple<size_t, bool> reqSetPoint;
    reqSetPoint = std::make_tuple(setPoint, persistency);

    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, setPoint](boost::system::error_code ec,
                                      sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set point property succeeded";
                return;
            }

            BMCWEB_LOG_ERROR << "Processor ID: " << processorId
                             << " set point property failed: " << ec;
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
                messages::propertyValueIncorrect(resp->res, "setPoint",
                                                 std::to_string(setPoint));
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
        "Set", "com.nvidia.Edpp", "SetPoint",
        std::variant<std::tuple<size_t, bool>>(reqSetPoint));
}

inline void getPowerMode(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& connectionName,
                         const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connectionName, objPath](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::variant<std::string>>>& propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Chassis properties";
                messages::internalError(asyncResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<std::string>>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "PowerMode")
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
                    std::string oemPowerMode =
                        redfish::chassis_utils::getPowerModeType(*value);
                    if (oemPowerMode.empty())
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["PowerMode"] =
                        oemPowerMode;
                }
            }
        },
        connectionName, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Control.Power.Mode");
}

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getPowerWatts(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisID)
{
    const std::string& sensorName = platformTotalPowerSensorName;
    const std::string& totalPowerPath =
        "/xyz/openbmc_project/sensors/power/" + sensorName;
    // Add total power sensor to associated chassis only
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisID, sensorName,
         totalPowerPath](const boost::system::error_code ec,
                         std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no endpoints = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            // Check chassisId for endpoint
            for (const std::string& endpointPath : *data)
            {
                sdbusplus::message::object_path objPath(endpointPath);
                const std::string& endpointId = objPath.filename();
                if (endpointId != chassisID)
                {
                    continue;
                }
                const std::array<const char*, 1> totalPowerInterfaces = {
                    "xyz.openbmc_project.Sensor.Value"};
                // Process sensor reading
                crow::connections::systemBus->async_method_call(
                    [asyncResp, chassisID, sensorName, totalPowerPath](
                        const boost::system::error_code ec,
                        const std::vector<std::pair<
                            std::string, std::vector<std::string>>>& object) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        for (const auto& tempObject : object)
                        {
                            const std::string& connectionName =
                                tempObject.first;
                            crow::connections::systemBus->async_method_call(
                                [asyncResp, sensorName,
                                 chassisID](const boost::system::error_code ec,
                                            const std::variant<double>& value) {
                                    if (ec)
                                    {
                                        BMCWEB_LOG_DEBUG
                                            << "Can't get Power Watts!";
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }

                                    const double* attributeValue =
                                        std::get_if<double>(&value);
                                    if (attributeValue == nullptr)
                                    {
                                        // illegal property
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    std::string tempPath =
                                        "/redfish/v1/Chassis/" + chassisID +
                                        "/Sensors/";
                                    asyncResp->res.jsonValue["PowerWatts"] = {
                                        {"Reading", *attributeValue},
                                        {"DataSourceUri",
                                         tempPath + sensorName},
                                        {"@odata.id", tempPath + sensorName}};
                                },
                                connectionName, totalPowerPath,
                                "org.freedesktop.DBus.Properties", "Get",
                                "xyz.openbmc_project.Sensor.Value", "Value");
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject",
                    totalPowerPath, totalPowerInterfaces);
            }
        },
        "xyz.openbmc_project.ObjectMapper", totalPowerPath + "/chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getPowerCap(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& connectionName,
                        const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connectionName, objPath](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::variant<uint32_t>>>&
                propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Chassis properties";
                messages::internalError(asyncResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<uint32_t>>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "SetPoint")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["PowerLimitWatts"]["SetPoint"] =
                        *value;
                }
            }
        },
        connectionName, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Control.Power.Cap");
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getEDPpData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& connectionName,
                        const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connectionName,
         objPath](const boost::system::error_code ec,
                  const SetPointProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "procesor EDPp scaling properties";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res
                .jsonValue["Oem"]["Nvidia"]["EDPpPercent"]["@odata.type"] =
                "#NvidiaEnvironmentMetrics.v1_0_0.NvidiaEnvironmentMetrics";

            for (const auto& [key, variant] : properties)
            {
                if (key == "SetPoint")
                {
                    using SetPointProperty = std::tuple<size_t, bool>;
                    const auto* setPoint =
                        std::get_if<SetPointProperty>(&variant);
                    if (setPoint != nullptr)
                    {
                        const auto& [limit, persistency] = *setPoint;
                        asyncResp->res.jsonValue["Oem"]["Nvidia"]["EDPpPercent"]
                                                ["SetPoint"] = limit;
                        asyncResp->res.jsonValue["Oem"]["Nvidia"]["EDPpPercent"]
                                                ["Persistency"] = {};
                    }
                }
                else if (key == "AllowableMax")
                {
                    const size_t* value = std::get_if<size_t>(&variant);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["EDPpPercent"]
                                            ["AllowableMax"] = *value;
                }
                else if (key == "AllowableMin")
                {
                    const size_t* value = std::get_if<size_t>(&variant);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["EDPpPercent"]
                                            ["AllowableMin"] = *value;
                }
            }
        },
        connectionName, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.Edpp");
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getPowerLimits(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connectionName, objPath](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::variant<uint32_t>>>&
                propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Chassis properties";
                messages::internalError(asyncResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<uint32_t>>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "MaxPowerWatts")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res
                        .jsonValue["PowerLimitWatts"]["AllowableMax"] = *value;
                }
                if (propertyName == "MinPowerWatts")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res
                        .jsonValue["PowerLimitWatts"]["AllowableMin"] = *value;
                }
            }
        },
        connectionName, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.PowerLimit");
}

inline void getControlMode(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connectionName,
         objPath](const boost::system::error_code ec,
                  const std::vector<std::pair<std::string, std::variant<bool>>>&
                      propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Chassis properties";
                messages::internalError(asyncResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<bool>>& property :
                 propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Manual")
                {
                    const bool* value = std::get_if<bool>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    std::string controlMode = "Automatic";
                    if (*value == true)
                    {
                        controlMode = "Manual";
                    }
                    asyncResp->res.jsonValue["PowerLimitWatts"]["ControlMode"] =
                        controlMode;
                }
            }
        },
        connectionName, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Control.Mode");
}

template <std::size_t SIZE>
inline void
    getPowerAndControlData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& resourceId,
                           const std::array<const char*, SIZE>& interfaces)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp,
         resourceId](const boost::system::error_code ec,
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
                if (objPath.filename() != resourceId)
                {
                    continue;
                }

                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

                const std::string& connectionName = connectionNames[0].first;
                const std::vector<std::string>& interfaces =
                    connectionNames[0].second;

                if (std::find(
                        interfaces.begin(), interfaces.end(),
                        "xyz.openbmc_project.Inventory.Decorator.PowerLimit") !=
                    interfaces.end())
                {
                    // Chassis power limit properties
                    getPowerLimits(asyncResp, connectionName, objPath);
                }
                if (std::find(interfaces.begin(), interfaces.end(),
                              "xyz.openbmc_project.Control.Power.Cap") !=
                    interfaces.end())
                {
                    // Chassis power setpoint properties
                    getPowerCap(asyncResp, connectionName, objPath);
                }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                if (std::find(interfaces.begin(), interfaces.end(),
                              "xyz.openbmc_project.Control.Power.Mode") !=
                    interfaces.end())
                {
                    // Chassis power setpoint properties
                    getPowerMode(asyncResp, connectionName, objPath);
                }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

                return;
            }

            messages::resourceNotFound(asyncResp->res,
                                       "#Chassis.v1_15_0.Chassis", resourceId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
/**
 * Handle the PATCH operation of the powerMode status property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp             Async HTTP response.
 * @param[in]       resourceId       Resource Id.
 * @param[in]       powerMode        New property value to apply.
 * @param[in]       objectPath       Path of resource object to modify.
 * @param[in]       serviceName      Service for resource object.
 */
inline void patchPowerMode(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                           const std::string& resourceId,
                           const std::optional<std::string>& powerMode,
                           const std::string& objectPath,
                           const std::string& serviceName,
                           const std::string& resourceType)
{
    std::string powerModeStatus =
        redfish::chassis_utils::convertToPowerModeType(*powerMode);
    if (powerModeStatus.empty())
    {
        // Invalid value
        messages::propertyValueIncorrect(resp->res, "PowerMode", *powerMode);
        BMCWEB_LOG_DEBUG << "Set power mode property failed"
                         << ":"
                         << "incorrect property value given";
        return;
    }
    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, resourceId, powerMode, resourceType](
            boost::system::error_code ec, sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set power mode property succeeded";
                return;
            }

            BMCWEB_LOG_ERROR << resourceType << ": " << resourceId
                             << " set power mode property failed: " << ec;
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
        serviceName, objectPath, "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Control.Power.Mode", "PowerMode",
        std::variant<std::string>(powerModeStatus));
}

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void
    getEnvironmentMetrics(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisID)
{
    BMCWEB_LOG_DEBUG
        << "Get properties for EnvironmentMetrics associated to chassis = "
        << chassisID;
    asyncResp->res.jsonValue["@odata.type"] =
        "#EnvironmentMetrics.v1_2_0.EnvironmentMetrics";
    asyncResp->res.jsonValue["Name"] = "Chassis Environment Metrics";
    asyncResp->res.jsonValue["Id"] = "EnvironmentMetrics";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/Chassis/" + chassisID + "/EnvironmentMetrics";
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
    asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
        "#NvidiaEnvironmentMetrics.v1_0_0.NvidiaEnvironmentMetrics";
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
    getfanSpeedsPercent(asyncResp, chassisID);
    getPowerWatts(asyncResp, chassisID);
    getPowerAndControlData(asyncResp, chassisID, interfaces);
}

/**
 * Handle the PATCH operation of the power limit property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       resourceId       Resource Id.
 * @param[in]       powerLimit      New property value to apply.
 * @param[in]       objectPath      Path of resource object to modify.
 * @param[in]       serviceName      Service for resource object.
 */
inline void patchPowerLimit(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                            const std::string& resourceId, const int powerLimit,
                            const std::string& objectPath,
                            const std::string& serviceName,
                            const std::string& resourceType)
{
    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, resourceId, powerLimit, resourceType](
            boost::system::error_code ec, sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set power limit property succeeded";
                return;
            }

            BMCWEB_LOG_ERROR << resourceType << ": " << resourceId
                             << " set power limit property failed: " << ec;
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
                messages::propertyValueIncorrect(resp->res, "powerLimit",
                                                 std::to_string(powerLimit));
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
        serviceName, objectPath, "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Control.Power.Cap", "SetPoint",
        std::variant<uint32_t>(static_cast<uint32_t>(powerLimit)));
}

inline void requestRoutesEnvironmentMetrics(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/EnvironmentMetrics/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID) {
                auto getChassisID =
                    [asyncResp, chassisID](
                        const std::optional<std::string>& validChassisID) {
                        if (!validChassisID)
                        {
                            BMCWEB_LOG_ERROR << "Not a valid chassis ID:"
                                             << chassisID;
                            messages::resourceNotFound(asyncResp->res,
                                                       "Chassis", chassisID);
                            return;
                        }

                        getEnvironmentMetrics(asyncResp, *validChassisID);
                    };
                redfish::chassis_utils::getValidChassisID(
                    asyncResp, chassisID, std::move(getChassisID));
            });

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/EnvironmentMetrics/")
        .privileges(redfish::privileges::patchChassis)
        .methods(
            boost::beast::http::verb::
                patch)([](const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisId) {
            std::optional<nlohmann::json> powerLimit;
            std::optional<nlohmann::json> oem;
            // Read json request
            if (!json_util::readJson(req, asyncResp->res, "PowerLimitWatts",
                                     powerLimit, "Oem", oem))
            {
                return;
            }
            // Update power limit
            if (powerLimit)
            {
                std::optional<int> setPoint;
                if (json_util::readJson(*powerLimit, asyncResp->res,
                                                        "SetPoint", setPoint))
                {
                    const std::array<const char*, 1> interfaces = {
                        "xyz.openbmc_project.Inventory.Item.Chassis"};

                    crow::connections::systemBus->async_method_call(
                        [asyncResp, chassisId, setPoint](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                        if (ec)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        // Iterate over all retrieved ObjectPaths.
                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            const std::string& path = object.first;
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                connectionNames = object.second;

                            sdbusplus::message::object_path objPath(path);
                            if (objPath.filename() != chassisId)
                            {
                                continue;
                            }

                            if (connectionNames.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 Connection names";
                                continue;
                            }

                            const std::string& connectionName =
                                connectionNames[0].first;
                            const std::vector<std::string>& interfaces =
                                connectionNames[0].second;

                            if (std::find(
                                    interfaces.begin(), interfaces.end(),
                                    "xyz.openbmc_project.Control.Power.Cap") !=
                                interfaces.end())
                            {
                                std::string resourceType = "Chassis";
                                patchPowerLimit(asyncResp, chassisId,
                                                *setPoint, objPath,
                                                connectionName, resourceType);
                            }

                            return;
                        }

                        messages::resourceNotFound(asyncResp->res,
                                                   "#Chassis.v1_15_0.Chassis",
                                                   chassisId);
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                    "/xyz/openbmc_project/inventory", 0, interfaces);
                 }

            }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            if (oem)
            {
                std::optional<nlohmann::json> nvidiaObj;
                if (!redfish::json_util::readJson(*oem, asyncResp->res,
                                                  "Nvidia", nvidiaObj))
                {
                    BMCWEB_LOG_ERROR
                        << "Illegal Property "
                        << oem->dump(2, ' ', true,
                                     nlohmann::json::error_handler_t::replace);
                    return;
                }
                if (nvidiaObj)
                {
                    std::optional<std::string> powerMode;
                    if (!redfish::json_util::readJson(
                            *nvidiaObj, asyncResp->res, "PowerMode", powerMode))
                    {
                        BMCWEB_LOG_ERROR
                            << "Illegal Property "
                            << nvidiaObj->dump(
                                   2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
                        return;
                    }

                    // Update power Mode
                    if (powerMode)
                    {
                        const std::array<const char*, 1> interfaces = {
                            "xyz.openbmc_project.Inventory.Item.Chassis"};

                        crow::connections::systemBus->async_method_call(
                            [asyncResp, chassisId, powerMode](
                                const boost::system::error_code ec,
                                const crow::openbmc_mapper::GetSubTreeType&
                                    subtree) {
                                if (ec)
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }

                                // Iterate over all retrieved ObjectPaths.
                                for (const std::pair<
                                         std::string,
                                         std::vector<std::pair<
                                             std::string,
                                             std::vector<std::string>>>>&
                                         object : subtree)
                                {
                                    const std::string& path = object.first;
                                    const std::vector<std::pair<
                                        std::string, std::vector<std::string>>>&
                                        connectionNames = object.second;

                                    sdbusplus::message::object_path objPath(
                                        path);
                                    if (objPath.filename() != chassisId)
                                    {
                                        continue;
                                    }

                                    if (connectionNames.size() < 1)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Got 0 Connection names";
                                        continue;
                                    }

                                    const std::string& connectionName =
                                        connectionNames[0].first;
                                    const std::vector<std::string>& interfaces =
                                        connectionNames[0].second;

                                    if (std::find(
                                            interfaces.begin(),
                                            interfaces.end(),
                                            "xyz.openbmc_project.Control.Power.Mode") !=
                                        interfaces.end())
                                    {
                                        std::string resourceType = "Chassis";
                                        patchPowerMode(asyncResp, chassisId,
                                                       *powerMode, objPath,
                                                       connectionName,
                                                       resourceType);
                                    }
                                    return;
                                }

                                messages::resourceNotFound(
                                    asyncResp->res, "#Chassis.v1_15_0.Chassis",
                                    chassisId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            "/xyz/openbmc_project/inventory", 0, interfaces);
                    }
                }
            }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
        });
}

inline double joulesToKwh(const double& joules)
{
    const double jtoKwhFactor = 2.77777778e-7;
    return jtoKwhFactor * joules;
}

inline void getSensorDataByService(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& service,
    const std::string& chassisId, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get sensor data.";
    using PropertyType =
        std::variant<std::string, double, uint64_t, std::vector<std::string>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;
    crow::connections::systemBus->async_method_call(
        [aResp, chassisId, objPath](const boost::system::error_code ec,
                                    const PropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Can't get sensor reading";
                messages::internalError(aResp->res);
                return;
            }
            for (const auto& property : properties)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Value")
                {
                    const double* attributeValue =
                        std::get_if<double>(&property.second);
                    // Take relevant code from sensors
                    std::vector<std::string> split;
                    // Reserve space for
                    // /xyz/openbmc_project/sensors/<name>/<subname>
                    split.reserve(6);
                    boost::algorithm::split(split, objPath,
                                            boost::is_any_of("/"));
                    if (split.size() < 6)
                    {
                        BMCWEB_LOG_ERROR << "Got path that isn't long enough "
                                         << objPath;
                        return;
                    }
                    // These indexes aren't intuitive, as boost::split puts an
                    // empty string at the beginning
                    const std::string& sensorType = split[4];
                    const std::string& sensorName = split[5];
                    BMCWEB_LOG_DEBUG << "sensorName " << sensorName
                                     << " sensorType " << sensorType;

                    std::string sensorURI =
                        (boost::format("/redfish/v1/Chassis/%s/Sensors/%s") %
                         chassisId % sensorName)
                            .str();
                    if (sensorType == "temperature")
                    {
                        aResp->res.jsonValue["TemperatureCelsius"] = {
                            {"Reading", *attributeValue},
                            {"DataSourceUri", sensorURI},
                        };
                    }
                    else if (sensorType == "power")
                    {
                        aResp->res.jsonValue["PowerWatts"] = {
                            {"Reading", *attributeValue},
                            {"DataSourceUri", sensorURI},
                        };
                    }
                    else if (sensorType == "energy")
                    {
                        aResp->res.jsonValue["EnergykWh"] = {
                            {"Reading", joulesToKwh(*attributeValue)},
                        };
                        aResp->res.jsonValue["EnergyJoules"] = {
                            {"Reading", *attributeValue},
                            {"DataSourceUri", sensorURI},
                        };
                    }
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

inline void getEnvironmentMetricsDataByService(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& service,
    const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get environment metrics data.";
    // Get parent chassis for sensors URI
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
                [aResp, service,
                 chassisId](const boost::system::error_code& e,
                            std::variant<std::vector<std::string>>& resp) {
                    if (e)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    std::vector<std::string>* data =
                        std::get_if<std::vector<std::string>>(&resp);
                    if (data == nullptr)
                    {
                        return;
                    }
                    for (const std::string& sensorPath : *data)
                    {
                        getSensorDataByService(aResp, service, chassisId,
                                               sensorPath);
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

inline void
    getProcessorEnvironmentMetricsData(std::shared_ptr<bmcweb::AsyncResp> aResp,
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
                for (const auto& [service, interfaces] : object)
                {
                    if (std::find(
                            interfaces.begin(), interfaces.end(),
                            "xyz.openbmc_project.Inventory.Decorator.PowerLimit") !=
                        interfaces.end())
                    {
                        getPowerLimits(aResp, service, path);
                    }
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Control.Power.Cap") !=
                        interfaces.end())
                    {
                        getPowerCap(aResp, service, path);
                    }
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Control.Mode") !=
                        interfaces.end())
                    {
                        getControlMode(aResp, service, path);
                    }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "com.nvidia.Edpp") != interfaces.end())
                    {
                        getEDPpData(aResp, service, path);
                    }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

                    getEnvironmentMetricsDataByService(aResp, service, path);
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

inline void requestRoutesProcessorEnvironmentMetrics(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Processors/<str>/EnvironmentMetrics")
        .privileges(redfish::privileges::getProcessor)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& processorId) {
            std::string envMetricsURI =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
            envMetricsURI += processorId;
            envMetricsURI += "/EnvironmentMetrics";
            asyncResp->res.jsonValue["@odata.type"] =
                "#EnvironmentMetrics.v1_2_0.EnvironmentMetrics";
            asyncResp->res.jsonValue["@odata.id"] = envMetricsURI;
            asyncResp->res.jsonValue["Id"] = "Environment Metrics";
            asyncResp->res.jsonValue["Name"] =
                processorId + " Environment Metrics";
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            asyncResp->res.jsonValue["Actions"]["Oem"]["Nvidia"]
                                    ["#NvidiaEnvironmentMetrics.ResetEDPp"] = {
                {"target",
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/" +
                     processorId +
                     "/EnvironmentMetrics/Actions/Oem/NvidiaEnvironmentMetrics.ResetEDPp"}};
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            getProcessorEnvironmentMetricsData(asyncResp, processorId);
        });

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Processors/<str>/EnvironmentMetrics")
        .privileges(redfish::privileges::patchProcessor)
        .methods(
            boost::beast::http::verb::
                patch)([](const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& processorId) {
            std::optional<nlohmann::json> powerLimit;
            std::optional<nlohmann::json> oemObject;

            // Read json request
            if (!json_util::readJson(req, asyncResp->res, "PowerLimitWatts",
                                     powerLimit, "Oem", oemObject))
            {
                return;
            }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            // update Edpp Setpoint
            if (std::optional<nlohmann::json> oemNvidiaObject;
                oemObject && json_util::readJson(*oemObject, asyncResp->res,
                                                 "Nvidia", oemNvidiaObject))
            {
                if (std::optional<nlohmann::json> EdppObject;
                    oemNvidiaObject &&
                    json_util::readJson(*oemNvidiaObject, asyncResp->res,
                                        "EDPpPercent", EdppObject))
                {
                    if (EdppObject)
                    {
                        std::optional<size_t> setPoint;
                        std::optional<bool> persistency;

                        if (!json_util::readJson(*EdppObject, asyncResp->res,
                                                 "SetPoint", setPoint,
                                                 "Persistency", persistency))
                        {
                            BMCWEB_LOG_ERROR
                                << "Cannot read values from Edpp tag";
                            return;
                        }

                        if (setPoint && persistency)
                        {
                            redfish::processor_utils::getProcessorObject(
                                asyncResp, processorId,
                                [setPoint, persistency](
                                    const std::shared_ptr<bmcweb::AsyncResp>&
                                        asyncResp,
                                    const std::string& processorId,
                                    const std::string& objectPath,
                                    const MapperServiceMap& serviceMap) {
                                    patchEdppSetPoint(asyncResp, processorId,
                                                      *setPoint, *persistency,
                                                      objectPath, serviceMap);
                                });
                        }
                        else if (setPoint)
                        {
                            redfish::processor_utils::getProcessorObject(
                                asyncResp, processorId,
                                [setPoint](
                                    const std::shared_ptr<bmcweb::AsyncResp>&
                                        asyncResp,
                                    const std::string& processorId,
                                    const std::string& objectPath,
                                    const MapperServiceMap& serviceMap) {
                                    patchEdppSetPoint(asyncResp, processorId,
                                                      *setPoint, false,
                                                      objectPath, serviceMap);
                                });
                        }
                    }
                }
            }

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            // Update power limit
            if (powerLimit)
            {
                std::optional<int> setPoint;
                if (json_util::readJson(*powerLimit, asyncResp->res,
                                                        "SetPoint", setPoint))
                {
                    const std::array<const char*, 2> interfaces = {
                        "xyz.openbmc_project.Inventory.Item.Cpu",
                        "xyz.openbmc_project.Inventory.Item.Accelerator"};

                    crow::connections::systemBus->async_method_call(
                        [asyncResp, processorId, setPoint](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                        if (ec)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        // Iterate over all retrieved ObjectPaths.
                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            const std::string& path = object.first;
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                connectionNames = object.second;

                            sdbusplus::message::object_path objPath(path);
                            if (objPath.filename() != processorId)
                            {
                                continue;
                            }

                            if (connectionNames.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 Connection names";
                                continue;
                            }

                            const std::string& connectionName =
                                connectionNames[0].first;
                            const std::vector<std::string>& interfaces =
                                connectionNames[0].second;

                            if (std::find(
                                    interfaces.begin(), interfaces.end(),
                                    "xyz.openbmc_project.Control.Power.Cap") !=
                                interfaces.end())
                            {
                                std::string resourceType = "Processors";
                                patchPowerLimit(asyncResp, processorId,
                                                *setPoint, objPath,
                                                connectionName, resourceType);
                            }

                            return;
                        }

                        messages::resourceNotFound(
                            asyncResp->res, "#Processor.v1_13_0.Processor",
                            processorId);
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                    "/xyz/openbmc_project/inventory", 0, interfaces);
                }
            }
        });
}

inline void
    getMemoryEnvironmentMetricsData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                    const std::string& dimmId)
{
    BMCWEB_LOG_DEBUG << "Get available system memory resource";
    crow::connections::systemBus->async_method_call(
        [dimmId, aResp{std::move(aResp)}](
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
                if (!boost::ends_with(path, dimmId))
                {
                    continue;
                }
                for (const auto& [service, interfaces] : object)
                {
                    getEnvironmentMetricsDataByService(aResp, service, path);
                }
                return;
            }
            // Object not found
            messages::resourceNotFound(
                aResp->res, "#EnvironmentMetrics.v1_2_0.EnvironmentMetrics",
                dimmId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Dimm"});
}

inline void requestRoutesMemoryEnvironmentMetrics(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Memory/<str>/EnvironmentMetrics")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& dimmId) {
                std::string envMetricsURI =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory/";
                envMetricsURI += dimmId;
                envMetricsURI += "/EnvironmentMetrics";
                asyncResp->res.jsonValue["@odata.type"] =
                    "#EnvironmentMetrics.v1_2_0.EnvironmentMetrics";
                asyncResp->res.jsonValue["@odata.id"] = envMetricsURI;
                asyncResp->res.jsonValue["Id"] = "EnvironmentMetrics";
                asyncResp->res.jsonValue["Name"] =
                    dimmId + " Environment Metrics";
                getMemoryEnvironmentMetricsData(asyncResp, dimmId);
            });
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void postEdppReset(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                          const std::string& processorId,
                          const std::string& cpuObjectPath,
                          const MapperServiceMap& serviceMap)
{

    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.Edpp") != interfaceList.end())
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

    // Call Edpp Reset Method
    crow::connections::systemBus->async_method_call(
        [resp, processorId](boost::system::error_code ec, const int retValue) {
            if (!ec)
            {

                if (retValue != 0)
                {
                    BMCWEB_LOG_ERROR << retValue;
                    messages::operationFailed(resp->res);
                }
                BMCWEB_LOG_DEBUG << "CPU:" << processorId
                                 << " Edpp Reset Succeded";
                messages::success(resp->res);
                return;
            }
            BMCWEB_LOG_DEBUG << ec;
            messages::internalError(resp->res);
            return;
        },
        *inventoryService, cpuObjectPath, "com.nvidia.Edpp", "Reset");
}

inline void requestRoutesEdppReset(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(
        app,
        "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/<str>/"
        "EnvironmentMetrics/Actions/Oem/NvidiaEnvironmentMetrics.ResetEDPp")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                redfish::processor_utils::getProcessorObject(
                    asyncResp, processorId,
                    [](const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& processorId,
                       const std::string& objectPath,
                       const MapperServiceMap& serviceMap) {
                        postEdppReset(asyncResp, processorId, objectPath,
                                      serviceMap);
                    });
            });
}

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

} // namespace redfish
