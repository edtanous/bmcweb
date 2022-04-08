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

#include <app.hpp>
#include <boost/format.hpp>
#include <utils/collection.hpp>
#include <utils/port_utils.hpp>
#include <utils/processor_utils.hpp>

#include <variant>

namespace redfish
{

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
 * @param[in]       fabricId    fabric id for redfish URI.
 */
inline void updatePortLinks(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& objPath,
                            const std::string& fabricId)
{
    BMCWEB_LOG_DEBUG << "Get Port Links";
    crow::connections::systemBus->async_method_call(
        [aResp, fabricId](const boost::system::error_code ec,
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
                sdbusplus::message::object_path objPath(portPath);
                const std::string& endpointId = objPath.filename();
                std::string endpointURI = "/redfish/v1/Fabrics/";
                endpointURI += fabricId;
                endpointURI += "/Endpoints/";
                endpointURI += endpointId;
                linksArray.push_back({{"@odata.id", endpointURI}});
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
inline void getPortObject(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& fabricId,
                          const std::string& switchId,
                          const std::string& portId, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Access port Data";
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
                if (objPath.filename() != portId)
                {
                    continue;
                }
                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }
                std::string portURI = "/redfish/v1/Fabrics/";
                portURI += fabricId;
                portURI += "/Switches/";
                portURI += switchId;
                portURI += "/Ports/";
                portURI += portId;
                asyncResp->res.jsonValue = {
                    {"@odata.type", "#Port.v1_4_0.Port"},
                    {"@odata.id", portURI},
                    {"Name", portId + " Resource"},
                    {"Id", portId}};
                std::string portMetricsURI = portURI + "/Metrics";
                asyncResp->res.jsonValue["Metrics"]["@odata.id"] =
                    portMetricsURI;

                const std::string& connectionName = connectionNames[0].first;
                redfish::port_utils::getPortData(asyncResp, connectionName,
                                                 path);
                updatePortLinks(asyncResp, path, fabricId);
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
    BMCWEB_LOG_DEBUG << "Get Switch Data";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for switch type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["SwitchType"] =
                        getSwitchType(*value);
                }
                else if (propertyName == "SupportedProtocols")
                {
                    nlohmann::json& protoArray =
                        asyncResp->res.jsonValue["SupportedProtocols"];
                    protoArray = nlohmann::json::array();
                    const std::vector<std::string>* protocols =
                        std::get_if<std::vector<std::string>>(&property.second);
                    if (protocols == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for supported protocols";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for enabled";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for asset properties";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue[propertyName] = *value;
                }
                else if (propertyName == "Version")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for revision";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["FirmwareVersion"] = *value;
                }
                else if (propertyName == "CurrentBandwidth")
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for CurrentBandwidth";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["CurrentBandwidthGbps"] = *value;
                }
                else if (propertyName == "MaxBandwidth")
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for MaxBandwidth";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for TotalSwitchWidth";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["TotalSwitchWidth"] = *value;
                }
                else if (propertyName == "CurrentPowerState")
                {
                    const std::string* state =
                        std::get_if<std::string>(&property.second);
                    if (*state ==
                        "xyz.openbmc_project.State.Chassis.PowerState.On")
                    {
                        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
                        asyncResp->res.jsonValue["Status"]["Health"] = "OK";
                    }
                    else if (*state ==
                             "xyz.openbmc_project.State.Chassis.PowerState.Off")
                    {
                        asyncResp->res.jsonValue["Status"]["State"] =
                            "StandbyOffline";
                        asyncResp->res.jsonValue["Status"]["Health"] =
                            "Critical";
                    }
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

inline void updateZoneData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& service,
                           const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Zone Data";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for zone type";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for enabled";
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
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#FabricCollection.FabricCollection";
                asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Fabrics";
                asyncResp->res.jsonValue["Name"] = "Fabric Collection";

                collection_util::getCollectionMembers(
                    asyncResp, "/redfish/v1/Fabrics",
                    {"xyz.openbmc_project.Inventory.Item.Fabric"});
            });
}

