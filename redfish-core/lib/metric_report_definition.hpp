#pragma once

#include "sensors.hpp"
#include "utils/telemetry_utils.hpp"
#include "utils/time_utils.hpp"

#include <app.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <utils/dbus_utils.hpp>

#include <map>
#include <tuple>
#include <variant>

namespace redfish
{

namespace telemetry
{

constexpr const char* metricReportDefinitionUri =
    "/redfish/v1/TelemetryService/MetricReportDefinitions";

using ReadingParameters =
    std::vector<std::tuple<sdbusplus::message::object_path, std::string,
                           std::string, std::string>>;

inline void
    fillReportDefinition(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& id,
                         const dbus::utility::DBusPropertiesMap& ret)
{
    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinition.v1_3_0.MetricReportDefinition";
    asyncResp->res.jsonValue["@odata.id"] =
        crow::utility::urlFromPieces("redfish", "v1", "TelemetryService",
                                     "MetricReportDefinitions", id)
            .string();
    asyncResp->res.jsonValue["Id"] = id;
    asyncResp->res.jsonValue["Name"] = id;
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] =
        crow::utility::urlFromPieces("redfish", "v1", "TelemetryService",
                                     "MetricReports", id)
            .string();
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    asyncResp->res.jsonValue["ReportUpdates"] = "Overwrite";

    const bool* emitsReadingsUpdate = nullptr;
    const bool* logToMetricReportsCollection = nullptr;
    const ReadingParameters* readingParameters = nullptr;
    const std::string* reportingType = nullptr;
    const uint64_t* interval = nullptr;

    const bool success = sdbusplus::unpackPropertiesNoThrow(
        dbus_utils::UnpackErrorPrinter(), ret, "EmitsReadingsUpdate",
        emitsReadingsUpdate, "LogToMetricReportsCollection",
        logToMetricReportsCollection, "ReadingParameters", readingParameters,
        "ReportingType", reportingType, "Interval", interval);

    if (!success)
    {
        messages::internalError(asyncResp->res);
        return;
    }

    std::vector<std::string> redfishReportActions;
    redfishReportActions.reserve(2);
    if (emitsReadingsUpdate != nullptr && *emitsReadingsUpdate)
    {
        redfishReportActions.emplace_back("RedfishEvent");
    }

    if (logToMetricReportsCollection != nullptr &&
        *logToMetricReportsCollection)
    {
        redfishReportActions.emplace_back("LogToMetricReportsCollection");
    }

    nlohmann::json metrics = nlohmann::json::array();
    if (readingParameters != nullptr)
    {
        for (const auto& [sensorPath, operationType, metricId, metadata] :
             *readingParameters)
        {
            metrics.push_back({
                {"MetricId", metricId},
                {"MetricProperties", {metadata}},
            });
        }
    }

    if (reportingType != nullptr)
    {
        asyncResp->res.jsonValue["MetricReportDefinitionType"] = *reportingType;
    }

    if (interval != nullptr)
    {
        asyncResp->res.jsonValue["Schedule"]["RecurrenceInterval"] =
            time_utils::toDurationString(std::chrono::milliseconds(*interval));
    }

