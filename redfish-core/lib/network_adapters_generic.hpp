#pragma once

#include "app.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"

#include <utils/chassis_utils.hpp>
#include <utils/collection.hpp>
#include <utils/json_utils.hpp>
#include <utils/port_utils.hpp>

namespace redfish
{

/**
 * @brief Retrieves valid getValidNetworkAdapter path
 * @param asyncResp   Pointer to object holding response data
 * @param callback  Callback for next step to get valid NetworkInterface path
 */
template <typename Callback>
void getValidNetworkAdapterPath(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& networkAdapterId, Callback&& callback)
{
    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.Inventory.Item.NetworkInterface"};

    auto respHandler =
        [callback{std::forward<Callback>(callback)}, asyncResp,
         networkAdapterId](const boost::system::error_code ec,
                           const dbus::utility::MapperGetSubTreePathsResponse&
                               networkAdapterPaths) mutable {
        if (ec)
        {
            BMCWEB_LOG_ERROR("getValidNetworkAdapterPath respHandler DBUS error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }

        std::optional<std::string> networkAdapterPath;
        std::string networkAdapterName;
        for (const std::string& networkAdapter : networkAdapterPaths)
        {
            sdbusplus::message::object_path path(networkAdapter);
            networkAdapterName = path.filename();
            if (networkAdapterName.empty())
            {
                BMCWEB_LOG_ERROR("Failed to find '/' in {}", networkAdapter);
                continue;
            }
            if (networkAdapterName == networkAdapterId)
            {
                networkAdapterPath = networkAdapter;
                break;
            }
        }
        callback(networkAdapterPath);
    };

    // Get the NetworkAdatper Collection
    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

inline void doNetworkAdaptersCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        BMCWEB_LOG_ERROR("Not a valid chassis ID{}", chassisId);
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] =
        "#NetworkAdapterCollection.NetworkAdapterCollection";
    asyncResp->res.jsonValue["Name"] = "Network Adapter Collection";
    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format("redfish/v1/Chassis/"+ chassisId+ "/NetworkAdapters");

    crow::connections::systemBus->async_method_call(
        [chassisId, asyncResp](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& objects) {
        if (ec == boost::system::errc::io_error)
        {
            asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
            asyncResp->res.jsonValue["Members@odata.count"] = 0;
            return;
        }

        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error {}", ec.value());
            messages::internalError(asyncResp->res);
            return;
        }

        nlohmann::json& members = asyncResp->res.jsonValue["Members"];
        members = nlohmann::json::array();
        std::map<std::string, int> networkAdaptersCollectionMap;
        std::vector<std::string> pathNames;
        for (const auto& object : objects)
        {
            sdbusplus::message::object_path path(object);
            std::string parentPath = path.parent_path();

            if (parentPath.find(chassisId) != std::string::npos ||
                path.filename() == chassisId)
            {
                nlohmann::json::object_t member;
                member["@odata.id"] =  boost::urls::format("redfish/v1/Chassis"+ chassisId + "NetworkAdapters" +path.filename());
                members.push_back(std::move(member));
            }
        }

        asyncResp->res.jsonValue["Members@odata.count"] = members.size();
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory/", 0,
        std::array<std::string, 1>{
            "xyz.openbmc_project.Inventory.Item.NetworkInterface"});
}

inline std::string convertHealthToRF(const std::string& health)
{
    if (health == "xyz.openbmc_project.State.Decorator.Health.HealthType.OK")
    {
        return "OK";
    }
    if (health ==
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning")
    {
        return "Warning";
    }
    if (health ==
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical")
    {
        return "Critical";
    }
    // Unknown or others
    return "";
}

inline void getHealthData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& service,
                          const std::string& objPath)
{
    using PropertiesMap =
        boost::container::flat_map<std::string,
                                   std::variant<std::string, size_t>>;

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
            if (propertyName == "Health")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned " "for port type");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Status"]["Health"] =
                    convertHealthToRF(*value);
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.State.Decorator.Health");
}

inline void
    getHealthByAssociation(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& objPath,
                           const std::string& networkAdapterId)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp,
         networkAdapterId](const boost::system::error_code& ec,
                           std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            // no state sensors attached.
            return;
        }

        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        for (const std::string& sensorPath : *data)
        {
            // Check Interface in Object or not
            crow::connections::systemBus->async_method_call(
                [asyncResp, sensorPath, networkAdapterId](
                    const boost::system::error_code ec,
                    const std::vector<std::pair<
                        std::string, std::vector<std::string>>>& object) {
                if (ec)
                {
                    // the path does not implement Decorator Health
                    // interfaces
                    return;
                }
                getHealthData(asyncResp, object.front().first, sensorPath);
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject", sensorPath,
                std::array<std::string, 1>(
                    {"xyz.openbmc_project.State.Decorator.Health"}));
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_states",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getAssetData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& objPath,
                         const std::string& networkAdapterId)
{
    using PropertyType = std::variant<std::string>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;

    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath, networkAdapterId](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                object) {
        if (ec)
        {
            // the path does not implement Decorator Asset
            // interfaces
            return;
        }

        std::string service = object.front().first;

        // Get interface properties
        crow::connections::systemBus->async_method_call(
            [asyncResp, service](const boost::system::error_code ec,
                                 const PropertiesMap& properties) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Manufacturer")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value != nullptr)
                    {
                        asyncResp->res.jsonValue["Manufacturer"] = *value;
                    }
                }
                else if (propertyName == "SerialNumber")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value != nullptr)
                    {
                        asyncResp->res.jsonValue["SerialNumber"] = *value;
                    }
                }
                else if (propertyName == "PartNumber")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value != nullptr)
                    {
                        asyncResp->res.jsonValue["SerialNumber"] = *value;
                    }
                }
                else if (propertyName == "Model")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value != nullptr)
                    {
                        asyncResp->res.jsonValue["Model"] = *value;
                    }
                }
            }
        },
            service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
            "xyz.openbmc_project.Inventory.Decorator.Asset");
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", objPath,
        std::array<std::string, 1>(
            {"xyz.openbmc_project.Inventory.Decorator.Asset"}));
}

