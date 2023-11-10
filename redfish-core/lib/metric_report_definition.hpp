#pragma once

#include "app.hpp"
#include "dbus_utility.hpp"
#include "generated/enums/metric_report_definition.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "sensors.hpp"
<<<<<<< HEAD
#include "utils/metric_report_utils.hpp"
=======
#include "utils/collection.hpp"
#include "utils/dbus_utils.hpp"
>>>>>>> origin/master-october-10
#include "utils/telemetry_utils.hpp"
#include "utils/time_utils.hpp"

#include <boost/container/flat_map.hpp>
<<<<<<< HEAD
#include <boost/regex.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
=======
#include <boost/url/format.hpp>
>>>>>>> origin/master-october-10
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>

#include <array>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace redfish
{

namespace telemetry
{

using ReadingParameters = std::vector<std::tuple<
    std::vector<std::tuple<sdbusplus::message::object_path, std::string>>,
    std::string, std::string, uint64_t>>;

inline bool verifyCommonErrors(crow::Response& res, const std::string& id,
                               const boost::system::error_code& ec)
{
    if (ec.value() == EBADR || ec == boost::system::errc::host_unreachable)
    {
        messages::resourceNotFound(res, "MetricReportDefinition", id);
        return false;
    }

    if (ec == boost::system::errc::file_exists)
    {
        messages::resourceAlreadyExists(res, "MetricReportDefinition", "Id",
                                        id);
        return false;
    }

    if (ec == boost::system::errc::too_many_files_open)
    {
        messages::createLimitReachedForResource(res);
        return false;
    }

    if (ec)
    {
        BMCWEB_LOG_ERROR("DBUS response error {}", ec);
        messages::internalError(res);
        return false;
    }

    return true;
}

inline metric_report_definition::ReportActionsEnum
    toRedfishReportAction(std::string_view dbusValue)
{
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportActions.EmitsReadingsUpdate")
    {
        return metric_report_definition::ReportActionsEnum::RedfishEvent;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportActions.LogToMetricReportsCollection")
    {
        return metric_report_definition::ReportActionsEnum::
            LogToMetricReportsCollection;
    }
    return metric_report_definition::ReportActionsEnum::Invalid;
}

inline std::string toDbusReportAction(std::string_view redfishValue)
{
    if (redfishValue == "RedfishEvent")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportActions.EmitsReadingsUpdate";
    }
    if (redfishValue == "LogToMetricReportsCollection")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportActions.LogToMetricReportsCollection";
    }
    return "";
}

inline metric_report_definition::MetricReportDefinitionType
    toRedfishReportingType(std::string_view dbusValue)
{
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportingType.OnChange")
    {
        return metric_report_definition::MetricReportDefinitionType::OnChange;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportingType.OnRequest")
    {
        return metric_report_definition::MetricReportDefinitionType::OnRequest;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportingType.Periodic")
    {
        return metric_report_definition::MetricReportDefinitionType::Periodic;
    }
    return metric_report_definition::MetricReportDefinitionType::Invalid;
}

inline std::string toDbusReportingType(std::string_view redfishValue)
{
    if (redfishValue == "OnChange")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportingType.OnChange";
    }
    if (redfishValue == "OnRequest")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportingType.OnRequest";
    }
    if (redfishValue == "Periodic")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportingType.Periodic";
    }
    return "";
}

inline metric_report_definition::CollectionTimeScope
    toRedfishCollectionTimeScope(std::string_view dbusValue)
{
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.CollectionTimescope.Point")
    {
        return metric_report_definition::CollectionTimeScope::Point;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.CollectionTimescope.Interval")
    {
        return metric_report_definition::CollectionTimeScope::Interval;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.CollectionTimescope.StartupInterval")
    {
        return metric_report_definition::CollectionTimeScope::StartupInterval;
    }
    return metric_report_definition::CollectionTimeScope::Invalid;
}

inline std::string toDbusCollectionTimeScope(std::string_view redfishValue)
{
    if (redfishValue == "Point")
    {
        return "xyz.openbmc_project.Telemetry.Report.CollectionTimescope.Point";
    }
    if (redfishValue == "Interval")
    {
        return "xyz.openbmc_project.Telemetry.Report.CollectionTimescope.Interval";
    }
    if (redfishValue == "StartupInterval")
    {
        return "xyz.openbmc_project.Telemetry.Report.CollectionTimescope.StartupInterval";
    }
    return "";
}

inline metric_report_definition::ReportUpdatesEnum
    toRedfishReportUpdates(std::string_view dbusValue)
{
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportUpdates.Overwrite")
    {
        return metric_report_definition::ReportUpdatesEnum::Overwrite;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportUpdates.AppendWrapsWhenFull")
    {
        return metric_report_definition::ReportUpdatesEnum::AppendWrapsWhenFull;
    }
    if (dbusValue ==
        "xyz.openbmc_project.Telemetry.Report.ReportUpdates.AppendStopsWhenFull")
    {
        return metric_report_definition::ReportUpdatesEnum::AppendStopsWhenFull;
    }
    return metric_report_definition::ReportUpdatesEnum::Invalid;
}

inline std::string toDbusReportUpdates(std::string_view redfishValue)
{
    if (redfishValue == "Overwrite")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportUpdates.Overwrite";
    }
    if (redfishValue == "AppendWrapsWhenFull")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportUpdates.AppendWrapsWhenFull";
    }
    if (redfishValue == "AppendStopsWhenFull")
    {
        return "xyz.openbmc_project.Telemetry.Report.ReportUpdates.AppendStopsWhenFull";
    }
    return "";
}

inline std::optional<nlohmann::json::array_t> getLinkedTriggers(
    std::span<const sdbusplus::message::object_path> triggerPaths)
{
    nlohmann::json::array_t triggers;

    for (const sdbusplus::message::object_path& path : triggerPaths)
    {
        if (path.parent_path() !=
            "/xyz/openbmc_project/Telemetry/Triggers/TelemetryService")
        {
            BMCWEB_LOG_ERROR("Property Triggers contains invalid value: {}",
                             path.str);
            return std::nullopt;
        }

        std::string id = path.filename();
        if (id.empty())
        {
            BMCWEB_LOG_ERROR("Property Triggers contains invalid value: {}",
                             path.str);
            return std::nullopt;
        }
        nlohmann::json::object_t trigger;
        trigger["@odata.id"] =
            boost::urls::format("/redfish/v1/TelemetryService/Triggers/{}", id);
        triggers.emplace_back(std::move(trigger));
    }

    return triggers;
}

inline void
    fillReportDefinition(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& id,
                         const dbus::utility::DBusPropertiesMap& properties)
{
<<<<<<< HEAD
    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinition.v1_4_1.MetricReportDefinition";
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
=======
    std::vector<std::string> reportActions;
    ReadingParameters readingParams;
    std::string reportingType;
    std::string reportUpdates;
    std::string name;
    uint64_t appendLimit = 0;
    uint64_t interval = 0;
    bool enabled = false;
    std::vector<sdbusplus::message::object_path> triggers;
>>>>>>> origin/master-october-10

    const bool success = sdbusplus::unpackPropertiesNoThrow(
        dbus_utils::UnpackErrorPrinter(), properties, "ReportingType",
        reportingType, "Interval", interval, "ReportActions", reportActions,
        "ReportUpdates", reportUpdates, "AppendLimit", appendLimit,
        "ReadingParameters", readingParams, "Name", name, "Enabled", enabled,
        "Triggers", triggers);

    if (!success)
    {
        messages::internalError(asyncResp->res);
        return;
    }

    metric_report_definition::MetricReportDefinitionType redfishReportingType =
        toRedfishReportingType(reportingType);
    if (redfishReportingType ==
        metric_report_definition::MetricReportDefinitionType::Invalid)
    {
        messages::internalError(asyncResp->res);
        return;
    }

    asyncResp->res.jsonValue["MetricReportDefinitionType"] =
        redfishReportingType;

    std::optional<nlohmann::json::array_t> linkedTriggers =
        getLinkedTriggers(triggers);
    if (!linkedTriggers)
    {
        messages::internalError(asyncResp->res);
        return;
    }

    asyncResp->res.jsonValue["Links"]["Triggers"] = std::move(*linkedTriggers);

    nlohmann::json::array_t redfishReportActions;
    for (const std::string& action : reportActions)
    {
        metric_report_definition::ReportActionsEnum redfishAction =
            toRedfishReportAction(action);
        if (redfishAction ==
            metric_report_definition::ReportActionsEnum::Invalid)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        redfishReportActions.emplace_back(redfishAction);
    }

    asyncResp->res.jsonValue["ReportActions"] = std::move(redfishReportActions);

    nlohmann::json::array_t metrics = nlohmann::json::array();
    for (const auto& [sensorData, collectionFunction, collectionTimeScope,
                      collectionDuration] : readingParams)
    {
        nlohmann::json::array_t metricProperties;

        for (const auto& [sensorPath, sensorMetadata] : sensorData)
        {
            metricProperties.emplace_back(sensorMetadata);
        }

        nlohmann::json::object_t metric;

        metric_report_definition::CalculationAlgorithmEnum
            redfishCollectionFunction =
                telemetry::toRedfishCollectionFunction(collectionFunction);
        if (redfishCollectionFunction ==
            metric_report_definition::CalculationAlgorithmEnum::Invalid)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        metric["CollectionFunction"] = redfishCollectionFunction;

        metric_report_definition::CollectionTimeScope
            redfishCollectionTimeScope =
                toRedfishCollectionTimeScope(collectionTimeScope);
        if (redfishCollectionTimeScope ==
            metric_report_definition::CollectionTimeScope::Invalid)
        {
<<<<<<< HEAD
            metrics.push_back({
                {"MetricId", metricId},
                {"MetricProperties", {metadata}},
            });
=======
            messages::internalError(asyncResp->res);
            return;
>>>>>>> origin/master-october-10
        }
        metric["CollectionTimeScope"] = redfishCollectionTimeScope;

        metric["MetricProperties"] = std::move(metricProperties);
        metric["CollectionDuration"] = time_utils::toDurationString(
            std::chrono::milliseconds(collectionDuration));
        metrics.emplace_back(std::move(metric));
    }
    asyncResp->res.jsonValue["Metrics"] = std::move(metrics);

    if (enabled)
    {
        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    }
    else
    {
        asyncResp->res.jsonValue["Status"]["State"] = "Disabled";
    }

    metric_report_definition::ReportUpdatesEnum redfishReportUpdates =
        toRedfishReportUpdates(reportUpdates);
    if (redfishReportUpdates ==
        metric_report_definition::ReportUpdatesEnum::Invalid)
    {
        messages::internalError(asyncResp->res);
        return;
    }
    asyncResp->res.jsonValue["ReportUpdates"] = redfishReportUpdates;

    asyncResp->res.jsonValue["MetricReportDefinitionEnabled"] = enabled;
    asyncResp->res.jsonValue["AppendLimit"] = appendLimit;
    asyncResp->res.jsonValue["Name"] = name;
    asyncResp->res.jsonValue["Schedule"]["RecurrenceInterval"] =
        time_utils::toDurationString(std::chrono::milliseconds(interval));
    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinition.v1_3_0.MetricReportDefinition";
    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format(
        "/redfish/v1/TelemetryService/MetricReportDefinitions/{}", id);
    asyncResp->res.jsonValue["Id"] = id;
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] = boost::urls::format(
        "/redfish/v1/TelemetryService/MetricReports/{}", id);
}