    asyncResp->res.jsonValue["Metrics"] = metrics;
    asyncResp->res.jsonValue["ReportActions"] = redfishReportActions;
}

struct AddReportArgs
{
    std::string name;
    std::string reportingType;
    bool emitsReadingsUpdate = false;
    bool logToMetricReportsCollection = false;
    uint64_t interval = 0;
    std::vector<std::pair<std::string, std::vector<std::string>>> metrics;
};

inline bool toDbusReportActions(crow::Response& res,
                                std::vector<std::string>& actions,
                                AddReportArgs& args)
{
    size_t index = 0;
    for (auto& action : actions)
    {
        if (action == "RedfishEvent")
        {
            args.emitsReadingsUpdate = true;
        }
        else if (action == "LogToMetricReportsCollection")
        {
            args.logToMetricReportsCollection = true;
        }
        else
        {
            messages::propertyValueNotInList(
                res, action, "ReportActions/" + std::to_string(index));
            return false;
        }
        index++;
    }
    return true;
}

inline bool getUserParameters(crow::Response& res, const crow::Request& req,
                              AddReportArgs& args)
{
    std::vector<nlohmann::json> metrics;
    std::vector<std::string> reportActions;
    std::optional<nlohmann::json> schedule;
    if (!json_util::readJsonPatch(req, res, "Id", args.name, "Metrics", metrics,
                                  "MetricReportDefinitionType",
                                  args.reportingType, "ReportActions",
                                  reportActions, "Schedule", schedule))
    {
        return false;
    }

    constexpr const char* allowedCharactersInName =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    if (args.name.empty() || args.name.find_first_not_of(
                                 allowedCharactersInName) != std::string::npos)
    {
        BMCWEB_LOG_ERROR << "Failed to match " << args.name
                         << " with allowed character "
                         << allowedCharactersInName;
        messages::propertyValueIncorrect(res, "Id", args.name);
        return false;
    }

    if (args.reportingType != "Periodic" && args.reportingType != "OnRequest")
    {
        messages::propertyValueNotInList(res, args.reportingType,
                                         "MetricReportDefinitionType");
        return false;
    }

    if (!toDbusReportActions(res, reportActions, args))
    {
        return false;
    }

    if (args.reportingType == "Periodic")
    {
        if (!schedule)
        {
            messages::createFailedMissingReqProperties(res, "Schedule");
            return false;
        }

        std::string durationStr;
        if (!json_util::readJson(*schedule, res, "RecurrenceInterval",
                                 durationStr))
        {
            return false;
        }

        std::optional<std::chrono::milliseconds> durationNum =
            time_utils::fromDurationString(durationStr);
        if (!durationNum)
        {
            messages::propertyValueIncorrect(res, "RecurrenceInterval",
                                             durationStr);
            return false;
        }
        args.interval = static_cast<uint64_t>(durationNum->count());
    }

    args.metrics.reserve(metrics.size());
    for (auto& m : metrics)
    {
        std::string id;
        std::vector<std::string> uris;
        if (!json_util::readJson(m, res, "MetricId", id, "MetricProperties",
                                 uris))
        {
            return false;
        }

        args.metrics.emplace_back(std::move(id), std::move(uris));
    }

    return true;
}

inline bool getChassisSensorNodeFromMetrics(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        metrics,
    boost::container::flat_set<std::pair<std::string, std::string>>& matched)
{
    for (const auto& metric : metrics)
    {
        const std::vector<std::string>& uris = metric.second;

        std::optional<IncorrectMetricUri> error =
            getChassisSensorNode(uris, matched);
        if (error)
        {
            messages::propertyValueIncorrect(asyncResp->res, error->uri,
                                             "MetricProperties/" +
                                                 std::to_string(error->index));
            return false;
        }
    }
    return true;
}

class AddReport
{
  public:
    AddReport(AddReportArgs argsIn,
              const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn) :
        asyncResp(asyncRespIn),
        args{std::move(argsIn)}
    {}
    ~AddReport()
    {
        if (asyncResp->res.result() != boost::beast::http::status::ok)
        {
            return;
        }

        telemetry::ReadingParameters readingParams;
        readingParams.reserve(args.metrics.size());

        for (const auto& [id, uris] : args.metrics)
        {
            for (size_t i = 0; i < uris.size(); i++)
            {
                const std::string& uri = uris[i];
                auto el = uriToDbus.find(uri);
                if (el == uriToDbus.end())
                {
                    BMCWEB_LOG_ERROR
                        << "Failed to find DBus sensor corresponding to URI "
                        << uri;
                    messages::propertyValueNotInList(asyncResp->res, uri,
                                                     "MetricProperties/" +
                                                         std::to_string(i));
                    return;
                }

                const std::string& dbusPath = el->second;
                readingParams.emplace_back(dbusPath, "SINGLE", id, uri);
            }
        }
        const std::shared_ptr<bmcweb::AsyncResp> aResp = asyncResp;
        crow::connections::systemBus->async_method_call(
            [aResp, name = args.name, uriToDbus = std::move(uriToDbus)](
                const boost::system::error_code ec, const std::string&) {
                if (ec == boost::system::errc::file_exists)
                {
                    messages::resourceAlreadyExists(
                        aResp->res, "MetricReportDefinition", "Id", name);
                    return;
                }
                if (ec == boost::system::errc::too_many_files_open)
                {
                    messages::createLimitReachedForResource(aResp->res);
                    return;
                }
                if (ec == boost::system::errc::argument_list_too_long)
                {
                    nlohmann::json metricProperties = nlohmann::json::array();
                    for (const auto& [uri, _] : uriToDbus)
                    {
                        metricProperties.emplace_back(uri);
                    }
                    messages::propertyValueIncorrect(aResp->res,
                                                     metricProperties.dump(),
                                                     "MetricProperties");
                    return;
                }
                if (ec)
                {
                    messages::internalError(aResp->res);
                    BMCWEB_LOG_ERROR << "respHandler DBus error " << ec;
                    return;
                }

                messages::created(aResp->res);
            },
            telemetry::service, "/xyz/openbmc_project/Telemetry/Reports",
            "xyz.openbmc_project.Telemetry.ReportManager", "AddReport",
            "TelemetryService/" + args.name, args.reportingType,
            args.emitsReadingsUpdate, args.logToMetricReportsCollection,
            args.interval, readingParams);
    }

