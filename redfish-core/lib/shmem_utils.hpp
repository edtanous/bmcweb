/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include "tal.hpp"

#include <nlohmann/json.hpp>
#include <unordered_set>

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

constexpr const char* metricReportDefinitionUri =
    "/redfish/v1/TelemetryService/MetricReportDefinitions";

constexpr const char* metricReportUri =
    "/redfish/v1/TelemetryService/MetricReports";

std::string gpuPrefix(platformGpuNamePrefix);
std::string platformDevicePrefix(PLATFORMDEVICEPREFIX);
std::string platformChassisName(PLATFORMCHASSISNAME);
std::string chassisName = platformDevicePrefix + "Chassis_";
std::string fpgaChassiName = platformDevicePrefix + "FPGA_";
std::string gpuName = platformDevicePrefix + gpuPrefix;
std::string nvSwitch = "NVSwitch_";
std::string pcieRetimer = platformDevicePrefix + "PCIeRetimer_";
std::string pcieSwtich = platformDevicePrefix + "PCIeSwitch_";
std::string processorModule = platformDevicePrefix + "ProcessorModule_";
std::string cpu = platformDevicePrefix + "CPU_";
std::string nvLink = "NVLink_";
std::string gpmInstances = "UtilizationPercent/";



inline std::string getSwitchId(const std::string &key) {
  // Use regular expressions to extract the numeric part after "NVSwitch_"
  std::regex  pattern("NVSwitch_(\\d+)");
  std::smatch match;
  std::string swithId = "";
  if (std::regex_search(key, match, pattern)) {
    swithId = match[1].str();
  }
  return swithId;
}

std::string replaceNumber(const std::string &input, const std::string &key,
                          const std::string     &value,
                          std::set<std::string> &replacedName) {
  std::regex  pattern(key + "(\\d+)");
  std::smatch match;
  std::string res = input;
  std::string out = "";
  bool firstMatch = false;
  if (value == "{BSWild}") {
    if (std::regex_search(res, match, pattern)) {
      std::string tmpPath = std::string("/redfish/v1/Chassis/");
      tmpPath += PLATFORMCHASSISNAME;
      tmpPath += "/Sensors/";
      tmpPath += "{BSWild}";

      size_t lastSlashPos = input.find_last_of('/');
      if (lastSlashPos != std::string::npos) {
        replacedName.insert(input.substr(lastSlashPos + 1));
      }
      out = tmpPath;
    }
  } else if (value == "{CWild}") {
    if (std::regex_search(res, match, pattern)) {
      if (!firstMatch) {
        std::string number = match[1].str();
        firstMatch = true;
        replacedName.insert(number);
      }
      size_t lastSlashPos = input.find_last_of('/');
      std::string dupSensorName = "";
      if (lastSlashPos != std::string::npos) {
        dupSensorName = input.substr(lastSlashPos + 1);
        std::regex expProcessorModule{"ProcessorModule_\\d"};
        dupSensorName = std::regex_replace(dupSensorName, expProcessorModule,
                                           "ProcessorModule_{PMWild}");

        std::regex expCpu{"CPU_\\d"};
        dupSensorName =
            std::regex_replace(dupSensorName, expCpu, "CPU_{CWild}");
      }
      std::string tmpPath = std::string("/redfish/v1/Chassis/");
      tmpPath += PLATFORMDEVICEPREFIX;
      tmpPath += "CPU_{CWild}";
      tmpPath += "/Sensors/";
      tmpPath += PLATFORMDEVICEPREFIX;
      tmpPath += dupSensorName;
      out = tmpPath;
    }
  } else if (value == "{PMWild}") {
    if (std::regex_search(res, match, pattern)) {
      if (!firstMatch) {
        std::string number = match[1].str();
        firstMatch = true;
        replacedName.insert(number);
      }
      size_t lastSlashPos = input.find_last_of('/');
      std::string dupSensorName = "";
      if (lastSlashPos != std::string::npos) {
        dupSensorName = input.substr(lastSlashPos + 1);
        std::regex expProcessorModule{"ProcessorModule_\\d"};
        dupSensorName = std::regex_replace(dupSensorName, expProcessorModule,
                                           "ProcessorModule_{PMWild}");
      }
      std::string tmpPath = std::string("/redfish/v1/Chassis/");
      tmpPath += PLATFORMDEVICEPREFIX;
      tmpPath += "ProcessorModule_{PMWild}";
      tmpPath += "/Sensors/";
      tmpPath += PLATFORMDEVICEPREFIX;
      tmpPath += dupSensorName;
      out = tmpPath;
    }
  } else {
    while (std::regex_search(res, match, pattern)) {
      if (!firstMatch) {
        std::string number = match[1].str();
        replacedName.insert(number);
        firstMatch = true;
      }
      res = std::regex_replace(res, pattern, key + value);
      out = res;
    }
  }
  return out;
}

