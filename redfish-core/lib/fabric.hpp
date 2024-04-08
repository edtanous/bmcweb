/*
// Copyright (c) 2021, NVIDIA Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "redfish_util.hpp"

#include <app.hpp>
#include <boost/format.hpp>
#include <utils/collection.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/port_utils.hpp>
#include <utils/processor_utils.hpp>

#include <variant>

namespace redfish
{

constexpr const char* inventoryRootPath =
    "/xyz/openbmc_project/inventory/system";
constexpr const char* inventoryFabricStr = "/fabrics/";
constexpr const char* inventorySwitchStr = "/Switches/";
constexpr const char* inventoryPortStr = "/Ports/";

inline std::string getSwitchType(const std::string& switchType)
{
    if (switchType ==
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.Ethernet")
    {
        return "Ethernet";
    }
    if (switchType == "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.FC")
    {
        return "FC";
    }
    if (switchType ==
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink")
    {
        return "NVLink";
    }
    if (switchType ==
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.OEM")
    {
        return "OEM";
    }
    if (switchType ==
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.PCIe")
    {
        return "PCIe";
    }

    // Unknown or others
    return "";
}

inline std::string getFabricType(const std::string& fabricType)
{
    if (fabricType ==
        "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.Ethernet")
    {
        return "Ethernet";
    }
    if (fabricType == "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.FC")
    {
        return "FC";
    }
    if (fabricType ==
        "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.NVLink")
    {
        return "NVLink";
    }
    if (fabricType ==
        "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.OEM")
    {
        return "OEM";
    }
    if (fabricType ==
        "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.PCIe")
    {
        return "PCIe";
    }

    // Unknown or others
    return "";
}

inline std::string getZoneType(const std::string& zoneType)
{
    if (zoneType == "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.Default")
    {
        return "Default";
    }
    if (zoneType ==
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfEndpoints")
    {
        return "ZoneOfEndpoints";
    }
    if (zoneType ==
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones")
    {
        return "ZoneOfZones";
    }
    if (zoneType ==
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfResourceBlocks")
    {
        return "ZoneOfResourceBlocks";
    }
    // Unknown or others
    return "";
}

/**
 * @brief Get all switch info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp   Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       processorId    processor id for redfish URI.
 */