    AddReport(const AddReport&) = delete;
    AddReport(AddReport&&) = delete;
    AddReport& operator=(const AddReport&) = delete;
    AddReport& operator=(AddReport&&) = delete;

    void insert(const std::map<std::string, std::string>& el)
    {
        uriToDbus.insert(el.begin(), el.end());
    }

  private:
    const std::shared_ptr<bmcweb::AsyncResp> asyncResp;
    AddReportArgs args;
    boost::container::flat_map<std::string, std::string> uriToDbus{};
};
} // namespace telemetry

inline void requestRoutesMetricReportDefinitionCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReportDefinitions/")
        .privileges(redfish::privileges::getMetricReportDefinitionCollection)
        .methods(boost::beast::http::verb::get)([&app](const crow::Request& req,
                                                       const std::shared_ptr<
                                                           bmcweb::AsyncResp>&
                                                           asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }

            asyncResp->res.jsonValue["@odata.type"] =
                "#MetricReportDefinitionCollection."
                "MetricReportDefinitionCollection";
            asyncResp->res.jsonValue["@odata.id"] =
                telemetry::metricReportDefinitionUri;
            asyncResp->res.jsonValue["Name"] = "Metric Definition Collection";
#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
            crow::connections::systemBus->async_method_call(
                [asyncResp](
                    boost::system::error_code ec,
                    const std::vector<std::string>& metricPaths) mutable {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    nlohmann::json& addMembers =
                        asyncResp->res.jsonValue["Members"];

                    for (const std::string& object : metricPaths)
                    {
                        // Get the metric object
                        std::string metricReportDefUriPath =
                            "/redfish/v1/TelemetryService/MetricReportDefinitions/";
                        if (boost::ends_with(object, "platformmetrics"))
                        {
                            addMembers.push_back(
                                {{"@odata.id",
                                  "/redfish/v1/TelemetryService/MetricReportDefinitions/" PLATFORMMETRICSID}});
                        }
                        else if (boost::ends_with(object, "memory"))
                        {
                            std::string memoryMetricId =
                                PLATFORMDEVICEPREFIX "MemoryMetrics";
                            memoryMetricId += "_0";
                            std::string uripath =
                                metricReportDefUriPath + memoryMetricId;
                            addMembers.push_back({{"@odata.id", uripath}});
                        }
                        else if (boost::ends_with(object, "processors"))
                        {
                            std::string processorMetricId =
                                PLATFORMDEVICEPREFIX "ProcessorMetrics";
                            processorMetricId += "_0";
                            std::string uripath =
                                metricReportDefUriPath + processorMetricId;
                            addMembers.push_back({{"@odata.id", uripath}});

                            std::string processorPortMetricId =
                                PLATFORMDEVICEPREFIX "ProcessorPortMetrics";
                            processorPortMetricId += "_0";
                            uripath =
                                metricReportDefUriPath + processorPortMetricId;
                            addMembers.push_back({{"@odata.id", uripath}});
                        }
                        else if (boost::ends_with(object, "Switches"))
                        {
                            std::string switchMetricId =
                                PLATFORMDEVICEPREFIX "NVSwitchMetrics";
                            switchMetricId += "_0";
                            std::string uripath =
                                metricReportDefUriPath + switchMetricId;
                            addMembers.push_back({{"@odata.id", uripath}});

                            std::string switchPortMetricId =
                                PLATFORMDEVICEPREFIX "NVSwitchPortMetrics";
                            switchPortMetricId += "_0";
                            uripath =
                                metricReportDefUriPath + switchPortMetricId;
                            addMembers.push_back({{"@odata.id", uripath}});
                        }
                    }
                    asyncResp->res.jsonValue["Members@odata.count"] =
                        addMembers.size();
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Sensor.Aggregation"});
            return;
#endif
            const std::vector<const char*> interfaces{
                telemetry::reportInterface};
            collection_util::getCollectionMembers(
                asyncResp, telemetry::metricReportDefinitionUri, interfaces,
                "/xyz/openbmc_project/Telemetry/Reports/TelemetryService");
        });

    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReportDefinitions/")
        .privileges(redfish::privileges::postMetricReportDefinitionCollection)
        .methods(
            boost::beast::http::verb::
                post)([&app](
                          const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }

            telemetry::AddReportArgs args;
            if (!telemetry::getUserParameters(asyncResp->res, req, args))
            {
                return;
            }

            boost::container::flat_set<std::pair<std::string, std::string>>
                chassisSensors;
            if (!telemetry::getChassisSensorNodeFromMetrics(
                    asyncResp, args.metrics, chassisSensors))
            {
                return;
            }

            auto addReportReq = std::make_shared<telemetry::AddReport>(
                std::move(args), asyncResp);
            for (const auto& [chassis, sensorType] : chassisSensors)
            {
                retrieveUriToDbusMap(
                    chassis, sensorType,
                    [asyncResp, addReportReq](
                        const boost::beast::http::status status,
                        const std::map<std::string, std::string>& uriToDbus) {
                        if (status != boost::beast::http::status::ok)
                        {
                            BMCWEB_LOG_ERROR
                                << "Failed to retrieve URI to dbus sensors map with err "
                                << static_cast<unsigned>(status);
                            return;
                        }
                        addReportReq->insert(uriToDbus);
                    });
            }
        });
}