void metricsReplacementsNonPlatformMetrics(const std::shared_ptr<bmcweb::AsyncResp> &asyncResp,
                                          std::vector<std::string> inputMetricProperties,
                                          const std::string& deviceType) {
  std::smatch match;
  std::set<int> nvSwitchId_Type_1;
  std::set<int> nvlinkId_Type_1;
  std::set<int> nvSwitchId_Type_2;
  std::set<int> nvlinkId_Type_2;
  std::set<int> gpuId;
  std::set<int> gpmInstance;
  nlohmann::json &wildCards = asyncResp->res.jsonValue["Wildcards"];
  for (const auto &e : inputMetricProperties) {
    if(deviceType == "NVSwitchPortMetrics" ){
        std::string switchType = getSwitchId(e);
        if((switchType == "0" || switchType == "3")){
            std::regex switchPattern(nvSwitch + "(\\d+)");
            if (std::regex_search(e, match, switchPattern)){
                int number = std::stoi(match[1].str());
                nvSwitchId_Type_1.insert(number);
            }
            std::regex nvLinkPattern( nvLink + "(\\d+)");
            if (std::regex_search(e, match, nvLinkPattern)){
                int number = std::stoi(match[1].str());
                nvlinkId_Type_1.insert(number);
            }
        }
        else if ((switchType == "1" || switchType == "2")){
            std::regex switchPattern( nvSwitch + "(\\d+)");
            if (std::regex_search(e, match, switchPattern)){
                int number = std::stoi(match[1].str());
                nvSwitchId_Type_2.insert(number);
            }
            std::regex nvLinkPattern( nvLink + "(\\d+)");
            if (std::regex_search(e, match, nvLinkPattern)){
                int number = std::stoi(match[1].str());
                nvlinkId_Type_2.insert(number);
            }
        }
    }
    if(deviceType == "NVSwitchMetrics"){
      std::regex switchPattern( nvSwitch + "(\\d+)");
      if (std::regex_search(e, match, switchPattern)){
          int number = std::stoi(match[1].str());
          nvSwitchId_Type_1.insert(number);
      }
    }
    if(deviceType == "MemoryMetrics" || deviceType == "ProcessorMetrics" || deviceType == "ProcessorGpmMetrics" || 
                      deviceType == "ProcessorPortMetrics" || deviceType == "ProcessorPortGpmMetrics" ){
      std::regex gpuPattern( gpuPrefix + "(\\d+)");
      if (std::regex_search(e, match, gpuPattern)){
          int number = std::stoi(match[1].str());
          gpuId.insert(number);
      }
    }
    if(deviceType == "ProcessorGpmMetrics" ){
      std::regex gpmInstancePattern( gpmInstances + "(\\d+)");
      if (std::regex_search(e, match, gpmInstancePattern)){
          int number = std::stoi(match[1].str());
          gpmInstance.insert(number);
      }
    }
    if(deviceType == "ProcessorPortMetrics" || deviceType == "ProcessorPortGpmMetrics" ){
      std::regex nvLinkPattern( nvLink + "(\\d+)");
      if (std::regex_search(e, match, nvLinkPattern)){
          int number = std::stoi(match[1].str());
          nvlinkId_Type_1.insert(number);
      }
    }
}
  if(deviceType == "NVSwitchPortMetrics" ){
    nlohmann::json devCountSwitchType_1 = nlohmann::json::array();
    for (const auto &e : nvSwitchId_Type_1) {
      devCountSwitchType_1.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "NVSwitchId_Type_1"},
      {"Values", devCountSwitchType_1},
    });
    nlohmann::json devCountNVlinkId_Type_1 = nlohmann::json::array();
    for (const auto &e : nvlinkId_Type_1) {
      devCountNVlinkId_Type_1.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "NvlinkId_Type_1"},
      {"Values", devCountNVlinkId_Type_1},
    });

    nlohmann::json devCountSwitchType_2 = nlohmann::json::array();
    for (const auto &e : nvSwitchId_Type_2) {
      devCountSwitchType_2.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "NVSwitchId_Type_2"},
      {"Values", devCountSwitchType_2},
    });
    nlohmann::json devCountNVlinkId_Type_2 = nlohmann::json::array();
    for (const auto &e : nvlinkId_Type_2) {
      devCountNVlinkId_Type_2.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "NvlinkId_Type_2"},
      {"Values", devCountNVlinkId_Type_2},
    });
  }
  if(deviceType == "NVSwitchMetrics" ){
    nlohmann::json devCountNVSwitchId = nlohmann::json::array();
    for (const auto &e : nvSwitchId_Type_1) {
      devCountNVSwitchId.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "NVSwitchId"},
      {"Values", devCountNVSwitchId},
    });
    }
  if(deviceType == "MemoryMetrics" || deviceType == "ProcessorMetrics" || deviceType == "ProcessorGpmMetrics" || 
                    deviceType == "ProcessorPortMetrics" || deviceType == "ProcessorPortGpmMetrics" ){
    nlohmann::json devCountGpuId = nlohmann::json::array();
    for (const auto &e : gpuId) {
      devCountGpuId.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "GpuId"},
      {"Values", devCountGpuId},
    });
  }
  if(deviceType == "ProcessorGpmMetrics" ){
    nlohmann::json devCountInstanceId = nlohmann::json::array();
    for (const auto &e : gpmInstance) {
      devCountInstanceId.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "InstanceId"},
      {"Values", devCountInstanceId},
    });
  }
  if(deviceType == "ProcessorPortMetrics" || deviceType == "ProcessorPortGpmMetrics" ){
    nlohmann::json devCountnvlinkId = nlohmann::json::array();
    for (const auto &e : nvlinkId_Type_1) {
      devCountnvlinkId.push_back(std::to_string(e));
    }
    wildCards.push_back({
      {"Name", "NvlinkId"},
      {"Values", devCountnvlinkId},
    });
  }
}