struct AddReportArgs
{
    struct MetricArgs
    {
        std::vector<std::string> uris;
        std::string collectionFunction;
        std::string collectionTimeScope;
        uint64_t collectionDuration = 0;
    };

    std::string id;
    std::string name;
    std::string reportingType;
    std::string reportUpdates;
    uint64_t appendLimit = std::numeric_limits<uint64_t>::max();
    std::vector<std::string> reportActions;
    uint64_t interval = std::numeric_limits<uint64_t>::max();
    std::vector<MetricArgs> metrics;
    bool metricReportDefinitionEnabled = true;
};

inline bool toDbusReportActions(crow::Response& res,
                                const std::vector<std::string>& actions,
                                std::vector<std::string>& outReportActions)
{
    size_t index = 0;
    for (const std::string& action : actions)
    {
        std::string dbusReportAction = toDbusReportAction(action);
        if (dbusReportAction.empty())
        {
            messages::propertyValueNotInList(
                res, action, "ReportActions/" + std::to_string(index));
            return false;
        }

        outReportActions.emplace_back(std::move(dbusReportAction));
        index++;
    }
    return true;
}

inline bool getUserMetric(crow::Response& res, nlohmann::json& metric,
                          AddReportArgs::MetricArgs& metricArgs)
{
    std::optional<std::vector<std::string>> uris;
    std::optional<std::string> collectionDurationStr;
    std::optional<std::string> collectionFunction;
    std::optional<std::string> collectionTimeScopeStr;

    if (!json_util::readJson(metric, res, "MetricProperties", uris,
                             "CollectionFunction", collectionFunction,
                             "CollectionTimeScope", collectionTimeScopeStr,
                             "CollectionDuration", collectionDurationStr))
    {
        return false;
    }

    if (uris)
    {
        metricArgs.uris = std::move(*uris);
    }

    if (collectionFunction)
    {
        std::string dbusCollectionFunction =
            telemetry::toDbusCollectionFunction(*collectionFunction);
        if (dbusCollectionFunction.empty())
        {
            messages::propertyValueIncorrect(res, "CollectionFunction",
                                             *collectionFunction);
            return false;
        }
        metricArgs.collectionFunction = std::move(dbusCollectionFunction);
    }

    if (collectionTimeScopeStr)
    {
        std::string dbusCollectionTimeScope =
            toDbusCollectionTimeScope(*collectionTimeScopeStr);
        if (dbusCollectionTimeScope.empty())
        {
            messages::propertyValueIncorrect(res, "CollectionTimeScope",
                                             *collectionTimeScopeStr);
            return false;
        }
        metricArgs.collectionTimeScope = std::move(dbusCollectionTimeScope);
    }

    if (collectionDurationStr)
    {
        std::optional<std::chrono::milliseconds> duration =
            time_utils::fromDurationString(*collectionDurationStr);

        if (!duration || duration->count() < 0)
        {
            messages::propertyValueIncorrect(res, "CollectionDuration",
                                             *collectionDurationStr);
            return false;
        }

        metricArgs.collectionDuration =
            static_cast<uint64_t>(duration->count());
    }

    return true;
}

inline bool getUserMetrics(crow::Response& res,
                           std::span<nlohmann::json> metrics,
                           std::vector<AddReportArgs::MetricArgs>& result)
{
    result.reserve(metrics.size());

    for (nlohmann::json& m : metrics)
    {
        AddReportArgs::MetricArgs metricArgs;

        if (!getUserMetric(res, m, metricArgs))
        {
            return false;
        }

        result.emplace_back(std::move(metricArgs));
    }

    return true;
}

inline bool getUserParameters(crow::Response& res, const crow::Request& req,
                              AddReportArgs& args)
{
    std::optional<std::string> id;
    std::optional<std::string> name;
    std::optional<std::string> reportingTypeStr;
    std::optional<std::string> reportUpdatesStr;
    std::optional<uint64_t> appendLimit;
    std::optional<bool> metricReportDefinitionEnabled;
    std::optional<std::vector<nlohmann::json>> metrics;
    std::optional<std::vector<std::string>> reportActionsStr;
    std::optional<nlohmann::json> schedule;

    if (!json_util::readJsonPatch(
            req, res, "Id", id, "Name", name, "Metrics", metrics,
            "MetricReportDefinitionType", reportingTypeStr, "ReportUpdates",
            reportUpdatesStr, "AppendLimit", appendLimit, "ReportActions",
            reportActionsStr, "Schedule", schedule,
            "MetricReportDefinitionEnabled", metricReportDefinitionEnabled))
    {
        return false;
    }

    if (id)
    {
        constexpr const char* allowedCharactersInId =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
        if (id->empty() ||
            id->find_first_not_of(allowedCharactersInId) != std::string::npos)
        {
            messages::propertyValueIncorrect(res, "Id", *id);
            return false;
        }
        args.id = *id;
    }

    if (name)
    {
        args.name = *name;
    }

    if (reportingTypeStr)
    {
        std::string dbusReportingType = toDbusReportingType(*reportingTypeStr);
        if (dbusReportingType.empty())
        {
            messages::propertyValueNotInList(res, *reportingTypeStr,
                                             "MetricReportDefinitionType");
            return false;
        }
        args.reportingType = dbusReportingType;
    }

    if (reportUpdatesStr)
    {
        std::string dbusReportUpdates = toDbusReportUpdates(*reportUpdatesStr);
        if (dbusReportUpdates.empty())
        {
            messages::propertyValueNotInList(res, *reportUpdatesStr,
                                             "ReportUpdates");
            return false;
        }
        args.reportUpdates = dbusReportUpdates;
    }

    if (appendLimit)
    {
        args.appendLimit = *appendLimit;
    }

    if (metricReportDefinitionEnabled)
    {
        args.metricReportDefinitionEnabled = *metricReportDefinitionEnabled;
    }

    if (reportActionsStr)
    {
        if (!toDbusReportActions(res, *reportActionsStr, args.reportActions))
        {
            return false;
        }
    }

    if (reportingTypeStr == "Periodic")
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
        if (!durationNum || durationNum->count() < 0)
        {
            messages::propertyValueIncorrect(res, "RecurrenceInterval",
                                             durationStr);
            return false;
        }
        args.interval = static_cast<uint64_t>(durationNum->count());
    }

    if (metrics)
    {
        if (!getUserMetrics(res, *metrics, args.metrics))
        {
            return false;
        }
    }

    return true;
}