#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
inline void
    processMetricProperties(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::vector<std::string>& sensorPaths,
                            const std::string& chassisId)
{
    // Get sensor reading from managed object
    for (const std::string& sensorPath : sensorPaths)
    {
        sdbusplus::message::object_path objectPath(sensorPath);
        std::string sensorName = objectPath.filename();

        nlohmann::json& metricProperties =
            asyncResp->res.jsonValue["MetricProperties"];
        nlohmann::json& wildCards = asyncResp->res.jsonValue["Wildcards"];

        std::string tmpPath = std::string("/redfish/v1/Chassis/");
        std::string dupSensorName = sensorName;
        std::string chassisName = "HGX_Chassis_0";
        if (chassisId == chassisName)
        {
            for (auto& item : wildCards.items())
            {
                nlohmann::json& itemObj = item.value();
                if (itemObj["Name"] == "BSWild")
                {
                    if (std::find(itemObj["Values"].begin(),
                                  itemObj["Values"].end(),
                                  sensorName) == itemObj["Values"].end())
                    {
                        itemObj["Values"].push_back(sensorName);
                    }
                }
            }
            tmpPath += chassisId;
            tmpPath += "/Sensors/";
            tmpPath += "{BSWild}";
        }
        else if (chassisId.find("GPU") != std::string::npos)
        {
            boost::replace_all(dupSensorName, chassisId, "GPU{GWild}");
            tmpPath += "GPU{GWild}";
            tmpPath += "/Sensors/";
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("NVSwitch") != std::string::npos)
        {
            std::string dupChassisId = chassisId;
            boost::replace_all(dupChassisId, "NVSwitch", "");
            std::string tmpChassisId = "NVS";
            tmpChassisId += dupChassisId;
            boost::replace_all(dupSensorName, tmpChassisId, "NVS{NWild}");
            tmpPath += "NVSwitch{NWild}";
            tmpPath += "/Sensors/";
            tmpPath += dupSensorName;
        }
        if (std::find(metricProperties.begin(), metricProperties.end(),
                      tmpPath) == metricProperties.end())
        {
            asyncResp->res.jsonValue["MetricProperties"].push_back(tmpPath);
        }
    }
}

