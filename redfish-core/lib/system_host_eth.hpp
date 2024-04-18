#pragma once

#include "bmcweb_config.h"

#include "dbus_singleton.hpp"
#include "ethernet.hpp"
#include "app.hpp"
#include "redfish_util.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "utils/chassis_utils.hpp"
#include "utils/json_utils.hpp"
#include "utils/collection.hpp"
#include "health.hpp"

namespace redfish
{
template <typename CallbackFunc>
void getEthernetIfaceListHost(CallbackFunc&& callback, const std::vector<const char*>& interfaces)
{
    crow::connections::systemBus->async_method_call(
        [callback{std::forward<CallbackFunc>(callback)}](
            const boost::system::error_code ec,
            const std::vector<std::string>& objects) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBUS response error {}", ec);
                callback(false, {}); // Invoke callback with failure
                return;
            }

            // Convert object paths to interface IDs or names
            boost::container::flat_set<std::string> ifaceList;
            for (const auto& objectPath : objects)
            {
                // Extract the interface ID or name from the object path
                std::string ifaceId = sdbusplus::message::object_path(objectPath).filename();
                if (!ifaceId.empty())
                {
                    ifaceList.emplace(std::move(ifaceId));
                }
            }

            // Invoke callback with success and the list of Ethernet interface IDs
            callback(true, std::move(ifaceList));
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/network/host0", 0, interfaces);
}

template <typename CallbackFunc>
void getEthernetIfaceService(const std::string& ethifaceId,
                          CallbackFunc&& callback,
                          const std::vector<const char*>& interfaces={"xyz.openbmc_project.Network.EthernetInterface"})
{
    crow::connections::systemBus->async_method_call(
        [ethifaceId{std::string{ethifaceId}},
         callback{std::forward<CallbackFunc>(callback)}](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBUS response error ", ec);
                callback(false, ""); // Invoke callback with failure
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
                const auto& connectionNames = object.second;
                
                if (path.find(ethifaceId) != std::string::npos) 
                {
                    std::string serviceName = connectionNames[0].first;
                    BMCWEB_LOG_ERROR("Service name: {}", serviceName);
                    callback(true, std::move(serviceName));
                    return;
                }
            }
            BMCWEB_LOG_ERROR("Service for ETH Iface {} not found", ethifaceId);
            callback(false, ""); // Invoke callback with failure
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/network/host0", 0, interfaces);
}

template <typename CallbackFunc>
void getEthernetIfaceDataHost(const std::string& ethifaceId,
                          CallbackFunc&& callback,
                          const std::vector<const char*>& interfaces={"xyz.openbmc_project.Network.EthernetInterface"})
{
    // First, call getEthernetIfaceService to get the serviceName
    getEthernetIfaceService(ethifaceId,
        [ethifaceId, callback, interfaces](bool success, const std::string& serviceName) {
            if (!success || serviceName.empty())
            {
                // Handle error
                EthernetInterfaceData ethData{};
                std::vector<IPv4AddressData> ipv4Data;
                std::vector<IPv6AddressData> ipv6Data;
                callback(false, ethData, ipv4Data, ipv6Data);
                return;
            }
    crow::connections::systemBus->async_method_call(
        [ethifaceId,
         callback](
            const boost::system::error_code errorCode,
            const dbus::utility::ManagedObjectType& resp) {
            EthernetInterfaceData ethData{};
            std::vector<IPv4AddressData> ipv4Data;
            std::vector<IPv6AddressData> ipv6Data;

            if (errorCode)
            {
                callback(false, ethData, ipv4Data, ipv6Data);
                return;
            }
            const std::string& ethifacePath = "host0/" + ethifaceId;
            bool found =
                extractEthernetInterfaceData(ethifacePath, resp, ethData);
            if (!found)
            {
                callback(false, ethData, ipv4Data, ipv6Data);
                return;
            }

            extractIPData(ethifacePath, resp, ipv4Data);
            // Fix global GW
            for (IPv4AddressData& ipv4 : ipv4Data)
            {
                if (((ipv4.linktype == LinkType::Global) &&
                     (ipv4.gateway == "0.0.0.0")) ||
                    (ipv4.origin == "DHCP") || (ipv4.origin == "Static"))
                {
                    ipv4.gateway = ethData.defaultGateway;
                }
            }

            extractIPV6Data(ethifacePath, resp, ipv6Data);
            // Finally make a callback with useful data
            callback(true, ethData, ipv4Data, ipv6Data);
        },
        serviceName,
        "/xyz/openbmc_project/network/host0",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        }, interfaces);
}

inline void requestHostEthernetInterfacesRoutes(App& app)
{
    BMCWEB_ROUTE(app,
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/EthernetInterfaces/")
            .privileges(redfish::privileges::getEthernetInterfaceCollection)
            .methods(boost::beast::http::verb::get)(
                [&app](const crow::Request& req,
                    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                    {
                        return;
                    }

                    asyncResp->res.jsonValue["@odata.type"] =
                        "#EthernetInterfaceCollection.EthernetInterfaceCollection";
                    asyncResp->res.jsonValue["@odata.id"] =
                        "/redfish/v1/Systems/" PLATFORMSYSTEMID "/EthernetInterfaces";
                    asyncResp->res.jsonValue["Name"] =
                        "Ethernet Network Interface Collection";
                    asyncResp->res.jsonValue["Description"] =
                        "Collection of EthernetInterfaces of the host";

                    // Get eth interface list, and call the below callback for JSON
                    // preparation
                    getEthernetIfaceListHost(
                        [asyncResp](const bool& success,
                                    const boost::container::flat_set<std::string>&
                                        ifaceList) {
                            if (!success)
                            {
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            nlohmann::json& ifaceArray =
                                asyncResp->res.jsonValue["Members"];
                            ifaceArray = nlohmann::json::array();
                            std::string tag = "_";
                            for (const std::string& ifaceItem : ifaceList)
                            {
                                std::size_t found = ifaceItem.find(tag);
                                if (found == std::string::npos)
                                {
                                    nlohmann::json::object_t iface;
                                    iface["@odata.id"] =
                                        "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                        "/EthernetInterfaces/" +
                                        ifaceItem;
                                    ifaceArray.push_back(std::move(iface));
                                }
                            }

                            asyncResp->res.jsonValue["Members@odata.count"] =
                                ifaceArray.size();
                            asyncResp->res.jsonValue["@odata.id"] =
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/EthernetInterfaces";
                        },
                        {"xyz.openbmc_project.Network.EthernetInterface"});
                });

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/EthernetInterfaces/<str>/")
        .privileges(redfish::privileges::getEthernetInterface)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& ifaceId) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                getEthernetIfaceDataHost(
                    ifaceId,
                    [asyncResp,
                     ifaceId](const bool& success,
                              const EthernetInterfaceData& ethData,
                              const std::vector<IPv4AddressData>&
                                  ipv4Data,
                              const std::vector<IPv6AddressData>&
                                  ipv6Data) {
                        if (!success)
                        {
                            // TODO(Pawel)consider distinguish between non
                            // existing object, and other errors
                            messages::resourceNotFound(
                                asyncResp->res, "EthernetInterface", ifaceId);
                            return;
                        }

                        // Keep using the v1.6.0 schema here as currently bmcweb
                        // have to use "VLANs" property deprecated in v1.7.0 for
                        // VLAN creation/deletion.
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#EthernetInterface.v1_6_0.EthernetInterface";
                        asyncResp->res.jsonValue["Name"] =
                            "Host Ethernet Interface";
                        asyncResp->res.jsonValue["Description"] =
                            "Host Network Interface for port " + ifaceId;

                        parseInterfaceData(asyncResp, ifaceId, ethData,
                                           ipv4Data, ipv6Data, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                            "/EthernetInterfaces/");
                    });
            });
}

} // namespace redfish
