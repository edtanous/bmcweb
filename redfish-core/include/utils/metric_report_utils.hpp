#pragma once
#include <utils/dbus_utils.hpp>
#include <utils/port_utils.hpp>

inline std::string getPropertySuffix(const std::string& ifaceName,
                                     const std::string& metricName)
{
    std::string suffix;
    // form redfish URI for device/sub device property
    if (ifaceName == "xyz.openbmc_project.Inventory.Item.Port")
    {
        if (metricName == "RXNoProtocolBytes")
        {
            suffix = "/Metrics#/Oem/Nvidia/RXNoProtocolBytes";
        }
        else if (metricName == "TXNoProtocolBytes")
        {
            suffix = "/Metrics#/Oem/Nvidia/TXNoProtocolBytes";
        }
        else if (metricName == "DataCRCCount")
        {
            suffix = "/Metrics#/Oem/Nvidia/NVLinkErrors/DataCRCCount";
        }
        else if (metricName == "FlitCRCCount")
        {
            suffix = "/Metrics#/Oem/Nvidia/NVLinkErrors/FlitCRCCount";
        }
        else if (metricName == "RecoveryCount")
        {
            suffix = "/Metrics#/Oem/Nvidia/NVLinkErrors/RecoveryCount";
        }
        else if (metricName == "ReplayErrorsCount")
        {
            suffix = "/Metrics#/Oem/Nvidia/NVLinkErrors/ReplayCount";
        }
        else if (metricName == "RuntimeError")
        {
            suffix = "/Metrics#/Oem/Nvidia/NVLinkErrors/RuntimeError";
        }
        else if (metricName == "TrainingError")
        {
            suffix = "/Metrics#/Oem/Nvidia/NVLinkErrors/TrainingError";
        }
        else if (metricName == "RXBytes")
        {
            suffix = "/Metrics#/RXBytes";
        }
        else if (metricName == "TXBytes")
        {
            suffix = "/Metrics#/TXBytes";
        }
        else if (metricName == "CurrentSpeed")
        {
            suffix = "#/CurrentSpeedGbps";
        }
        else if (metricName == "MaxSpeed")
        {
            suffix = "#/MaxSpeedGbps";
        }
        else if (metricName == "LinkStatus")
        {
            suffix = "#/LinkStatus";
        }
        else if (metricName == "TXWidth")
        {
            suffix = "#/Oem/Nvidia/TXWidth";
        }
        else if (metricName == "RXWidth")
        {
            suffix = "#/Oem/Nvidia/RXWidth";
        }
    }
    else if (ifaceName == "xyz.openbmc_project.State.ProcessorPerformance")
    {
        if (metricName == "ThrottleReason")
        {
            suffix = "/Oem/Nvidia/ThrottleReasons";
        }
    }
    else if (ifaceName == "xyz.openbmc_project.PCIe.PCIeECC")
    {
        if (metricName == "nonfeCount")
        {
            suffix = "/PCIeErrors/NonFatalErrorCount";
        }
        else if (metricName == "feCount")
        {
            suffix = "/PCIeErrors/FatalErrorCount";
        }
        else if (metricName == "ceCount" || metricName == "PCIeECC.ceCount")
        {
            suffix = "/PCIeErrors/CorrectableErrorCount";
        }
        else if (metricName == "L0ToRecoveryCount")
        {
            suffix = "/PCIeErrors/L0ToRecoveryCount";
        }
        else if (metricName == "NAKReceivedCount")
        {
            suffix = "/PCIeErrors/NAKReceivedCount";
        }
        else if (metricName == "ReplayCount")
        {
            suffix = "/PCIeErrors/ReplayCount";
        }
        else if (metricName == "NAKSentCount")
        {
            suffix = "/PCIeErrors/NAKSentCount";
        }
        else if (metricName == "ReplayRolloverCount")
        {
            suffix = "/PCIeErrors/ReplayRolloverCount";
        }
        else if (metricName == "PCIeType")
        {
            suffix = "#/PCIeInterface/PCIeType";
        }
        else if (metricName == "MaxLanes")
        {
            suffix = "#/PCIeInterface/MaxLanes";
        }
    }
    else if (ifaceName == "xyz.openbmc_project.Memory.MemoryECC")
    {
        if (metricName == "ueCount")
        {
            suffix = "/UncorrectableECCErrorCount";
        }
        else if (metricName == "ceCount")
        {
            suffix = "/CorrectableECCErrorCount";
        }
    }
    else if (ifaceName ==
             "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig")
    {
        if (metricName == "Utilization")
        {
            suffix = "/BandwidthPercent";
        }
        else if (metricName == "OperatingSpeed")
        {
            suffix = "/OperatingSpeedMHz";
        }
    }
    else if (ifaceName == "xyz.openbmc_project.Inventory.Item.Dimm")
    {
        if (metricName == "MemoryConfiguredSpeedInMhz")
        {
            suffix = "/OperatingSpeedMHz";
        }
        else if (metricName == "Utilization")
        {
            suffix = "/BandwidthPercent";
        }
    }
    else if (ifaceName == "xyz.openbmc_project.Inventory.Item.PCIeDevice")
    {
        if (metricName == "PCIeType")
        {
            suffix = "#/PCIeInterface/PCIeType";
        }
        else if (metricName == "MaxLanes")
        {
            suffix = "#/PCIeInterface/MaxLanes";
        }
    }
    else if (ifaceName == "com.nvidia.MemoryRowRemapping")
    {
        if (metricName == "ueRowRemappingCount")
        {
            suffix = "/Oem/Nvidia/RowRemapping/UncorrectableRowRemappingCount";
        }
        else if (metricName == "ceRowRemappingCount")
        {
            suffix = "/Oem/Nvidia/RowRemapping/CorrectableRowRemappingCount";
        }
        else if (metricName == "RowRemappingFailureState")
        {
            suffix = "/Oem/Nvidia/RowRemappingFailed";
        }
    }
    else
    {
        suffix.clear();
    }
    return suffix;
}