inline bool getChassisSensorNodeFromMetrics(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    std::span<const AddReportArgs::MetricArgs> metrics,
    boost::container::flat_set<std::pair<std::string, std::string>>& matched)
{
    for (const auto& metric : metrics)
    {
<<<<<<< HEAD
        const std::vector<std::string>& uris = metric.second;

        std::optional<IncorrectMetricUri> error = getChassisSensorNode(uris,
                                                                       matched);
=======
        std::optional<IncorrectMetricUri> error =
            getChassisSensorNode(metric.uris, matched);
>>>>>>> origin/master-october-10
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
        args(std::move(argsIn))
    {}

    ~AddReport()
    {
        boost::asio::post(crow::connections::systemBus->get_io_context(),
                          std::bind_front(&performAddReport, asyncResp, args,
                                          std::move(uriToDbus)));
    }

    static void performAddReport(
        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
        const AddReportArgs& args,
        const boost::container::flat_map<std::string, std::string>& uriToDbus)
    {
        if (asyncResp->res.result() != boost::beast::http::status::ok)
        {
            return;
        }

        telemetry::ReadingParameters readingParams;
        readingParams.reserve(args.metrics.size());

        for (const auto& metric : args.metrics)
        {
            std::vector<
                std::tuple<sdbusplus::message::object_path, std::string>>
                sensorParams;
            sensorParams.reserve(metric.uris.size());

            for (size_t i = 0; i < metric.uris.size(); i++)
            {
                const std::string& uri = metric.uris[i];
                auto el = uriToDbus.find(uri);
                if (el == uriToDbus.end())
                {
                    BMCWEB_LOG_ERROR(
                        "Failed to find DBus sensor corresponding to URI {}",
                        uri);
                    messages::propertyValueNotInList(asyncResp->res, uri,
                                                     "MetricProperties/" +
                                                         std::to_string(i));
                    return;
                }

                const std::string& dbusPath = el->second;
                sensorParams.emplace_back(dbusPath, uri);
            }

            readingParams.emplace_back(
                std::move(sensorParams), metric.collectionFunction,
                metric.collectionTimeScope, metric.collectionDuration);
        }

        crow::connections::systemBus->async_method_call(
            [asyncResp, id = args.id, uriToDbus](
                const boost::system::error_code& ec, const std::string&) {
            if (ec == boost::system::errc::file_exists)
            {
                messages::resourceAlreadyExists(
                    asyncResp->res, "MetricReportDefinition", "Id", id);
                return;
            }
            if (ec == boost::system::errc::too_many_files_open)
            {
                messages::createLimitReachedForResource(asyncResp->res);
                return;
            }
            if (ec == boost::system::errc::argument_list_too_long)
            {
                nlohmann::json metricProperties = nlohmann::json::array();
                for (const auto& [uri, _] : uriToDbus)
                {
                    metricProperties.emplace_back(uri);
                }
                messages::propertyValueIncorrect(
                    asyncResp->res, metricProperties, "MetricProperties");
                return;
            }
            if (ec)
            {
                messages::internalError(asyncResp->res);
                BMCWEB_LOG_ERROR("respHandler DBus error {}", ec);
                return;
            }

<<<<<<< HEAD
            messages::created(aResp->res);
        },
=======
            messages::created(asyncResp->res);
            },
>>>>>>> origin/master-october-10
            telemetry::service, "/xyz/openbmc_project/Telemetry/Reports",
            "xyz.openbmc_project.Telemetry.ReportManager", "AddReport",
            "TelemetryService/" + args.id, args.name, args.reportingType,
            args.reportUpdates, args.appendLimit, args.reportActions,
            args.interval, readingParams, args.metricReportDefinitionEnabled);
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
    std::shared_ptr<bmcweb::AsyncResp> asyncResp;
    AddReportArgs args;
    boost::container::flat_map<std::string, std::string> uriToDbus{};
};

class UpdateMetrics
{
  public:
    UpdateMetrics(std::string_view idIn,
                  const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn) :
        id(idIn),
        asyncResp(asyncRespIn)
    {}

    ~UpdateMetrics()
    {
        try
        {
            setReadingParams();
        }
        catch (const std::exception& e)
        {
            BMCWEB_LOG_ERROR("{}", e.what());
        }
        catch (...)
        {
            BMCWEB_LOG_ERROR("Unknown error");
        }
    }

    UpdateMetrics(const UpdateMetrics&) = delete;
    UpdateMetrics(UpdateMetrics&&) = delete;
    UpdateMetrics& operator=(const UpdateMetrics&) = delete;
    UpdateMetrics& operator=(UpdateMetrics&&) = delete;

    std::string id;
    std::map<std::string, std::string> metricPropertyToDbusPaths;

    void insert(const std::map<std::string, std::string>&
                    additionalMetricPropertyToDbusPaths)
    {
        metricPropertyToDbusPaths.insert(
            additionalMetricPropertyToDbusPaths.begin(),
            additionalMetricPropertyToDbusPaths.end());
    }

    void emplace(std::span<const std::tuple<sdbusplus::message::object_path,
                                            std::string>>
                     pathAndUri,
                 const AddReportArgs::MetricArgs& metricArgs)
    {
        readingParamsUris.emplace_back(metricArgs.uris);
        readingParams.emplace_back(
            std::vector(pathAndUri.begin(), pathAndUri.end()),
            metricArgs.collectionFunction, metricArgs.collectionTimeScope,
            metricArgs.collectionDuration);
    }

    void setReadingParams()
    {
        if (asyncResp->res.result() != boost::beast::http::status::ok)
        {
            return;
        }

<<<<<<< HEAD
        asyncResp->res.jsonValue["@odata.type"] =
            "#MetricReportDefinitionCollection."
            "MetricReportDefinitionCollection";
        asyncResp->res.jsonValue["@odata.id"] =
            telemetry::metricReportDefinitionUri;
        asyncResp->res.jsonValue["Name"] = "Metric Definition Collection";
#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
        crow::connections::systemBus->async_method_call(
            [asyncResp](boost::system::error_code ec,
                        const std::vector<std::string>& metricPaths) mutable {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            nlohmann::json& addMembers = asyncResp->res.jsonValue["Members"];

            for (const std::string& object : metricPaths)
            {
                // Get the metric object
                std::string metricReportDefUriPath =
                    "/redfish/v1/TelemetryService/MetricReportDefinitions/";
                if (boost::ends_with(object, "platformmetrics"))
                {
                    std::string uripath = metricReportDefUriPath;
                    uripath += PLATFORMMETRICSID;
                    if (!containsJsonObject(addMembers, "@odata.id", uripath))
                    {
                        addMembers.push_back({{"@odata.id", uripath}});
                    }
                }
                else if (boost::ends_with(object, "memory"))
                {
                    std::string memoryMetricId = PLATFORMDEVICEPREFIX
                        "MemoryMetrics";
                    memoryMetricId += "_0";
                    std::string uripath = metricReportDefUriPath +
                                          memoryMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});
                }
                else if (boost::ends_with(object, "processors"))
                {
                    std::string processorMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorMetrics";
                    processorMetricId += "_0";
                    std::string uripath = metricReportDefUriPath +
                                          processorMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});

                    std::string processorPortMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorPortMetrics";
                    processorPortMetricId += "_0";
                    uripath = metricReportDefUriPath + processorPortMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});

                    std::string processorGpmMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorGPMMetrics";
                    processorGpmMetricId += "_0";
                    uripath = metricReportDefUriPath + processorGpmMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});

                    std::string processorportGpmMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorPortGPMMetrics";
                    processorportGpmMetricId += "_0";
                    uripath = metricReportDefUriPath + processorportGpmMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});
                }
                else if (boost::ends_with(object, "Switches"))
                {
                    std::string switchMetricId = PLATFORMDEVICEPREFIX
                        "NVSwitchMetrics";
                    switchMetricId += "_0";
                    std::string uripath = metricReportDefUriPath +
                                          switchMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});

                    std::string switchPortMetricId = PLATFORMDEVICEPREFIX
                        "NVSwitchPortMetrics";
                    switchPortMetricId += "_0";
                    uripath = metricReportDefUriPath + switchPortMetricId;
                    addMembers.push_back({{"@odata.id", uripath}});
                }
            }
            asyncResp->res.jsonValue["Members@odata.count"] = addMembers.size();
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Sensor.Aggregation"});
        return;
#endif
        const std::vector<const char*> interfaces{telemetry::reportInterface};
        collection_util::getCollectionMembers(
            asyncResp, telemetry::metricReportDefinitionUri, interfaces,
            "/xyz/openbmc_project/Telemetry/Reports/TelemetryService");
    });
