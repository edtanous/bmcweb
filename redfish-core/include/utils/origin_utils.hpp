/*!
 * @file    origin_utils.cpp
 * @brief   Source code for utility functions of handling origin of condition.
 */

#pragma once

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <chrono>

namespace redfish
{
namespace origin_utils
{

const std::string inventorySubTree = "/xyz/openbmc_project/inventory";
const std::string sensorSubTree = "/xyz/openbmc_project/sensors";

// All the Chassis Devices follows pattern:
// "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1/PCIeDevices/GPU_SXM_1"
// or "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1"

// We find "/xyz/openbmc_project/inventory/system/chassis/" and remove it to use
// the device path: "HGX_GPU_SXM_1/PCIeDevices/GPU_SXM_1" and create redfish
// URI:
// "/redfish/v1/Chassis/HGX_GPU_SXM_1/PCIeDevices/GPU_SXM_1"
const std::string chassisPrefixDbus =
    "/xyz/openbmc_project/inventory/system/chassis/";
const std::string chassisPrefix = "/redfish/v1/Chassis/";

// All the Fabric Devices follows pattern:

// "/xyz/openbmc_project/inventory/system/fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_0/Ports"

// We find "/xyz/openbmc_project/inventory/system/fabrics/" and remove it to use
// the device path: "HGX_NVLinkFabric_0/Switches/NVSwitch_0/Ports" and create
// redfish URI:
// "/redfish/v1/Fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_0/Ports"
const std::string fabricsPrefixDbus =
    "/xyz/openbmc_project/inventory/system/fabrics/";
const std::string fabricsPrefix = "/redfish/v1/Fabrics/";

// All the Memory Devices follows pattern:

// "/xyz/openbmc_project/inventory/system/memory/GPU_SXM_1_DRAM_0"

// We find "/xyz/openbmc_project/inventory/system/memory/" and remove it to use
// the device path: "GPU_SXM_1_DRAM_0" and create redfish URI:
// "/redfish/v1/Systems/HGX_Baseboard_0/Memory/GPU_SXM_1_DRAM_0"
const std::string memoryPrefixDbus =
    "/xyz/openbmc_project/inventory/system/memory/";
const std::string memoryPrefix =
    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory/";

// All the Processor Devices follows pattern:

// "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1/Ports/NVLink_0"

// We find "/xyz/openbmc_project/inventory/system/processors/" and remove it to
// use the device path: "GPU_SXM_1/Ports/NVLink_0" and create redfish URI:
// "/redfish/v1/Systems/HGX_Baseboard_0/Processors/GPU_SXM_1/Ports/NVLink_0"
const std::string processorPrefixDbus =
    "/xyz/openbmc_project/inventory/system/processors/";
const std::string processorPrefix =
    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";

// All the Processor Devices follows pattern:

// "/xyz/openbmc_project/software/HGX_FW_FPGA_0"

// We find "/xyz/openbmc_project/software/" and remove it to use the device
// path: "HGX_FW_FPGA_0" and create redfish URI:
// "/redfish/v1/UpdateService/FirmwareInventory/HGX_FW_FPGA_0"
const std::string softwarePrefixDbus = "/xyz/openbmc_project/software/";
const std::string firmwarePrefix =
    "/redfish/v1/UpdateService/FirmwareInventory/";

std::map<std::string, std::string> dBusToRedfishURI = {
    {chassisPrefixDbus, chassisPrefix},
    {fabricsPrefixDbus, fabricsPrefix},
    {processorPrefixDbus, processorPrefix},
    {memoryPrefixDbus, memoryPrefix},
    {softwarePrefixDbus, firmwarePrefix}};

/**
 * Utility function for populating async response with
 * service conditions json containing origin of condition
 * device
 */

static void oocUtilServiceConditions(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& ooc,
    const std::string& messageArgs, const std::string& timestamp,
    const std::string& severity, const std::string& id,
    const std::string& messageId)
{
    nlohmann::json j;
    std::string arg0 = messageArgs.substr(0, messageArgs.find(','));
    std::string arg1 =
        messageArgs.substr(messageArgs.find(',') + 1, messageArgs.length());
    boost::trim(arg0);
    boost::trim(arg1);
    j["MessageArgs"] = nlohmann::json::array();
    j["MessageArgs"].push_back(arg0);
    j["MessageArgs"].push_back(arg1);
    j["MessageId"] = messageId;
    if (messageId == "ResourceEvent.1.0.ResourceErrorThresholdExceeded")
    {
        j["Message"] = "The resource property " + arg0 +
                       " has exceeded error threshold of value " + arg1 + ". ";
    }
    else
    {
        j["Message"] = "The resource property " + arg0 +
                       " has detected errors of type '" + arg1 + "'.";
    }
    j["Timestamp"] = timestamp;
    j["Severity"] = severity;
    j["OriginOfCondition"]["@odata.id"] = ooc;
    j["LogEntry"]["@odata.id"] = "/redfish/v1/Systems/" PLATFORMSYSTEMID "/"
                                 "LogServices/EventLog/Entries/" +
                                 id;

    BMCWEB_LOG_DEBUG << "Populating service conditions with ooc " << ooc
                     << "\n";

    if (asyncResp->res.jsonValue.contains("Conditions"))
    {
        for (auto& elem : asyncResp->res.jsonValue["Conditions"])
        {
            if (elem.contains("LogEntry") &&
                elem["LogEntry"]["@odata.id"] == j["LogEntry"]["@odata.id"])
            {
                return;
            }
        }
        asyncResp->res.jsonValue["Conditions"].push_back(j);
    }
    else
    {
        for (auto& elem : asyncResp->res.jsonValue["Status"]["Conditions"])
        {
            if (elem.contains("LogEntry") &&
                elem["LogEntry"]["@odata.id"] == j["LogEntry"]["@odata.id"])
            {
                return;
            }
        }
        asyncResp->res.jsonValue["Status"]["Conditions"].push_back(j);
    }
}

/**
 * Utility function for populating async response with
 * origin of condition device for system events
 */

static void oocUtil(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                    nlohmann::json& logEntry, const std::string& id,
                    const std::string& ooc, const std::string& severity = "",
                    const std::string& messageArgs = "",
                    const std::string& timestamp = "",
                    const std::string& messageId = "")
{
    if (!severity.empty())
    {
        oocUtilServiceConditions(asyncResp, ooc, messageArgs, timestamp,
                                 severity, id, messageId);
        return;
    }

    if (!asyncResp->res.jsonValue.contains("Members"))
    {
        asyncResp->res.jsonValue["Links"]["OriginOfCondition"]["@odata.id"] =
            ooc;
        return;
    }

    logEntry["Links"]["OriginOfCondition"]["@odata.id"] = ooc;

    return;
}

/**
 * Wrapper function for setting origin of condition
 * based on DBus path that will walk through different
 * device methods as necessary to set OOC properly
 */
inline void convertDbusObjectToOriginOfCondition(
    const std::string& path, const std::string& id,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    nlohmann::json& logEntry, const std::string& severity = "",
    const std::string& messageArgs = "", const std::string& timestamp = "",
    const std::string& messageId = "")
{
    sdbusplus::message::object_path objPath(path);
    std::string deviceName = objPath.filename();
    if (deviceName.empty())
    {
        BMCWEB_LOG_DEBUG
            << "Empty OriginOfCondition provided to convertDbusObjectToOriginOfCondition"
            << "For path: " << path << "\n";
        return;
    }

    for (auto& it : dBusToRedfishURI)
    {
        if (path.find(it.first) != std::string::npos)
        {
            std::string newPath = path.substr(it.first.length(), path.length());
            oocUtil(asyncResp, logEntry, id, it.second + newPath, severity,
                    messageArgs, timestamp, messageId);
            return;
        }
    }

    BMCWEB_LOG_ERROR
        << "No Matching prefix found for OriginOfCondition DBus object Path: "
        << path << "\n";
    return;
}

} // namespace origin_utils
} // namespace redfish