std::string
    generateURI(const std::string& deviceType, const std::string& deviceName,
                const std::string& subDeviceName, const std::string& devicePath,
                const std::string& metricName, const std::string& ifaceName)
{
    std::string metricURI;
    std::string propSuffix;
    // form redfish URI for sub device
    if (deviceType == "ProcessorPortMetrics")
    {
        metricURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
        metricURI += "/Processors/";
        metricURI += deviceName;
        metricURI += "/Ports/";
        metricURI += subDeviceName;
        propSuffix = getPropertySuffix(ifaceName, metricName);
    }
    else if (deviceType == "NVSwitchPortMetrics")
    {
       metricURI = "/redfish/v1/Fabrics/" PLATFORMDEVICEPREFIX;
       metricURI += "NVLinkFabric_0/Switches/";
       metricURI += deviceName;
       metricURI += "/Ports/";
       metricURI += subDeviceName;
       propSuffix = getPropertySuffix(ifaceName, metricName);
    }
    else if (deviceType == "ProcessorMetrics")
    {
        metricURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
        metricURI += "/Processors/";
        metricURI += deviceName;
        metricURI += "/ProcessorMetrics#";
        if (ifaceName == "xyz.openbmc_project.Memory.MemoryECC")
        {
            metricURI += "/CacheMetricsTotal/LifeTime";
        }
        else if (ifaceName == "xyz.openbmc_project.PCIe.PCIeECC")
        {
            if (metricName == "PCIeType" || metricName == "MaxLanes")
            {
                sdbusplus::message::object_path deviceObjectPath(devicePath);
                const std::string childDeviceName = deviceObjectPath.filename();
                std::string parentDeviceName = PLATFORMDEVICEPREFIX;
                parentDeviceName += childDeviceName;
                metricURI = "/redfish/v1/Chassis/";
                metricURI += parentDeviceName;
                metricURI += "/PCIeDevices/";
                metricURI += childDeviceName;
            }
        }
        propSuffix = getPropertySuffix(ifaceName, metricName);
    }
    else if (deviceType == "NVSwitchMetrics")
    {
        metricURI = "/redfish/v1/Fabrics/" PLATFORMDEVICEPREFIX;
        metricURI += "NVLinkFabric_0/Switches/";
        metricURI += deviceName;
        metricURI += "/SwitchMetrics#";
        if (ifaceName == "xyz.openbmc_project.Memory.MemoryECC")
        {
            metricURI += "/InternalMemoryMetrics/LifeTime";
        }
        propSuffix = getPropertySuffix(ifaceName, metricName);
    }
    else if (deviceType == "MemoryMetrics")
    {
        metricURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID;
        metricURI += "/Memory/";
        metricURI += deviceName;
        if (ifaceName == "com.nvidia.MemoryRowRemapping")
        {
            if (metricName == "RowRemappingFailureState" ||
                metricName == "RowRemappingPendingState")
            {
                metricURI += "#";
            }
            else
            {
                metricURI += "/MemoryMetrics#";
            }
        }
        else if (ifaceName == "xyz.openbmc_project.Memory.MemoryECC")
        {
            metricURI += "/MemoryMetrics#/LifeTime";
        }
        else
        {
            metricURI += "/MemoryMetrics#";
        }
        propSuffix = getPropertySuffix(ifaceName, metricName);
    }
    else
    {
        metricURI.clear();
    }

    // Check if property Suffix is empty then property doesn;t exist
    if (!propSuffix.empty())
    {
        metricURI += propSuffix;
    }
    else
    {
        metricURI.clear();
    }
    return metricURI;
}