=======
        for (size_t index = 0; index < readingParamsUris.size(); ++index)
        {
            std::span<const std::string> newUris = readingParamsUris[index];
>>>>>>> origin/master-october-10

            const std::optional<std::vector<
                std::tuple<sdbusplus::message::object_path, std::string>>>
                readingParam = sensorPathToUri(newUris);

            if (!readingParam)
            {
                return;
            }

            std::get<0>(readingParams[index]) = *readingParam;
        }

        crow::connections::systemBus->async_method_call(
            [asyncResp(this->asyncResp),
             reportId = id](const boost::system::error_code& ec) {
            if (!verifyCommonErrors(asyncResp->res, reportId, ec))
            {
                return;
            }
            },
            "xyz.openbmc_project.Telemetry", getDbusReportPath(id),
            "org.freedesktop.DBus.Properties", "Set",
            "xyz.openbmc_project.Telemetry.Report", "ReadingParameters",
            dbus::utility::DbusVariantType{readingParams});
    }

  private:
    std::optional<
        std::vector<std::tuple<sdbusplus::message::object_path, std::string>>>
        sensorPathToUri(std::span<const std::string> uris) const
    {
        std::vector<std::tuple<sdbusplus::message::object_path, std::string>>
            result;

        for (const std::string& uri : uris)
        {
            auto it = metricPropertyToDbusPaths.find(uri);
            if (it == metricPropertyToDbusPaths.end())
            {
                messages::propertyValueNotInList(asyncResp->res, uri,
                                                 "MetricProperties");
                return {};
            }
            result.emplace_back(it->second, uri);
        }

        return result;
    }

    const std::shared_ptr<bmcweb::AsyncResp> asyncResp;
    std::vector<std::vector<std::string>> readingParamsUris;
    ReadingParameters readingParams{};
};

inline void
    setReportEnabled(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     std::string_view id, bool enabled)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, id = std::string(id)](const boost::system::error_code& ec) {
        if (!verifyCommonErrors(asyncResp->res, id, ec))
        {
            return;
        }
        },
        "xyz.openbmc_project.Telemetry", getDbusReportPath(id),
        "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Telemetry.Report", "Enabled",
        dbus::utility::DbusVariantType{enabled});
}

inline void setReportTypeAndInterval(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, std::string_view id,
    const std::string& reportingType, uint64_t recurrenceInterval)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, id = std::string(id)](const boost::system::error_code& ec) {
        if (!verifyCommonErrors(asyncResp->res, id, ec))
        {
            return;
        }
        },
        "xyz.openbmc_project.Telemetry", getDbusReportPath(id),
        "xyz.openbmc_project.Telemetry.Report", "SetReportingProperties",
        reportingType, recurrenceInterval);
}

inline void
    setReportUpdates(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     std::string_view id, const std::string& reportUpdates)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, id = std::string(id)](const boost::system::error_code& ec) {
        if (!verifyCommonErrors(asyncResp->res, id, ec))
        {
            return;
        }
        },
        "xyz.openbmc_project.Telemetry", getDbusReportPath(id),
        "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Telemetry.Report", "ReportUpdates",
        dbus::utility::DbusVariantType{reportUpdates});
}

inline void
    setReportActions(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     std::string_view id,
                     const std::vector<std::string>& dbusReportActions)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, id = std::string(id)](const boost::system::error_code& ec) {
        if (!verifyCommonErrors(asyncResp->res, id, ec))
        {
            return;
        }
        },
        "xyz.openbmc_project.Telemetry", getDbusReportPath(id),
        "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Telemetry.Report", "ReportActions",
        dbus::utility::DbusVariantType{dbusReportActions});
}

inline void
    setReportMetrics(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     std::string_view id, std::span<nlohmann::json> metrics)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, telemetry::service,
        telemetry::getDbusReportPath(id), telemetry::reportInterface,
        [asyncResp, id = std::string(id),
         redfishMetrics = std::vector<nlohmann::json>(metrics.begin(),
                                                      metrics.end())](
            boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& properties) mutable {
        if (!redfish::telemetry::verifyCommonErrors(asyncResp->res, id, ec))
        {
            return;
        }

        ReadingParameters readingParams;

        const bool success = sdbusplus::unpackPropertiesNoThrow(
            dbus_utils::UnpackErrorPrinter(), properties, "ReadingParameters",
            readingParams);

        if (!success)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        auto updateMetricsReq = std::make_shared<UpdateMetrics>(id, asyncResp);

        boost::container::flat_set<std::pair<std::string, std::string>>
            chassisSensors;

        size_t index = 0;
        for (nlohmann::json& metric : redfishMetrics)
        {
            if (metric.is_null())
            {
                continue;
            }

            AddReportArgs::MetricArgs metricArgs;
            std::vector<
                std::tuple<sdbusplus::message::object_path, std::string>>
                pathAndUri;

            if (index < readingParams.size())
            {
                const ReadingParameters::value_type& existing =
                    readingParams[index];

                pathAndUri = std::get<0>(existing);
                metricArgs.collectionFunction = std::get<1>(existing);
                metricArgs.collectionTimeScope = std::get<2>(existing);
                metricArgs.collectionDuration = std::get<3>(existing);
            }

            if (!getUserMetric(asyncResp->res, metric, metricArgs))
            {
                return;
            }

            std::optional<IncorrectMetricUri> error =
                getChassisSensorNode(metricArgs.uris, chassisSensors);

            if (error)
            {
                messages::propertyValueIncorrect(
                    asyncResp->res, error->uri,
                    "MetricProperties/" + std::to_string(error->index));
                return;
            }

            updateMetricsReq->emplace(pathAndUri, metricArgs);
            index++;
        }

        for (const auto& [chassis, sensorType] : chassisSensors)
        {
            retrieveUriToDbusMap(
                chassis, sensorType,
                [asyncResp, updateMetricsReq](
                    const boost::beast::http::status status,
                    const std::map<std::string, std::string>& uriToDbus) {
                if (status != boost::beast::http::status::ok)
                {
                    BMCWEB_LOG_ERROR(
                        "Failed to retrieve URI to dbus sensors map with err {}",
                        static_cast<unsigned>(status));
                    return;
                }
<<<<<<< HEAD
                addReportReq->insert(uriToDbus);
            });
=======
                updateMetricsReq->insert(uriToDbus);
                });
>>>>>>> origin/master-october-10
        }
    });
}