inline void processChassisSensorsMetric(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisPath)
{
    auto getAllChassisHandler =
        [asyncResp,
         chassisPath](const boost::system::error_code ec,
                      std::variant<std::vector<std::string>>& chassisLinks) {
            std::vector<std::string> chassisPaths;
            if (ec)
            {
                // no chassis links = no failures
                BMCWEB_LOG_ERROR << "getAllChassisSensors DBUS error: " << ec;
            }
            // Add parent chassis to the list
            chassisPaths.emplace_back(chassisPath);
            // Add underneath chassis paths
            std::vector<std::string>* chassisData =
                std::get_if<std::vector<std::string>>(&chassisLinks);
            if (chassisData != nullptr)
            {
                for (const std::string& path : *chassisData)
                {
                    chassisPaths.emplace_back(path);
                }
            }
            // Sort the chassis for sensors paths
            std::sort(chassisPaths.begin(), chassisPaths.end());
            // Process all sensors to all chassis
            for (const std::string& objectPath : chassisPaths)
            {
                // Get the chassis path for respective sensors
                sdbusplus::message::object_path path(objectPath);
                const std::string& chassisId = path.filename();
                auto getAllChassisSensors =
                    [asyncResp,
                     chassisId](const boost::system::error_code ec,
                                const std::variant<std::vector<std::string>>&
                                    variantEndpoints) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR
                                << "getAllChassisSensors DBUS error: " << ec;
                            return;
                        }
                        const std::vector<std::string>* sensorPaths =
                            std::get_if<std::vector<std::string>>(
                                &(variantEndpoints));
                        if (sensorPaths == nullptr)
                        {
                            BMCWEB_LOG_ERROR
                                << "getAllChassisSensors empty sensors list"
                                << "\n";
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        // Process sensor readings

                        nlohmann::json& wildCards =
                            asyncResp->res.jsonValue["Wildcards"];

                        for (auto& item : wildCards.items())
                        {
                            nlohmann::json& itemObj = item.value();
                            if (itemObj["Name"] == "GWild" &&
                                chassisId.find("GPU") != std::string::npos)
                            {
                                size_t v = itemObj["Values"].size();
                                itemObj["Values"].push_back(std::to_string(v));
                            }
                            if (itemObj["Name"] == "NWild" &&
                                chassisId.find("NVSwitch") != std::string::npos)
                            {
                                size_t v = itemObj["Values"].size();
                                itemObj["Values"].push_back(std::to_string(v));
                            }
                        }

                        processMetricProperties(asyncResp, *sensorPaths,
                                                chassisId);
                    };
                crow::connections::systemBus->async_method_call(
                    getAllChassisSensors, "xyz.openbmc_project.ObjectMapper",
                    objectPath + "/all_sensors",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");
            }
        };
    // Get all chassis
    crow::connections::systemBus->async_method_call(
        getAllChassisHandler, "xyz.openbmc_project.ObjectMapper",
        chassisPath + "/all_chassis", "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getPlatformMetricsProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Chassis"};
    auto respHandler =
        [asyncResp, chassisId](const boost::system::error_code ec,
                               const std::vector<std::string>& chassisPaths) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "getPlatformMetricsProperties respHandler DBUS error: "
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
                // Identify sensor services for sensor readings
                processChassisSensorsMetric(asyncResp, chassisPath);

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

inline void getPlatformMetricReportDefinition(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id)
{
    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinition.v1_3_0.MetricReportDefinition";
    asyncResp->res.jsonValue["@odata.id"] =
        telemetry::metricReportDefinitionUri + std::string("/") + id;
    asyncResp->res.jsonValue["Id"] = id;
    asyncResp->res.jsonValue["Name"] = id;
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] =
        telemetry::metricReportUri + std::string("/") + id;

    nlohmann::json metricPropertiesAray = nlohmann::json::array();
    nlohmann::json metricGpuCount = nlohmann::json::array();
    nlohmann::json metricNVSwitchCount = nlohmann::json::array();
    nlohmann::json metricBaseboarSensorArray = nlohmann::json::array();

    nlohmann::json wildCards = nlohmann::json::array();
    wildCards.push_back({
        {"Name", "GWild"},
        {"Values", metricGpuCount},
    });

    wildCards.push_back({
        {"Name", "NWild"},
        {"Values", metricNVSwitchCount},
    });

    wildCards.push_back({
        {"Name", "BSWild"},
        {"Values", metricBaseboarSensorArray},
    });

    asyncResp->res.jsonValue["MetricProperties"] = metricPropertiesAray;
    asyncResp->res.jsonValue["Wildcards"] = wildCards;
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    asyncResp->res.jsonValue["ReportUpdates"] = "Overwrite";
    asyncResp->res.jsonValue["MetricReportDefinitionType"] = "OnRequest";
    std::vector<std::string> redfishReportActions;
    redfishReportActions.emplace_back("LogToMetricReportsCollection");
    asyncResp->res.jsonValue["ReportActions"] = redfishReportActions;
    getPlatformMetricsProperties(asyncResp, "HGX_Chassis_0");
}
inline void populateMetricProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& deviceType)
{
    nlohmann::json metricPropertiesAray = nlohmann::json::array();
    if (deviceType == "MemoryMetrics")
    {
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0#/Oem/Nvidia/RowRemappingFailed");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0/MemoryMetrics#/OperatingSpeedMHz");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0/MemoryMetrics#/BandwidthPercent");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0/MemoryMetrics#/LifeTime/CorrectableECCErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0/MemoryMetrics#/LifeTime/UncorrectableECCErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0/MemoryMetrics#/Oem/Nvidia/RowRemapping/CorrectableRowRemappingCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_{GpuId}_DRAM_0/MemoryMetrics#/Oem/Nvidia/RowRemapping/UncorrectableRowRemappingCount");
    }
    else if (deviceType == "ProcessorMetrics")
    {
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}#/Status/State");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}#/SystemInterface/PCIe/PCIeType");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}#/SystemInterface/PCIe/MaxLanes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/OperatingSpeedMHz");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/BandwidthPercent");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/CacheMetricsTotal/LifeTime/CorrectableECCErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/CacheMetricsTotal/LifeTime/UncorrectableECCErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/CorrectableErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/NonFatalErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/FatalErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/L0ToRecoveryCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/ReplayCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/ReplayRolloverCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/NAKSentCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/PCIeErrors/NAKReceivedCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/ThrottleReasons");
    }
    else if (deviceType == "NVSwitchMetrics")
    {
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/InternalMemoryMetrics/LifeTime/CorrectableECCErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/InternalMemoryMetrics/LifeTime/UncorrectableECCErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/CorrectableErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/NonFatalErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/FatalErrorCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/L0ToRecoveryCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/ReplayCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/ReplayRolloverCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/NAKSentCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/NAKReceivedCount");
    }
    else if (deviceType == "ProcessorPortMetrics")
    {
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}#/CurrentSpeedGbps");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/TXBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/RXBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/TXNoProtocolBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/RXNoProtocolBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RuntimeError");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/TrainingError");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/ReplayCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RecoveryCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/FlitCRCCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/DataCRCCount");
    }
    else if (deviceType == "NVSwitchPortMetrics")
    {
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/CurrentSpeedGbps");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/MaxSpeedGbps");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/Oem/Nvidia/TXWidth");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/Oem/Nvidia/RXWidth");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/LinkStatus");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/TXBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/RXBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/TXNoProtocolBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/RXNoProtocolBytes");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RuntimeError");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/TrainingError");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/ReplayCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RecoveryCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/FlitCRCCount");
        metricPropertiesAray.push_back(
            "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/DataCRCCount");
    }
    asyncResp->res.jsonValue["MetricProperties"] = metricPropertiesAray;
}

