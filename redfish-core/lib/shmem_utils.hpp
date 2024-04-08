#pragma once
#include "tal.hpp"

#include <nlohmann/json.hpp>

namespace redfish
{
namespace shmem
{

inline void
    getShmemPlatformMetrics(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& metricId,
                            const uint64_t& requestTimestamp = 0)
{
    BMCWEB_LOG_DEBUG("getShmemPlatformMetrics :{} Requested at : {}", metricId,
                     requestTimestamp);
    try
    {
        const auto& values = tal::TelemetryAggregator::getAllMrds(metricId);
        asyncResp->res.jsonValue["@odata.type"] =
            "#MetricReport.v1_4_2.MetricReport";
        std::string metricUri = "/redfish/v1/TelemetryService/MetricReports/";
        metricUri += metricId;
        asyncResp->res.jsonValue["@odata.id"] = metricUri;
        asyncResp->res.jsonValue["Id"] = metricId;
        asyncResp->res.jsonValue["Name"] = metricId;
        std::string metricDefinitionUri =
            "/redfish/v1/TelemetryService/MetricReportDefinitions";
        metricDefinitionUri += "/";
        metricDefinitionUri += metricId;
        asyncResp->res.jsonValue["MetricReportDefinition"]["@odata.id"] =
            metricDefinitionUri;
        nlohmann::json& resArray = asyncResp->res.jsonValue["MetricValues"];
        nlohmann::json thisMetric = nlohmann::json::object();

        if (metricId == PLATFORMMETRICSID)
        {
            asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                "#NvidiaMetricReport.v1_0_0.NvidiaMetricReport";
            asyncResp->res
                .jsonValue["Oem"]["Nvidia"]["SensingIntervalMilliseconds"] =
                pmSensingInterval;
            for (const auto& e : values)
            {
                thisMetric["MetricValue"] = e.sensorValue;
                thisMetric["Timestamp"] = e.timestampStr;
                thisMetric["MetricProperty"] = e.metricProperty;
                thisMetric["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaMetricReport.v1_0_0.NvidiaMetricReport";
                thisMetric["Oem"]["Nvidia"]["MetricValueStale"] = true;
                if (requestTimestamp != 0 && thisMetric["MetricValue"] != "nan")
                {
                    int64_t freshness =
                        static_cast<int64_t>(requestTimestamp - e.timestamp);
                    if (freshness <= staleSensorUpperLimitms)
                    {
                        thisMetric["Oem"]["Nvidia"]["MetricValueStale"] = false;
                    }
                    // enable this line for sensor age calculation
                    // thisMetric["Oem"]["Nvidia"]["FreshnessInms"] = freshness;
                }
                resArray.push_back(thisMetric);
            }
        }
        else
        {
            for (const auto& e : values)
            {
                thisMetric["MetricValue"] = e.sensorValue;
                thisMetric["Timestamp"] = e.timestampStr;
                thisMetric["MetricProperty"] = e.metricProperty;
                resArray.push_back(thisMetric);
            }
        }
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_ERROR("Exception while getting MRD values: {}", e.what());
        messages::resourceNotFound(asyncResp->res, "MetricReport", metricId);
    }
}
} // namespace shmem
} // namespace redfish