inline void
    getConnectedPortLinks(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                          const std::string& objPath,
                          const std::string& processorId)
{
    BMCWEB_LOG_DEBUG("Get Connected Port Links");
    BMCWEB_LOG_DEBUG("{}", objPath);
    crow::connections::systemBus->async_method_call(
        [aResp, processorId](const boost::system::error_code ec,
                             std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no endpoint = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        nlohmann::json& linksArray =
            aResp->res.jsonValue["Links"]["ConnectedPorts"];
        BMCWEB_LOG_DEBUG("populating ConnectedPorts");
        linksArray = nlohmann::json::array();
        for (const std::string& portPath : *data)
        {
            sdbusplus::message::object_path objPath(portPath);
            const std::string& endpointId = objPath.filename();
            BMCWEB_LOG_DEBUG("{}", endpointId);
            std::string endpointURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                      "/Processors/";
            endpointURI += processorId;
            endpointURI += "/Ports/";
            endpointURI += endpointId;
            linksArray.push_back({{"@odata.id", endpointURI}});
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/processor_port",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}
/**
 * @brief Get all switch info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp   Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       fabricId    fabric id for redfish URI.
 */
inline void updatePortLinks(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& objPath,
                            const std::string& fabricId)
{
    BMCWEB_LOG_DEBUG("Get Port Links");
    crow::connections::systemBus->async_method_call(
        [aResp, objPath,
         fabricId](const boost::system::error_code ec,
                   std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no endpoint = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        nlohmann::json& linksArray =
            aResp->res.jsonValue["Links"]["AssociatedEndpoints"];
        linksArray = nlohmann::json::array();
        for (const std::string& portPath : *data)
        {
            sdbusplus::message::object_path portObjPath(portPath);
            const std::string& endpointId = portObjPath.filename();
            std::string endpointURI = "/redfish/v1/Fabrics/";
            endpointURI += fabricId;
            endpointURI += "/Endpoints/";
            endpointURI += endpointId;
            linksArray.push_back({{"@odata.id", endpointURI}});
            getConnectedPortLinks(aResp, objPath, endpointId);
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/associated_endpoint",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Get all port info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       fabricId    Fabric Id.
 * @param[in]       switchId    Switch Id.
 * @param[in]       portId      Port Id.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getFabricsPortObject(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& fabricId,
                         const std::string& switchId, const std::string& portId)
{
    std::string objPath = std::string(inventoryRootPath);
    objPath += std::string(inventoryFabricStr) + fabricId;
    objPath += std::string(inventorySwitchStr) + switchId;
    objPath += std::string(inventoryPortStr);

    BMCWEB_LOG_DEBUG("Access port Data");
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, fabricId, switchId,
         portId](const boost::system::error_code ec,
                 const crow::openbmc_mapper::GetSubTreeType& subtree) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        // Iterate over all retrieved
        // ObjectPaths.
        for (const std::pair<
                 std::string,
                 std::vector<std::pair<std::string, std::vector<std::string>>>>&
                 object : subtree)
        {
            // Get the portId object
            const std::string& path = object.first;
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                connectionNames = object.second;
            sdbusplus::message::object_path objPath(path);
            if (objPath.filename() != portId ||
                path.find(fabricId) == std::string::npos ||
                path.find(switchId) == std::string::npos)
            {
                continue;
            }
            if (connectionNames.size() < 1)
            {
                BMCWEB_LOG_ERROR("Got 0 Connection names");
                continue;
            }
            std::string portURI = "/redfish/v1/Fabrics/";
            portURI += fabricId;
            portURI += "/Switches/";
            portURI += switchId;
            portURI += "/Ports/";
            portURI += portId;
            asyncResp->res.jsonValue = {{"@odata.type", "#Port.v1_4_0.Port"},
                                        {"@odata.id", portURI},
                                        {"Name", portId + " Resource"},
                                        {"Id", portId}};
            std::string portMetricsURI = portURI + "/Metrics";
            asyncResp->res.jsonValue["Metrics"]["@odata.id"] = portMetricsURI;
#ifndef BMCWEB_DISABLE_CONDITIONS_ARRAY
            asyncResp->res.jsonValue["Status"]["Conditions"] =
                nlohmann::json::array();
#endif // BMCWEB_DISABLE_CONDITIONS_ARRAY

            const std::string& connectionName = connectionNames[0].first;
            redfish::port_utils::getPortData(asyncResp, connectionName,
                                             objPath);
            updatePortLinks(asyncResp, objPath, fabricId);
            return;
        }
        // Couldn't find an object with that name.
        // Return an error
        messages::resourceNotFound(asyncResp->res, "#Port.v1_4_0.Port", portId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", objPath, 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item."
                                   "Port"});
}

/**
 * @brief Get all switch info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    updateSwitchData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& service, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get Switch Data");
    using PropertyType = std::variant<std::string, bool, double, size_t,
                                      std::vector<std::string>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;
    // Get interface properties
    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath](const boost::system::error_code ec,
                             const PropertiesMap& properties) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "Type")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for switch type");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["SwitchType"] = getSwitchType(*value);
            }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            else if (propertyName == "DeviceId")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for DeviceId");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["DeviceId"] = *value;
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaSwitch.v1_0_0.NvidiaSwitch";
            }
            else if (propertyName == "VendorId")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for VendorId");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["VendorId"] = *value;
            }
            else if (propertyName == "PCIeReferenceClockEnabled")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for PCIeReferenceClockEnabled");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]
                                        ["PCIeReferenceClockEnabled"] = *value;
            }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            else if (propertyName == "SupportedProtocols")
            {
                nlohmann::json& protoArray =
                    asyncResp->res.jsonValue["SupportedProtocols"];
                protoArray = nlohmann::json::array();
                const std::vector<std::string>* protocols =
                    std::get_if<std::vector<std::string>>(&property.second);
                if (protocols == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for supported protocols");
                    messages::internalError(asyncResp->res);
                    return;
                }
                for (const std::string& protocol : *protocols)
                {
                    protoArray.push_back(getSwitchType(protocol));
                }
            }
            else if (propertyName == "Enabled")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for enabled");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Enabled"] = *value;
            }
            else if ((propertyName == "Model") ||
                     (propertyName == "PartNumber") ||
                     (propertyName == "SerialNumber") ||
                     (propertyName == "Manufacturer"))
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for asset properties");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue[propertyName] = *value;
            }
            else if (propertyName == "CurrentBandwidth")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for CurrentBandwidth");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["CurrentBandwidthGbps"] = *value;
            }
            else if (propertyName == "MaxBandwidth")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for MaxBandwidth");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["MaxBandwidthGbps"] = *value;
            }
            else if (propertyName == "TotalSwitchWidth")
            {
                const size_t* value = std::get_if<size_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for TotalSwitchWidth");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["TotalSwitchWidth"] = *value;
            }
            else if (propertyName == "CurrentPowerState")
            {
                const std::string* state =
                    std::get_if<std::string>(&property.second);
                if (*state == "xyz.openbmc_project.State.Chassis.PowerState.On")
                {
                    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
                }
                else if (*state ==
                         "xyz.openbmc_project.State.Chassis.PowerState.Off")
                {
                    asyncResp->res.jsonValue["Status"]["State"] =
                        "StandbyOffline";
                }
            }
            else if (propertyName == "UUID")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned "
                                     "for UUID");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["UUID"] = *value;
            }
        }

        getComponentFirmwareVersion(asyncResp, objPath);
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");

    asyncResp->res.jsonValue["Status"]["Health"] = "OK";
#ifndef BMCWEB_DISABLE_HEALTH_ROLLUP
    asyncResp->res.jsonValue["Status"]["HealthRollup"] = "OK";
#endif // BMCWEB_DISABLE_HEALTH_ROLLUP
    // update switch health
#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
    std::shared_ptr<HealthRollup> health = std::make_shared<HealthRollup>(
        objPath, [asyncResp](const std::string& rootHealth,
                             const std::string& healthRollup) {
        asyncResp->res.jsonValue["Status"]["Health"] = rootHealth;
#ifndef BMCWEB_DISABLE_HEALTH_ROLLUP
        asyncResp->res.jsonValue["Status"]["HealthRollup"] = healthRollup;
#endif // BMCWEB_DISABLE_HEALTH_ROLLUP
    });
    health->start();

#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
}

inline void updateZoneData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& service,
                           const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get Zone Data");
    using PropertyType =
        std::variant<std::string, bool, size_t, std::vector<std::string>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;
    // Get interface properties
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const PropertiesMap& properties) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "Type")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for zone type");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["ZoneType"] = getZoneType(*value);
            }

            else if (propertyName == "RoutingEnabled")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for enabled");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["DefaultRoutingEnabled"] = *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

/**
 * FabricCollection derived class for delivering Fabric Collection Schema
 */
inline void requestRoutesFabricCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#FabricCollection.FabricCollection";
        asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Fabrics";
        asyncResp->res.jsonValue["Name"] = "Fabric Collection";
        constexpr std::array<std::string_view, 1> interface{
            "xyz.openbmc_project.Inventory.Item.Fabric"};

        collection_util::getCollectionMembers(
            asyncResp, boost::urls::format("/redfish/v1/Fabrics"), interface,
            "/xyz/openbmc_project/inventory");
    });
}

/**
 * Fabric override class for delivering Fabric Schema
 */
inline void requestRoutesFabric(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId(std::string(fabricId))](
                const boost::system::error_code ec,
                const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;
                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != fabricId)
                {
                    continue;
                }
                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR("Got 0 Connection names");
                    continue;
                }

                asyncResp->res.jsonValue["@odata.type"] =
                    "#Fabric.v1_2_0.Fabric";
                asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Fabrics/" +
                                                        fabricId;
                asyncResp->res.jsonValue["Id"] = fabricId;
                asyncResp->res.jsonValue["Name"] = fabricId + " Resource";
                asyncResp->res.jsonValue["Endpoints"] = {
                    {"@odata.id",
                     "/redfish/v1/Fabrics/" + fabricId + "/Endpoints"}};
                asyncResp->res.jsonValue["Switches"] = {
                    {"@odata.id",
                     "/redfish/v1/Fabrics/" + fabricId + "/Switches"}};
                asyncResp->res.jsonValue["Zones"] = {
                    {"@odata.id",
                     "/redfish/v1/Fabrics/" + fabricId + "/Zones"}};

                const std::string& connectionName = connectionNames[0].first;

                // Fabric item properties
                crow::connections::systemBus->async_method_call(
                    [asyncResp](
                        const boost::system::error_code ec,
                        const std::vector<std::pair<
                            std::string, dbus::utility::DbusVariantType>>&
                            propertiesList) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    for (const std::pair<std::string,
                                         dbus::utility::DbusVariantType>&
                             property : propertiesList)
                    {
                        if (property.first == "Type")
                        {
                            const std::string* value =
                                std::get_if<std::string>(&property.second);
                            if (value == nullptr)
                            {
                                BMCWEB_LOG_DEBUG("Null value returned "
                                                 "for fabric type");
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            asyncResp->res.jsonValue["FabricType"] =
                                getFabricType(*value);
                        }
                    }
                },
                    connectionName, path, "org.freedesktop.DBus.Properties",
                    "GetAll", "xyz.openbmc_project.Inventory.Item.Fabric");

                return;
            }
            // Couldn't find an object with that name. Return an error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

/**
 * SwitchCollection derived class for delivering Switch Collection Schema
 */
inline void requestRoutesSwitchCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#SwitchCollection.SwitchCollection";
        asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Fabrics/" +
                                                fabricId + "/Switches";
        asyncResp->res.jsonValue["Name"] = "Switch Collection";

        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId](const boost::system::error_code ec,
                                  const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                constexpr std::array<std::string_view, 1> interface{
                    "xyz.openbmc_project.Inventory.Item.Switch"};
                collection_util::getCollectionMembers(
                    asyncResp,
                    boost::urls::format("/redfish/v1/Fabrics/" + fabricId +
                                        "/Switches"),
                    interface, object.c_str());
                return;
            }
            // Couldn't find an object with that name. Return an
            // error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

/**
 * @brief Fill out links for parent chassis PCIeDevice by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       chassisName D-Bus object chassisName.
 */