void metricsReplacements(std::vector<std::string> name,
                         const std::shared_ptr<bmcweb::AsyncResp> &asyncResp,
                         std::vector<std::string> inputMetricProperties) {
  std::set<std::string> metricProperties;
  nlohmann::json &wildCards = asyncResp->res.jsonValue["Wildcards"];
  std::set<std::string> wildCardValues;
  for (const auto &e : inputMetricProperties) {
    std::string metricProperty = replaceNumber(
        e, name[0], name[1], wildCardValues);
    if (metricProperty != "") {
      metricProperties.insert(metricProperty);
    }
  }
  // insert set to json payload here
  nlohmann::json devCount = nlohmann::json::array();
  for (const auto &e : wildCardValues) {
    devCount.push_back(e);
  }

  wildCards.push_back({
      {"Name", name[2]},
      {"Values", devCount},
  });
  for (const auto &property : metricProperties) {
    asyncResp->res.jsonValue["MetricProperties"].push_back(property);
  }
  return;
}

inline void
getShmemMetricsDefinition(const std::shared_ptr<bmcweb::AsyncResp> &asyncResp,
                          const std::string &metricId,
                          const std::string &deviceType) {
  BMCWEB_LOG_DEBUG("getShmemMetricsDefinitions :{}", metricId);

  std::vector<std::string> chassisPlatformEnvironmentMetricsReplacements = {
    {chassisName, "{BSWild}", "BSWild"}};
  std::vector<std::string> processorPlatformEnvironmentMetricsReplacements = {
    {processorModule, "{PMWild}", "PMWild"}};
  std::vector<std::string> cpuPlatformEnvironmentMetricsReplacements = {
    {cpu, "{CWild}", "CWild"}};
  std::vector<std::string> fpgaPlatformEnvironmentMetricsReplacements = {
    {fpgaChassiName, "{FWild}", "FWild"}};
  std::vector<std::string> gpuPlatformEnvironmentMetricsReplacements = {
    {gpuName, "{GWild}", "GWild"}};
  std::vector<std::string> nvSwitchPlatformEnvironmentMetricsReplacements = {
    {nvSwitch, "{NWild}", "NWild"}};
  std::vector<std::string> pcieRetimerPlatformEnvironmentMetricsReplacements = {
    {pcieRetimer, "{PRWild}", "PRWild"}};
  std::vector<std::string> pcieSwitchPlatformEnvironmentMetricsReplacements = {
    {pcieSwtich, "{PSWild}", "PSWild"}};

  try {
    const auto &values = tal::TelemetryAggregator::getAllMrds(metricId);
    std::vector<std::string> inputMetricProperties;
    std::unordered_set<std::string> inputMetricPropertiesSet;
    nlohmann::json wildCards = nlohmann::json::array();
    asyncResp->res.jsonValue["Wildcards"] = wildCards;
    for (const auto &e : values) {
      if(deviceType == "NVSwitchPortMetrics" || deviceType == "ProcessorPortMetrics" || deviceType == "ProcessorPortGpmMetrics"){
        std::string result = e.metricProperty;
        size_t pos = result.find("#");
        if (pos != std::string::npos) {
            result = result.substr(0, pos);
        }
        inputMetricPropertiesSet.insert(result);
      }
      else{
        inputMetricProperties.push_back(e.metricProperty);
      }
    }
    if(deviceType == "NVSwitchPortMetrics" || deviceType == "ProcessorPortMetrics" || deviceType == "ProcessorPortGpmMetrics" ){
      for (const auto &e : inputMetricPropertiesSet) {
        inputMetricProperties.push_back(e);
      }
    }

    if (deviceType == "PlatformEnvironmentMetrics") {
      nvSwitch = platformDevicePrefix + "NVSwitch_";
      asyncResp->res.jsonValue["@odata.type"] =
          "#MetricReportDefinition.v1_4_1.MetricReportDefinition";
      asyncResp->res.jsonValue["@odata.id"] =
          metricReportDefinitionUri + std::string("/") + metricId;
      asyncResp->res.jsonValue["Id"] = metricId;
      nlohmann::json metricPropertiesAray = nlohmann::json::array();
      asyncResp->res.jsonValue["MetricProperties"] = metricPropertiesAray;

      metricsReplacements(chassisPlatformEnvironmentMetricsReplacements,
                          asyncResp, inputMetricProperties);
      metricsReplacements(processorPlatformEnvironmentMetricsReplacements,
                          asyncResp, inputMetricProperties);
      metricsReplacements(cpuPlatformEnvironmentMetricsReplacements, asyncResp,
                          inputMetricProperties);
      metricsReplacements(fpgaPlatformEnvironmentMetricsReplacements, asyncResp,
                          inputMetricProperties);
      metricsReplacements(gpuPlatformEnvironmentMetricsReplacements, asyncResp,
                          inputMetricProperties);
      metricsReplacements(nvSwitchPlatformEnvironmentMetricsReplacements,
                          asyncResp, inputMetricProperties);
      metricsReplacements(pcieRetimerPlatformEnvironmentMetricsReplacements,
                          asyncResp, inputMetricProperties);
      metricsReplacements(pcieSwitchPlatformEnvironmentMetricsReplacements,
                          asyncResp, inputMetricProperties);

      asyncResp->res.jsonValue["MetricReport"]["@odata.id"] =
          metricReportUri + std::string("/") + metricId;
      asyncResp->res.jsonValue["MetricReportDefinitionType"] = "OnRequest";
      asyncResp->res.jsonValue["Name"] = metricId;
      std::vector<std::string> redfishReportActions;
      redfishReportActions.emplace_back("LogToMetricReportsCollection");
      asyncResp->res.jsonValue["ReportActions"] = redfishReportActions;
      asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
      asyncResp->res.jsonValue["ReportUpdates"] = "Overwrite";
    } else {
      nvSwitch = "NVSwitch_";
      metricsReplacementsNonPlatformMetrics(asyncResp, inputMetricProperties, deviceType);
    }
  } catch (std::exception const &e) {
    BMCWEB_LOG_ERROR("Exception while getting MRD values: {}", e.what());
    messages::resourceNotFound(asyncResp->res, "MetricReport", metricId);
  }
}

inline void getShmemMetricsReportCollection(
    const std::shared_ptr<bmcweb::AsyncResp> &asyncResp) {
  BMCWEB_LOG_ERROR("Exception while getShmemMetricsReportDefinition");
  try {
    const auto &values = tal::TelemetryAggregator::getMrdNamespaces();
    nlohmann::json &addMembers = asyncResp->res.jsonValue["Members"];
    for (std::string memoryMetricId : values) {
      // Get the metric object
      std::string metricReportDefUriPath =
        "/redfish/v1/TelemetryService/MetricReportDefinitions/";
      std::string uripath = metricReportDefUriPath + memoryMetricId;
      addMembers.push_back({{"@odata.id", uripath}});
    }
    asyncResp->res.jsonValue["Members@odata.count"] = addMembers.size();
  } catch (std::exception const &e) {
    BMCWEB_LOG_ERROR("Exception while getting MRD: {}", e.what());
  }
}

} // namespace shmem
} // namespace redfish
