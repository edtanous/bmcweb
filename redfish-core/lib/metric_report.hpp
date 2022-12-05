#pragma once

#include "bmcweb_config.h"

#include "thermal_metrics.hpp"
#include "utils/collection.hpp"
#include "utils/telemetry_utils.hpp"
#include "utils/time_utils.hpp"

#include <app.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>

#include <chrono>

namespace redfish
{

namespace telemetry
{
constexpr const char* metricReportDefinitionUriStr =
    "/redfish/v1/TelemetryService/MetricReportDefinitions";
constexpr const char* metricReportUri =
    "/redfish/v1/TelemetryService/MetricReports";

using Readings =
    std::vector<std::tuple<std::string, std::string, double, uint64_t>>;
using TimestampReadings = std::tuple<uint64_t, Readings>;

inline nlohmann::json toMetricValues(const Readings& readings)
{
    nlohmann::json metricValues = nlohmann::json::array_t();

    for (const auto& [id, metadata, sensorValue, timestamp] : readings)
    {
        metricValues.push_back({
            {"MetricId", id},
            {"MetricProperty", metadata},
            {"MetricValue", std::to_string(sensorValue)},
            {"Timestamp", redfish::time_utils::getDateTimeUintMs(timestamp)},
        });
    }

    return metricValues;
}

inline bool fillReport(nlohmann::json& json, const std::string& id,
                       const TimestampReadings& timestampReadings)
{
    json["@odata.type"] = "#MetricReport.v1_3_0.MetricReport";
    json["@odata.id"] =
        crow::utility::urlFromPieces("redfish", "v1", "TelemetryService",
                                     "MetricReports", id)
            .string();
    json["Id"] = id;
    json["Name"] = id;
    json["MetricReportDefinition"]["@odata.id"] =
        crow::utility::urlFromPieces("redfish", "v1", "TelemetryService",
                                     "MetricReportDefinitions", id)
            .string();

    const auto& [timestamp, readings] = timestampReadings;
    json["Timestamp"] = redfish::time_utils::getDateTimeUintMs(timestamp);
    json["MetricValues"] = toMetricValues(readings);
    return true;
}
} // namespace telemetry

inline void requestRoutesMetricReportCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReports/")
        .privileges(redfish::privileges::getMetricReportCollection)
        .methods(
            boost::beast::http::verb::
                get)([](const crow::Request&,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            asyncResp->res.jsonValue["@odata.type"] =
                "#MetricReportCollection.MetricReportCollection";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/TelemetryService/MetricReports";
            asyncResp->res.jsonValue["Name"] = "Metric Report Collection";
#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
            nlohmann::json& addMembers = asyncResp->res.jsonValue["Members"];
            addMembers.push_back(
                {{"@odata.id",
                  "/redfish/v1/TelemetryService/MetricReports/" PLATFORMMETRICSID}});
            asyncResp->res.jsonValue["Members@odata.count"] = addMembers.size();
            return;
#endif
            const std::vector<const char*> interfaces{
                telemetry::reportInterface};
            collection_util::getCollectionMembers(
                asyncResp, telemetry::metricReportUri, interfaces,
                "/xyz/openbmc_project/Telemetry/Reports/TelemetryService");
        });
}
#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
inline void getSensorMap(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& serviceName,
                         const std::string& objectPath,
                         const uint32_t& staleSensorUpperLimit,
                         const uint64_t& requestTimestamp)
{

    using sensorMap = std::map<
        std::string,
        std::tuple<std::variant<std::string, int, int16_t, int64_t, uint16_t,
                                uint32_t, uint64_t, double, bool>,
                   uint64_t, sdbusplus::message::object_path>>;

    sdbusplus::asio::getProperty<sensorMap>(
        *crow::connections::systemBus, serviceName, objectPath,
        "xyz.openbmc_project.Sensor.Aggregation", "SensorMetrics",
        [asyncResp, staleSensorUpperLimit,
         requestTimestamp](const boost::system::error_code ec,
                           const sensorMap& sensorMetrics) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(asyncResp->res);
                return;
            }
            nlohmann::json& resArray = asyncResp->res.jsonValue["MetricValues"];

            for (const auto& i : sensorMetrics)
            {
                nlohmann::json thisMetric = nlohmann::json::object();
                std::string sensorName = i.first;
                auto data = i.second;
                auto var = std::get<0>(data);
                const double* reading = std::get_if<double>(&var);
                thisMetric["MetricValue"] = std::to_string(*reading);
                // sensorUpdatetimeSteadyClock is in ms
                const uint64_t sensorUpdatetimeSteadyClock = std::get<1>(data);
                /*
                the complex code here converts sensorUpdatetimeSteadyClock
                from std::chrono::steady_clock to std::chrono::system_clock
                */
                const uint64_t sensorUpdatetimeSystemClock =
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()) -
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count()) +
                    sensorUpdatetimeSteadyClock;
                thisMetric["Timestamp"] = redfish::time_utils::getDateTimeUintMs(
                    sensorUpdatetimeSystemClock);
                sdbusplus::message::object_path chassisPath = std::get<2>(data);
                std::string sensorUri = "/redfish/v1/Chassis/";
                sensorUri += chassisPath.filename();
                sensorUri += "/Sensors/";
                sensorUri += sensorName;
                thisMetric["MetricProperty"] = sensorUri;
                thisMetric["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaMetricReport.v1_0_0.NvidiaMetricReport";
                thisMetric["Oem"]["Nvidia"]["MetricValueStale"] = true;
                if (requestTimestamp != 0 && thisMetric["MetricValue"] != "nan")
                {
                    // Note: requestTimestamp, sensorUpdatetimeSteadyClock uses
                    // steadyclock to calculate time
                    int64_t freshness = static_cast<int64_t>(
                        requestTimestamp - sensorUpdatetimeSteadyClock);

                    if (freshness <= staleSensorUpperLimit)
                    {
                        thisMetric["Oem"]["Nvidia"]["MetricValueStale"] = false;
                    }
                }
                resArray.push_back(thisMetric);
            }
        });
}