inline void getSwitchParentChassisPCIeDeviceLink(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath,
    const std::string& chassisName)
{
    crow::connections::systemBus->async_method_call(
        [aResp, chassisName](const boost::system::error_code ec,
                             std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no chassis = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr && data->size() > 1)
        {
            // Chassis must have single parent chassis
            return;
        }
        const std::string& parentChassisPath = data->front();
        sdbusplus::message::object_path objectPath(parentChassisPath);
        std::string parentChassisName = objectPath.filename();
        if (parentChassisName.empty())
        {
            messages::internalError(aResp->res);
            return;
        }
        crow::connections::systemBus->async_method_call(
            [aResp, chassisName, parentChassisName](
                const boost::system::error_code ec,
                const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(aResp->res);
                return;
            }
            for (const auto& [objectPath, serviceMap] : subtree)
            {
                // Process same device
                if (!boost::ends_with(objectPath, chassisName))
                {
                    continue;
                }
                std::string pcieDeviceLink = "/redfish/v1/Chassis/";
                pcieDeviceLink += parentChassisName;
                pcieDeviceLink += "/PCIeDevices/";
                pcieDeviceLink += chassisName;
                aResp->res.jsonValue["Links"]["PCIeDevice"] = {
                    {"@odata.id", pcieDeviceLink}};
            }
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", parentChassisPath,
            0,
            std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item."
                                       "PCIeDevice"});
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out links association to parent chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getSwitchChassisLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                         const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get parent chassis link");
    crow::connections::systemBus->async_method_call(
        [aResp, objPath](const boost::system::error_code ec,
                         std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no chassis = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr && data->size() > 1)
        {
            // Switch must have single parent chassis
            return;
        }
        const std::string& chassisPath = data->front();
        sdbusplus::message::object_path objectPath(chassisPath);
        std::string chassisName = objectPath.filename();
        if (chassisName.empty())
        {
            messages::internalError(aResp->res);
            return;
        }
        aResp->res.jsonValue["Links"]["Chassis"] = {
            {"@odata.id", "/redfish/v1/Chassis/" + chassisName}};

        // Get PCIeDevice on this chassis
        crow::connections::systemBus->async_method_call(
            [aResp, chassisName](const boost::system::error_code ec,
                                 std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("Chassis has no connected PCIe devices");
                return; // no pciedevices = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr && data->size() > 1)
            {
                // Chassis must have single pciedevice
                BMCWEB_LOG_ERROR("chassis must have single pciedevice");
                return;
            }

            for (const std::string& pcieDevicePath : *data)
            {
                sdbusplus::message::object_path objectPath(pcieDevicePath);
                std::string pcieDeviceName = objectPath.filename();
                if (pcieDeviceName.empty())
                {
                    BMCWEB_LOG_ERROR("chassis pciedevice name empty");
                    return;
                }
                std::string pcieDeviceLink = "/redfish/v1/Chassis/";
                pcieDeviceLink += chassisName;
                pcieDeviceLink += "/PCIeDevices/";
                pcieDeviceLink += pcieDeviceName;
                aResp->res.jsonValue["Links"]["PCIeDevice"] = {
                    {"@odata.id", pcieDeviceLink}};
            }
        },
            "xyz.openbmc_project.ObjectMapper", chassisPath + "/pciedevice",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out links association to parent chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       fabricId    Fabric Id.
 */
inline void
    getSwitchEndpointsLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& objPath,
                           const std::string& fabricId)
{
    BMCWEB_LOG_DEBUG("Get endpoint links");
    crow::connections::systemBus->async_method_call(
        [aResp, fabricId](const boost::system::error_code ec,
                          std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no endpoints = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        nlohmann::json& linksArray = aResp->res.jsonValue["Links"]["Endpoints"];
        linksArray = nlohmann::json::array();
        for (const std::string& endpointPath : *data)
        {
            sdbusplus::message::object_path objPath(endpointPath);
            const std::string& endpointId = objPath.filename();
            std::string endpointURI = "/redfish/v1/Fabrics/";
            endpointURI += fabricId;
            endpointURI += "/Endpoints/";
            endpointURI += endpointId;
            linksArray.push_back({{"@odata.id", endpointURI}});
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_endpoints",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out managed by links association to manager service by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getManagerLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get managed_by links");
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no managed_by association = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        nlohmann::json& linksArray = aResp->res.jsonValue["Links"]["ManagedBy"];
        linksArray = nlohmann::json::array();
        for (const std::string& endpointPath : *data)
        {
            sdbusplus::message::object_path objPath(endpointPath);
            const std::string& endpointId = objPath.filename();
            std::string endpointURI = "/redfish/v1/Managers/";
            endpointURI += endpointId;
            linksArray.push_back({{"@odata.id", endpointURI}});
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/managed_by",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out links association to parent chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       fabricId    Fabric Id.
 */
inline void
    getZoneEndpointsLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                         const std::string& objPath,
                         const std::string& fabricId)
{
    BMCWEB_LOG_DEBUG("Get zone endpoint links");
    crow::connections::systemBus->async_method_call(
        [aResp, fabricId](const boost::system::error_code ec,
                          std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no endpoints = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        nlohmann::json& linksArray = aResp->res.jsonValue["Links"]["Endpoints"];
        linksArray = nlohmann::json::array();
        for (const std::string& endpointPath : *data)
        {
            sdbusplus::message::object_path objPath(endpointPath);
            const std::string& endpointId = objPath.filename();
            std::string endpointURI = "/redfish/v1/Fabrics/";
            endpointURI += fabricId;
            endpointURI += "/Endpoints/";
            endpointURI += endpointId;
            linksArray.push_back({{"@odata.id", endpointURI}});
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/zone_endpoints",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}
/**
 * Switch override class for delivering Switch Schema
 */
inline void requestRoutesSwitch(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& switchId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId,
             switchId](const boost::system::error_code ec,
                       const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                crow::connections::systemBus->async_method_call(
                    [asyncResp, fabricId, switchId](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Iterate over all retrieved ObjectPaths.
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<std::string,
                                                   std::vector<std::string>>>>&
                             object : subtree)
                    {
                        // Get the switchId object
                        const std::string& path = object.first;
                        const std::vector<
                            std::pair<std::string, std::vector<std::string>>>&
                            connectionNames = object.second;
                        sdbusplus::message::object_path objPath(path);
                        if (objPath.filename() != switchId)
                        {
                            continue;
                        }
                        if (connectionNames.size() < 1)
                        {
                            BMCWEB_LOG_ERROR("Got 0 Connection names");
                            continue;
                        }
                        std::string switchURI = "/redfish/v1/Fabrics/";
                        switchURI += fabricId;
                        switchURI += "/Switches/";
                        switchURI += switchId;
                        std::string portsURI = switchURI;
                        portsURI += "/Ports";
                        std::string switchMetricURI = switchURI;
                        switchMetricURI += "/SwitchMetrics";
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#Switch.v1_8_0.Switch";
                        asyncResp->res.jsonValue["@odata.id"] = switchURI;
                        asyncResp->res.jsonValue["Id"] = switchId;
                        asyncResp->res.jsonValue["Name"] = switchId +
                                                           " Resource";
                        asyncResp->res.jsonValue["Ports"] = {
                            {"@odata.id", portsURI}};
                        asyncResp->res.jsonValue["Metrics"] = {
                            {"@odata.id", switchMetricURI}};
                        std::string switchResetURI = "/redfish/v1/Fabrics/";
                        switchResetURI += fabricId;
                        switchResetURI += "/Switches/";
                        switchResetURI += switchId;
                        switchResetURI += "/Actions/Switch.Reset";
                        asyncResp->res.jsonValue["Actions"]["#Switch.Reset"] = {
                            {"target", switchResetURI},
                            {"ResetType@Redfish.AllowableValues",
                             {"ForceRestart"}}};
                        const std::string& connectionName =
                            connectionNames[0].first;
                        updateSwitchData(asyncResp, connectionName, path);
                        // Link association to parent chassis
                        getSwitchChassisLink(asyncResp, path);
                        // Link association to endpoints
                        getSwitchEndpointsLink(asyncResp, path, fabricId);
                        // Link association to manager
                        getManagerLink(asyncResp, path);

#ifndef BMCWEB_DISABLE_CONDITIONS_ARRAY
                        redfish::conditions_utils::populateServiceConditions(
                            asyncResp, switchId);
#endif // BMCWEB_DISABLE_CONDITIONS_ARRAY
                        return;
                    }
                    // Couldn't find an object with that name.
                    // Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Switch.v1_8_0.Switch", switchId);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", object, 0,
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item.Switch"});
                return;
            }
            // Couldn't find an object with that name. Return an error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

using DimmProperties =
    boost::container::flat_map<std::string, dbus::utility::DbusVariantType>;
inline void
    getInternalMemoryMetrics(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& service,
                             const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get memory ecc data.");
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const DimmProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            if (property.first == "ceCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["InternalMemoryMetrics"]["LifeTime"]
                                    ["CorrectableECCErrorCount"] = *value;
            }
            else if (property.first == "ueCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["InternalMemoryMetrics"]["LifeTime"]
                                    ["UncorrectableECCErrorCount"] = *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Memory.MemoryECC");
}

inline void requestRoutesSwitchMetrics(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>/SwitchMetrics")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& switchId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId,
             switchId](const boost::system::error_code ec,
                       const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                crow::connections::systemBus->async_method_call(
                    [asyncResp, fabricId, switchId](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Iterate over all retrieved ObjectPaths.
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<std::string,
                                                   std::vector<std::string>>>>&
                             object : subtree)
                    {
                        // Get the switchId object
                        const std::string& path = object.first;
                        const std::vector<
                            std::pair<std::string, std::vector<std::string>>>&
                            connectionNames = object.second;
                        sdbusplus::message::object_path objPath(path);
                        if (objPath.filename() != switchId)
                        {
                            continue;
                        }
                        if (connectionNames.size() < 1)
                        {
                            BMCWEB_LOG_ERROR("Got 0 Connection names");
                            continue;
                        }
                        std::string switchURI = "/redfish/v1/Fabrics/";
                        switchURI += fabricId;
                        switchURI += "/Switches/";
                        switchURI += switchId;
                        std::string switchMetricURI = switchURI;
                        switchMetricURI += "/SwitchMetrics";
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#SwitchMetrics.v1_0_0.SwitchMetrics";
                        asyncResp->res.jsonValue["@odata.id"] = switchMetricURI;
                        asyncResp->res.jsonValue["Id"] = switchId;
                        asyncResp->res.jsonValue["Name"] = switchId +
                                                           " Metrics";
                        const std::string& connectionName =
                            connectionNames[0].first;
                        const std::vector<std::string>& interfaces =
                            connectionNames[0].second;
                        if (std::find(interfaces.begin(), interfaces.end(),
                                      "xyz.openbmc_project.Memory.MemoryECC") !=
                            interfaces.end())
                        {
                            getInternalMemoryMetrics(asyncResp, connectionName,
                                                     path);
                        }
                        if (std::find(interfaces.begin(), interfaces.end(),
                                      "xyz.openbmc_project.PCIe.PCIeECC") !=
                            interfaces.end())
                        {
                            redfish::processor_utils::getPCIeErrorData(
                                asyncResp, connectionName, path);
                        }

                        return;
                    }
                    // Couldn't find an object with that name.
                    // Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Switch.v1_8_0.Switch", switchId);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", object, 0,
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item.Switch"});
                return;
            }
            // Couldn't find an object with that name. Return an error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

inline std::string getNVSwitchResetType(const std::string& processorType)
{
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.ForceOff")
    {
        return "ForceOff";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.ForceOn")
    {
        return "ForceOn";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.ForceRestart")
    {
        return "ForceRestart";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.GracefulRestart")
    {
        return "GracefulRestart";
    }
    if (processorType ==
        "xyz.openbmc_project.Control.Processor.Reset.ResetTypes.GracefulShutdown")
    {
        return "GracefulShutdown";
    }
    // Unknown or others
    return "";
}

inline void nvswitchPostResetType(
    const std::shared_ptr<bmcweb::AsyncResp>& resp, const std::string& switchId,
    const std::string& cpuObjectPath, const std::string& resetType,
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "xyz.openbmc_project.Control.Processor.Reset") !=
            interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        messages::internalError(resp->res);
        return;
    }
    const std::string conName = *inventoryService;
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, conName, cpuObjectPath,
        "xyz.openbmc_project.Control.Processor.Reset", "ResetType",
        [resp, resetType, switchId, conName, cpuObjectPath](

            const boost::system::error_code ec, const std::string& property) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBus response, error for ResetType ");
            BMCWEB_LOG_ERROR("{}", ec.message());
            messages::internalError(resp->res);
            return;
        }

        const std::string processorResetType = getNVSwitchResetType(property);
        if (processorResetType != resetType)
        {
            BMCWEB_LOG_DEBUG("Property Value Incorrect");
            messages::actionParameterNotSupported(resp->res, "ResetType",
                                                  resetType);
            return;
        }
        // Set the property, with handler to check error responses
        crow::connections::systemBus->async_method_call(
            [resp, switchId](boost::system::error_code ec, const int retValue) {
            if (!ec)
            {
                if (retValue != 0)
                {
                    BMCWEB_LOG_ERROR("{}", retValue);
                    messages::internalError(resp->res);
                }
                BMCWEB_LOG_DEBUG("Switch:{} Reset Succeded", switchId);
                messages::success(resp->res);
                return;
            }
            BMCWEB_LOG_DEBUG("{}", ec);
            messages::internalError(resp->res);
            return;
        }, conName, cpuObjectPath,
            "xyz.openbmc_project.Control.Processor.Reset", "Reset");
    });
}

/**
 * Functions triggers appropriate NVSwitch Reset requests on DBus
 */
inline void requestRoutesNVSwitchReset(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>/"
                      "Actions/Switch.Reset")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::post)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& switchId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        std::optional<std::string> resetType;
        if (!redfish::json_util::readJsonAction(req, asyncResp->res,
                                                "ResetType", resetType))
        {
            return;
        }
        if (resetType)
        {
            crow::connections::systemBus->async_method_call(
                [asyncResp, fabricId, switchId,
                 resetType](const boost::system::error_code ec,
                            const std::vector<std::string>& objects) {
                if (ec)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }

                for (const std::string& object : objects)
                {
                    // Get the fabricId object
                    if (!boost::ends_with(object, fabricId))
                    {
                        continue;
                    }
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, fabricId, switchId,
                         resetType](const boost::system::error_code ec,
                                    const crow::openbmc_mapper::GetSubTreeType&
                                        subtree) {
                        if (ec)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        // Iterate over all retrieved ObjectPaths.
                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            // Get the switchId object
                            const std::string& path = object.first;
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                connectionNames = object.second;
                            sdbusplus::message::object_path objPath(path);
                            if (objPath.filename() != switchId)
                            {
                                continue;
                            }
                            if (connectionNames.size() < 1)
                            {
                                BMCWEB_LOG_ERROR("Got 0 Connection names");
                                continue;
                            }
                            nvswitchPostResetType(asyncResp, switchId, objPath,

                                                  *resetType, connectionNames);
                            return;
                        }
                        // Couldn't find an object with that name.
                        // Return an error
                        messages::resourceNotFound(
                            asyncResp->res, "#Switch.v1_8_0.Switch", switchId);
                    },
                        "xyz.openbmc_project.ObjectMapper",
                        "/xyz/openbmc_project/object_mapper",
                        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                        object, 0,
                        std::array<const char*, 1>{
                            "xyz.openbmc_project.Inventory.Item.Switch"});
                    return;
                }
                // Couldn't find an object with that name. Return an
                // error
                messages::resourceNotFound(asyncResp->res,
                                           "#Fabric.v1_2_0.Fabric", fabricId);
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.Fabric"});
        }
    });
}

/**
 * PortCollection derived class for delivering Port Collection Schema
 */
inline void requestRoutesPortCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>/Ports/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& switchId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        std::string portsURI = "/redfish/v1/Fabrics/";
        portsURI += fabricId;
        portsURI += "/Switches/";
        portsURI += switchId;
        portsURI += "/Ports";
        asyncResp->res.jsonValue["@odata.type"] =
            "#PortCollection.PortCollection";
        asyncResp->res.jsonValue["@odata.id"] = portsURI;
        asyncResp->res.jsonValue["Name"] = "Port Collection";

        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId, switchId,
             portsURI](const boost::system::error_code ec,
                       const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                crow::connections::systemBus->async_method_call(
                    [asyncResp, fabricId, switchId,
                     portsURI](const boost::system::error_code ec,
                               const std::vector<std::string>& objects) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    for (const std::string& object : objects)
                    {
                        // Get the switchId object
                        if (!boost::ends_with(object, switchId))
                        {
                            continue;
                        }
                        constexpr std::array<std::string_view, 1> interface{
                            "xyz.openbmc_project.Inventory.Item.Port"};
                        collection_util::getCollectionMembers(
                            asyncResp, boost::urls::format(portsURI), interface,
                            object.c_str());
                        return;
                    }
                    // Couldn't find an object with that name.
                    // Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Switch.v1_8_0.Switch", switchId);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                    object.c_str(), 0,
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item.Switch"});
                return;
            }
            // Couldn't find an object with that name. Return an error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

/**
 * Port override class for delivering Port Schema
 */
inline void requestRoutesPort(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>/Ports/<str>")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& switchId,
                   const std::string& portId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        getFabricsPortObject(asyncResp, fabricId, switchId, portId);
    });
}

inline void requestRoutesZoneCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Zones/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#ZoneCollection.ZoneCollection";
        asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Fabrics/" +
                                                fabricId + "/Zones";
        asyncResp->res.jsonValue["Name"] = "Zone Collection";

        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId](const boost::system::error_code ec,
                                  const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                constexpr std::array<std::string_view, 1> interface{
                    "xyz.openbmc_project.Inventory.Item.Zone"};
                collection_util::getCollectionMembers(
                    asyncResp,
                    boost::urls::format("/redfish/v1/Fabrics/" + fabricId +
                                        "/Zones"),
                    interface, object.c_str());
                return;
            }
            // Couldn't find an object with that name. Return an
            // error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

inline void requestRoutesZone(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Zones/<str>")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& zoneId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId,
             zoneId](const boost::system::error_code ec,
                     const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                crow::connections::systemBus->async_method_call(
                    [asyncResp, fabricId, zoneId](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Iterate over all retrieved ObjectPaths.
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<std::string,
                                                   std::vector<std::string>>>>&
                             object : subtree)
                    {
                        // Get the zoneId object
                        const std::string& path = object.first;
                        const std::vector<
                            std::pair<std::string, std::vector<std::string>>>&
                            connectionNames = object.second;
                        sdbusplus::message::object_path objPath(path);
                        if (objPath.filename() != zoneId)
                        {
                            continue;
                        }
                        if (connectionNames.size() < 1)
                        {
                            BMCWEB_LOG_ERROR("Got 0 Connection names");
                            continue;
                        }
                        std::string zoneURI = "/redfish/v1/Fabrics/";
                        zoneURI += fabricId;
                        zoneURI += "/Zones/";
                        zoneURI += zoneId;
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#Zone.v1_6_1.Zone";
                        asyncResp->res.jsonValue["@odata.id"] = zoneURI;
                        asyncResp->res.jsonValue["Id"] = zoneId;
                        asyncResp->res.jsonValue["Name"] = " Zone " + zoneId;
                        const std::string& connectionName =
                            connectionNames[0].first;
                        updateZoneData(asyncResp, connectionName, path);

                        // Link association to endpoints
                        getZoneEndpointsLink(asyncResp, path, fabricId);
                        return;
                    }
                    // Couldn't find an object with that name.
                    // Return an error
                    messages::resourceNotFound(asyncResp->res,
                                               "#Zone.v1_6_1.Zone", zoneId);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", object, 0,
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item.Zone"});
                return;
            }
            // Couldn't find an object with that name. Return an error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

/**
 * Endpoint derived class for delivering Endpoint Collection Schema
 */
inline void requestRoutesEndpointCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Endpoints/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#EndpointCollection.EndpointCollection";
        asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Fabrics/" +
                                                fabricId + "/Endpoints";
        asyncResp->res.jsonValue["Name"] = "Endpoint Collection";

        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId](const boost::system::error_code ec,
                                  const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                constexpr std::array<std::string_view, 1> interface{
                    "xyz.openbmc_project.Inventory.Item.Endpoint"};
                collection_util::getCollectionMembers(
                    asyncResp,
                    boost::urls::format("/redfish/v1/Fabrics/" + fabricId +
                                        "/Endpoints"),
                    interface, object.c_str());
                return;
            }
            // Couldn't find an object with that name. Return an
            // error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

/**
 * @brief Get all endpoint pcie device info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       entityLink  redfish entity link.
 */
inline void getProcessorPCIeDeviceData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& service,
    const std::string& objPath, const std::string& entityLink)
{
    crow::connections::systemBus->async_method_call(
        [aResp, entityLink](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, std::variant<std::string>>& pcieDevProperties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        // Get the device data from single function
        const std::string& function = "0";
        std::string deviceId, vendorId, subsystemId, subsystemVendorId;
        for (const auto& property : pcieDevProperties)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "Function" + function + "DeviceId")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    deviceId = *value;
                }
            }
            else if (propertyName == "Function" + function + "VendorId")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    vendorId = *value;
                }
            }
            else if (propertyName == "Function" + function + "SubsystemId")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    subsystemId = *value;
                }
            }
            else if (propertyName ==
                     "Function" + function + "SubsystemVendorId")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    subsystemVendorId = *value;
                }
            }
        }
        nlohmann::json& connectedEntitiesArray =
            aResp->res.jsonValue["ConnectedEntities"];
        connectedEntitiesArray.push_back(
            {{"EntityType", "Processor"},
             {"EntityPciId",
              {{"DeviceId", deviceId},
               {"VendorId", vendorId},
               {"SubsystemId", subsystemId},
               {"SubsystemVendorId", subsystemVendorId}}},
             {"EntityLink", {{"@odata.id", entityLink}}}});
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.PCIeDevice");
}