static inline std::string toPCIeType(const std::string& pcieType)
{
    if (pcieType ==
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen1")
    {
        return "Gen1";
    }
    if (pcieType ==
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen2")
    {
        return "Gen2";
    }
    if (pcieType ==
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen3")
    {
        return "Gen3";
    }
    if (pcieType ==
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen4")
    {
        return "Gen4";
    }
    if (pcieType ==
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen5")
    {
        return "Gen5";
    }
    // Unknown or others
    return "Unknown";
}

std::string translateReading(const std::string& ifaceName,
                             const std::string& metricName,
                             const std::string& reading)
{
    std::string metricValue;
    if (ifaceName == "xyz.openbmc_project.State.ProcessorPerformance")
    {
        if (metricName == "ThrottleReason")
        {
            metricValue = redfish::dbus_utils::toReasonType(reading);
        }
    }
    else if (ifaceName == "xyz.openbmc_project.PCIe.PCIeECC")
    {
        if (metricName == "PCIeType")
        {
            metricValue = toPCIeType(reading);
        }
    }
    else if (ifaceName == "xyz.openbmc_project.Inventory.Item.Port")
    {
        if (metricName == "LinkStatus")
        {
            metricValue = redfish::port_utils::getLinkStatusType(reading);
        }
    }
    else
    {
        metricValue = reading;
    }
    return metricValue;
}
void getMetricValue(const std::string& deviceType,
                    const std::string& deviceName,
                    const std::string& subDeviceName,
                    const std::string& devicePath,
                    const std::string& metricName, const std::string& ifaceName,
                    const dbus::utility::DbusVariantType& value,
                    const uint64_t& t, nlohmann::json& resArray)
{
    nlohmann::json thisMetric = nlohmann::json::object();
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
        t;

    if (const std::vector<std::string>* readingArray =
            std::get_if<std::vector<std::string>>(&value))
    {
        // This is for the property whose value is of type list and each element
        // in the list on the redfish is represented with
        // "PropertyName/<index_of_list_element>". and it always starts with 0
        // Eg:- ThrottleReasosns: [Idle, AppClock]-> "Idle" maps to ThrottleReasons/0
        int i = 0;
        for (const std::string& reading : *readingArray)
        {
            std::string val = translateReading(ifaceName, metricName, reading);
            thisMetric["MetricValue"] = val;
            std::string metricProp =
                generateURI(deviceType, deviceName, subDeviceName, devicePath,
                            metricName, ifaceName);
            metricProp += "/";
            metricProp += std::to_string(i);
            thisMetric["MetricProperty"] = metricProp;
            thisMetric["Timestamp"] = redfish::time_utils::getDateTimeUintMs(
                sensorUpdatetimeSystemClock);
            resArray.push_back(thisMetric);
            i++;
        }
    }
    else
    {
        const std::string metricURI =
            generateURI(deviceType, deviceName, subDeviceName, devicePath,
                        metricName, ifaceName);
        if (metricURI.empty())
        {
            return;
        }
        thisMetric["MetricProperty"] = metricURI;
        thisMetric["Timestamp"] =
            redfish::time_utils::getDateTimeUintMs(sensorUpdatetimeSystemClock);
        if (const std::string* reading = std::get_if<std::string>(&value))
        {
            std::string val = translateReading(ifaceName, metricName, *reading);
            thisMetric["MetricValue"] = val;
        }
        else if (const int* reading = std::get_if<int>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const int16_t* reading = std::get_if<int16_t>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const int64_t* reading = std::get_if<int64_t>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const uint16_t* reading = std::get_if<uint16_t>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const uint32_t* reading = std::get_if<uint32_t>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const uint64_t* reading = std::get_if<uint64_t>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const double* reading = std::get_if<double>(&value))
        {

            thisMetric["MetricValue"] = std::to_string(*reading);
        }
        else if (const bool* reading = std::get_if<bool>(&value))
        {
            thisMetric["MetricValue"] = "false";
            if (*reading == true)
            {
                thisMetric["MetricValue"] = "true";
            }
        }
        resArray.push_back(thisMetric);
    }
}

inline std::string getKeyNameonTimeStampIface(const std::string& ifaceName)
{
    size_t pos = ifaceName.find_last_of('.');
    if (pos == std::string::npos)
    {
        pos = 0;
    }
    else
    {
        pos++;
    }
    // "Port"
    std::string iface = ifaceName.substr(pos);
    return iface;
}