/**
 * Fabric override class for delivering Fabric Schema
 */
inline void requestRoutesFabric(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/")
        .privileges({{"Login"}})
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId) {
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
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<std::string,
                                                   std::vector<std::string>>>>&
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
                            BMCWEB_LOG_ERROR << "Got 0 Connection names";
                            continue;
                        }

                        asyncResp->res.jsonValue["@odata.type"] =
                            "#Fabric.v1_2_0.Fabric";
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/Fabrics/" + fabricId;
                        asyncResp->res.jsonValue["Id"] = fabricId;
                        asyncResp->res.jsonValue["Name"] =
                            fabricId + " Resource";
                        asyncResp->res.jsonValue["Endpoints"] = {
                            {"@odata.id",
                             "/redfish/v1/Fabrics/" + fabricId + "/Endpoints"}};
                        asyncResp->res.jsonValue["Switches"] = {
                            {"@odata.id",
                             "/redfish/v1/Fabrics/" + fabricId + "/Switches"}};
                        asyncResp->res.jsonValue["Zones"] = {
                            {"@odata.id",
                             "/redfish/v1/Fabrics/" + fabricId + "/Zones"}};

                        const std::string& connectionName =
                            connectionNames[0].first;

                        // Fabric item properties
                        crow::connections::systemBus->async_method_call(
                            [asyncResp](
                                const boost::system::error_code ec,
                                const std::vector<
                                    std::pair<std::string,
                                              dbus::utility::DbusVariantType>>&
                                    propertiesList) {
                                if (ec)
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                for (const std::pair<
                                         std::string,
                                         dbus::utility::DbusVariantType>&
                                         property : propertiesList)
                                {
                                    if (property.first == "Type")
                                    {
                                        const std::string* value =
                                            std::get_if<std::string>(
                                                &property.second);
                                        if (value == nullptr)
                                        {
                                            BMCWEB_LOG_DEBUG
                                                << "Null value returned "
                                                   "for fabric type";
                                            messages::internalError(
                                                asyncResp->res);
                                            return;
                                        }
                                        asyncResp->res.jsonValue["FabricType"] =
                                            getFabricType(*value);
                                    }
                                }
                            },
                            connectionName, path,
                            "org.freedesktop.DBus.Properties", "GetAll",
                            "xyz.openbmc_project.Inventory.Item.Fabric");

                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& fabricId) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#SwitchCollection.SwitchCollection";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Fabrics/" + fabricId + "/Switches";
                asyncResp->res.jsonValue["Name"] = "Switch Collection";

                crow::connections::systemBus->async_method_call(
                    [asyncResp,
                     fabricId](const boost::system::error_code ec,
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
                            collection_util::getCollectionMembers(
                                asyncResp,
                                "/redfish/v1/Fabrics/" + fabricId + "/Switches",
                                {"xyz.openbmc_project.Inventory.Item.Switch"},
                                object.c_str());
                            return;
                        }
                        // Couldn't find an object with that name. Return an
                        // error
                        messages::resourceNotFound(
                            asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                parentChassisPath, 0,
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
    BMCWEB_LOG_DEBUG << "Get parent chassis link";
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

            // Check if PCIeDevice on this chassis
            crow::connections::systemBus->async_method_call(
                [aResp, chassisName, chassisPath](
                    const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    if (ec)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    // If PCIeDevice doesn't exists on this chassis
                    // Check PCIeDevice on its parent chassis
                    if (subtree.empty())
                    {
                        getSwitchParentChassisPCIeDeviceLink(aResp, chassisPath,
                                                             chassisName);
                    }
                    else
                    {
                        for (const auto& [objectPath, serviceMap] : subtree)
                        {
                            // Process same device
                            if (!boost::ends_with(objectPath, chassisName))
                            {
                                continue;
                            }
                            std::string pcieDeviceLink = "/redfish/v1/Chassis/";
                            pcieDeviceLink += chassisName;
                            pcieDeviceLink += "/PCIeDevices/";
                            pcieDeviceLink += chassisName;
                            aResp->res.jsonValue["Links"]["PCIeDevice"] = {
                                {"@odata.id", pcieDeviceLink}};
                        }
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree", chassisPath,
                0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice"});
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
    BMCWEB_LOG_DEBUG << "Get endpoint links";
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
            nlohmann::json& linksArray =
                aResp->res.jsonValue["Links"]["Endpoints"];
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
    BMCWEB_LOG_DEBUG << "Get zone endpoint links";
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
            nlohmann::json& linksArray =
                aResp->res.jsonValue["Links"]["Endpoints"];
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
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId,
                                              const std::string& switchId) {
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
                                             std::string,
                                             std::vector<std::string>>>>&
                                         object : subtree)
                                {
                                    // Get the switchId object
                                    const std::string& path = object.first;
                                    const std::vector<std::pair<
                                        std::string, std::vector<std::string>>>&
                                        connectionNames = object.second;
                                    sdbusplus::message::object_path objPath(
                                        path);
                                    if (objPath.filename() != switchId)
                                    {
                                        continue;
                                    }
                                    if (connectionNames.size() < 1)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Got 0 Connection names";
                                        continue;
                                    }
                                    std::string switchURI =
                                        "/redfish/v1/Fabrics/";
                                    switchURI += fabricId;
                                    switchURI += "/Switches/";
                                    switchURI += switchId;
                                    std::string portsURI = switchURI;
                                    portsURI += "/Ports";
                                    std::string switchMetricURI = switchURI;
                                    switchMetricURI += "/SwitchMetrics";
                                    asyncResp->res.jsonValue["@odata.type"] =
                                        "#Switch.v1_6_0.Switch";
                                    asyncResp->res.jsonValue["@odata.id"] =
                                        switchURI;
                                    asyncResp->res.jsonValue["Id"] = switchId;
                                    asyncResp->res.jsonValue["Name"] =
                                        switchId + " Resource";
                                    asyncResp->res.jsonValue["Ports"] = {
                                        {"@odata.id", portsURI}};
                                    asyncResp->res.jsonValue["Metrics"] = {
                                        {"@odata.id", switchMetricURI}};
                                    const std::string& connectionName =
                                        connectionNames[0].first;
                                    updateSwitchData(asyncResp, connectionName,
                                                     path);
                                    // Link association to parent chassis
                                    getSwitchChassisLink(asyncResp, path);
                                    // Link association to endpoints
                                    getSwitchEndpointsLink(asyncResp, path,
                                                           fabricId);
                                    return;
                                }
                                // Couldn't find an object with that name.
                                // Return an error
                                messages::resourceNotFound(
                                    asyncResp->res, "#Switch.v1_6_0.Switch",
                                    switchId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            object, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Switch"});
                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
    BMCWEB_LOG_DEBUG << "Get memory ecc data.";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const DimmProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "ceCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
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
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
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
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId,
                                              const std::string& switchId) {
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
                                             std::string,
                                             std::vector<std::string>>>>&
                                         object : subtree)
                                {
                                    // Get the switchId object
                                    const std::string& path = object.first;
                                    const std::vector<std::pair<
                                        std::string, std::vector<std::string>>>&
                                        connectionNames = object.second;
                                    sdbusplus::message::object_path objPath(
                                        path);
                                    if (objPath.filename() != switchId)
                                    {
                                        continue;
                                    }
                                    if (connectionNames.size() < 1)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Got 0 Connection names";
                                        continue;
                                    }
                                    std::string switchURI =
                                        "/redfish/v1/Fabrics/";
                                    switchURI += fabricId;
                                    switchURI += "/Switches/";
                                    switchURI += switchId;
                                    std::string switchMetricURI = switchURI;
                                    switchMetricURI += "/SwitchMetrics";
                                    asyncResp->res.jsonValue["@odata.type"] =
                                        "#SwitchMetrics.v1_0_0.SwitchMetrics";
                                    asyncResp->res.jsonValue["@odata.id"] =
                                        switchMetricURI;
                                    asyncResp->res.jsonValue["Id"] = switchId;
                                    asyncResp->res.jsonValue["Name"] =
                                        switchId + " Metrics";
                                    const std::string& connectionName =
                                        connectionNames[0].first;
                                    const std::vector<std::string>& interfaces =
                                        connectionNames[0].second;
                                    if (std::find(
                                            interfaces.begin(),
                                            interfaces.end(),
                                            "xyz.openbmc_project.Memory.MemoryECC") !=
                                        interfaces.end())
                                    {
                                        getInternalMemoryMetrics(
                                            asyncResp, connectionName, path);
                                    }
                                    if (std::find(
                                            interfaces.begin(),
                                            interfaces.end(),
                                            "xyz.openbmc_project.PCIe.PCIeECC") !=
                                        interfaces.end())
                                    {
                                        redfish::processor_utils::
                                            getPCIeErrorData(asyncResp,
                                                             connectionName,
                                                             path);
                                    }

                                    return;
                                }
                                // Couldn't find an object with that name.
                                // Return an error
                                messages::resourceNotFound(
                                    asyncResp->res, "#Switch.v1_6_0.Switch",
                                    switchId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            object, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Switch"});
                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
 * PortCollection derived class for delivering Port Collection Schema
 */
inline void requestRoutesPortCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>/Ports/")
        .privileges({{"Login"}})
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId,
                                              const std::string& switchId) {
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
                            [asyncResp, fabricId, switchId, portsURI](
                                const boost::system::error_code ec,
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
                                    collection_util::getCollectionMembers(
                                        asyncResp, portsURI,
                                        {"xyz.openbmc_project.Inventory.Item."
                                         "Port"},
                                        object.c_str());
                                    return;
                                }
                                // Couldn't find an object with that name.
                                // Return an error
                                messages::resourceNotFound(
                                    asyncResp->res, "#Switch.v1_6_0.Switch",
                                    switchId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper",
                            "GetSubTreePaths", object.c_str(), 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Switch"});
                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId,
                                              const std::string& switchId,
                                              const std::string& portId) {
            crow::connections::systemBus->async_method_call(
                [asyncResp, fabricId, switchId,
                 portId](const boost::system::error_code ec,
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
                             portId](const boost::system::error_code ec,
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
                                    getPortObject(asyncResp, fabricId, switchId,
                                                  portId, object);
                                    return;
                                }
                                // Couldn't find an object with that name.
                                // Return an error
                                messages::resourceNotFound(
                                    asyncResp->res, "#Switch.v1_6_0.Switch",
                                    switchId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper",
                            "GetSubTreePaths", object, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Switch"});
                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.Fabric"});
        });
}

inline void requestRoutesZoneCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Zones/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& fabricId) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#ZoneCollection.ZoneCollection";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Fabrics/" + fabricId + "/Zones";
                asyncResp->res.jsonValue["Name"] = "Zone Collection";

                crow::connections::systemBus->async_method_call(
                    [asyncResp,
                     fabricId](const boost::system::error_code ec,
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
                            collection_util::getCollectionMembers(
                                asyncResp,
                                "/redfish/v1/Fabrics/" + fabricId + "/Zones",
                                {"xyz.openbmc_project.Inventory.Item.Zone"},
                                object.c_str());
                            return;
                        }
                        // Couldn't find an object with that name. Return an
                        // error
                        messages::resourceNotFound(
                            asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId,
                                              const std::string& zoneId) {
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
                            [asyncResp, fabricId,
                             zoneId](const boost::system::error_code ec,
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
                                             std::string,
                                             std::vector<std::string>>>>&
                                         object : subtree)
                                {
                                    // Get the zoneId object
                                    const std::string& path = object.first;
                                    const std::vector<std::pair<
                                        std::string, std::vector<std::string>>>&
                                        connectionNames = object.second;
                                    sdbusplus::message::object_path objPath(
                                        path);
                                    if (objPath.filename() != zoneId)
                                    {
                                        continue;
                                    }
                                    if (connectionNames.size() < 1)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Got 0 Connection names";
                                        continue;
                                    }
                                    std::string zoneURI =
                                        "/redfish/v1/Fabrics/";
                                    zoneURI += fabricId;
                                    zoneURI += "/Zones/";
                                    zoneURI += zoneId;
                                    asyncResp->res.jsonValue["@odata.type"] =
                                        "#Zone.v1_6_1.Zone";
                                    asyncResp->res.jsonValue["@odata.id"] =
                                        zoneURI;
                                    asyncResp->res.jsonValue["Id"] = zoneId;
                                    asyncResp->res.jsonValue["Name"] =
                                        " Zone " + zoneId;
                                    const std::string& connectionName =
                                        connectionNames[0].first;
                                    updateZoneData(asyncResp, connectionName,
                                                   path);

                                    // Link association to endpoints
                                    getZoneEndpointsLink(asyncResp, path,
                                                         fabricId);
                                    return;
                                }
                                // Couldn't find an object with that name.
                                // Return an error
                                messages::resourceNotFound(asyncResp->res,
                                                           "#Zone.v1_6_1.Zone",
                                                           zoneId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            object, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Zone"});
                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& fabricId) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#EndpointCollection.EndpointCollection";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Fabrics/" + fabricId + "/Endpoints";
                asyncResp->res.jsonValue["Name"] = "Endpoint Collection";

                crow::connections::systemBus->async_method_call(
                    [asyncResp,
                     fabricId](const boost::system::error_code ec,
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
                            collection_util::getCollectionMembers(
                                asyncResp,
                                "/redfish/v1/Fabrics/" + fabricId +
                                    "/Endpoints",
                                {"xyz.openbmc_project.Inventory.Item.Endpoint"},
                                object.c_str());
                            return;
                        }
                        // Couldn't find an object with that name. Return an
                        // error
                        messages::resourceNotFound(
                            asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
                BMCWEB_LOG_DEBUG << "DBUS response error";
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
    BMCWEB_LOG_DEBUG << "Get endpoint health.";

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
                BMCWEB_LOG_DEBUG << "DBUS response error";
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
                    const bool* isFunctional =
                        std::get_if<bool>(&property.second);
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
                [aResp, chassisName, parentChassisName, entityLink,
                 processorPath](
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
                            BMCWEB_LOG_ERROR << "Got 0 service "
                                                "names";
                            messages::internalError(aResp->res);
                            return;
                        }
                        const std::string& serviceName = serviceMap[0].first;
                        // Get PCIeDevice Data
                        getProcessorPCIeDeviceData(aResp, serviceName,
                                                   objectPath, entityLink);
                        // Update processor health
                        getProcessorEndpointHealth(aResp, serviceName,
                                                   processorPath);
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                parentChassisPath, 0,
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
inline void
    getProcessorEndpointData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& processorPath,
                             const std::string& entityLink)
{
    BMCWEB_LOG_DEBUG << "Get processor endpoint data";
    crow::connections::systemBus->async_method_call(
        [aResp, processorPath,
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
            // Check if PCIeDevice on this chassis
            crow::connections::systemBus->async_method_call(
                [aResp, chassisName, chassisPath, entityLink, processorPath](
                    const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    if (ec)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    // If PCIeDevice doesn't exists on this chassis
                    // Check PCIeDevice on its parent chassis
                    if (subtree.empty())
                    {
                        getProcessorParentEndpointData(aResp, chassisPath,
                                                       chassisName, entityLink,
                                                       processorPath);
                    }
                    else
                    {
                        for (const auto& [objectPath, serviceMap] : subtree)
                        {
                            // Process same device
                            if (!boost::ends_with(objectPath, chassisName))
                            {
                                continue;
                            }
                            if (serviceMap.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 service names";
                                messages::internalError(aResp->res);
                                return;
                            }
                            const std::string& serviceName =
                                serviceMap[0].first;
                            // Get PCIeDevice Data
                            getProcessorPCIeDeviceData(aResp, serviceName,
                                                       objectPath, entityLink);
                            // Update processor health
                            getProcessorEndpointHealth(aResp, serviceName,
                                                       processorPath);
                        }
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree", chassisPath,
                0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice"});
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
                BMCWEB_LOG_DEBUG << "DBUS response error";
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
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for protocol type";
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
    BMCWEB_LOG_DEBUG << "Get endpoint port data";
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
                    BMCWEB_LOG_ERROR << "Invalid port link path";
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
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            // Filter port link object
                            if (object.first != portPath)
                            {
                                continue;
                            }
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                connectionNames = object.second;
                            if (connectionNames.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 Connection names";
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
                        getConnectedPortsLinks(aResp, portPaths, fabricId,
                                               switchPath);
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                processorPath + "/all_switches",
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
    BMCWEB_LOG_DEBUG << "Get Endpoint Data";
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
                    BMCWEB_LOG_ERROR << "Invalid entity link path";
                    continue;
                }
                std::string entityInventoryPath =
                    entityPath.substr(0, separator);
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
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            // Filter entity link object
                            if (object.first != entityPath)
                            {
                                continue;
                            }
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                connectionNames = object.second;
                            if (connectionNames.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 Connection names";
                                continue;
                            }
                            const std::vector<std::string>& interfaces =
                                connectionNames[0].second;
                            const std::string acceleratorInterface =
                                "xyz.openbmc_project.Inventory.Item."
                                "Accelerator";
                            if (std::find(interfaces.begin(), interfaces.end(),
                                          acceleratorInterface) !=
                                interfaces.end())
                            {
                                sdbusplus::message::object_path objectPath(
                                    entityPath);
                                const std::string& entityLink =
                                    "/redfish/v1/Systems/system/Processors/" +
                                    objectPath.filename();
                                // Get processor PCIe device data
                                getProcessorEndpointData(aResp, entityPath,
                                                         entityLink);
                                // Get port endpoint data
                                getEndpointPortData(aResp, objPath, entityPath,
                                                    fabricId);
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
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& fabricId, const std::string& endpointId) {
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
                                                 std::string,
                                                 std::vector<std::string>>>>&
                                             object : subtree)
                                    {
                                        // Get the endpointId object
                                        const std::string& path = object.first;
                                        sdbusplus::message::object_path objPath(
                                            path);
                                        if (objPath.filename() != endpointId)
                                        {
                                            continue;
                                        }
                                        asyncResp->res
                                            .jsonValue["@odata.type"] =
                                            "#Endpoint.v1_6_0.Endpoint";
                                        asyncResp->res.jsonValue["@odata.id"] =
                                            std::string("/redfish/v1/Fabrics/")
                                                .append(fabricId)
                                                .append("/Endpoints/")
                                                .append(endpointId);
                                        asyncResp->res.jsonValue["Id"] =
                                            endpointId;
                                        asyncResp->res.jsonValue["Name"] =
                                            endpointId + " Endpoint Resource";
                                        nlohmann::json& connectedEntitiesArray =
                                            asyncResp->res
                                                .jsonValue["ConnectedEntities"];
                                        connectedEntitiesArray =
                                            nlohmann::json::array();
                                        updateEndpointData(asyncResp, path,
                                                           fabricId);
                                        return;
                                    }
                                    // Couldn't find an object with that name.
                                    // Return an error
                                    messages::resourceNotFound(
                                        asyncResp->res,
                                        "#Endpoint.v1_6_0.Endpoint",
                                        endpointId);
                                },
                                "xyz.openbmc_project.ObjectMapper",
                                "/xyz/openbmc_project/object_mapper",
                                "xyz.openbmc_project.ObjectMapper",
                                "GetSubTree", object, 0,
                                std::array<const char*, 1>{
                                    "xyz.openbmc_project.Inventory.Item."
                                    "Endpoint"});
                            return;
                        }
                        // Couldn't find an object with that name. Return an
                        // error
                        messages::resourceNotFound(
                            asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
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
 * @param[in]       fabricId    Fabric Id.
 * @param[in]       switchId    Switch Id.
 * @param[in]       portId      Port Id.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getPortMetricsData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& fabricId, const std::string& switchId,
                       const std::string& portId, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Access port metrics data";
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, fabricId, switchId,
         portId](const boost::system::error_code ec,
                 const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved objectPaths
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
                if (objPath.filename() != portId)
                {
                    continue;
                }
                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }
                std::string portMetricsURI =
                    (boost::format("/redfish/v1/Fabrics/%s/Switches/%s/Ports/"
                                   "%s/Metrics") %
                     fabricId % switchId % portId)
                        .str();
                asyncResp->res.jsonValue = {
                    {"@odata.type", "#PortMetrics.v1_2_0.PortMetrics"},
                    {"@odata.id", portMetricsURI},
                    {"Name", portId + " Port Metrics"},
                    {"Id", "PortMetrics"}};

                const std::string& connectionName = connectionNames[0].first;
                crow::connections::systemBus->async_method_call(
                    [asyncResp](
                        const boost::system::error_code ec,
                        const boost::container::flat_map<
                            std::string, std::variant<size_t>>& properties) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        for (const auto& property : properties)
                        {
                            if ((property.first == "TXBytes") ||
                                (property.first == "RXBytes"))
                            {
                                const size_t* value =
                                    std::get_if<size_t>(&property.second);
                                if (value == nullptr)
                                {
                                    BMCWEB_LOG_DEBUG << "Null value returned "
                                                        "for TX/RX bytes";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                asyncResp->res.jsonValue[property.first] =
                                    *value;
                            }
                        }
                    },
                    connectionName, objPath, "org.freedesktop.DBus.Properties",
                    "GetAll", "xyz.openbmc_project.Inventory.Item.Port");
                return;
            }
            // Couldn't find an object with that name. Return an error
            messages::resourceNotFound(asyncResp->res, "#Port.v1_4_0.Port",
                                       portId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", objPath, 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item."
                                   "Port"});
} // namespace redfish

/**
 * Port Metrics override class for delivering Port Metrics Schema
 */
inline void requestRoutesPortMetrics(App& app)
{
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Fabrics/<str>/Switches/<str>/Ports/<str>/Metrics")
        .privileges({{"Login"}})
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& fabricId,
                                              const std::string& switchId,
                                              const std::string& portId) {
            crow::connections::systemBus->async_method_call(
                [asyncResp, fabricId, switchId,
                 portId](const boost::system::error_code ec,
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
                             portId](const boost::system::error_code ec,
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
                                    getPortMetricsData(asyncResp, fabricId,
                                                       switchId, portId,
                                                       object);
                                    return;
                                }
                                // Couldn't find an object with that name.
                                // Return an error
                                messages::resourceNotFound(
                                    asyncResp->res, "#Switch.v1_6_0.Switch",
                                    switchId);
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper",
                            "GetSubTreePaths", object, 0,
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Inventory.Item.Switch"});
                        return;
                    }
                    // Couldn't find an object with that name. Return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Fabric.v1_2_0.Fabric", fabricId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                "/xyz/openbmc_project/inventory", 0,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.Fabric"});
        });
}

} // namespace redfish