#pragma once
#include <boost/regex.hpp>

namespace redfish
{
namespace nvidia_metric_report_def_utils
{

constexpr const char* metricReportDefinitionUri =
    "/redfish/v1/TelemetryService/MetricReportDefinitions";

constexpr const char* metricReportUri =
    "/redfish/v1/TelemetryService/MetricReports";

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
            //index for sensor
            int i=0;
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
            // PlatformEnvMetric doesn't contain Voltage sensor therfore skipping voltage sensor from metric def
            if(dupSensorName.find("Voltage")!=std::string::npos)
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
            boost::replace_all(dupSensorName, chassisId, "PCIeRetimer_{PRWild}");
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
            boost::replace_all(dupSensorName, chassisId, "ProcessorModule_{PMWild}");
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "ProcessorModule_{PMWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }
        else if (chassisId.find("CPU") != std::string::npos)
        {
            if((dupSensorName.find("Temp")==std::string::npos) &&
                (dupSensorName.find("Energy")==std::string::npos) &&
                (dupSensorName.find("Power")==std::string::npos))
            {
                continue;
            }

            boost::regex expProcessorModule{"ProcessorModule_\\d"};
            dupSensorName = boost::regex_replace(dupSensorName, expProcessorModule, "ProcessorModule_{PMWild}");

            boost::regex expCpu{"CPU_\\d"};
            dupSensorName = boost::regex_replace(dupSensorName, expCpu, "CPU_{CWild}");

            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += "CPU_{CWild}";
            tmpPath += "/Sensors/";
            tmpPath += PLATFORMDEVICEPREFIX;
            tmpPath += dupSensorName;
        }

        if (std::find(metricProperties.begin(), metricProperties.end(),
                      tmpPath) == metricProperties.end())
        {
            if(strcmp(tmpPath.c_str(), "/redfish/v1/Chassis/") != 0)
            {
                asyncResp->res.jsonValue["MetricProperties"].push_back(tmpPath);
            }
        }
    }
}