<<<<<<< HEAD
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
        std::string chassisName = PLATFORMCHASSISNAME;
        std::string fpgaChassiName = PLATFORMDEVICEPREFIX;
        fpgaChassiName += "FPGA_0";

        if (chassisId == chassisName)
        {
            // PlatformEnvMetric doesn't contain AltitudePressure sensor
            // therfore skipping voltage sensor from metric def
            if (dupSensorName.find("AltitudePressure") != std::string::npos)
            {
                continue;
            }
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
        else if (chassisId == fpgaChassiName)
        {
            boost::replace_all(dupSensorName, chassisId, "FPGA_{FWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "FPGA_{FWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
            // index for sensor
            int i = 0;
            for (auto& item : wildCards.items())
            {
                nlohmann::json& itemObj = item.value();
                if (itemObj["Name"] == "FWild")
                {
                    if (std::find(itemObj["Values"].begin(),
                                  itemObj["Values"].end(),
                                  sensorName) == itemObj["Values"].end())
                    {
                        itemObj["Values"].push_back(std::to_string(i));
                    }
                }
            }
        }
        else if (chassisId.find("GPU") != std::string::npos)
        {
            // PlatformEnvMetric doesn't contain Voltage sensor therfore
            // skipping voltage sensor from metric def
            if (dupSensorName.find("Voltage") != std::string::npos)
            {
                continue;
            }
            std::string gpuPrefix(platformGpuNamePrefix);
            boost::replace_all(dupSensorName, chassisId, gpuPrefix + "{GWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += gpuPrefix + "{GWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("NVSwitch") != std::string::npos)
        {
            boost::replace_all(dupSensorName, chassisId, "NVSwitch_{NWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "NVSwitch_{NWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("PCIeRetimer") != std::string::npos)
        {
            boost::replace_all(dupSensorName, chassisId,
                               "PCIeRetimer_{PRWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "PCIeRetimer_{PRWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("PCIeSwitch") != std::string::npos)
        {
            boost::replace_all(dupSensorName, chassisId, "PCIeSwitch_{PSWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "PCIeSwitch_{PSWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("ProcessorModule") != std::string::npos)
        {
            boost::replace_all(dupSensorName, chassisId,
                               "ProcessorModule_{PMWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "ProcessorModule_{PMWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("CPU") != std::string::npos)
        {
            if ((dupSensorName.find("Temp") == std::string::npos) &&
                (dupSensorName.find("Energy") == std::string::npos) &&
                (dupSensorName.find("Power") == std::string::npos))
            {
                continue;
            }

            boost::regex expProcessorModule{"ProcessorModule_\\d"};
            dupSensorName = boost::regex_replace(
                dupSensorName, expProcessorModule, "ProcessorModule_{PMWild}");

            boost::regex expCpu{"CPU_\\d"};
            dupSensorName = boost::regex_replace(dupSensorName, expCpu,
                                                 "CPU_{CWild}");

            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "CPU_{CWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }

        if (std::find(metricProperties.begin(), metricProperties.end(),
                      tmpPath) == metricProperties.end())
        {
            if (strcmp(tmpPath.c_str(), "/redfish/v1/Chassis/") != 0)
            {
                asyncResp->res.jsonValue["MetricProperties"].push_back(tmpPath);
            }
        }
    }
}

inline void processChassisSensorsMetric(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisPath, bool recursive = false)
{
    auto getAllChassisHandler =
        [asyncResp, chassisPath,
         recursive](const boost::system::error_code ec,
                    std::variant<std::vector<std::string>>& chassisLinks) {
        std::vector<std::string> chassisPaths;
        if (ec)
=======
inline void handleMetricReportDefinitionCollectionHead(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.addHeader(
        boost::beast::http::field::link,
        "</redfish/v1/JsonSchemas/MetricReportDefinitionCollection/MetricReportDefinitionCollection.json>; rel=describedby");
}

inline void handleMetricReportDefinitionCollectionGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.addHeader(
        boost::beast::http::field::link,
        "</redfish/v1/JsonSchemas/MetricReportDefinition/MetricReportDefinition.json>; rel=describedby");

    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinitionCollection."
        "MetricReportDefinitionCollection";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/TelemetryService/MetricReportDefinitions";
    asyncResp->res.jsonValue["Name"] = "Metric Definition Collection";
    constexpr std::array<std::string_view, 1> interfaces{
        telemetry::reportInterface};
    collection_util::getCollectionMembers(
        asyncResp,
        boost::urls::url(
            "/redfish/v1/TelemetryService/MetricReportDefinitions"),
        interfaces, "/xyz/openbmc_project/Telemetry/Reports/TelemetryService");
}

inline void
    handleReportPatch(App& app, const crow::Request& req,
                      const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      std::string_view id)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    std::optional<std::string> reportingTypeStr;
    std::optional<std::string> reportUpdatesStr;
    std::optional<bool> metricReportDefinitionEnabled;
    std::optional<std::vector<nlohmann::json>> metrics;
    std::optional<std::vector<std::string>> reportActionsStr;
    std::optional<nlohmann::json> schedule;

    if (!json_util::readJsonPatch(
            req, asyncResp->res, "Metrics", metrics,
            "MetricReportDefinitionType", reportingTypeStr, "ReportUpdates",
            reportUpdatesStr, "ReportActions", reportActionsStr, "Schedule",
            schedule, "MetricReportDefinitionEnabled",
            metricReportDefinitionEnabled))
    {
        return;
    }

    if (metricReportDefinitionEnabled)
    {
        setReportEnabled(asyncResp, id, *metricReportDefinitionEnabled);
    }

    if (reportUpdatesStr)
    {
        std::string dbusReportUpdates = toDbusReportUpdates(*reportUpdatesStr);
        if (dbusReportUpdates.empty())
        {
            messages::propertyValueNotInList(asyncResp->res, *reportUpdatesStr,
                                             "ReportUpdates");
            return;
        }
        setReportUpdates(asyncResp, id, dbusReportUpdates);
    }

    if (reportActionsStr)
    {
        std::vector<std::string> dbusReportActions;
        if (!toDbusReportActions(asyncResp->res, *reportActionsStr,
                                 dbusReportActions))
        {
            return;
        }
        setReportActions(asyncResp, id, dbusReportActions);
    }

    if (reportingTypeStr || schedule)
    {
        std::string dbusReportingType;
        if (reportingTypeStr)
        {
            dbusReportingType = toDbusReportingType(*reportingTypeStr);
            if (dbusReportingType.empty())
            {
                messages::propertyValueNotInList(asyncResp->res,
                                                 *reportingTypeStr,
                                                 "MetricReportDefinitionType");
                return;
            }
        }

        uint64_t recurrenceInterval = std::numeric_limits<uint64_t>::max();
        if (schedule)
        {
            std::string durationStr;
            if (!json_util::readJson(*schedule, asyncResp->res,
                                     "RecurrenceInterval", durationStr))
            {
                return;
            }

            std::optional<std::chrono::milliseconds> durationNum =
                time_utils::fromDurationString(durationStr);
            if (!durationNum || durationNum->count() < 0)
            {
                messages::propertyValueIncorrect(
                    asyncResp->res, "RecurrenceInterval", durationStr);
                return;
            }

            recurrenceInterval = static_cast<uint64_t>(durationNum->count());
        }

        setReportTypeAndInterval(asyncResp, id, dbusReportingType,
                                 recurrenceInterval);
    }

    if (metrics)
    {
        setReportMetrics(asyncResp, id, *metrics);
    }
}

inline void
    handleReportDelete(App& app, const crow::Request& req,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       std::string_view id)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    const std::string reportPath = getDbusReportPath(id);

    crow::connections::systemBus->async_method_call(
        [asyncResp,
         reportId = std::string(id)](const boost::system::error_code& ec) {
        if (!verifyCommonErrors(asyncResp->res, reportId, ec))
>>>>>>> origin/master-october-10
        {
            // no chassis links = no failures
            BMCWEB_LOG_ERROR << "getAllChassisSensors DBUS error: " << ec;
        }
<<<<<<< HEAD
        // Add parent chassis to the list
        if (!recursive)
        {
            chassisPaths.emplace_back(chassisPath);
        }
        // Add underneath chassis paths
        std::vector<std::string>* chassisData =
            std::get_if<std::vector<std::string>>(&chassisLinks);
        if (chassisData != nullptr)
        {
            for (const std::string& path : *chassisData)
            {
                chassisPaths.emplace_back(path);
                // process nest chassis. e.g CPU and GPU under ProcessorModule
                // chassis
                processChassisSensorsMetric(asyncResp, path, true);
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
                    BMCWEB_LOG_ERROR << "getAllChassisSensors DBUS error: "
                                     << ec;
                    return;
                }
                const std::vector<std::string>* sensorPaths =
                    std::get_if<std::vector<std::string>>(&(variantEndpoints));
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
                    std::string name = itemObj["Name"];

                    if ((name == "NWild" &&
                         chassisId.find("NVSwitch") != std::string::npos) ||
                        (name == "PRWild" &&
                         chassisId.find("PCIeRetimer") != std::string::npos) ||
                        (name == "PSWild" &&
                         chassisId.find("PCIeSwitch") != std::string::npos) ||
                        (name == "PMWild" &&
                         chassisId.find("ProcessorModule") !=
                             std::string::npos) ||
                        (name == "CWild" &&
                         chassisId.find("CPU") != std::string::npos))
                    {
                        size_t v = itemObj["Values"].size();
                        itemObj["Values"].push_back(std::to_string(v));
                    }
                    else if ((name == "GWild" &&
                              chassisId.find("GPU") != std::string::npos))
                    {
                        size_t v = itemObj["Values"].size() + 1;
                        itemObj["Values"].push_back(std::to_string(v));
                    }
                }

                processMetricProperties(asyncResp, *sensorPaths, chassisId);
            };
            crow::connections::systemBus->async_method_call(
                getAllChassisSensors, "xyz.openbmc_project.ObjectMapper",
                objectPath + "/all_sensors", "org.freedesktop.DBus.Properties",
                "Get", "xyz.openbmc_project.Association", "endpoints");
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
        "#MetricReportDefinition.v1_4_1.MetricReportDefinition";
    asyncResp->res.jsonValue["@odata.id"] =
        telemetry::metricReportDefinitionUri + std::string("/") + id;
    asyncResp->res.jsonValue["Id"] = id;
    asyncResp->res.jsonValue["Name"] = id;
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] =
        telemetry::metricReportUri + std::string("/") + id;

    nlohmann::json metricPropertiesAray = nlohmann::json::array();
    nlohmann::json metricGpuCount = nlohmann::json::array();
    nlohmann::json metricNVSwitchCount = nlohmann::json::array();
    nlohmann::json metricPCIeSwitchCount = nlohmann::json::array();
    nlohmann::json metricPCIeRetimerCount = nlohmann::json::array();
    nlohmann::json metricFPGACount = nlohmann::json::array();
    nlohmann::json metricBaseboardSensorArray = nlohmann::json::array();
    nlohmann::json metricProcessorModuleCountArray = nlohmann::json::array();
    nlohmann::json metricCpuCountArray = nlohmann::json::array();

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
        {"Name", "PRWild"},
        {"Values", metricPCIeRetimerCount},
    });

    wildCards.push_back({
        {"Name", "PSWild"},
        {"Values", metricPCIeSwitchCount},
    });

    wildCards.push_back({
        {"Name", "FWild"},
        {"Values", metricFPGACount},
    });

    wildCards.push_back({
        {"Name", "BSWild"},
        {"Values", metricBaseboardSensorArray},
    });

    wildCards.push_back({
        {"Name", "PMWild"},
        {"Values", metricProcessorModuleCountArray},
    });

    wildCards.push_back({
        {"Name", "CWild"},
        {"Values", metricCpuCountArray},
    });

    asyncResp->res.jsonValue["MetricProperties"] = metricPropertiesAray;
    asyncResp->res.jsonValue["Wildcards"] = wildCards;
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    asyncResp->res.jsonValue["ReportUpdates"] = "Overwrite";
    asyncResp->res.jsonValue["MetricReportDefinitionType"] = "OnRequest";
    std::vector<std::string> redfishReportActions;
    redfishReportActions.emplace_back("LogToMetricReportsCollection");
    asyncResp->res.jsonValue["ReportActions"] = redfishReportActions;
    getPlatformMetricsProperties(asyncResp, PLATFORMCHASSISNAME);
}

// This code is added to form platform independent URIs for the aggregated
// metric properties
inline std::string getMemoryMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    std::string gpuPrefix(platformGpuNamePrefix);
    if (propertyName == "RowRemappingFailed")
    {
        propURI += "/Memory/" + gpuPrefix +
                   "{GpuId}_DRAM_0#/Oem/Nvidia/RowRemappingFailed";
    }
    else if (propertyName == "OperatingSpeedMHz")
    {
        propURI += "/Memory/" + gpuPrefix +
                   "{GpuId}_DRAM_0/MemoryMetrics#/OperatingSpeedMHz";
    }
    else if (propertyName == "BandwidthPercent")
    {
        propURI += "/Memory/" + gpuPrefix +
                   "{GpuId}_DRAM_0/MemoryMetrics#/BandwidthPercent";
    }
    else if (propertyName == "CorrectableECCErrorCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix +
            "{GpuId}_DRAM_0/MemoryMetrics#/LifeTime/CorrectableECCErrorCount";
    }
    else if (propertyName == "UncorrectableECCErrorCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix +
            "{GpuId}_DRAM_0/MemoryMetrics#/LifeTime/UncorrectableECCErrorCount";
    }
    else if (propertyName == "CorrectableRowRemappingCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix +
            "{GpuId}_DRAM_0/MemoryMetrics#/Oem/Nvidia/RowRemapping/CorrectableRowRemappingCount";
    }
    else if (propertyName == "UncorrectableRowRemappingCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix +
            "{GpuId}_DRAM_0/MemoryMetrics#/Oem/Nvidia/RowRemapping/UncorrectableRowRemappingCount";
    }
    return propURI;
}

// This code is added to form platform independent URIs for the aggregated
// metric properties
inline std::string getProcessorGpmMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;

    if (propertyName == "DMMAUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/DMMAUtilizationPercent";
    }
    else if (propertyName == "FP16ActivityPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/FP16ActivityPercent";
    }
    else if (propertyName == "FP32ActivityPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/FP32ActivityPercent";
    }
    else if (propertyName == "FP64ActivityPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/FP64ActivityPercent";
    }
    else if (propertyName == "GraphicsEngineActivityPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/GraphicsEngineActivityPercent";
    }
    else if (propertyName == "HMMAUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/HMMAUtilizationPercent";
    }
    else if (propertyName == "IMMAUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/IMMAUtilizationPercent";
    }
    else if (propertyName == "IntergerActivityUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/IntergerActivityUtilizationPercent";
    }
    else if (propertyName == "NVDecUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVDecUtilizationPercent";
    }
    else if (propertyName == "NVJpgInstanceUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVJpgInstanceUtilizationPercent";
    }
    else if (propertyName == "NVDecInstanceUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVDecInstanceUtilizationPercent";
    }
    else if (propertyName == "NVJpgUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVJpgUtilizationPercent";
    }
    else if (propertyName == "NVLinkDataTxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVLinkDataTxBandwidthGbps";
    }
    else if (propertyName == "NVLinkDataRxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVLinkDataRxBandwidthGbps";
    }
    else if (propertyName == "NVLinkRawTxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVLinkRawTxBandwidthGbps";
    }
    else if (propertyName == "NVLinkRawRxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVLinkRawRxBandwidthGbps";
    }
    else if (propertyName == "NVOfaUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/NVOfaUtilizationPercent";
    }
    else if (propertyName == "PCIeRawTxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/PCIeRawTxBandwidthGbps";
    }
    else if (propertyName == "PCIeRawRxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/PCIeRawRxBandwidthGbps";
    }
    else if (propertyName == "SMActivityPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/SMActivityPercent";
    }
    else if (propertyName == "SMOccupancyPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/SMOccupancyPercent";
    }
    else if (propertyName == "TensorCoreActivityPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/ProcessorMetrics#/Oem/Nvidia/TensorCoreActivityPercent";
    }
    else if (propertyName == "CapacityUtilizationPercent")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/MemorySummary/MemoryMetrics#/CapacityUtilizationPercent";
    }
    return propURI;
}

// This code is added to form platform independent URIs for the aggregated
// metric properties
inline std::string getProcessorPortGpmMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    if (propertyName == "NVLinkDataTxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkDataTxBandwidthGbps";
    }
    else if (propertyName == "NVLinkDataRxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkDataRxBandwidthGbps";
    }
    else if (propertyName == "NVLinkRawTxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkRawTxBandwidthGbps";
    }
    else if (propertyName == "NVLinkRawRxBandwidthGbps")
    {
        propURI +=
            "/Processors/GPU_SXM_{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkRawRxBandwidthGbps";
    }
    return propURI;
}

inline void populateGpmMetricProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& deviceType)
{
    nlohmann::json metricPropertiesAray = nlohmann::json::array();
    if (deviceType == "ProcessorGpmMetrics")
    {
        std::string propName = "TensorCoreActivityPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "SMOccupancyPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "SMActivityPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "PCIeRawTxBandwidthGbps";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "PCIeRawRxBandwidthGbps";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVOfaUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVLinkRawTxBandwidthGbps";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVLinkRawRxBandwidthGbps";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVLinkDataTxBandwidthGbps";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVLinkDataRxBandwidthGbps";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVJpgUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVJpgInstanceUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVDecInstanceUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "NVDecUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "IntergerActivityUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "IMMAUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "HMMAUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "GraphicsEngineActivityPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "FP64ActivityPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "FP32ActivityPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "FP16ActivityPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));

        propName = "DMMAUtilizationPercent";
        metricPropertiesAray.push_back(getProcessorGpmMetricURIDef(propName));
    }
    else if (deviceType == "ProcessorPortGpmMetrics")
    {
        std::string propName = "NVLinkDataTxBandwidthGbps";
        metricPropertiesAray.push_back(
            getProcessorPortGpmMetricURIDef(propName));

        propName = "NVLinkDataRxBandwidthGbps";
        metricPropertiesAray.push_back(
            getProcessorPortGpmMetricURIDef(propName));

        propName = "NVLinkRawTxBandwidthGbps";
        metricPropertiesAray.push_back(
            getProcessorPortGpmMetricURIDef(propName));

        propName = "NVLinkRawRxBandwidthGbps";
        metricPropertiesAray.push_back(
            getProcessorPortGpmMetricURIDef(propName));
    }
    asyncResp->res.jsonValue["MetricProperties"] = metricPropertiesAray;
}

inline std::string getProcessorMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    std::string gpuPrefix(platformGpuNamePrefix);
    if (propertyName == "State")
    {
        propURI += "/Processors/" + gpuPrefix + "{GpuId}#/Status/State";
    }
    else if (propertyName == "PCIeType")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}#/SystemInterface/PCIe/PCIeType";
    }
    else if (propertyName == "MaxLanes")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}#/SystemInterface/PCIe/MaxLanes";
    }
    else if (propertyName == "OperatingSpeedMHz")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/OperatingSpeedMHz";
    }
    else if (propertyName == "BandwidthPercent")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/BandwidthPercent";
    }
    else if (propertyName == "CorrectableECCErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/ProcessorMetrics#/CacheMetricsTotal/LifeTime/CorrectableECCErrorCount";
    }
    else if (propertyName == "UncorrectableECCErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/ProcessorMetrics#/CacheMetricsTotal/LifeTime/UncorrectableECCErrorCount";
    }
    else if (propertyName == "CorrectableErrorCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/CorrectableErrorCount";
    }
    else if (propertyName == "NonFatalErrorCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/NonFatalErrorCount";
    }
    else if (propertyName == "FatalErrorCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/FatalErrorCount";
    }
    else if (propertyName == "L0ToRecoveryCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/L0ToRecoveryCount";
    }
    else if (propertyName == "ReplayCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/ReplayCount";
    }
    else if (propertyName == "ReplayRolloverCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/ReplayRolloverCount";
    }
    else if (propertyName == "NAKSentCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/NAKSentCount";
    }
    else if (propertyName == "NAKReceivedCount")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PCIeErrors/NAKReceivedCount";
    }
    else if (propertyName == "ThrottleReasons")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/Oem/Nvidia/ThrottleReasons";
    }
    else if (propertyName == "AccumulatedGPUContextUtilizationDuration")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/ProcessorMetrics#/Oem/Nvidia/AccumulatedGPUContextUtilizationDuration";
    }
    else if (propertyName == "AccumulatedSMUtilizationDuration")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/ProcessorMetrics#/Oem/Nvidia/AccumulatedSMUtilizationDuration";
    }
    else if (propertyName == "PCIeTXBytes")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/Oem/Nvidia/PCIeTXBytes";
    }
    else if (propertyName == "PCIeRXBytes")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/Oem/Nvidia/PCIeRXBytes";
    }
    else if (propertyName == "PowerLimitThrottleDuration")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/PowerLimitThrottleDuration";
    }
    else if (propertyName == "ThermalLimitThrottleDuration")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/ProcessorMetrics#/ThermalLimitThrottleDuration";
    }
    else if (propertyName == "HardwareViolationThrottleDuration")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/ProcessorMetrics#/Oem/Nvidia/HardwareViolationThrottleDuration";
    }
    else if (propertyName == "GlobalSoftwareViolationThrottleDuration")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/ProcessorMetrics#/Oem/Nvidia/GlobalSoftwareViolationThrottleDuration";
    }
    return propURI;
}