inline void
    doNetworkAdapter(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& chassisId,
                     const std::string& networkAdapterId,
                     const std::optional<std::string>& validNetworkAdapterPath)
{
    if (!validNetworkAdapterPath)
    {
        BMCWEB_LOG_ERROR("Not a valid networkAdapter ID{}", networkAdapterId);
        messages::resourceNotFound(asyncResp->res, "NetworkAdapter",
                                   networkAdapterId);
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] =
        "#NetworkAdapter.v1_9_0.NetworkAdapter";
    asyncResp->res.jsonValue["Name"] = networkAdapterId;
    asyncResp->res.jsonValue["Id"] = networkAdapterId;

    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format("redfish/v1/Chassis/"+ chassisId+ "/NetworkAdapters/"+chassisId);
        
    asyncResp->res.jsonValue["Ports"]["@odata.id"] =
        boost::urls::format("redfish/v1/Chassis/"+ chassisId + "/NetworkAdapters/"+ chassisId+ "/Ports");
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    getAssetData(asyncResp, *validNetworkAdapterPath, networkAdapterId);
    getHealthByAssociation(asyncResp, *validNetworkAdapterPath,
                           networkAdapterId);
}

inline void
    doPortCollection(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& chassisId,
                     const std::string& networkAdapterId,
                     const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        BMCWEB_LOG_ERROR("Not a valid chassis ID{}", chassisId);
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }

    asyncResp->res.jsonValue["@odata.type"] = "#PortCollection.PortCollection";
    asyncResp->res.jsonValue["Name"] = "Port Collection";
    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format("redfish/v1/Chassis/"+ chassisId+"/NetworkAdapters/"+networkAdapterId+"/Ports");

    crow::connections::systemBus->async_method_call(
        [chassisId, networkAdapterId, asyncResp{std::move(asyncResp)}](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& objects) {
        if (ec == boost::system::errc::io_error)
        {
            asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
            asyncResp->res.jsonValue["Members@odata.count"] = 0;
            return;
        }

        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error {}", ec.value());
            messages::internalError(asyncResp->res);
            return;
        }

        for (const auto& path : objects)
        {
            if (!boost::ends_with(path, networkAdapterId))
            {
                continue;
            }
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/Chassis/" + chassisId + "/NetworkAdapters/" +
                networkAdapterId + "/Ports";
            asyncResp->res.jsonValue["@odata.type"] =
                "#PortCollection.PortCollection";
            asyncResp->res.jsonValue["Name"] = "Port Collection";

            collection_util::getCollectionMembersByAssociation(
                asyncResp,
                "/redfish/v1/Chassis/" + chassisId + "/NetworkAdapters/" +
                    networkAdapterId + "/Ports",
                path + "/all_states",
                {"xyz.openbmc_project.Inventory.Item.Port"});
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.NetworkInterface"});
}

inline void handleNetworkAdaptersCollectionGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        std::bind_front(doNetworkAdaptersCollection, asyncResp, chassisId));
}

