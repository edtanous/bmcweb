/*!
 * @file    origin_utils.cpp
 * @brief   Source code for utility functions of handling origin of condition.
 */

#pragma once

#include <boost/algorithm/string.hpp>

#include <algorithm>

namespace redfish
{
namespace origin_utils
{

const std::string inventorySubTree = "/xyz/openbmc_project/inventory";
const std::string sensorSubTree = "/xyz/openbmc_project/sensors";
const std::string processorPrefix =
    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
const std::string memoryPrefix =
    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory/";
constexpr int searchDepth = 4;

/**
 * Utility function for populating async response with
 * origin of condition device for system events
 */
static void oocUtil(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                    const std::string& ooc, const std::string& id)
{
    if (!asyncResp->res.jsonValue.contains("Members"))
    {
        asyncResp->res.jsonValue["Links"]["OriginOfCondition"]["@odata.id"] =
            ooc;
    }
    else
    {
        for (auto& entry : asyncResp->res.jsonValue["Members"])
        {
            if (entry["Id"] == id)
            {
                entry["Links"]["OriginOfCondition"]["@odata.id"] = ooc;
                return;
            }
        }
    }
}

/**
 * Asynchronously converts DBus path to redfishURI
 * and populates origin of condition specifically for
 * paths that we need to query a particular chassis for
 * such as sensor devices or pcie devices
 */
static void asyncPathConversionChassisAssoc(
    const std::string& path, const std::string& redfishPrefix,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id,
    const std::string& endpointType)
{
    sdbusplus::message::object_path objPath(path);
    std::string name = objPath.filename();
    if (name.empty())
    {
        BMCWEB_LOG_ERROR << "File name empty for dbus path: " << path << "\n";
        return;
    }

    const std::string& deviceName = name;
    crow::connections::systemBus->async_method_call(
        [redfishPrefix, id, deviceName, path, endpointType,
         asyncResp{asyncResp}](const boost::system::error_code ec,
                               std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Failed: " << ec << "\n";
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            std::vector<std::string> paths = *data;
            std::string chassisPath;
            if (paths.size() > 0)
            {
                chassisPath = paths[0];
            }
            else
            {
                BMCWEB_LOG_ERROR << "Empty chassis paths for " << path << "\n";
                return;
            }

            BMCWEB_LOG_DEBUG << "chassis path is " << chassisPath << "\n";
            sdbusplus::message::object_path objPath(chassisPath);
            std::string chassisName = objPath.filename();
            BMCWEB_LOG_DEBUG << "chassis name is " << chassisName << "\n";

            std::string oocDevice = redfishPrefix;
            oocDevice.append(chassisName);
            oocDevice.append(endpointType);
            oocDevice.append(deviceName);

            BMCWEB_LOG_DEBUG << "oocDevice " << oocDevice << "\n";
            oocUtil(asyncResp, oocDevice, id);
        },
        "xyz.openbmc_project.ObjectMapper", path + "/chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * Wrapper function for setting origin of condition
 * based on DBus path that will walk through different
 * device methods as necessary to set OOC properly
 */
inline void convertDbusObjectToOriginOfCondition(
    const std::string& path,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id)
{
    sdbusplus::message::object_path objPath(path);
    std::string name = objPath.filename();
    if (name.empty())
    {
        BMCWEB_LOG_ERROR << "File name empty for dbus path: " << path << "\n";
        return;
    }

    const std::string& chassisPrefix = "/redfish/v1/Chassis/";
    if (boost::starts_with(path, sensorSubTree))
    {
        asyncPathConversionChassisAssoc(path, chassisPrefix, asyncResp, id,
                                        "/Sensors/");
        return;
    }

    const std::string& deviceName = name;
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, deviceName, id, chassisPrefix, path](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objects) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Failed: " << ec << "\n";
                return;
            }
            if (objects.empty())
            {
                BMCWEB_LOG_ERROR << "Objects empty!\n";
                return;
            }

            for (const auto& object : objects)
            {
                for (const auto& interfaces : object.second)
                {
                    if (interfaces ==
                        "xyz.openbmc_project.Inventory.Item.PCIeDevice")
                    {
                        asyncPathConversionChassisAssoc(path, chassisPrefix,
                                                        asyncResp, id,
                                                        "/PCIeDevices/");
                        return;
                    }
                    if (interfaces ==
                            "xyz.openbmc_project.Inventory.Item.Accelerator" ||
                        interfaces == "xyz.openbmc_project.Inventory.Item.Cpu")
                    {
                        oocUtil(asyncResp,
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Processors/" +
                                    deviceName,
                                id);
                        return;
                    }
                    if (interfaces == "xyz.openbmc_project.Inventory.Item.Dimm")
                    {
                        oocUtil(asyncResp,
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Memory/" +
                                    deviceName,
                                id);
                        return;
                    }
                    oocUtil(asyncResp, "/redfish/v1/Chassis/" + deviceName, id);
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 6>{
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.PCIeDevice",
            "xyz.openbmc_project.Inventory.Item.Cpu",
            "xyz.openbmc_project.Inventory.Item.Board",
            "xyz.openbmc_project.Inventory.Item.Chassis",
            "xyz.openbmc_project.Inventory.Item.Dimm"});
}

} // namespace origin_utils
} // namespace redfish