inline std::string getWildCardDevId(const std::string& deviceType)
{
    std::string wildCardId;
    if (deviceType == "MemoryMetrics" || deviceType == "ProcessorMetrics" ||
        deviceType == "ProcessorPortMetrics")
    {
        wildCardId = "GpuId";
    }
    else if (deviceType == "NVSwitchMetrics" ||
             deviceType == "NVSwitchPortMetrics")
    {
        wildCardId = "NVSwitchId";
    }
    return wildCardId;
}
inline std::string getWildCardSubDevId(const std::string& deviceType)
{
    std::string wildCardId;
    if (deviceType == "ProcessorPortMetrics" ||
        deviceType == "NVSwitchPortMetrics")
    {
        wildCardId = "NvlinkId";
    }
    return wildCardId;
}

inline void populateMetricPropertiesAndWildcards(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& deviceType)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp,
         deviceType](boost::system::error_code ec,
                     const std::vector<std::string>& objPaths) mutable {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            populateMetricProperties(asyncResp, deviceType);
            nlohmann::json wildCards = nlohmann::json::array();
            int wildCardMinForDevice = -1;
            int wildCardMaxForDevice = -1;
            int wildCardMinForSubDevice = -1;
            int wildCardMaxForSubDevice = -1;
            // "deviceIdentifier" defined to count number of no of subdevices on one device
            std::string deviceIdentifier;
            std::string wildCardDeviceId = getWildCardDevId(deviceType);
            std::string wildCardSubDeviceId = getWildCardSubDevId(deviceType);
            for (const std::string& object : objPaths)
            {
                sdbusplus::message::object_path path(object);
                const std::string& deviceName = path.filename();
                const std::string& parentName = path.parent_path().filename();
                const std::string& grandParentName =
                    path.parent_path().parent_path().filename();
                const std::string& devTypeOnDbus =
                    path.parent_path().parent_path().parent_path().filename();
                if (parentName == "memory" )
                {
                    if (deviceType == "MemoryMetrics")
                    {
                        if (wildCardMinForDevice == -1)
                        {
                            // Index start with 1 for memory devices
                            wildCardMinForDevice = 1;
                            wildCardMaxForDevice = 1;
                        }
                        else
                        {
                            wildCardMaxForDevice++;
                        }
                    }
                }
                else if (parentName == "processors")
                {
                    if (deviceType == "ProcessorMetrics" ||
                        deviceType == "ProcessorPortMetrics")
                    {
                        if (wildCardMinForDevice == -1)
                        {
                            // Index start with 1 for gpu processor devices
                            wildCardMinForDevice = 1;
                            wildCardMaxForDevice = 1;
                        }
                        else
                        {
                            wildCardMaxForDevice++;
                        }
                    }
                }
                else if (parentName == "Switches")
                {
                    if (deviceType == "NVSwitchMetrics" ||
                        deviceType == "NVSwitchPortMetrics")
                    {
                        if (wildCardMinForDevice == -1)
                        {
                            // Index start with 0 for switches devices
                            wildCardMinForDevice = 0;
                            wildCardMaxForDevice = 0;
                        }
                        else
                        {
                            wildCardMaxForDevice++;
                        }
                    }
                }
                else if (parentName == "Ports")
                {
                    if (devTypeOnDbus == "processors" &&
                        deviceType == "ProcessorPortMetrics")
                    {
                        if (wildCardMinForSubDevice == -1)
                        {
                            // Index start with 0 for GPU NVLink devices
                            deviceIdentifier = grandParentName;
                            wildCardMinForSubDevice = 0;
                            wildCardMaxForSubDevice = 0;
                        }
                        else if(deviceIdentifier == grandParentName)
                        {
                            wildCardMaxForSubDevice++;
                        }
                    }
                    else if (devTypeOnDbus == "Switches" &&
                             deviceType == "NVSwitchPortMetrics")
                    {
                        if (wildCardMinForSubDevice == -1)
                        {
                            // Index start with 0 for NVSwitch NVLink devices
                            wildCardMinForSubDevice = 0;
                            wildCardMaxForSubDevice = 0;
                            deviceIdentifier = grandParentName;
                        }
                        else if (deviceIdentifier == grandParentName)
                        {
                            // Count sub devices on a particular device
                            wildCardMaxForSubDevice++;
                        }
                    }
                }
            }
            nlohmann::json devCount = nlohmann::json::array();
            for (int i = wildCardMinForDevice; i <= wildCardMaxForDevice; i++)
            {
                devCount.push_back(std::to_string(i));
            }
            wildCards.push_back({
                {"Name", wildCardDeviceId},
                {"Values", devCount},
            });
            if (deviceType == "ProcessorPortMetrics" ||
                deviceType == "NVSwitchPortMetrics")
            {
                nlohmann::json subDevCount = nlohmann::json::array();
                for (int i = wildCardMinForSubDevice;
                     i <= wildCardMaxForSubDevice; i++)
                {
                    subDevCount.push_back(std::to_string(i));
                }
                wildCards.push_back({
                    {"Name", wildCardSubDeviceId},
                    {"Values", subDevCount},
                });
            }
            asyncResp->res.jsonValue["Wildcards"] = wildCards;
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{"oem.nvidia.Timestamp"});
}

