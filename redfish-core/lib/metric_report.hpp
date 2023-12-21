#pragma once
#include "app.hpp"
namespace redfish
{

namespace telemetry
{

using Readings = std::vector<std::tuple<std::string, double, uint64_t>>;
using TimestampReadings = std::tuple<uint64_t, Readings>;

bool fillReport(nlohmann::json& json, const std::string& id,
                const TimestampReadings& timestampReadings);

} // namespace telemetry
void requestRoutesMetricReportCollection(App&);
void requestRoutesMetricReport(App&);
} // namespace redfish