inline void handleNetworkAdapterGetNext(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId, const std::string& networkAdapterId,
    const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        BMCWEB_LOG_ERROR("Not a valid chassis ID{}", chassisId);
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }

    getValidNetworkAdapterPath(asyncResp, networkAdapterId,
                               std::bind_front(doNetworkAdapter, asyncResp,
                                               chassisId, networkAdapterId));
}

inline void
    handleNetworkAdapterGet(App& app, const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& chassisId,
                            const std::string& networkAdapterId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        std::bind_front(doNetworkAdapter, asyncResp, chassisId,
                        networkAdapterId));
}

inline void handlePortsCollectionGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    [[maybe_unused]] const std::string& networkAdapterId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        std::bind_front(doPortCollection, asyncResp, chassisId,
                        networkAdapterId));
}

inline void getPortData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& service, const std::string& objPath,
                        const std::string& chassisId,
                        const std::string& networkAdapterId,
                        const std::string& portId)
{
    asyncResp->res.jsonValue["@odata.type"] = "#Port.v1_6_0.Port";
    asyncResp->res.jsonValue["Id"] = portId;
    asyncResp->res.jsonValue["Name"] = "Port";
    asyncResp->res.jsonValue["LinkNetworkTechnology"] = "Ethernet";
    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format("redfish/v1/Chassis/" +chassisId+"/NetworkAdapters/"+networkAdapterId+"/Ports/"+portId);

    using PropertiesMap =
        boost::container::flat_map<std::string,
                                   std::variant<std::string, size_t>>;
    // Get interface properties
    crow::connections::systemBus->async_method_call(
        [asyncResp, networkAdapterId](const boost::system::error_code ec,
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
                    BMCWEB_LOG_ERROR("Null value returned " "for port type");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PortType"] =
                    port_utils::getPortType(*value);
            }
            else if (propertyName == "CurrentSpeed")
            {
                const size_t* value = std::get_if<size_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned " "for current speed");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["CurrentSpeedGbps"] = *value;
            }
            else if (propertyName == "Protocol")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned " "for protocol type");
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["PortProtocol"] =
                    port_utils::getPortProtocol(*value);
            }
            else if (propertyName == "LinkStatus")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned " "for link status");
                    messages::internalError(asyncResp->res);
                    return;
                }
                if (*value ==
                        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.LinkDown" ||
                    *value ==
                        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.LinkUp")
                {
                    asyncResp->res.jsonValue["Status"]["Health"] = "OK";
                }
                else if (
                    *value ==
                    "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.NoLink")
                {
                    asyncResp->res.jsonValue["Status"]["Health"] = "Critical";
                }
            }
            else if (propertyName == "LinkState")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned " "for link state");
                    messages::internalError(asyncResp->res);
                    return;
                }
                if (*value ==
                    "xyz.openbmc_project.Inventory.Item.Port.LinkStates.Enabled")
                {
                    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
                }
                else if (
                    *value ==
                    "xyz.openbmc_project.Inventory.Item.Port.LinkStates.Disabled")
                {
                    asyncResp->res.jsonValue["Status"]["State"] = "Disabled";
                }
                else if (
                    *value ==
                    "xyz.openbmc_project.Inventory.Item.Port.LinkStates.Error")
                {
                    asyncResp->res.jsonValue["Status"]["State"] =
                        "UnavailableOffline";
                }
                else
                {
                    asyncResp->res.jsonValue["Status"]["State"] = "Absent";
                }
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Port");
}