inline void processChassisSensorsMetric(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisPath, bool recursive=false)
{
    auto getAllChassisHandler =
        [asyncResp,
         chassisPath, recursive](const boost::system::error_code ec,
                      std::variant<std::vector<std::string>>& chassisLinks) {
            std::vector<std::string> chassisPaths;
            if (ec)
            {
                // no chassis links = no failures
                BMCWEB_LOG_ERROR("getAllChassisSensors DBUS error: {}", ec);
            }
            // Add parent chassis to the list
            if(!recursive) {
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
                    //process nest chassis. e.g CPU and GPU under ProcessorModule chassis
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
                            BMCWEB_LOG_ERROR("getAllChassisSensors DBUS error: {}", ec);
                            return;
                        }
                        const std::vector<std::string>* sensorPaths =
                            std::get_if<std::vector<std::string>>(
                                &(variantEndpoints));
                        if (sensorPaths == nullptr)
                        {
                            BMCWEB_LOG_ERROR("getAllChassisSensors empty sensors list");
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

                            if((name == "NWild" && chassisId.find("NVSwitch") != std::string::npos) ||
                            (name == "PRWild" && chassisId.find("PCIeRetimer") != std::string::npos) ||
                            (name == "PSWild" && chassisId.find("PCIeSwitch") != std::string::npos) ||
                            (name == "PMWild" && chassisId.find("ProcessorModule") != std::string::npos) ||
                            (name == "CWild" && chassisId.find("CPU") != std::string::npos))
                            {
                                size_t v = itemObj["Values"].size();
                                itemObj["Values"].push_back(std::to_string(v));
                            }
                            else if((name == "GWild" && chassisId.find("GPU") != std::string::npos))
                            {
                                size_t v = itemObj["Values"].size() + 1;
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
                BMCWEB_LOG_ERROR("getPlatformMetricsProperties respHandler DBUS error: {}", ec);
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& chassisPath : chassisPaths)
            {
                sdbusplus::message::object_path path(chassisPath);
                const std::string& chassisName = path.filename();
                if (chassisName.empty())
                {
                    BMCWEB_LOG_ERROR("Failed to find '/' in {}", chassisPath);
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
        metricReportDefinitionUri + std::string("/") + id;
    asyncResp->res.jsonValue["Id"] = id;
    asyncResp->res.jsonValue["Name"] = id;
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] =
        metricReportUri + std::string("/") + id;

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

// This code is added to form platform independent URIs for the aggregated metric properties
inline std::string getMemoryMetricURIDef(std::string &propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    std::string gpuPrefix(platformGpuNamePrefix);
    if (propertyName == "RowRemappingFailed")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0#/Oem/Nvidia/RowRemappingFailed";
    }
    else if (propertyName == "OperatingSpeedMHz")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0/MemoryMetrics#/OperatingSpeedMHz";
    }
    else if (propertyName == "BandwidthPercent")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0/MemoryMetrics#/BandwidthPercent";
    }
    else if (propertyName == "CorrectableECCErrorCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0/MemoryMetrics#/LifeTime/CorrectableECCErrorCount";
    }
    else if (propertyName == "UncorrectableECCErrorCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0/MemoryMetrics#/LifeTime/UncorrectableECCErrorCount";
    }
    else if (propertyName == "CorrectableRowRemappingCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0/MemoryMetrics#/Oem/Nvidia/RowRemapping/CorrectableRowRemappingCount";
    }
    else if (propertyName == "UncorrectableRowRemappingCount")
    {
        propURI +=
            "/Memory/" + gpuPrefix + "{GpuId}_DRAM_0/MemoryMetrics#/Oem/Nvidia/RowRemapping/UncorrectableRowRemappingCount";
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

inline std::string getProcessorMetricURIDef(std::string &propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    std::string gpuPrefix(platformGpuNamePrefix);
    if (propertyName == "State")
    {
        propURI += "/Processors/" + gpuPrefix + "{GpuId}#/Status/State";
    }
    else if (propertyName == "PCIeType")
    {
        propURI += "/Processors/" + gpuPrefix + "{GpuId}#/SystemInterface/PCIe/PCIeType";
    }
    else if (propertyName == "MaxLanes")
    {
        propURI += "/Processors/" + gpuPrefix + "{GpuId}#/SystemInterface/PCIe/MaxLanes";
    }
    else if (propertyName == "OperatingSpeedMHz")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/OperatingSpeedMHz";
    }
    else if (propertyName == "BandwidthPercent")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/BandwidthPercent";
    }
    else if (propertyName == "CorrectableECCErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/CacheMetricsTotal/LifeTime/CorrectableECCErrorCount";
    }
    else if (propertyName == "UncorrectableECCErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/CacheMetricsTotal/LifeTime/UncorrectableECCErrorCount";
    }
    else if (propertyName == "CorrectableErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/CorrectableErrorCount";
    }
    else if (propertyName == "NonFatalErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/NonFatalErrorCount";
    }
    else if (propertyName == "FatalErrorCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/FatalErrorCount";
    }
    else if (propertyName == "L0ToRecoveryCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/L0ToRecoveryCount";
    }
    else if (propertyName == "ReplayCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/ReplayCount";
    }
    else if (propertyName == "ReplayRolloverCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/ReplayRolloverCount";
    }
    else if (propertyName == "NAKSentCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/NAKSentCount";
    }
    else if (propertyName == "NAKReceivedCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/PCIeErrors/NAKReceivedCount";
    }
    else if (propertyName == "ThrottleReasons")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/ProcessorMetrics#/Oem/Nvidia/ThrottleReasons";
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

inline std::string getNVSwitchMetricURIDef(std::string &propertyName)
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

inline std::string getProcessorPortMetricURIDef(std::string &propertyName)
{
    std::string propURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
    std::string gpuPrefix(platformGpuNamePrefix);
    if (propertyName == "CurrentSpeedGbps")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}#/CurrentSpeedGbps";
    }
    else if (propertyName == "TXBytes")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/TXBytes";
    }
    else if (propertyName == "RXBytes")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/RXBytes";
    }
    else if (propertyName == "TXNoProtocolBytes")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/TXNoProtocolBytes";
    }
    else if (propertyName == "RXNoProtocolBytes")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/RXNoProtocolBytes";
    }
    else if (propertyName == "RuntimeError")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RuntimeError";
    }
    else if (propertyName == "TrainingError")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/TrainingError";
    }
    else if (propertyName == "ReplayCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/ReplayCount";
    }
    else if (propertyName == "RecoveryCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/RecoveryCount";
    }
    else if (propertyName == "FlitCRCCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/FlitCRCCount";
    }
    else if (propertyName == "DataCRCCount")
    {
        propURI +=
            "/Processors/" + gpuPrefix + "{GpuId}/Ports/NVLink_{NvlinkId}/Metrics#/Oem/Nvidia/NVLinkErrors/DataCRCCount";
    }
    return propURI;
}

