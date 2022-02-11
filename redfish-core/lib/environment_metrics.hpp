#pragma once

#include <app.hpp>
#include <boost/format.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/json_utils.hpp>

namespace redfish
{
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

inline void getPowerWatts(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisID)
{
    const std::array<const char*, 1> totalPowerInterfaces = {
        "xyz.openbmc_project.Sensor.Value"};
    const std::string& totalPowerPath =
        "/xyz/openbmc_project/sensors/power/Total_Power";
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisID, totalPowerPath](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                object) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(asyncResp->res);
                return;
            }
            for (const auto& tempObject : object)
            {
                const std::string& connectionName = tempObject.first;
                crow::connections::systemBus->async_method_call(
                    [asyncResp, chassisID](const boost::system::error_code ec,
                                           const std::variant<double>& value) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "Can't get Power Watts!";
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
                            "/redfish/v1/Chassis/" + chassisID + "/Sensors/";
                        asyncResp->res.jsonValue["PowerWatts"] = {
                            {"Reading", *attributeValue},
                            {"DataSourceUri", tempPath + "Total_Power"},
                            {"@odata.id", tempPath + "Total_Power"}};
                    },
                    connectionName, totalPowerPath,
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Sensor.Value", "Value");
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", totalPowerPath,
        totalPowerInterfaces);
}

inline void
    getEnvironmentMetrics(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisID)
{
    BMCWEB_LOG_DEBUG
        << "Get properties for EnvironmentMetrics associated to chassis = "
        << chassisID;
    asyncResp->res.jsonValue["@odata.type"] =
        "#EnvironmentMetrics.v1_0_0.EnvironmentMetrics";
    asyncResp->res.jsonValue["Name"] = "Chassis Environment Metrics";
    asyncResp->res.jsonValue["Id"] = "EnvironmentMetrics";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/Chassis/" + chassisID + "/EnvironmentMetrics";
    getfanSpeedsPercent(asyncResp, chassisID);
    getPowerWatts(asyncResp, chassisID);
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
}

inline void getSensorDataByService(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& service,
    const std::string& chassisId, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get sensor data.";
    crow::connections::systemBus->async_method_call(
        [aResp, chassisId, objPath](const boost::system::error_code ec,
                                    const std::variant<double>& value) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Can't get sensor reading";
                messages::internalError(aResp->res);
                return;
            }

            const double* attributeValue = std::get_if<double>(&value);
            if (attributeValue == nullptr)
            {
                // illegal property
                messages::internalError(aResp->res);
                return;
            }

            // Take relevant code from sensors
            std::vector<std::string> split;
            // Reserve space for
            // /xyz/openbmc_project/sensors/<name>/<subname>
            split.reserve(6);
            boost::algorithm::split(split, objPath, boost::is_any_of("/"));
            if (split.size() < 6)
            {
                BMCWEB_LOG_ERROR << "Got path that isn't long enough "
                                 << objPath;
                return;
            }
            // These indexes aren't intuitive, as boost::split puts an empty
            // string at the beginning
            const std::string& sensorType = split[4];
            const std::string& sensorName = split[5];
            BMCWEB_LOG_DEBUG << "sensorName " << sensorName << " sensorType "
                             << sensorType;

            std::string sensorURI =
                (boost::format("/redfish/v1/Chassis/%s/Sensors/%s") %
                 chassisId % sensorName)
                    .str();
            if (sensorType == "temperature")
            {
                aResp->res.jsonValue["TemperatureCelsius"] = {
                    {"Reading", *attributeValue},
                    {"DataSourceUri", sensorURI},
                    {"@odata.id", sensorURI}};
            }
            else if (sensorType == "power")
            {
                aResp->res.jsonValue["PowerWatts"] = {
                    {"Reading", *attributeValue},
                    {"DataSourceUri", sensorURI},
                    {"@odata.id", sensorURI}};
            }
            else if (sensorType == "energy")
            {
                aResp->res.jsonValue["EnergykWh"] = {
                    {"Reading", *attributeValue},
                    {"DataSourceUri", sensorURI},
                    {"@odata.id", sensorURI}};
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Sensor.Value", "Value");
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
                "xyz.openbmc_project.ObjectMapper", objPath + "/all_sensors",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
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
                    getEnvironmentMetricsDataByService(aResp, service, path);
                }
                return;
            }
            // Object not found
            messages::resourceNotFound(
                aResp->res, "#EnvironmentMetrics.v1_0_0.EnvironmentMetrics",
                processorId);
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
    BMCWEB_ROUTE(
        app, "/redfish/v1/Systems/system/Processors/<str>/EnvironmentMetrics")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& processorId) {
                std::string envMetricsURI =
                    "/redfish/v1/Systems/system/Processors/";
                envMetricsURI += processorId;
                envMetricsURI += "/EnvironmentMetrics";
                asyncResp->res.jsonValue["@odata.type"] =
                    "#EnvironmentMetrics.v1_0_0.EnvironmentMetrics";
                asyncResp->res.jsonValue["@odata.id"] = envMetricsURI;
                asyncResp->res.jsonValue["Id"] = "Environment Metrics";
                asyncResp->res.jsonValue["Name"] =
                    processorId + " Environment Metrics";
                getProcessorEnvironmentMetricsData(asyncResp, processorId);
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
                aResp->res, "#EnvironmentMetrics.v1_0_0.EnvironmentMetrics",
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
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/system/Memory/<str>/EnvironmentMetrics")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& dimmId) {
                std::string envMetricsURI =
                    "/redfish/v1/Systems/system/Memory/";
                envMetricsURI += dimmId;
                envMetricsURI += "/EnvironmentMetrics";
                asyncResp->res.jsonValue["@odata.type"] =
                    "#EnvironmentMetrics.v1_0_0.EnvironmentMetrics";
                asyncResp->res.jsonValue["@odata.id"] = envMetricsURI;
                asyncResp->res.jsonValue["Id"] = "EnvironmentMetrics";
                asyncResp->res.jsonValue["Name"] =
                    dimmId + " Environment Metrics";
                getMemoryEnvironmentMetricsData(asyncResp, dimmId);
            });
}

} // namespace redfish