inline void
    getProcessorEndpointHealth(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                               const std::string& service,
                               const std::string& objPath)
{
    // Set the default value of state
    aResp->res.jsonValue["Status"]["State"] = "Enabled";
    aResp->res.jsonValue["Status"]["Health"] = "OK";

    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const boost::container::flat_map<
                    std::string, std::variant<std::string, uint32_t, uint16_t,
                                              bool>>& properties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        for (const auto& property : properties)
        {
            if (property.first == "Present")
            {
                const bool* isPresent = std::get_if<bool>(&property.second);
                if (isPresent == nullptr)
                {
                    // Important property not in desired type
                    messages::internalError(aResp->res);
                    return;
                }
                if (*isPresent == false)
                {
                    aResp->res.jsonValue["Status"]["State"] = "Absent";
                }
            }
            else if (property.first == "Functional")
            {
                const bool* isFunctional = std::get_if<bool>(&property.second);
                if (isFunctional == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                if (*isFunctional == false)
                {
                    aResp->res.jsonValue["Status"]["Health"] = "Critical";
                }
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
    return;
}

/**
 * @brief Fill out links for parent chassis PCIeDevice by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       chassisName D-Bus object chassisName.
 * @param[in]       entityLink  redfish entity link.
 */
inline void getProcessorParentEndpointData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& objPath,
    const std::string& chassisName, const std::string& entityLink,
    const std::string& processorPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, chassisName, entityLink,
         processorPath](const boost::system::error_code ec,
                        std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no chassis = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr && data->size() > 1)
        {
            // Chassis must have single parent chassis
            return;
        }
        const std::string& parentChassisPath = data->front();
        sdbusplus::message::object_path objectPath(parentChassisPath);
        std::string parentChassisName = objectPath.filename();
        if (parentChassisName.empty())
        {
            messages::internalError(aResp->res);
            return;
        }
        crow::connections::systemBus->async_method_call(
            [aResp, chassisName, parentChassisName, entityLink, processorPath](
                const boost::system::error_code ec,
                const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(aResp->res);
                return;
            }
            for (const auto& [objectPath, serviceMap] : subtree)
            {
                // Process same device
                if (!boost::ends_with(objectPath, chassisName))
                {
                    continue;
                }
                if (serviceMap.size() < 1)
                {
                    BMCWEB_LOG_ERROR("Got 0 service "
                                     "names");
                    messages::internalError(aResp->res);
                    return;
                }
                const std::string& serviceName = serviceMap[0].first;
                // Get PCIeDevice Data
                getProcessorPCIeDeviceData(aResp, serviceName, objectPath,
                                           entityLink);
                // Update processor health
                getProcessorEndpointHealth(aResp, serviceName, processorPath);
            }
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", parentChassisPath,
            0,
            std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item."
                                       "PCIeDevice"});
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Get all endpoint pcie device info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       processorPath     D-Bus service to query.
 * @param[in]       entityLink  redfish entity link.
 */
inline void getEndpointData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& processorPath,
                            const std::string& entityLink,
                            const std::string& serviceName)
{
    crow::connections::systemBus->async_method_call(
        [aResp, processorPath, serviceName,
         entityLink](const boost::system::error_code ec,
                     std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no chassis = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr && data->size() > 1)
        {
            // Processor must have single parent chassis
            return;
        }
        const std::string& chassisPath = data->front();
        sdbusplus::message::object_path objectPath(chassisPath);
        std::string chassisName = objectPath.filename();
        if (chassisName.empty())
        {
            messages::internalError(aResp->res);
            return;
        }

        crow::connections::systemBus->async_method_call(
            [aResp, processorPath, serviceName,
             entityLink](const boost::system::error_code ec,
                         std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("Chassis has no connected PCIe devices");
                return; // no pciedevices = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr && data->size() > 1)
            {
                // Chassis must have single pciedevice
                BMCWEB_LOG_ERROR("chassis must have single pciedevice");
                return;
            }
            const std::string& pcieDevicePath = data->front();
            sdbusplus::message::object_path objectPath(pcieDevicePath);
            std::string pcieDeviceName = objectPath.filename();
            if (pcieDeviceName.empty())
            {
                BMCWEB_LOG_ERROR("chassis pciedevice name empty");
                messages::internalError(aResp->res);
                return;
            }
            // Get PCIeDevice Data
            getProcessorPCIeDeviceData(aResp, serviceName, pcieDevicePath,
                                       entityLink);
            // Update processor health
            getProcessorEndpointHealth(aResp, serviceName, processorPath);
        },
            "xyz.openbmc_project.ObjectMapper", chassisPath + "/pciedevice",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    },
        "xyz.openbmc_project.ObjectMapper", processorPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Get all endpoint pcie device info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getPortData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                        const std::string& service, const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const boost::container::flat_map<
                    std::string, std::variant<std::string, bool, size_t,
                                              std::vector<std::string>>>&
                    properties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        // Get port protocol
        for (const auto& property : properties)
        {
            if (property.first == "Protocol")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for protocol type");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["EndpointProtocol"] =
                    redfish::port_utils::getPortProtocol(*value);
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Port");
}

/**
 * @brief Get all endpoint connected port info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       portPath    D-Bus object to query.
 * @param[in]       fabricId    Fabric Id.
 * @param[in]       switchId    Switch Id.
 */
inline void
    getConnectedPortsLinks(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::vector<std::string>& portPaths,
                           const std::string& fabricId,
                           const std::string& switchPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, portPaths, fabricId,
         switchPath](const boost::system::error_code ec,
                     const std::vector<std::string>& objects) {
        if (ec)
        {
            messages::internalError(aResp->res);
            return;
        }
        nlohmann::json& linksConnectedPortsArray =
            aResp->res.jsonValue["Links"]["ConnectedPorts"];

        sdbusplus::message::object_path objPath(switchPath);
        const std::string& switchId = objPath.filename();
        // Add port link if exists in switch ports
        for (const std::string& portPath : portPaths)
        {
            if (std::find(objects.begin(), objects.end(), portPath) !=
                objects.end())
            {
                sdbusplus::message::object_path portObjPath(portPath);
                const std::string& portId = portObjPath.filename();
                {
                    std::string portURI =
                        (boost::format(
                             "/redfish/v1/Fabrics/%s/Switches/%s/Ports/"
                             "%s") %
                         fabricId % switchId % portId)
                            .str();
                    linksConnectedPortsArray.push_back(
                        {{"@odata.id", portURI}});
                }
            }
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths", switchPath, 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Port"});
}

/**
 * @brief Get all endpoint zone info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp           Async HTTP response.
 * @param[in]       endpointPath    D-Bus object to query.
 * @param[in]       fabricId        Fabric Id
 */
inline void getEndpointZoneData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& endpointPath,
                                const std::string& fabricId)
{
    // Get connected zone link
    crow::connections::systemBus->async_method_call(
        [aResp, fabricId](const boost::system::error_code ec,
                          std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no zones = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        nlohmann::json& linksZonesArray =
            aResp->res.jsonValue["Links"]["Zones"];
        linksZonesArray = nlohmann::json::array();
        std::string zoneURI;
        for (const std::string& zonePath : *data)
        {
            // Get subtree for switchPath link path
            sdbusplus::message::object_path dbusObjPath(zonePath);
            const std::string& zoneId = dbusObjPath.filename();
            if (zonePath.find(fabricId) != std::string::npos)
            {
                zoneURI = "/redfish/v1/Fabrics/" + fabricId + "/Zones/";
            }
            zoneURI += zoneId;
            linksZonesArray.push_back({{"@odata.id", zoneURI}});
        }
    },
        "xyz.openbmc_project.ObjectMapper", endpointPath + "/zone",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Get all endpoint port info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp           Async HTTP response.
 * @param[in]       endpointPath    D-Bus object to query.
 * @param[in]       processorPath   D-Bus object to query.
 * @param[in]       fabricId        Fabric Id
 */
inline void getEndpointPortData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& endpointPath,
                                const std::string& processorPath,
                                const std::string& fabricId)
{
    // Endpoint protocol
    crow::connections::systemBus->async_method_call(
        [aResp, processorPath,
         fabricId](const boost::system::error_code ec,
                   std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no endpoint port = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        for (const std::string& portPath : *data)
        {
            // Get subtree for port parent path
            size_t separator = portPath.rfind('/');
            if (separator == std::string::npos)
            {
                BMCWEB_LOG_ERROR("Invalid port link path");
                continue;
            }
            std::string portInventoryPath = portPath.substr(0, separator);
            // Get port subtree
            crow::connections::systemBus->async_method_call(
                [aResp, portPath](
                    const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
                if (ec)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                // Iterate over all retrieved ObjectPaths.
                for (const std::pair<
                         std::string,
                         std::vector<
                             std::pair<std::string, std::vector<std::string>>>>&
                         object : subtree)
                {
                    // Filter port link object
                    if (object.first != portPath)
                    {
                        continue;
                    }
                    const std::vector<
                        std::pair<std::string, std::vector<std::string>>>&
                        connectionNames = object.second;
                    if (connectionNames.size() < 1)
                    {
                        BMCWEB_LOG_ERROR("Got 0 Connection names");
                        continue;
                    }
                    const std::string& connectionName =
                        connectionNames[0].first;
                    const std::vector<std::string>& interfaces =
                        connectionNames[0].second;
                    const std::string portInterface =
                        "xyz.openbmc_project.Inventory.Item.Port";
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  portInterface) != interfaces.end())
                    {
                        // Get port protocol data
                        getPortData(aResp, connectionName, portPath);
                    }
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                portInventoryPath, 0, std::array<const char*, 0>());
        }
        const std::vector<std::string> portPaths = *data;
        // Get connected switches port links
        crow::connections::systemBus->async_method_call(
            [aResp, portPaths,
             fabricId](const boost::system::error_code ec,
                       std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no switches = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& linksConnectedPortsArray =
                aResp->res.jsonValue["Links"]["ConnectedPorts"];
            linksConnectedPortsArray = nlohmann::json::array();
            std::sort(data->begin(), data->end());
            for (const std::string& switchPath : *data)
            {
                getConnectedPortsLinks(aResp, portPaths, fabricId, switchPath);
            }
        },
            "xyz.openbmc_project.ObjectMapper", processorPath + "/all_switches",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    },
        "xyz.openbmc_project.ObjectMapper", endpointPath + "/connected_port",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Get all endpoint info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   aResp   Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       fabricId    Fabric Id.
 */
inline void updateEndpointData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                               const std::string& objPath,
                               const std::string& fabricId)
{
    BMCWEB_LOG_DEBUG("Get Endpoint Data");
    crow::connections::systemBus->async_method_call(
        [aResp, objPath,
         fabricId](const boost::system::error_code ec,
                   std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no entity link = no failures
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            return;
        }
        for (const std::string& entityPath : *data)
        {
            // Get subtree for entity link parent path
            size_t separator = entityPath.rfind('/');
            if (separator == std::string::npos)
            {
                BMCWEB_LOG_ERROR("Invalid entity link path");
                continue;
            }
            std::string entityInventoryPath = entityPath.substr(0, separator);
            // Get entity subtree
            crow::connections::systemBus->async_method_call(
                [aResp, objPath, entityPath, fabricId](
                    const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
                if (ec)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                // Iterate over all retrieved ObjectPaths.
                for (const std::pair<
                         std::string,
                         std::vector<
                             std::pair<std::string, std::vector<std::string>>>>&
                         object : subtree)
                {
                    // Filter entity link object
                    if (object.first != entityPath)
                    {
                        continue;
                    }
                    const std::vector<
                        std::pair<std::string, std::vector<std::string>>>&
                        connectionNames = object.second;
                    if (connectionNames.size() < 1)
                    {
                        BMCWEB_LOG_ERROR("Got 0 Connection names");
                        continue;
                    }

                    for (const auto& connectionName : connectionNames)
                    {
                        const std::vector<std::string>& interfaces =
                            connectionName.second;
                        const std::string acceleratorInterface =
                            "xyz.openbmc_project.Inventory.Item."
                            "Accelerator";
                        if (std::find(interfaces.begin(), interfaces.end(),
                                      acceleratorInterface) != interfaces.end())
                        {
                            std::string servName = connectionName.first;
                            sdbusplus::message::object_path objectPath(
                                entityPath);
                            const std::string& entityLink =
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Processors/" +
                                objectPath.filename();
                            // Get processor PCIe device data
                            getEndpointData(aResp, entityPath, entityLink,
                                            servName);
                            // Get port endpoint data
                            getEndpointPortData(aResp, objPath, entityPath,
                                                fabricId);
                            // Get zone links
                            getEndpointZoneData(aResp, objPath, fabricId);
                        }
                        const std::string switchInterface =
                            "xyz.openbmc_project.Inventory.Item.Switch";

                        if (std::find(interfaces.begin(), interfaces.end(),
                                      switchInterface) != interfaces.end())
                        {
                            BMCWEB_LOG_DEBUG("Item type switch ");
                            std::string servName = connectionName.first;

                            sdbusplus::message::object_path objectPath(
                                entityPath);
                            std::string entityName = objectPath.filename();
                            // get switch type endpint
                            crow::connections::systemBus->async_method_call(
                                [aResp, objPath, entityPath, entityName,
                                 servName, fabricId](
                                    const boost::system::error_code ec,
                                    std::variant<std::vector<std::string>>&
                                        resp) {
                                if (ec)
                                {
                                    BMCWEB_LOG_ERROR(
                                        "fabric not found for switch entity");
                                    return; // no processors identified
                                            // for pcieslotpath
                                }

                                std::vector<std::string>* data =
                                    std::get_if<std::vector<std::string>>(
                                        &resp);
                                if (data == nullptr)
                                {
                                    BMCWEB_LOG_ERROR(
                                        "processor data null for pcieslot ");
                                    return;
                                }

                                std::string fabricName;
                                for (const std::string& fabricPath : *data)
                                {
                                    sdbusplus::message::object_path dbusObjPath(
                                        fabricPath);
                                    fabricName = dbusObjPath.filename();
                                }
                                std::string entityLink = "/redfish/v1/Fabrics/";
                                entityLink += fabricName;
                                entityLink += "/Switches/";
                                entityLink += entityName;
                                // Get processor/switch PCIe device data
                                getEndpointData(aResp, entityPath, entityLink,
                                                servName);
                                // Get port endpoint data
                                getEndpointPortData(aResp, objPath, entityPath,
                                                    fabricId);
                                // Get zone links
                                getEndpointZoneData(aResp, objPath, fabricId);
                            },
                                "xyz.openbmc_project.ObjectMapper",
                                entityPath + "/fabrics",
                                "org.freedesktop.DBus.Properties", "Get",
                                "xyz.openbmc_project.Association", "endpoints");
                        }
                        const std::string cpuInterface =
                            "xyz.openbmc_project.Inventory.Item.Cpu";
                        if (std::find(interfaces.begin(), interfaces.end(),
                                      cpuInterface) != interfaces.end())
                        {
                            std::string servName = connectionName.first;
                            sdbusplus::message::object_path objectPath(
                                entityPath);
                            const std::string& entityLink =
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Processors/" +
                                objectPath.filename();
                            // Add EntityLink
                            nlohmann::json& connectedEntitiesArray =
                                aResp->res.jsonValue["ConnectedEntities"];
                            connectedEntitiesArray.push_back(
                                {{"EntityType", "Processor"},
                                 {"EntityLink", {{"@odata.id", entityLink}}}});

                            // Get port endpoint data
                            getEndpointPortData(aResp, objPath, entityPath,
                                                fabricId);
                            // Get zone links
                            getEndpointZoneData(aResp, objPath, fabricId);

                            // Update processor health
                            getProcessorEndpointHealth(aResp, servName,
                                                       entityPath);
                        }
                    }
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                entityInventoryPath, 0, std::array<const char*, 0>());
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/entity_link",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * Endpoint override class for delivering Endpoint Schema
 */
inline void requestRoutesEndpoint(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Endpoints/<str>")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& endpointId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        crow::connections::systemBus->async_method_call(
            [asyncResp, fabricId,
             endpointId](const boost::system::error_code ec,
                         const std::vector<std::string>& objects) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const std::string& object : objects)
            {
                // Get the fabricId object
                if (!boost::ends_with(object, fabricId))
                {
                    continue;
                }
                crow::connections::systemBus->async_method_call(
                    [asyncResp, fabricId, endpointId](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Iterate over all retrieved ObjectPaths.
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<std::string,
                                                   std::vector<std::string>>>>&
                             object : subtree)
                    {
                        // Get the endpointId object
                        const std::string& path = object.first;
                        sdbusplus::message::object_path objPath(path);
                        if (objPath.filename() != endpointId)
                        {
                            continue;
                        }
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#Endpoint.v1_6_0.Endpoint";
                        asyncResp->res.jsonValue["@odata.id"] =
                            std::string("/redfish/v1/Fabrics/")
                                .append(fabricId)
                                .append("/Endpoints/")
                                .append(endpointId);
                        asyncResp->res.jsonValue["Id"] = endpointId;
                        asyncResp->res.jsonValue["Name"] = endpointId +
                                                           " Endpoint Resource";
                        nlohmann::json& connectedEntitiesArray =
                            asyncResp->res.jsonValue["ConnectedEntities"];
                        connectedEntitiesArray = nlohmann::json::array();
                        updateEndpointData(asyncResp, path, fabricId);
                        return;
                    }
                    // Couldn't find an object with that name.
                    // Return an error
                    messages::resourceNotFound(asyncResp->res,
                                               "#Endpoint.v1_6_0.Endpoint",
                                               endpointId);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", object, 0,
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item."
                        "Endpoint"});
                return;
            }
            // Couldn't find an object with that name. Return an
            // error
            messages::resourceNotFound(asyncResp->res, "#Fabric.v1_2_0.Fabric",
                                       fabricId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
            "/xyz/openbmc_project/inventory", 0,
            std::array<const char*, 1>{
                "xyz.openbmc_project.Inventory.Item.Fabric"});
    });
}

/**
 * @brief Get all port info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       service     Service.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       fabricId    Fabric Id.
 * @param[in]       switchId    Switch Id.
 * @param[in]       portId      Port Id.
 */
inline void getFabricsPortMetricsData(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& objPath,
    const std::string& fabricId, const std::string& switchId,
    const std::string& portId)
{
    BMCWEB_LOG_DEBUG("Access port metrics data");

    std::string portMetricsURI =
        (boost::format("/redfish/v1/Fabrics/%s/Switches/%s/Ports/"
                       "%s/Metrics") %
         fabricId % switchId % portId)
            .str();
    asyncResp->res.jsonValue = {
        {"@odata.type", "#PortMetrics.v1_3_0.PortMetrics"},
        {"@odata.id", portMetricsURI},
        {"Name", portId + " Port Metrics"},
        {"Id", "Metrics"}};

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const boost::container::flat_map<
                        std::string, std::variant<uint16_t, uint32_t, uint64_t,
                                                  int64_t>>& properties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(asyncResp->res);
            return;
        }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
        asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
            "#NvidiaPortMetrics.v1_0_0.NvidiaPortMetrics";
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
        for (const auto& property : properties)
        {
            if ((property.first == "TXBytes") || (property.first == "RXBytes"))
            {
                const size_t* value = std::get_if<size_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for TX/RX bytes");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue[property.first] = *value;
            }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            else if (property.first == "RXNoProtocolBytes")
            {
                const uint64_t* value = std::get_if<uint64_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for RXNoProtocolBytes");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["RXNoProtocolBytes"] =
                    *value;
            }
            else if (property.first == "TXNoProtocolBytes")
            {
                const uint64_t* value = std::get_if<uint64_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for TXNoProtocolBytes");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["TXNoProtocolBytes"] =
                    *value;
            }
            else if (property.first == "DataCRCCount")
            {
                const uint32_t* value = std::get_if<uint32_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for DataCRCCount");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                        ["DataCRCCount"] = *value;
            }
            else if (property.first == "FlitCRCCount")
            {
                const uint32_t* value = std::get_if<uint32_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for FlitCRCCount");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                        ["FlitCRCCount"] = *value;
            }
            else if (property.first == "RecoveryCount")
            {
                const uint32_t* value = std::get_if<uint32_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for RecoveryCount");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                        ["RecoveryCount"] = *value;
            }
            else if (property.first == "ReplayErrorsCount")
            {
                const uint32_t* value = std::get_if<uint32_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for ReplayCount");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                        ["ReplayCount"] = *value;
            }
            else if (property.first == "RuntimeError")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for RuntimeError");
                    messages::internalError(asyncResp->res);
                    return;
                }

                if (*value != 0)
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["RuntimeError"] = true;
                }
                else
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["RuntimeError"] = false;
                }
            }
            else if (property.first == "TrainingError")
            {
                const uint16_t* value = std::get_if<uint16_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for TrainingError");
                    messages::internalError(asyncResp->res);
                    return;
                }

                if (*value != 0)
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["TrainingError"] = true;
                }
                else
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["NVLinkErrors"]
                                            ["TrainingError"] = false;
                }
            }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            else if (property.first == "ceCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res
                    .jsonValue["PCIeErrors"]["CorrectableErrorCount"] = *value;
            }
            else if (property.first == "nonfeCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["NonFatalErrorCount"] =
                    *value;
            }
            else if (property.first == "feCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["FatalErrorCount"] =
                    *value;
            }
            else if (property.first == "L0ToRecoveryCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["L0ToRecoveryCount"] =
                    *value;
            }
            else if (property.first == "ReplayCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["ReplayCount"] = *value;
            }
            else if (property.first == "ReplayRolloverCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["ReplayRolloverCount"] =
                    *value;
            }
            else if (property.first == "NAKSentCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["NAKSentCount"] = *value;
            }
            else if (property.first == "NAKReceivedCount")
            {
                const int64_t* value = std::get_if<int64_t>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PCIeErrors"]["NAKReceivedCount"] =
                    *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

/**
 * Port Metrics override class for delivering Port Metrics Schema
 */
inline void requestRoutesPortMetrics(App& app)
{
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Fabrics/<str>/Switches/<str>/Ports/<str>/Metrics")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& fabricId, const std::string& switchId,
                   const std::string& portId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        std::string objPath = std::string(inventoryRootPath);
        objPath += std::string(inventoryFabricStr) + fabricId;
        objPath += std::string(inventorySwitchStr) + switchId;
        objPath += std::string(inventoryPortStr);

        crow::connections::systemBus->async_method_call(
            [asyncResp{asyncResp}, fabricId, switchId,
             portId](const boost::system::error_code ec,
                     const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved
            // ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                // Get the portId object
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;
                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != portId ||
                    path.find(fabricId) == std::string::npos ||
                    path.find(switchId) == std::string::npos)
                {
                    continue;
                }
                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR("Got 0 Connection names");
                    continue;
                }
                const std::string& connectionName = connectionNames[0].first;

                getFabricsPortMetricsData(asyncResp, connectionName, objPath,
                                          fabricId, switchId, portId);
                return;
            }
            // Couldn't find an object with that name.
            // Return an error
            messages::resourceNotFound(asyncResp->res, "#Port.v1_4_0.Port",
                                       portId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", objPath, 0,
            std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item."
                                       "Port"});
    });
}

} // namespace redfish