inline std::string getNVSwitchPortMetricURIDef(std::string &propertyName)
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
                BMCWEB_LOG_DEBUG("DBUS response error: {}", ec);
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
                        if( deviceName.find("C2C_") != std::string::npos)
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
                deviceType == "NVSwitchPortMetrics" ||
                deviceType == "ProcessorPortGpmMetrics")
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
        deviceType != "NVSwitchPortMetrics" &&
        deviceType != "ProcessorGpmMetrics" &&
        deviceType != "ProcessorPortGpmMetrics")
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.id"] =
        metricReportDefinitionUri + std::string("/") + id;
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
    asyncResp->res.jsonValue["MetricReport"]["@odata.id"] = metricReportUri + std::string("/") + id;
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
                BMCWEB_LOG_DEBUG("DBUS response error: {}", ec);
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
                        std::string processorPortGpmMetricId =
                            PLATFORMDEVICEPREFIX "ProcessorPortGPMMetrics_0";
                        std::string processorGpmMetricId =
                            PLATFORMDEVICEPREFIX "ProcessorGPMMetrics_0";

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

inline void getMetricReportCollection(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
     BMCWEB_LOG_DEBUG("getMetricReportCollection");
     crow::connections::systemBus->async_method_call(
        [asyncResp](boost::system::error_code ec,
        const std::vector<std::string>& metricPaths) mutable {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
        nlohmann::json& addMembers = asyncResp->res.jsonValue["Members"];

        for(const std::string& object : metricPaths)
        {
            // Get the metric object
            std::string metricReportDefUriPath =
                "/redfish/v1/TelemetryService/MetricReportDefinitions/";
            if (boost::ends_with(object, "platformmetrics"))
            {
                std::string uripath = metricReportDefUriPath;
                uripath += PLATFORMMETRICSID;
                    if(!containsJsonObject(addMembers, "@odata.id", uripath))
                    {
                    addMembers.push_back({{"@odata.id", uripath}});
                    }
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

                std::string processorGpmMetricId =
                    PLATFORMDEVICEPREFIX "ProcessorGPMMetrics";
                processorGpmMetricId += "_0";
                uripath =
                    metricReportDefUriPath + processorGpmMetricId;
                addMembers.push_back({{"@odata.id", uripath}});

                std::string processorportGpmMetricId =
                    PLATFORMDEVICEPREFIX "ProcessorPortGPMMetrics";
                processorportGpmMetricId += "_0";
                uripath = metricReportDefUriPath +
                            processorportGpmMetricId;
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
        asyncResp->res.jsonValue["Members@odata.count"] = addMembers.size();
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Sensor.Aggregation"});
    }
} // nvidia_chassis_utils
} // namespace redfish