inline void getPlatforMetricsFromSensorMap(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const uint64_t& requestTimestamp = 0)
{
    using MapperServiceMap =
        std::vector<std::pair<std::string, std::vector<std::string>>>;

    // Map of object paths to MapperServiceMaps
    using MapperGetSubTreeResponse =
        std::vector<std::pair<std::string, MapperServiceMap>>;

    crow::connections::systemBus->async_method_call(
        [asyncResp,
         requestTimestamp](boost::system::error_code ec,
                           const MapperGetSubTreeResponse& subtree) mutable {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] =
                "#MetricReport.v1_3_0.MetricReport";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/TelemetryService/MetricReports/" PLATFORMMETRICSID;
            asyncResp->res.jsonValue["Id"] = PLATFORMMETRICSID;
            asyncResp->res.jsonValue["Name"] = PLATFORMMETRICSID;
            asyncResp->res.jsonValue["MetricReportDefinition"]["@odata.id"] =
                telemetry::metricReportDefinitionUriStr +
                std::string("/" PLATFORMMETRICSID);
            asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                "#NvidiaMetricReport.v1_0_0.NvidiaMetricReport";
            asyncResp->res
                .jsonValue["Oem"]["Nvidia"]["SensingIntervalMilliseconds"] =
                pmSensingInterval;
            asyncResp->res.jsonValue["MetricValues"] = nlohmann::json::array();
            for (const auto& [path, serviceMap] : subtree)
            {
                const std::string objectPath = path;
                for (const auto& [conName, interfaceList] : serviceMap)
                {
                    const std::string serviceName = conName;
                    sdbusplus::asio::getProperty<uint32_t>(
                        *crow::connections::systemBus, serviceName, objectPath,
                        "xyz.openbmc_project.Sensor.Aggregation",
                        "StaleSensorUpperLimitms",
                        [asyncResp, objectPath, serviceName, requestTimestamp](
                            const boost::system::error_code ec1,
                            const uint32_t& staleSensorUpperLimit) {
                            if (ec1)
                            {
                                BMCWEB_LOG_DEBUG << "DBUS response error";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            getSensorMap(asyncResp, serviceName, objectPath,
                                         staleSensorUpperLimit,
                                         requestTimestamp);
                        });
                }
                return;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Sensor.Aggregation"});
}

inline void
    getPlatformMetrics(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& chassisId,
                       const uint64_t& requestTimestamp = 0)
{
    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Chassis"};
    auto respHandler = [asyncResp, requestTimestamp, chassisId](
                           const boost::system::error_code ec,
                           const std::vector<std::string>& chassisPaths) {
        if (ec)
        {
            BMCWEB_LOG_ERROR << "getPlatformMetrics respHandler DBUS error: "
                             << ec;
            messages::internalError(asyncResp->res);
            return;
        }

        for (const std::string& chassisPath : chassisPaths)
        {
            sdbusplus::message::object_path path(chassisPath);
            const std::string& chassisName = path.filename();
            if (chassisName.empty())
            {
                BMCWEB_LOG_ERROR << "Failed to find '/' in " << chassisPath;
                continue;
            }
            if (chassisName != chassisId)
            {
                continue;
            }
            asyncResp->res.jsonValue["@odata.type"] =
                "#MetricReport.v1_3_0.MetricReport";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/TelemetryService/MetricReports/" PLATFORMMETRICSID;
            asyncResp->res.jsonValue["Id"] = PLATFORMMETRICSID;
            asyncResp->res.jsonValue["Name"] = PLATFORMMETRICSID;
            asyncResp->res.jsonValue["MetricReportDefinition"]["@odata.id"] =
                telemetry::metricReportDefinitionUriStr +
                std::string("/" PLATFORMMETRICSID);
            asyncResp->res.jsonValue["MetricValues"] = nlohmann::json::array();
            // Identify sensor services for sensor readings
            processSensorServices(asyncResp, chassisPath, "all",
                                  pmSensingInterval, requestTimestamp);
            return;
        }
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
    };
    // Get the Chassis Collection
    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

inline void requestRoutesPlatformMetricReport(App& app)
{
    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReports/" PLATFORMMETRICSID
                 "/")
        .privileges(redfish::privileges::getMetricReport)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }

                // get current timestamp, to determine the staleness
                const uint64_t requestTimestamp = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
                BMCWEB_LOG_DEBUG << "Request submitted at" << requestTimestamp;
                getPlatforMetricsFromSensorMap(asyncResp, requestTimestamp);
            });
}
#endif

inline void requestRoutesMetricReport(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReports/<str>/")
        .privileges(redfish::privileges::getMetricReport)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& id) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                const std::string reportPath = telemetry::getDbusReportPath(id);
                crow::connections::systemBus->async_method_call(
                    [asyncResp, id,
                     reportPath](const boost::system::error_code& ec) {
                        if (ec.value() == EBADR ||
                            ec == boost::system::errc::host_unreachable)
                        {
                            messages::resourceNotFound(asyncResp->res,
                                                       "MetricReport", id);
                            return;
                        }
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "respHandler DBus error " << ec;
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        sdbusplus::asio::getProperty<
                            telemetry::TimestampReadings>(
                            *crow::connections::systemBus, telemetry::service,
                            reportPath, telemetry::reportInterface, "Readings",
                            [asyncResp,
                             id](const boost::system::error_code ec2,
                                 const telemetry::TimestampReadings& ret) {
                                if (ec2)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "respHandler DBus error " << ec2;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }

                                telemetry::fillReport(asyncResp->res.jsonValue,
                                                      id, ret);
                            });
                    },
                    telemetry::service, reportPath, telemetry::reportInterface,
                    "Update");
            });
}
} // namespace redfish