inline std::string getNVSwitchMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Fabrics/";
    propURI += PLATFORMDEVICEPREFIX;
    propURI += "NVLinkFabric_0";
    if (propertyName == "CorrectableECCErrorCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/InternalMemoryMetrics/LifeTime/CorrectableECCErrorCount";
    }
    else if (propertyName == "UncorrectableECCErrorCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/InternalMemoryMetrics/LifeTime/UncorrectableECCErrorCount";
    }
    else if (propertyName == "CorrectableErrorCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/CorrectableErrorCount";
    }
    else if (propertyName == "NonFatalErrorCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/NonFatalErrorCount";
    }
    else if (propertyName == "FatalErrorCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/FatalErrorCount";
    }
    else if (propertyName == "L0ToRecoveryCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/L0ToRecoveryCount";
    }
    else if (propertyName == "ReplayCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/ReplayCount";
    }
    else if (propertyName == "ReplayRolloverCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/ReplayRolloverCount";
    }
    else if (propertyName == "NAKSentCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/NAKSentCount";
    }
    else if (propertyName == "NAKReceivedCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/SwitchMetrics#/PCIeErrors/NAKReceivedCount";
    }
    return propURI;
}

inline std::string getProcessorPortMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    std::string gpuPrefix(platformGpuNamePrefix);
    if (propertyName == "CurrentSpeedGbps")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/Ports/NVLink_{NvlinkId}#/CurrentSpeedGbps";
    }
    else if (propertyName == "TXBytes")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/TXBytes";
    }
    else if (propertyName == "RXBytes")
    {
        propURI += "/Processors/" + gpuPrefix +
                   "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/RXBytes";
    }
    else if (propertyName == "TXNoProtocolBytes")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/TXNoProtocolBytes";
    }
    else if (propertyName == "RXNoProtocolBytes")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/RXNoProtocolBytes";
    }
    else if (propertyName == "RuntimeError")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RuntimeError";
    }
    else if (propertyName == "TrainingError")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/TrainingError";
    }
    else if (propertyName == "ReplayCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/ReplayCount";
    }
    else if (propertyName == "RecoveryCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RecoveryCount";
    }
    else if (propertyName == "FlitCRCCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/FlitCRCCount";
    }
    else if (propertyName == "DataCRCCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix +
            "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/DataCRCCount";
    }
    return propURI;
}