inline void getMetricReportDefForAggregatedMetrics(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id,
    const std::string& deviceType)
{
    if (deviceType != "MemoryMetrics" && deviceType != "ProcessorMetrics" &&
        deviceType != "NVSwitchMetrics" &&
        deviceType != "ProcessorPortMetrics" &&
        deviceType != "NVSwitchPortMetrics")
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.id"] =
        telemetry::metricReportDefinitionUri + std::string("/") + id;
    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinition.v1_3_0.MetricReportDefinition";
    asyncResp->res.jsonValue["Id"] = id;
    asyncResp->res.jsonValue["Name"] = id;
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    asyncResp->res.jsonValue["MetricReportDefinitionType"] = "OnRequest";
    std::vector<std::string> redfishReportActions;
    redfishReportActions.emplace_back("LogToMetricReportsCollection");
    asyncResp->res.jsonValue["ReportActions"] = redfishReportActions;
    asyncResp->res.jsonValue["ReportUpdates"] = "Overwrite";
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] =
        telemetry::metricReportUri + std::string("/") + id;
    populateMetricPropertiesAndWildcards(asyncResp, deviceType);
}


inline void validateAndGetMetricReportDefinition(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id)
{
    using MapperServiceMap =
        std::vector<std::pair<std::string, std::vector<std::string>>>;

    // Map of object paths to MapperServiceMaps
    using MapperGetSubTreeResponse =
        std::vector<std::pair<std::string, MapperServiceMap>>;
    crow::connections::systemBus->async_method_call(
        [asyncResp, id](boost::system::error_code ec,
                        const MapperGetSubTreeResponse& subtree) mutable {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            bool validMetricId = false;
            std::string deviceType;
            std::string serviceName;
            for (const auto& [path, serviceMap] : subtree)
            {
                const std::string objectPath = path;
                for (const auto& [conName, interfaceList] : serviceMap)
                {
                    if (boost::ends_with(objectPath, "platformmetrics"))
                    {
                        if (id == PLATFORMMETRICSID)
                        {
                            validMetricId = true;
                        }
                    }
                    else if (boost::ends_with(objectPath, "memory"))
                    {
                        std::string memoryMetricId =
                            PLATFORMDEVICEPREFIX "MemoryMetrics_0";
                        if (id == memoryMetricId)
                        {
                            serviceName = conName;
                            validMetricId = true;
                            deviceType = "MemoryMetrics";
                        }
                    }
                    else if (boost::ends_with(objectPath, "processors"))
                    {
                        std::string processorMetricId =
                            PLATFORMDEVICEPREFIX "ProcessorMetrics_0";
                        std::string processorPortMetricId =
                            PLATFORMDEVICEPREFIX "ProcessorPortMetrics_0";
                        if (id == processorMetricId)
                        {
                            serviceName = conName;
                            validMetricId = true;
                            deviceType = "ProcessorMetrics";
                        }
                        else if (id == processorPortMetricId)
                        {
                            serviceName = conName;
                            validMetricId = true;
                            deviceType = "ProcessorPortMetrics";
                        }
                    }
                    else if (boost::ends_with(objectPath, "Switches"))
                    {
                        std::string nvSwitchMetricId =
                            PLATFORMDEVICEPREFIX "NVSwitchMetrics_0";
                        std::string nvSwitchPortMetricId =
                            PLATFORMDEVICEPREFIX "NVSwitchPortMetrics_0";
                        if (id == nvSwitchMetricId)
                        {
                            serviceName = conName;
                            validMetricId = true;
                            deviceType = "NVSwitchMetrics";
                        }
                        else if (id == nvSwitchPortMetricId)
                        {
                            serviceName = conName;
                            validMetricId = true;
                            deviceType = "NVSwitchPortMetrics";
                        }
                    }
                }
            }
            if (!validMetricId)
            {
                messages::resourceNotFound(asyncResp->res,
                                           "MetricReportDefinition", id);
            }
            else
            {
                if (id == PLATFORMMETRICSID)
                {
                    getPlatformMetricReportDefinition(asyncResp, id);
                }
                else
                {
                    getMetricReportDefForAggregatedMetrics(asyncResp, id,
                                                           deviceType);
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Sensor.Aggregation"});
}
#endif

inline void requestRoutesMetricReportDefinition(App& app)
{
    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReportDefinitions/<str>/")
        .privileges(redfish::privileges::getMetricReportDefinition)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& id) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
                validateAndGetMetricReportDefinition(asyncResp, id);
#else
                sdbusplus::asio::getAllProperties(
                    *crow::connections::systemBus, telemetry::service,
                    telemetry::getDbusReportPath(id),
                    telemetry::reportInterface,
                    [asyncResp,
                     id](const boost::system::error_code ec,
                         const std::vector<std::pair<
                             std::string, dbus::utility::DbusVariantType>>&
                             ret) {
                        if (ec.value() == EBADR ||
                            ec == boost::system::errc::host_unreachable)
                        {
                            messages::resourceNotFound(
                                asyncResp->res, "MetricReportDefinition", id);
                            return;
                        }
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "respHandler DBus error " << ec;
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        telemetry::fillReportDefinition(asyncResp, id, ret);
                    });
#endif
            });
    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReportDefinitions/<str>/")
        .privileges(redfish::privileges::deleteMetricReportDefinitionCollection)
        .methods(boost::beast::http::verb::delete_)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& id)

            {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }

                const std::string reportPath = telemetry::getDbusReportPath(id);

                crow::connections::systemBus->async_method_call(
                    [asyncResp, id](const boost::system::error_code ec) {
                        /*
                         * boost::system::errc and std::errc are missing value
                         * for EBADR error that is defined in Linux.
                         */
                        if (ec.value() == EBADR)
                        {
                            messages::resourceNotFound(
                                asyncResp->res, "MetricReportDefinition", id);
                            return;
                        }

                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "respHandler DBus error " << ec;
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        asyncResp->res.result(
                            boost::beast::http::status::no_content);
                    },
                    telemetry::service, reportPath,
                    "xyz.openbmc_project.Object.Delete", "Delete");
            });
}
} // namespace redfish