inline void getPortDataByAssociation(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& objPath, const std::string& chassisId,
    const std::string& networkAdapterId, const std::string& portId)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisId, networkAdapterId,
         portId](const boost::system::error_code& ec,
                 std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            // no state sensors attached.
            messages::internalError(asyncResp->res);
            return;
        }

        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        for (const std::string& sensorPath : *data)
        {
            // Check Interface in Object or not
            crow::connections::systemBus->async_method_call(
                [asyncResp, sensorPath, chassisId, networkAdapterId,
                 portId](const boost::system::error_code ec,
                         const std::vector<std::pair<
                             std::string, std::vector<std::string>>>& object) {
                if (ec)
                {
                    // the path does not implement item port
                    // interfaces
                    return;
                }

                sdbusplus::message::object_path path(sensorPath);
                if (path.filename() != portId || object.size() != 1)
                {
                    return;
                }

                getPortData(asyncResp, object.front().first, sensorPath,
                            chassisId, networkAdapterId, portId);
            },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject", sensorPath,
                std::array<std::string, 1>(
                    {"xyz.openbmc_project.Inventory.Item.Port"}));
        }
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/all_states",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void doPort(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& chassisId,
                   const std::string& networkAdapterId,
                   const std::string& portId,
                   const std::optional<std::string>& validNetworkAdapterPath)
{
    if (!validNetworkAdapterPath)
    {
        BMCWEB_LOG_ERROR("Not a valid networkAdapter ID{}", networkAdapterId);
        messages::resourceNotFound(asyncResp->res, "networkAdapter",
                                   networkAdapterId);
        return;
    }

    getPortDataByAssociation(asyncResp, *validNetworkAdapterPath, chassisId,
                             networkAdapterId, portId);
}

inline void doPortWithValidChassisId(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId, const std::string& networkAdapterId,
    const std::string& portId,
    const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        BMCWEB_LOG_ERROR("Not a valid chassis ID{}", chassisId);
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }

    getValidNetworkAdapterPath(asyncResp, networkAdapterId,
                               std::bind_front(doPort, asyncResp, chassisId,
                                               networkAdapterId, portId));
}

inline void handlePortGet(App& app, const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisId,
                          const std::string& networkAdapterId,
                          const std::string& portId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        std::bind_front(doPortWithValidChassisId, asyncResp, chassisId,
                        networkAdapterId, portId));
}

inline void requestRoutesNetworkAdapters(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/NetworkAdapters/")
        .privileges(redfish::privileges::getNetworkAdapterCollection)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleNetworkAdaptersCollectionGet, std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/NetworkAdapters/<str>")
        .privileges(redfish::privileges::getNetworkAdapter)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleNetworkAdapterGet, std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/NetworkAdapters/<str>/Ports")
        .privileges(redfish::privileges::getPortCollection)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handlePortsCollectionGet, std::ref(app)));
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Chassis/<str>/NetworkAdapters/<str>/Ports/<str>/")
        .privileges(redfish::privileges::getPort)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handlePortGet, std::ref(app)));
}

} // namespace redfish