inline std::string getNVSwitchPortMetricURIDef(std::string& propertyName)
{
    std::string propURI = "/redfish/v1/Fabrics/";
    propURI += PLATFORMDEVICEPREFIX;
    propURI += "NVLinkFabric_0";
    if (propertyName == "CurrentSpeedGbps")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/CurrentSpeedGbps";
    }
    else if (propertyName == "MaxSpeedGbps")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/MaxSpeedGbps";
    }
    else if (propertyName == "TXWidth")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/Oem/Nvidia/TXWidth";
    }
    else if (propertyName == "RXWidth")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/Oem/Nvidia/RXWidth";
    }
    else if (propertyName == "LinkStatus")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}#/LinkStatus";
    }
    else if (propertyName == "TXBytes")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/TXBytes";
    }
    else if (propertyName == "RXBytes")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/RXBytes";
    }
    else if (propertyName == "TXNoProtocolBytes")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/TXNoProtocolBytes";
    }
    else if (propertyName == "RXNoProtocolBytes")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/RXNoProtocolBytes";
    }
    else if (propertyName == "RuntimeError")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RuntimeError";
    }
    else if (propertyName == "TrainingError")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/TrainingError";
    }
    else if (propertyName == "ReplayCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/ReplayCount";
    }
    else if (propertyName == "RecoveryCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RecoveryCount";
    }
    else if (propertyName == "FlitCRCCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/FlitCRCCount";
    }
    else if (propertyName == "DataCRCCount")
    {
        propURI +=
            "/Switches/NVSwitch_{NVSwitchId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/DataCRCCount";
    }
    return propURI;
}

inline void populateMetricProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& deviceType)
{
    nlohmann::json metricPropertiesAray = nlohmann::json::array();
    if (deviceType == "MemoryMetrics")
    {
        std::string propName = "RowRemappingFailed";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));

        propName = "OperatingSpeedMHz";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));

        propName = "BandwidthPercent";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));

        propName = "CorrectableECCErrorCount";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));

        propName = "UncorrectableECCErrorCount";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));

        propName = "CorrectableRowRemappingCount";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));

        propName = "UncorrectableRowRemappingCount";
        metricPropertiesAray.push_back(getMemoryMetricURIDef(propName));
    }
    else if (deviceType == "ProcessorMetrics")
    {
        std::string propName = "State";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "PCIeType";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "MaxLanes";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "OperatingSpeedMHz";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "BandwidthPercent";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "CorrectableECCErrorCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "UncorrectableECCErrorCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "CorrectableErrorCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "NonFatalErrorCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "FatalErrorCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "L0ToRecoveryCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "ReplayCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "ReplayRolloverCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "NAKSentCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "NAKReceivedCount";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "ThrottleReasons";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "ThermalLimitThrottleDuration";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "PowerLimitThrottleDuration";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "PCIeTXBytes";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "PCIeRXBytes";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "AccumulatedGPUContextUtilizationDuration";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "AccumulatedSMUtilizationDuration";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        // propName = "CapacityUtilizationPercent";
        // metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "GlobalSoftwareViolationThrottleDuration";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));

        propName = "HardwareViolationThrottleDuration";
        metricPropertiesAray.push_back(getProcessorMetricURIDef(propName));
    }
    else if (deviceType == "NVSwitchMetrics")
    {
        std::string propName = "CorrectableECCErrorCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "UncorrectableECCErrorCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "CorrectableErrorCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "NonFatalErrorCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "FatalErrorCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "L0ToRecoveryCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "ReplayCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "ReplayRolloverCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "NAKSentCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));

        propName = "NAKReceivedCount";
        metricPropertiesAray.push_back(getNVSwitchMetricURIDef(propName));
    }
    else if (deviceType == "ProcessorPortMetrics")
    {
        std::string propName = "CurrentSpeedGbps";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "TXBytes";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "RXBytes";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "TXNoProtocolBytes";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "RXNoProtocolBytes";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "RuntimeError";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "TrainingError";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "ReplayCount";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "RecoveryCount";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "FlitCRCCount";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));

        propName = "DataCRCCount";
        metricPropertiesAray.push_back(getProcessorPortMetricURIDef(propName));
    }
    else if (deviceType == "NVSwitchPortMetrics")
    {
        std::string propName = "CurrentSpeedGbps";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "MaxSpeedGbps";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "TXWidth";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "RXWidth";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "LinkStatus";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "TXBytes";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "RXBytes";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "TXNoProtocolBytes";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "RXNoProtocolBytes";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "RuntimeError";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "TrainingError";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "ReplayCount";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "RecoveryCount";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "FlitCRCCount";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));

        propName = "DataCRCCount";
        metricPropertiesAray.push_back(getNVSwitchPortMetricURIDef(propName));
    }
    asyncResp->res.jsonValue["MetricProperties"] = metricPropertiesAray;
}

inline std::string getWildCardDevId(const std::string& deviceType)
{
    std::string wildCardId;
    if (deviceType == "MemoryMetrics" || deviceType == "ProcessorMetrics" ||
        deviceType == "ProcessorPortMetrics" ||
        deviceType == "ProcessorGpmMetrics" ||
        deviceType == "ProcessorPortGpmMetrics")
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
        deviceType == "NVSwitchPortMetrics" ||
        deviceType == "ProcessorPortGpmMetrics")
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
        if (deviceType == "ProcessorGpmMetrics" ||
            deviceType == "ProcessorPortGpmMetrics")
        {
            populateGpmMetricProperties(asyncResp, deviceType);
        }
        else
        {
            populateMetricProperties(asyncResp, deviceType);
        }

        nlohmann::json wildCards = nlohmann::json::array();
        int wildCardMinForDevice = -1;
        int wildCardMaxForDevice = -1;
        int wildCardMinForSubDevice = -1;
        int wildCardMaxForSubDevice = -1;
        // "deviceIdentifier" defined to count number of no of subdevices on one
        // device
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
            if (parentName == "memory")
            {
                if (deviceType == "MemoryMetrics")
                {
                    if (wildCardMinForDevice == -1)
                    {
                        // Index start with 1 for memory devices
                        wildCardMinForDevice = gpuIndexStart;
                        wildCardMaxForDevice = gpuIndexStart;
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
                    deviceType == "ProcessorPortMetrics" ||
                    deviceType == "ProcessorGpmMetrics" ||
                    deviceType == "ProcessorPortGpmMetrics")
                {
                    if (wildCardMinForDevice == -1)
                    {
                        // Index start with 1 for gpu processor devices
                        wildCardMinForDevice = gpuIndexStart;
                        wildCardMaxForDevice = gpuIndexStart;
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
                if ((devTypeOnDbus == "processors") &&
                    (deviceType == "ProcessorPortMetrics" ||
                     deviceType == "ProcessorPortGpmMetrics"))
                {
                    if (deviceName.find("C2C_") != std::string::npos)
                    {
                        continue;
                    }
                    if (wildCardMinForSubDevice == -1)
                    {
                        // Index start with 0 for GPU NVLink devices
                        deviceIdentifier = grandParentName;
                        wildCardMinForSubDevice = 0;
                        wildCardMaxForSubDevice = 0;
                    }
                    else if (deviceIdentifier == grandParentName)
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
            deviceType == "NVSwitchPortMetrics" ||
            deviceType == "ProcessorPortGpmMetrics")
        {
            nlohmann::json subDevCount = nlohmann::json::array();
            for (int i = wildCardMinForSubDevice; i <= wildCardMaxForSubDevice;
                 i++)
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
        deviceType != "NVSwitchPortMetrics" &&
        deviceType != "ProcessorGpmMetrics" &&
        deviceType != "ProcessorPortGpmMetrics")
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.id"] =
        telemetry::metricReportDefinitionUri + std::string("/") + id;
    asyncResp->res.jsonValue["@odata.type"] =
        "#MetricReportDefinition.v1_4_1.MetricReportDefinition";
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
        std::string devicePath;

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
                    std::string memoryMetricId = PLATFORMDEVICEPREFIX
                        "MemoryMetrics_0";
                    if (id == memoryMetricId)
                    {
                        serviceName = conName;
                        validMetricId = true;
                        deviceType = "MemoryMetrics";
                    }
                }
                else if (boost::ends_with(objectPath, "processors"))
                {
                    std::string processorMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorMetrics_0";
                    std::string processorPortMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorPortMetrics_0";
                    std::string processorPortGpmMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorPortGPMMetrics_0";
                    std::string processorGpmMetricId = PLATFORMDEVICEPREFIX
                        "ProcessorGPMMetrics_0";

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
                    else if (id == processorGpmMetricId)
                    {
                        serviceName = conName;
                        validMetricId = true;
                        deviceType = "ProcessorGpmMetrics";
                        devicePath = objectPath;
                    }
                    if (id == processorPortGpmMetricId)
                    {
                        serviceName = conName;
                        validMetricId = true;
                        deviceType = "ProcessorPortGpmMetrics";
                        devicePath = objectPath;
                    }
                }
                else if (boost::ends_with(objectPath, "Switches"))
                {
                    std::string nvSwitchMetricId = PLATFORMDEVICEPREFIX
                        "NVSwitchMetrics_0";
                    std::string nvSwitchPortMetricId = PLATFORMDEVICEPREFIX
                        "NVSwitchPortMetrics_0";
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
            messages::resourceNotFound(asyncResp->res, "MetricReportDefinition",
                                       id);
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
            telemetry::getDbusReportPath(id), telemetry::reportInterface,
            [asyncResp,
             id](const boost::system::error_code ec,
                 const std::vector<std::pair<
                     std::string, dbus::utility::DbusVariantType>>& ret) {
            if (ec.value() == EBADR ||
                ec == boost::system::errc::host_unreachable)
            {
                messages::resourceNotFound(asyncResp->res,
                                           "MetricReportDefinition", id);
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
                messages::resourceNotFound(asyncResp->res,
                                           "MetricReportDefinition", id);
                return;
            }

            if (ec)
            {
                BMCWEB_LOG_ERROR << "respHandler DBus error " << ec;
                messages::internalError(asyncResp->res);
                return;
            }

            asyncResp->res.result(boost::beast::http::status::no_content);
        },
            telemetry::service, reportPath, "xyz.openbmc_project.Object.Delete",
            "Delete");
    });
=======
        asyncResp->res.result(boost::beast::http::status::no_content);
        },
        service, reportPath, "xyz.openbmc_project.Object.Delete", "Delete");
}
} // namespace telemetry

inline void handleMetricReportDefinitionsPost(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
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
    if (!telemetry::getChassisSensorNodeFromMetrics(asyncResp, args.metrics,
                                                    chassisSensors))
    {
        return;
    }

    auto addReportReq = std::make_shared<telemetry::AddReport>(std::move(args),
                                                               asyncResp);
    for (const auto& [chassis, sensorType] : chassisSensors)
    {
        retrieveUriToDbusMap(
            chassis, sensorType,
            [asyncResp, addReportReq](
                const boost::beast::http::status status,
                const std::map<std::string, std::string>& uriToDbus) {
            if (status != boost::beast::http::status::ok)
            {
                BMCWEB_LOG_ERROR(
                    "Failed to retrieve URI to dbus sensors map with err {}",
                    static_cast<unsigned>(status));
                return;
            }
            addReportReq->insert(uriToDbus);
            });
    }
}

inline void
    handleMetricReportHead(App& app, const crow::Request& req,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& /*id*/)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.addHeader(
        boost::beast::http::field::link,
        "</redfish/v1/JsonSchemas/MetricReport/MetricReport.json>; rel=describedby");
}

inline void
    handleMetricReportGet(App& app, const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& id)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.addHeader(
        boost::beast::http::field::link,
        "</redfish/v1/JsonSchemas/MetricReport/MetricReport.json>; rel=describedby");

    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, telemetry::service,
        telemetry::getDbusReportPath(id), telemetry::reportInterface,
        [asyncResp, id](const boost::system::error_code& ec,
                        const dbus::utility::DBusPropertiesMap& properties) {
        if (!redfish::telemetry::verifyCommonErrors(asyncResp->res, id, ec))
        {
            return;
        }

        telemetry::fillReportDefinition(asyncResp, id, properties);
        });
>>>>>>> origin/master-october-10
}

inline void handleMetricReportDelete(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id)

{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    const std::string reportPath = telemetry::getDbusReportPath(id);

    crow::connections::systemBus->async_method_call(
        [asyncResp, id](const boost::system::error_code& ec) {
        /*
         * boost::system::errc and std::errc are missing value
         * for EBADR error that is defined in Linux.
         */
        if (ec.value() == EBADR)
        {
            messages::resourceNotFound(asyncResp->res, "MetricReportDefinition",
                                       id);
            return;
        }

        if (ec)
        {
            BMCWEB_LOG_ERROR("respHandler DBus error {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }

        asyncResp->res.result(boost::beast::http::status::no_content);
        },
        telemetry::service, reportPath, "xyz.openbmc_project.Object.Delete",
        "Delete");
}

inline void requestRoutesMetricReportDefinitionCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReportDefinitions/")
        .privileges(redfish::privileges::headMetricReportDefinitionCollection)
        .methods(boost::beast::http::verb::head)(std::bind_front(
            telemetry::handleMetricReportDefinitionCollectionHead,
            std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReportDefinitions/")
        .privileges(redfish::privileges::getMetricReportDefinitionCollection)
        .methods(boost::beast::http::verb::get)(std::bind_front(
            telemetry::handleMetricReportDefinitionCollectionGet,
            std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/TelemetryService/MetricReportDefinitions/")
        .privileges(redfish::privileges::postMetricReportDefinitionCollection)
        .methods(boost::beast::http::verb::post)(
            std::bind_front(handleMetricReportDefinitionsPost, std::ref(app)));
}

inline void requestRoutesMetricReportDefinition(App& app)
{
    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReportDefinitions/<str>/")
        .privileges(redfish::privileges::getMetricReportDefinition)
        .methods(boost::beast::http::verb::head)(
            std::bind_front(handleMetricReportHead, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReportDefinitions/<str>/")
        .privileges(redfish::privileges::getMetricReportDefinition)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleMetricReportGet, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReportDefinitions/<str>/")
        .privileges(redfish::privileges::deleteMetricReportDefinition)
        .methods(boost::beast::http::verb::delete_)(
            std::bind_front(handleMetricReportDelete, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/TelemetryService/MetricReportDefinitions/<str>/")
        .privileges(redfish::privileges::patchMetricReportDefinition)
        .methods(boost::beast::http::verb::patch)(
            std::bind_front(telemetry::handleReportPatch, std::ref(app)));
}
} // namespace redfish
