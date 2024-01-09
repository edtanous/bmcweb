
#include "ethernet.hpp"

#include "bmcweb_config.h"

#include "app.hpp"
#include "dbus_singleton.hpp"
#include "dbus_utility.hpp"
#include "error_messages.hpp"
#include "health.hpp"
#include "human_sort.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "utils/ip_utils.hpp"
#include "utils/json_utils.hpp"

#include <boost/url/format.hpp>

#include <array>
#include <optional>
#include <ranges>
#include <regex>
#include <string_view>
#include <vector>

namespace redfish
{

struct DHCPParameters
{
    std::optional<bool> dhcpv4Enabled;
    std::optional<bool> useDnsServers;
    std::optional<bool> useNtpServers;
    std::optional<bool> useDomainName;
    std::optional<std::string> dhcpv6OperatingMode;
};

// Helper function that changes bits netmask notation (i.e. /24)
// into full dot notation
std::string getNetmask(unsigned int bits)
{
    uint32_t value = 0xffffffff << (32 - bits);
    std::string netmask = std::to_string((value >> 24) & 0xff) + "." +
                          std::to_string((value >> 16) & 0xff) + "." +
                          std::to_string((value >> 8) & 0xff) + "." +
                          std::to_string(value & 0xff);
    return netmask;
}

bool translateDhcpEnabledToBool(const std::string& inputDHCP, bool isIPv4)
{
    if (isIPv4)
    {
        return (
            (inputDHCP ==
             "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.v4") ||
            (inputDHCP ==
             "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.both"));
    }
    return ((inputDHCP ==
             "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.v6") ||
            (inputDHCP ==
             "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.both"));
}

std::string getDhcpEnabledEnumeration(bool isIPv4, bool isIPv6)
{
    if (isIPv4 && isIPv6)
    {
        return "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.both";
    }
    if (isIPv4)
    {
        return "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.v4";
    }
    if (isIPv6)
    {
        return "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.v6";
    }
    return "xyz.openbmc_project.Network.EthernetInterface.DHCPConf.none";
}

std::string translateAddressOriginDbusToRedfish(const std::string& inputOrigin,
                                                bool isIPv4)
{
    if (inputOrigin == "xyz.openbmc_project.Network.IP.AddressOrigin.Static")
    {
        return "Static";
    }
    if (inputOrigin == "xyz.openbmc_project.Network.IP.AddressOrigin.LinkLocal")
    {
        if (isIPv4)
        {
            return "IPv4LinkLocal";
        }
        return "LinkLocal";
    }
    if (inputOrigin == "xyz.openbmc_project.Network.IP.AddressOrigin.DHCP")
    {
        if (isIPv4)
        {
            return "DHCP";
        }
        return "DHCPv6";
    }
    if (inputOrigin == "xyz.openbmc_project.Network.IP.AddressOrigin.SLAAC")
    {
        return "SLAAC";
    }
    return "";
}

bool extractEthernetInterfaceData(
    const std::string& ethifaceId,
    const dbus::utility::ManagedObjectType& dbusData,
    EthernetInterfaceData& ethData)
{
    bool idFound = false;
    for (const auto& objpath : dbusData)
    {
        for (const auto& ifacePair : objpath.second)
        {
            if (objpath.first == "/xyz/openbmc_project/network/" + ethifaceId)
            {
                idFound = true;
                if (ifacePair.first == "xyz.openbmc_project.Network.MACAddress")
                {
                    for (const auto& propertyPair : ifacePair.second)
                    {
                        if (propertyPair.first == "MACAddress")
                        {
                            const std::string* mac =
                                std::get_if<std::string>(&propertyPair.second);
                            if (mac != nullptr)
                            {
                                ethData.macAddress = *mac;
                            }
                        }
                    }
                }
                else if (ifacePair.first == "xyz.openbmc_project.Network.VLAN")
                {
                    for (const auto& propertyPair : ifacePair.second)
                    {
                        if (propertyPair.first == "Id")
                        {
                            const uint32_t* id =
                                std::get_if<uint32_t>(&propertyPair.second);
                            if (id != nullptr)
                            {
                                ethData.vlanId = *id;
                            }
                        }
                    }
                }
                else if (ifacePair.first ==
                         "xyz.openbmc_project.Network.EthernetInterface")
                {
                    for (const auto& propertyPair : ifacePair.second)
                    {
                        if (propertyPair.first == "AutoNeg")
                        {
                            const bool* autoNeg =
                                std::get_if<bool>(&propertyPair.second);
                            if (autoNeg != nullptr)
                            {
                                ethData.autoNeg = *autoNeg;
                            }
                        }
                        else if (propertyPair.first == "Speed")
                        {
                            const uint32_t* speed =
                                std::get_if<uint32_t>(&propertyPair.second);
                            if (speed != nullptr)
                            {
                                ethData.speed = *speed;
                            }
                        }
                        else if (propertyPair.first == "MTU")
                        {
                            const size_t* mtuSize =
                                std::get_if<size_t>(&propertyPair.second);
                            if (mtuSize != nullptr)
                            {
                                ethData.mtuSize = *mtuSize;
                            }
                        }
                        else if (propertyPair.first == "LinkUp")
                        {
                            const bool* linkUp =
                                std::get_if<bool>(&propertyPair.second);
                            if (linkUp != nullptr)
                            {
                                ethData.linkUp = *linkUp;
                            }
                        }
                        else if (propertyPair.first == "NICEnabled")
                        {
                            const bool* nicEnabled =
                                std::get_if<bool>(&propertyPair.second);
                            if (nicEnabled != nullptr)
                            {
                                ethData.nicEnabled = *nicEnabled;
                            }
                        }
                        else if (propertyPair.first == "IPv6AcceptRA")
                        {
                            const bool* ipv6AcceptRa =
                                std::get_if<bool>(&propertyPair.second);
                            if (ipv6AcceptRa != nullptr)
                            {
                                ethData.ipv6AcceptRa = *ipv6AcceptRa;
                            }
                        }
                        else if (propertyPair.first == "Nameservers")
                        {
                            const std::vector<std::string>* nameservers =
                                std::get_if<std::vector<std::string>>(
                                    &propertyPair.second);
                            if (nameservers != nullptr)
                            {
                                ethData.nameServers = *nameservers;
                            }
                        }
                        else if (propertyPair.first == "StaticNameServers")
                        {
                            const std::vector<std::string>* staticNameServers =
                                std::get_if<std::vector<std::string>>(
                                    &propertyPair.second);
                            if (staticNameServers != nullptr)
                            {
                                ethData.staticNameServers = *staticNameServers;
                            }
                        }
                        else if (propertyPair.first == "DHCPEnabled")
                        {
                            const std::string* dhcpEnabled =
                                std::get_if<std::string>(&propertyPair.second);
                            if (dhcpEnabled != nullptr)
                            {
                                ethData.dhcpEnabled = *dhcpEnabled;
                            }
                        }
                        else if (propertyPair.first == "DomainName")
                        {
                            const std::vector<std::string>* domainNames =
                                std::get_if<std::vector<std::string>>(
                                    &propertyPair.second);
                            if (domainNames != nullptr)
                            {
                                ethData.domainnames = *domainNames;
                            }
                        }
                        else if (propertyPair.first == "DefaultGateway")
                        {
                            const std::string* defaultGateway =
                                std::get_if<std::string>(&propertyPair.second);
                            if (defaultGateway != nullptr)
                            {
                                std::string defaultGatewayStr = *defaultGateway;
                                if (defaultGatewayStr.empty())
                                {
                                    ethData.defaultGateway = "0.0.0.0";
                                }
                                else
                                {
                                    ethData.defaultGateway = defaultGatewayStr;
                                }
                            }
                        }
                        else if (propertyPair.first == "DefaultGateway6")
                        {
                            const std::string* defaultGateway6 =
                                std::get_if<std::string>(&propertyPair.second);
                            if (defaultGateway6 != nullptr)
                            {
                                std::string defaultGateway6Str =
                                    *defaultGateway6;
                                if (defaultGateway6Str.empty())
                                {
                                    ethData.ipv6DefaultGateway =
                                        "0:0:0:0:0:0:0:0";
                                }
                                else
                                {
                                    ethData.ipv6DefaultGateway =
                                        defaultGateway6Str;
                                }
                            }
                        }
                    }
                }
            }

            if (objpath.first == "/xyz/openbmc_project/network/dhcp")
            {
                if (ifacePair.first ==
                    "xyz.openbmc_project.Network.DHCPConfiguration")
                {
                    for (const auto& propertyPair : ifacePair.second)
                    {
                        if (propertyPair.first == "DNSEnabled")
                        {
                            const bool* dnsEnabled =
                                std::get_if<bool>(&propertyPair.second);
                            if (dnsEnabled != nullptr)
                            {
                                ethData.dnsEnabled = *dnsEnabled;
                            }
                        }
                        else if (propertyPair.first == "NTPEnabled")
                        {
                            const bool* ntpEnabled =
                                std::get_if<bool>(&propertyPair.second);
                            if (ntpEnabled != nullptr)
                            {
                                ethData.ntpEnabled = *ntpEnabled;
                            }
                        }
                        else if (propertyPair.first == "HostNameEnabled")
                        {
                            const bool* hostNameEnabled =
                                std::get_if<bool>(&propertyPair.second);
                            if (hostNameEnabled != nullptr)
                            {
                                ethData.hostNameEnabled = *hostNameEnabled;
                            }
                        }
                    }
                }
            }
            // System configuration shows up in the global namespace, so no need
            // to check eth number
            if (ifacePair.first ==
                "xyz.openbmc_project.Network.SystemConfiguration")
            {
                for (const auto& propertyPair : ifacePair.second)
                {
                    if (propertyPair.first == "HostName")
                    {
                        const std::string* hostname =
                            std::get_if<std::string>(&propertyPair.second);
                        if (hostname != nullptr)
                        {
                            ethData.hostName = *hostname;
                        }
                    }
                }
            }
        }
    }
    return idFound;
}

// Helper function that extracts data for single ethernet ipv6 address
void extractIPV6Data(const std::string& ethifaceId,
                     const dbus::utility::ManagedObjectType& dbusData,
                     std::vector<IPv6AddressData>& ipv6Config)
{
    const std::string ipPathStart = "/xyz/openbmc_project/network/" +
                                    ethifaceId;

    // Since there might be several IPv6 configurations aligned with
    // single ethernet interface, loop over all of them
    for (const auto& objpath : dbusData)
    {
        // Check if proper pattern for object path appears
        if (objpath.first.str.starts_with(ipPathStart + "/"))
        {
            for (const auto& interface : objpath.second)
            {
                if (interface.first == "xyz.openbmc_project.Network.IP")
                {
                    auto type = std::ranges::find_if(interface.second,
                                                     [](const auto& property) {
                        return property.first == "Type";
                    });
                    if (type == interface.second.end())
                    {
                        continue;
                    }

                    const std::string* typeStr =
                        std::get_if<std::string>(&type->second);

                    if (typeStr == nullptr ||
                        (*typeStr !=
                         "xyz.openbmc_project.Network.IP.Protocol.IPv6"))
                    {
                        continue;
                    }

                    // Instance IPv6AddressData structure, and set as
                    // appropriate
                    IPv6AddressData& ipv6Address = ipv6Config.emplace_back();
                    ipv6Address.id =
                        objpath.first.str.substr(ipPathStart.size());
                    for (const auto& property : interface.second)
                    {
                        if (property.first == "Address")
                        {
                            const std::string* address =
                                std::get_if<std::string>(&property.second);
                            if (address != nullptr)
                            {
                                ipv6Address.address = *address;
                            }
                        }
                        else if (property.first == "Origin")
                        {
                            const std::string* origin =
                                std::get_if<std::string>(&property.second);
                            if (origin != nullptr)
                            {
                                ipv6Address.origin =
                                    translateAddressOriginDbusToRedfish(*origin,
                                                                        false);
                            }
                        }
                        else if (property.first == "PrefixLength")
                        {
                            const uint8_t* prefix =
                                std::get_if<uint8_t>(&property.second);
                            if (prefix != nullptr)
                            {
                                ipv6Address.prefixLength = *prefix;
                            }
                        }
                        else if (property.first == "Type" ||
                                 property.first == "Gateway")
                        {
                            // Type & Gateway is not used
                        }
                        else
                        {
                            BMCWEB_LOG_ERROR(
                                "Got extra property: {} on the {} object",
                                property.first, objpath.first.str);
                        }
                    }
                }
            }
        }
    }
}

// Helper function that extracts data for single ethernet ipv4 address
void extractIPData(const std::string& ethifaceId,
                   const dbus::utility::ManagedObjectType& dbusData,
                   std::vector<IPv4AddressData>& ipv4Config)
{
    const std::string ipPathStart = "/xyz/openbmc_project/network/" +
                                    ethifaceId;

    // Since there might be several IPv4 configurations aligned with
    // single ethernet interface, loop over all of them
    for (const auto& objpath : dbusData)
    {
        // Check if proper pattern for object path appears
        if (objpath.first.str.starts_with(ipPathStart + "/"))
        {
            for (const auto& interface : objpath.second)
            {
                if (interface.first == "xyz.openbmc_project.Network.IP")
                {
                    auto type = std::ranges::find_if(interface.second,
                                                     [](const auto& property) {
                        return property.first == "Type";
                    });
                    if (type == interface.second.end())
                    {
                        continue;
                    }

                    const std::string* typeStr =
                        std::get_if<std::string>(&type->second);

                    if (typeStr == nullptr ||
                        (*typeStr !=
                         "xyz.openbmc_project.Network.IP.Protocol.IPv4"))
                    {
                        continue;
                    }

                    // Instance IPv4AddressData structure, and set as
                    // appropriate
                    IPv4AddressData& ipv4Address = ipv4Config.emplace_back();
                    ipv4Address.id =
                        objpath.first.str.substr(ipPathStart.size());
                    for (const auto& property : interface.second)
                    {
                        if (property.first == "Address")
                        {
                            const std::string* address =
                                std::get_if<std::string>(&property.second);
                            if (address != nullptr)
                            {
                                ipv4Address.address = *address;
                            }
                        }
                        else if (property.first == "Origin")
                        {
                            const std::string* origin =
                                std::get_if<std::string>(&property.second);
                            if (origin != nullptr)
                            {
                                ipv4Address.origin =
                                    translateAddressOriginDbusToRedfish(*origin,
                                                                        true);
                            }
                        }
                        else if (property.first == "PrefixLength")
                        {
                            const uint8_t* mask =
                                std::get_if<uint8_t>(&property.second);
                            if (mask != nullptr)
                            {
                                // convert it to the string
                                ipv4Address.netmask = getNetmask(*mask);
                            }
                        }
                        else if (property.first == "Type" ||
                                 property.first == "Gateway")
                        {
                            // Type & Gateway is not used
                        }
                        else
                        {
                            BMCWEB_LOG_ERROR(
                                "Got extra property: {} on the {} object",
                                property.first, objpath.first.str);
                        }
                    }
                    // Check if given address is local, or global
                    ipv4Address.linktype =
                        ipv4Address.address.starts_with("169.254.")
                            ? LinkType::Local
                            : LinkType::Global;
                }
            }
        }
    }
}

/**
 * @brief Deletes given IPv4 interface
 *
 * @param[in] ifaceId     Id of interface whose IP should be deleted
 * @param[in] ipHash      DBus Hash id of IP that should be deleted
 * @param[io] asyncResp   Response object that will be returned to client
 *
 * @return None
 */
void deleteIPAddress(const std::string& ifaceId, const std::string& ipHash,
                     const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
        }
    },
        "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId + ipHash,
        "xyz.openbmc_project.Object.Delete", "Delete");
}

void updateIPv4DefaultGateway(
    const std::string& ifaceId, const std::string& gateway,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.EthernetInterface", "DefaultGateway",
        gateway, [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        asyncResp->res.result(boost::beast::http::status::no_content);
    });
}
/**
 * @brief Creates a static IPv4 entry
 *
 * @param[in] ifaceId      Id of interface upon which to create the IPv4 entry
 * @param[in] prefixLength IPv4 prefix syntax for the subnet mask
 * @param[in] gateway      IPv4 address of this interfaces gateway
 * @param[in] address      IPv4 address to assign to this interface
 * @param[io] asyncResp    Response object that will be returned to client
 *
 * @return None
 */
void createIPv4(const std::string& ifaceId, uint8_t prefixLength,
                const std::string& gateway, const std::string& address,
                const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    auto createIpHandler = [asyncResp, ifaceId,
                            gateway](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        updateIPv4DefaultGateway(ifaceId, gateway, asyncResp);
    };

    crow::connections::systemBus->async_method_call(
        std::move(createIpHandler), "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.IP.Create", "IP",
        "xyz.openbmc_project.Network.IP.Protocol.IPv4", address, prefixLength,
        gateway);
}

/**
 * @brief Deletes the IPv6 entry for this interface and creates a replacement
 * static IPv6 entry
 *
 * @param[in] ifaceId      Id of interface upon which to create the IPv6 entry
 * @param[in] id           The unique hash entry identifying the DBus entry
 * @param[in] prefixLength IPv6 prefix syntax for the subnet mask
 * @param[in] address      IPv6 address to assign to this interface
 * @param[io] asyncResp    Response object that will be returned to client
 *
 * @return None
 */

enum class IpVersion
{
    IpV4,
    IpV6
};

void deleteAndCreateIPAddress(
    IpVersion version, const std::string& ifaceId, const std::string& id,
    uint8_t prefixLength, const std::string& address,
    const std::string& gateway,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, version, ifaceId, address, prefixLength,
         gateway](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
        }
        std::string protocol = "xyz.openbmc_project.Network.IP.Protocol.";
        protocol += version == IpVersion::IpV4 ? "IPv4" : "IPv6";
        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code& ec2) {
            if (ec2)
            {
                messages::internalError(asyncResp->res);
            }
        },
            "xyz.openbmc_project.Network",
            "/xyz/openbmc_project/network/" + ifaceId,
            "xyz.openbmc_project.Network.IP.Create", "IP", protocol, address,
            prefixLength, gateway);
    },
        "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId + id,
        "xyz.openbmc_project.Object.Delete", "Delete");
}

/**
 * @brief Creates IPv6 with given data
 *
 * @param[in] ifaceId      Id of interface whose IP should be added
 * @param[in] prefixLength Prefix length that needs to be added
 * @param[in] address      IP address that needs to be added
 * @param[io] asyncResp    Response object that will be returned to client
 *
 * @return None
 */
void createIPv6(const std::string& ifaceId, uint8_t prefixLength,
                const std::string& address,
                const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    auto createIpHandler = [asyncResp,
                            address](const boost::system::error_code& ec) {
        if (ec)
        {
            if (ec == boost::system::errc::io_error)
            {
                messages::propertyValueFormatError(asyncResp->res, address,
                                                   "Address");
            }
            else
            {
                messages::internalError(asyncResp->res);
            }
        }
    };
    // Passing null for gateway, as per redfish spec IPv6StaticAddresses object
    // does not have associated gateway property
    crow::connections::systemBus->async_method_call(
        std::move(createIpHandler), "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.IP.Create", "IP",
        "xyz.openbmc_project.Network.IP.Protocol.IPv6", address, prefixLength,
        "");
}

/**
 * Function that retrieves all properties for given Ethernet Interface
 * Object
 * from EntityManager Network Manager
 * @param ethiface_id a eth interface id to query on DBus
 * @param callback a function that shall be called to convert Dbus output
 * into JSON
 */
template <typename CallbackFunc>
void getEthernetIfaceData(const std::string& ethifaceId,
                          CallbackFunc&& callback)
{
    sdbusplus::message::object_path path("/xyz/openbmc_project/network");
    dbus::utility::getManagedObjects(
        "xyz.openbmc_project.Network", path,
        [ethifaceId{std::string{ethifaceId}},
         callback{std::forward<CallbackFunc>(callback)}](
            const boost::system::error_code& ec,
            const dbus::utility::ManagedObjectType& resp) {
        EthernetInterfaceData ethData{};
        std::vector<IPv4AddressData> ipv4Data;
        std::vector<IPv6AddressData> ipv6Data;

        if (ec)
        {
            callback(false, ethData, ipv4Data, ipv6Data);
            return;
        }

        bool found = extractEthernetInterfaceData(ethifaceId, resp, ethData);
        if (!found)
        {
            callback(false, ethData, ipv4Data, ipv6Data);
            return;
        }

        extractIPData(ethifaceId, resp, ipv4Data);
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

        extractIPV6Data(ethifaceId, resp, ipv6Data);
        // Finally make a callback with useful data
        callback(true, ethData, ipv4Data, ipv6Data);
    });
}

/**
 * Function that retrieves all Ethernet Interfaces available through Network
 * Manager
 * @param callback a function that shall be called to convert Dbus output
 * into JSON.
 */
template <typename CallbackFunc>
void getEthernetIfaceList(CallbackFunc&& callback)
{
    sdbusplus::message::object_path path("/xyz/openbmc_project/network");
    dbus::utility::getManagedObjects(
        "xyz.openbmc_project.Network", path,
        [callback{std::forward<CallbackFunc>(callback)}](
            const boost::system::error_code& ec,
            const dbus::utility::ManagedObjectType& resp) {
        // Callback requires vector<string> to retrieve all available
        // ethernet interfaces
        std::vector<std::string> ifaceList;
        ifaceList.reserve(resp.size());
        if (ec)
        {
            callback(false, ifaceList);
            return;
        }

        // Iterate over all retrieved ObjectPaths.
        for (const auto& objpath : resp)
        {
            // And all interfaces available for certain ObjectPath.
            for (const auto& interface : objpath.second)
            {
                // If interface is
                // xyz.openbmc_project.Network.EthernetInterface, this is
                // what we're looking for.
                if (interface.first ==
                    "xyz.openbmc_project.Network.EthernetInterface")
                {
                    std::string ifaceId = objpath.first.filename();
                    if (ifaceId.empty())
                    {
                        continue;
                    }
                    // and put it into output vector.
                    ifaceList.emplace_back(ifaceId);
                }
            }
        }

        std::ranges::sort(ifaceList, AlphanumLess<std::string>());

        // Finally make a callback with useful data
        callback(true, ifaceList);
    });
}

void handleHostnamePatch(const std::string& hostname,
                         const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    // SHOULD handle host names of up to 255 characters(RFC 1123)
    if (hostname.length() > 255)
    {
        messages::propertyValueFormatError(asyncResp->res, hostname,
                                           "HostName");
        return;
    }
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/config",
        "xyz.openbmc_project.Network.SystemConfiguration", "HostName", hostname,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
        }
    });
}

void handleMTUSizePatch(const std::string& ifaceId, const size_t mtuSize,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    sdbusplus::message::object_path objPath = "/xyz/openbmc_project/network/" +
                                              ifaceId;
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network", objPath,
        "xyz.openbmc_project.Network.EthernetInterface", "MTU", mtuSize,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
        }
    });
}

void handleDomainnamePatch(const std::string& ifaceId,
                           const std::string& domainname,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    std::vector<std::string> vectorDomainname = {domainname};
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.EthernetInterface", "DomainName",
        vectorDomainname, [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
        }
    });
}

bool isHostnameValid(const std::string& hostname)
{
    // A valid host name can never have the dotted-decimal form (RFC 1123)
    if (std::ranges::all_of(hostname, ::isdigit))
    {
        return false;
    }
    // Each label(hostname/subdomains) within a valid FQDN
    // MUST handle host names of up to 63 characters (RFC 1123)
    // labels cannot start or end with hyphens (RFC 952)
    // labels can start with numbers (RFC 1123)
    const static std::regex pattern(
        "^[a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9]$");

    return std::regex_match(hostname, pattern);
}

bool isDomainnameValid(const std::string& domainname)
{
    // Can have multiple subdomains
    // Top Level Domain's min length is 2 character
    const static std::regex pattern(
        "^([A-Za-z0-9][a-zA-Z0-9\\-]{1,61}|[a-zA-Z0-9]{1,30}\\.)*[a-zA-Z]{2,}$");

    return std::regex_match(domainname, pattern);
}

void handleFqdnPatch(const std::string& ifaceId, const std::string& fqdn,
                     const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    // Total length of FQDN must not exceed 255 characters(RFC 1035)
    if (fqdn.length() > 255)
    {
        messages::propertyValueFormatError(asyncResp->res, fqdn, "FQDN");
        return;
    }

    size_t pos = fqdn.find('.');
    if (pos == std::string::npos)
    {
        messages::propertyValueFormatError(asyncResp->res, fqdn, "FQDN");
        return;
    }

    std::string hostname;
    std::string domainname;
    domainname = (fqdn).substr(pos + 1);
    hostname = (fqdn).substr(0, pos);

    if (!isHostnameValid(hostname) || !isDomainnameValid(domainname))
    {
        messages::propertyValueFormatError(asyncResp->res, fqdn, "FQDN");
        return;
    }

    handleHostnamePatch(hostname, asyncResp);
    handleDomainnamePatch(ifaceId, domainname, asyncResp);
}

void handleMACAddressPatch(const std::string& ifaceId,
                           const std::string& macAddress,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    static constexpr std::string_view dbusNotAllowedError =
        "xyz.openbmc_project.Common.Error.NotAllowed";

    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.MACAddress", "MACAddress", macAddress,
        [asyncResp](const boost::system::error_code& ec,
                    const sdbusplus::message_t& msg) {
        if (ec)
        {
            const sd_bus_error* err = msg.get_error();
            if (err == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            if (err->name == dbusNotAllowedError)
            {
                messages::propertyNotWritable(asyncResp->res, "MACAddress");
                return;
            }
            messages::internalError(asyncResp->res);
            return;
        }
    });
}

void setDHCPEnabled(const std::string& ifaceId, const std::string& propertyName,
                    const bool v4Value, const bool v6Value,
                    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const std::string dhcp = getDhcpEnabledEnumeration(v4Value, v6Value);
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.EthernetInterface", propertyName, dhcp,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus responses error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
        messages::success(asyncResp->res);
    });
}

void setEthernetInterfaceBoolProperty(
    const std::string& ifaceId, const std::string& propertyName,
    const bool& value, const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.EthernetInterface", propertyName, value,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus responses error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
    });
}

void setDHCPv4Config(const std::string& propertyName, const bool& value,
                     const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    BMCWEB_LOG_DEBUG("{} = {}", propertyName, value);
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/dhcp",
        "xyz.openbmc_project.Network.DHCPConfiguration", propertyName, value,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus responses error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
    });
}

void handleSLAACAutoConfigPatch(
    const std::string& ifaceId, bool ipv6AutoConfigEnabled,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    sdbusplus::message::object_path path("/xyz/openbmc_project/network");
    path /= ifaceId;
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network", path,
        "xyz.openbmc_project.Network.EthernetInterface", "IPv6AcceptRA",
        ipv6AutoConfigEnabled,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus responses error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
        messages::success(asyncResp->res);
    });
}

void handleDHCPPatch(const std::string& ifaceId,
                     const EthernetInterfaceData& ethData,
                     const DHCPParameters& v4dhcpParms,
                     const DHCPParameters& v6dhcpParms,
                     const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    bool ipv4Active = translateDhcpEnabledToBool(ethData.dhcpEnabled, true);
    bool ipv6Active = translateDhcpEnabledToBool(ethData.dhcpEnabled, false);

    bool nextv4DHCPState =
        v4dhcpParms.dhcpv4Enabled ? *v4dhcpParms.dhcpv4Enabled : ipv4Active;

    bool nextv6DHCPState{};
    if (v6dhcpParms.dhcpv6OperatingMode)
    {
        if ((*v6dhcpParms.dhcpv6OperatingMode != "Enabled") &&
            (*v6dhcpParms.dhcpv6OperatingMode != "Disabled"))
        {
            messages::propertyValueFormatError(asyncResp->res,
                                               *v6dhcpParms.dhcpv6OperatingMode,
                                               "OperatingMode");
            return;
        }
        nextv6DHCPState = (*v6dhcpParms.dhcpv6OperatingMode == "Enabled");
    }
    else
    {
        nextv6DHCPState = ipv6Active;
    }

    bool nextDNS{};
    if (v4dhcpParms.useDnsServers && v6dhcpParms.useDnsServers)
    {
        if (*v4dhcpParms.useDnsServers != *v6dhcpParms.useDnsServers)
        {
            messages::generalError(asyncResp->res);
            return;
        }
        nextDNS = *v4dhcpParms.useDnsServers;
    }
    else if (v4dhcpParms.useDnsServers)
    {
        nextDNS = *v4dhcpParms.useDnsServers;
    }
    else if (v6dhcpParms.useDnsServers)
    {
        nextDNS = *v6dhcpParms.useDnsServers;
    }
    else
    {
        nextDNS = ethData.dnsEnabled;
    }

    bool nextNTP{};
    if (v4dhcpParms.useNtpServers && v6dhcpParms.useNtpServers)
    {
        if (*v4dhcpParms.useNtpServers != *v6dhcpParms.useNtpServers)
        {
            messages::generalError(asyncResp->res);
            return;
        }
        nextNTP = *v4dhcpParms.useNtpServers;
    }
    else if (v4dhcpParms.useNtpServers)
    {
        nextNTP = *v4dhcpParms.useNtpServers;
    }
    else if (v6dhcpParms.useNtpServers)
    {
        nextNTP = *v6dhcpParms.useNtpServers;
    }
    else
    {
        nextNTP = ethData.ntpEnabled;
    }

    bool nextUseDomain{};
    if (v4dhcpParms.useDomainName && v6dhcpParms.useDomainName)
    {
        if (*v4dhcpParms.useDomainName != *v6dhcpParms.useDomainName)
        {
            messages::generalError(asyncResp->res);
            return;
        }
        nextUseDomain = *v4dhcpParms.useDomainName;
    }
    else if (v4dhcpParms.useDomainName)
    {
        nextUseDomain = *v4dhcpParms.useDomainName;
    }
    else if (v6dhcpParms.useDomainName)
    {
        nextUseDomain = *v6dhcpParms.useDomainName;
    }
    else
    {
        nextUseDomain = ethData.hostNameEnabled;
    }

    BMCWEB_LOG_DEBUG("set DHCPEnabled...");
    setDHCPEnabled(ifaceId, "DHCPEnabled", nextv4DHCPState, nextv6DHCPState,
                   asyncResp);
    BMCWEB_LOG_DEBUG("set DNSEnabled...");
    setDHCPv4Config("DNSEnabled", nextDNS, asyncResp);
    BMCWEB_LOG_DEBUG("set NTPEnabled...");
    setDHCPv4Config("NTPEnabled", nextNTP, asyncResp);
    BMCWEB_LOG_DEBUG("set HostNameEnabled...");
    setDHCPv4Config("HostNameEnabled", nextUseDomain, asyncResp);
}

std::vector<IPv4AddressData>::const_iterator getNextStaticIpEntry(
    const std::vector<IPv4AddressData>::const_iterator& head,
    const std::vector<IPv4AddressData>::const_iterator& end)
{
    return std::find_if(head, end, [](const IPv4AddressData& value) {
        return value.origin == "Static";
    });
}

std::vector<IPv6AddressData>::const_iterator getNextStaticIpEntry(
    const std::vector<IPv6AddressData>::const_iterator& head,
    const std::vector<IPv6AddressData>::const_iterator& end)
{
    return std::find_if(head, end, [](const IPv6AddressData& value) {
        return value.origin == "Static";
    });
}

void handleIPv4StaticPatch(const std::string& ifaceId,
                           nlohmann::json::array_t& input,
                           const std::vector<IPv4AddressData>& ipv4Data,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (input.empty())
    {
        messages::propertyValueTypeError(asyncResp->res, input,
                                         "IPv4StaticAddresses");
        return;
    }

    unsigned entryIdx = 1;
    // Find the first static IP address currently active on the NIC and
    // match it to the first JSON element in the IPv4StaticAddresses array.
    // Match each subsequent JSON element to the next static IP programmed
    // into the NIC.
    std::vector<IPv4AddressData>::const_iterator nicIpEntry =
        getNextStaticIpEntry(ipv4Data.cbegin(), ipv4Data.cend());

    for (nlohmann::json& thisJson : input)
    {
        std::string pathString = "IPv4StaticAddresses/" +
                                 std::to_string(entryIdx);

        if (!thisJson.is_null() && !thisJson.empty())
        {
            std::optional<std::string> address;
            std::optional<std::string> subnetMask;
            std::optional<std::string> gateway;

            if (!json_util::readJson(thisJson, asyncResp->res, "Address",
                                     address, "SubnetMask", subnetMask,
                                     "Gateway", gateway))
            {
                messages::propertyValueFormatError(asyncResp->res, thisJson,
                                                   pathString);
                return;
            }

            // Find the address/subnet/gateway values. Any values that are
            // not explicitly provided are assumed to be unmodified from the
            // current state of the interface. Merge existing state into the
            // current request.
            if (address)
            {
                if (!ip_util::ipv4VerifyIpAndGetBitcount(*address))
                {
                    messages::propertyValueFormatError(asyncResp->res, *address,
                                                       pathString + "/Address");
                    return;
                }
            }
            else if (nicIpEntry != ipv4Data.cend())
            {
                address = (nicIpEntry->address);
            }
            else
            {
                messages::propertyMissing(asyncResp->res,
                                          pathString + "/Address");
                return;
            }

            uint8_t prefixLength = 0;
            if (subnetMask)
            {
                if (!ip_util::ipv4VerifyIpAndGetBitcount(*subnetMask,
                                                         &prefixLength))
                {
                    messages::propertyValueFormatError(
                        asyncResp->res, *subnetMask,
                        pathString + "/SubnetMask");
                    return;
                }
            }
            else if (nicIpEntry != ipv4Data.cend())
            {
                if (!ip_util::ipv4VerifyIpAndGetBitcount(nicIpEntry->netmask,
                                                         &prefixLength))
                {
                    messages::propertyValueFormatError(
                        asyncResp->res, nicIpEntry->netmask,
                        pathString + "/SubnetMask");
                    return;
                }
            }
            else
            {
                messages::propertyMissing(asyncResp->res,
                                          pathString + "/SubnetMask");
                return;
            }

            if (gateway)
            {
                if (!ip_util::ipv4VerifyIpAndGetBitcount(*gateway))
                {
                    messages::propertyValueFormatError(asyncResp->res, *gateway,
                                                       pathString + "/Gateway");
                    return;
                }
            }
            else if (nicIpEntry != ipv4Data.cend())
            {
                gateway = nicIpEntry->gateway;
            }
            else
            {
                messages::propertyMissing(asyncResp->res,
                                          pathString + "/Gateway");
                return;
            }

            if (nicIpEntry != ipv4Data.cend())
            {
                deleteAndCreateIPAddress(IpVersion::IpV4, ifaceId,
                                         nicIpEntry->id, prefixLength, *address,
                                         *gateway, asyncResp);
                nicIpEntry = getNextStaticIpEntry(++nicIpEntry,
                                                  ipv4Data.cend());
            }
            else
            {
                createIPv4(ifaceId, prefixLength, *gateway, *address,
                           asyncResp);
            }
            entryIdx++;
        }
        else
        {
            if (nicIpEntry == ipv4Data.cend())
            {
                // Requesting a DELETE/DO NOT MODIFY action for an item
                // that isn't present on the eth(n) interface. Input JSON is
                // in error, so bail out.
                if (thisJson.is_null())
                {
                    messages::resourceCannotBeDeleted(asyncResp->res);
                    return;
                }
                messages::propertyValueFormatError(asyncResp->res, thisJson,
                                                   pathString);
                return;
            }

            if (thisJson.is_null())
            {
                deleteIPAddress(ifaceId, nicIpEntry->id, asyncResp);
            }
            if (nicIpEntry != ipv4Data.cend())
            {
                nicIpEntry = getNextStaticIpEntry(++nicIpEntry,
                                                  ipv4Data.cend());
            }
            entryIdx++;
        }
    }
}

void handleStaticNameServersPatch(
    const std::string& ifaceId,
    const std::vector<std::string>& updatedStaticNameServers,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, "xyz.openbmc_project.Network",
        "/xyz/openbmc_project/network/" + ifaceId,
        "xyz.openbmc_project.Network.EthernetInterface", "StaticNameServers",
        updatedStaticNameServers,
        [asyncResp](const boost::system::error_code& ec) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }
    });
}

void handleIPv6StaticAddressesPatch(
    const std::string& ifaceId, const nlohmann::json::array_t& input,
    const std::vector<IPv6AddressData>& ipv6Data,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (input.empty())
    {
        messages::propertyValueTypeError(asyncResp->res, input,
                                         "IPv6StaticAddresses");
        return;
    }
    size_t entryIdx = 1;
    std::vector<IPv6AddressData>::const_iterator nicIpEntry =
        getNextStaticIpEntry(ipv6Data.cbegin(), ipv6Data.cend());
    for (const nlohmann::json& thisJson : input)
    {
        std::string pathString = "IPv6StaticAddresses/" +
                                 std::to_string(entryIdx);

        if (!thisJson.is_null() && !thisJson.empty())
        {
            std::optional<std::string> address;
            std::optional<uint8_t> prefixLength;
            nlohmann::json thisJsonCopy = thisJson;
            if (!json_util::readJson(thisJsonCopy, asyncResp->res, "Address",
                                     address, "PrefixLength", prefixLength))
            {
                messages::propertyValueFormatError(asyncResp->res, thisJson,
                                                   pathString);
                return;
            }

            const std::string* addr = nullptr;
            uint8_t prefix = 0;

            // Find the address and prefixLength values. Any values that are
            // not explicitly provided are assumed to be unmodified from the
            // current state of the interface. Merge existing state into the
            // current request.
            if (address)
            {
                addr = &(*address);
            }
            else if (nicIpEntry != ipv6Data.end())
            {
                addr = &(nicIpEntry->address);
            }
            else
            {
                messages::propertyMissing(asyncResp->res,
                                          pathString + "/Address");
                return;
            }

            if (prefixLength)
            {
                prefix = *prefixLength;
            }
            else if (nicIpEntry != ipv6Data.end())
            {
                prefix = nicIpEntry->prefixLength;
            }
            else
            {
                messages::propertyMissing(asyncResp->res,
                                          pathString + "/PrefixLength");
                return;
            }

            if (nicIpEntry != ipv6Data.end())
            {
                deleteAndCreateIPAddress(IpVersion::IpV6, ifaceId,
                                         nicIpEntry->id, prefix, *addr, "",
                                         asyncResp);
                nicIpEntry = getNextStaticIpEntry(++nicIpEntry,
                                                  ipv6Data.cend());
            }
            else
            {
                createIPv6(ifaceId, *prefixLength, *addr, asyncResp);
            }
            entryIdx++;
        }
        else
        {
            if (nicIpEntry == ipv6Data.end())
            {
                // Requesting a DELETE/DO NOT MODIFY action for an item
                // that isn't present on the eth(n) interface. Input JSON is
                // in error, so bail out.
                if (thisJson.is_null())
                {
                    messages::resourceCannotBeDeleted(asyncResp->res);
                    return;
                }
                messages::propertyValueFormatError(asyncResp->res, thisJson,
                                                   pathString);
                return;
            }

            if (thisJson.is_null())
            {
                deleteIPAddress(ifaceId, nicIpEntry->id, asyncResp);
            }
            if (nicIpEntry != ipv6Data.cend())
            {
                nicIpEntry = getNextStaticIpEntry(++nicIpEntry,
                                                  ipv6Data.cend());
            }
            entryIdx++;
        }
    }
}

std::string extractParentInterfaceName(const std::string& ifaceId)
{
    std::size_t pos = ifaceId.find('_');
    return ifaceId.substr(0, pos);
}

void parseInterfaceData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& ifaceId,
                        const EthernetInterfaceData& ethData,
                        const std::vector<IPv4AddressData>& ipv4Data,
                        const std::vector<IPv6AddressData>& ipv6Data)
{
    nlohmann::json& jsonResponse = asyncResp->res.jsonValue;
    jsonResponse["Id"] = ifaceId;
    jsonResponse["@odata.id"] = boost::urls::format(
        "/redfish/v1/Managers/bmc/EthernetInterfaces/{}", ifaceId);
    jsonResponse["InterfaceEnabled"] = ethData.nicEnabled;

    if constexpr (bmcwebEnableHealthPopulate)
    {
        constexpr std::array<std::string_view, 1> inventoryForEthernet = {
            "xyz.openbmc_project.Inventory.Item.Ethernet"};
        auto health = std::make_shared<HealthPopulate>(asyncResp);
        dbus::utility::getSubTreePaths(
            "/", 0, inventoryForEthernet,
            [health](const boost::system::error_code& ec,
                     const dbus::utility::MapperGetSubTreePathsResponse& resp) {
            if (ec)
            {
                return;
            }

            health->inventory = resp;
        });

        health->populate();
    }

    if (ethData.nicEnabled)
    {
        jsonResponse["LinkStatus"] = ethData.linkUp ? "LinkUp" : "LinkDown";
        jsonResponse["Status"]["State"] = "Enabled";
    }
    else
    {
        jsonResponse["LinkStatus"] = "NoLink";
        jsonResponse["Status"]["State"] = "Disabled";
    }

    jsonResponse["SpeedMbps"] = ethData.speed;
    jsonResponse["MTUSize"] = ethData.mtuSize;
    jsonResponse["MACAddress"] = ethData.macAddress;
    jsonResponse["DHCPv4"]["DHCPEnabled"] =
        translateDhcpEnabledToBool(ethData.dhcpEnabled, true);
    jsonResponse["DHCPv4"]["UseNTPServers"] = ethData.ntpEnabled;
    jsonResponse["DHCPv4"]["UseDNSServers"] = ethData.dnsEnabled;
    jsonResponse["DHCPv4"]["UseDomainName"] = ethData.hostNameEnabled;

    jsonResponse["DHCPv6"]["OperatingMode"] =
        translateDhcpEnabledToBool(ethData.dhcpEnabled, false) ? "Enabled"
                                                               : "Disabled";
    jsonResponse["DHCPv6"]["UseNTPServers"] = ethData.ntpEnabled;
    jsonResponse["DHCPv6"]["UseDNSServers"] = ethData.dnsEnabled;
    jsonResponse["DHCPv6"]["UseDomainName"] = ethData.hostNameEnabled;
    jsonResponse["StatelessAddressAutoConfig"]["IPv6AutoConfigEnabled"] =
        ethData.ipv6AcceptRa;

    if (!ethData.hostName.empty())
    {
        jsonResponse["HostName"] = ethData.hostName;

        // When domain name is empty then it means, that it is a network
        // without domain names, and the host name itself must be treated as
        // FQDN
        std::string fqdn = ethData.hostName;
        if (!ethData.domainnames.empty())
        {
            fqdn += "." + ethData.domainnames[0];
        }
        jsonResponse["FQDN"] = fqdn;
    }

    if (ethData.vlanId)
    {
        jsonResponse["EthernetInterfaceType"] = "Virtual";
        jsonResponse["VLAN"]["VLANEnable"] = true;
        jsonResponse["VLAN"]["VLANId"] = *ethData.vlanId;
        jsonResponse["VLAN"]["Tagged"] = true;

        nlohmann::json::array_t relatedInterfaces;
        nlohmann::json& parentInterface = relatedInterfaces.emplace_back();
        parentInterface["@odata.id"] =
            boost::urls::format("/redfish/v1/Managers/bmc/EthernetInterfaces",
                                extractParentInterfaceName(ifaceId));
        jsonResponse["Links"]["RelatedInterfaces"] =
            std::move(relatedInterfaces);
    }
    else
    {
        jsonResponse["EthernetInterfaceType"] = "Physical";
    }

    jsonResponse["NameServers"] = ethData.nameServers;
    jsonResponse["StaticNameServers"] = ethData.staticNameServers;

    nlohmann::json& ipv4Array = jsonResponse["IPv4Addresses"];
    nlohmann::json& ipv4StaticArray = jsonResponse["IPv4StaticAddresses"];
    ipv4Array = nlohmann::json::array();
    ipv4StaticArray = nlohmann::json::array();
    for (const auto& ipv4Config : ipv4Data)
    {
        std::string gatewayStr = ipv4Config.gateway;
        if (gatewayStr.empty())
        {
            gatewayStr = "0.0.0.0";
        }
        nlohmann::json::object_t ipv4;
        ipv4["AddressOrigin"] = ipv4Config.origin;
        ipv4["SubnetMask"] = ipv4Config.netmask;
        ipv4["Address"] = ipv4Config.address;
        ipv4["Gateway"] = gatewayStr;

        if (ipv4Config.origin == "Static")
        {
            ipv4StaticArray.push_back(ipv4);
        }

        ipv4Array.emplace_back(std::move(ipv4));
    }

    std::string ipv6GatewayStr = ethData.ipv6DefaultGateway;
    if (ipv6GatewayStr.empty())
    {
        ipv6GatewayStr = "0:0:0:0:0:0:0:0";
    }

    jsonResponse["IPv6DefaultGateway"] = ipv6GatewayStr;

    nlohmann::json& ipv6Array = jsonResponse["IPv6Addresses"];
    nlohmann::json& ipv6StaticArray = jsonResponse["IPv6StaticAddresses"];
    ipv6Array = nlohmann::json::array();
    ipv6StaticArray = nlohmann::json::array();
    nlohmann::json& ipv6AddrPolicyTable =
        jsonResponse["IPv6AddressPolicyTable"];
    ipv6AddrPolicyTable = nlohmann::json::array();
    for (const auto& ipv6Config : ipv6Data)
    {
        nlohmann::json::object_t ipv6;
        ipv6["Address"] = ipv6Config.address;
        ipv6["PrefixLength"] = ipv6Config.prefixLength;
        ipv6["AddressOrigin"] = ipv6Config.origin;

        ipv6Array.emplace_back(std::move(ipv6));
        if (ipv6Config.origin == "Static")
        {
            nlohmann::json::object_t ipv6Static;
            ipv6Static["Address"] = ipv6Config.address;
            ipv6Static["PrefixLength"] = ipv6Config.prefixLength;
            ipv6StaticArray.emplace_back(std::move(ipv6Static));
        }
    }
}

void afterDelete(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                 const std::string& ifaceId,
                 const boost::system::error_code& ec,
                 const sdbusplus::message_t& m)
{
    if (!ec)
    {
        return;
    }
    const sd_bus_error* dbusError = m.get_error();
    if (dbusError == nullptr)
    {
        messages::internalError(asyncResp->res);
        return;
    }
    BMCWEB_LOG_DEBUG("DBus error: {}", dbusError->name);

    if (std::string_view("org.freedesktop.DBus.Error.UnknownObject") ==
        dbusError->name)
    {
        messages::resourceNotFound(asyncResp->res, "EthernetInterface",
                                   ifaceId);
        return;
    }
    if (std::string_view("org.freedesktop.DBus.Error.UnknownMethod") ==
        dbusError->name)
    {
        messages::resourceCannotBeDeleted(asyncResp->res);
        return;
    }
    messages::internalError(asyncResp->res);
}

void afterVlanCreate(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& parentInterfaceUri,
                     const std::string& vlanInterface,
                     const boost::system::error_code& ec,
                     const sdbusplus::message_t& m

)
{
    if (ec)
    {
        const sd_bus_error* dbusError = m.get_error();
        if (dbusError == nullptr)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        BMCWEB_LOG_DEBUG("DBus error: {}", dbusError->name);

        if (std::string_view(
                "xyz.openbmc_project.Common.Error.ResourceNotFound") ==
            dbusError->name)
        {
            messages::propertyValueNotInList(
                asyncResp->res, parentInterfaceUri,
                "Links/RelatedInterfaces/0/@odata.id");
            return;
        }
        if (std::string_view(
                "xyz.openbmc_project.Common.Error.InvalidArgument") ==
            dbusError->name)
        {
            messages::resourceAlreadyExists(asyncResp->res, "EthernetInterface",
                                            "Id", vlanInterface);
            return;
        }
        messages::internalError(asyncResp->res);
        return;
    }

    const boost::urls::url vlanInterfaceUri = boost::urls::format(
        "/redfish/v1/Managers/bmc/EthernetInterfaces/{}", vlanInterface);
    asyncResp->res.addHeader("Location", vlanInterfaceUri.buffer());
}

void requestEthernetInterfacesRoutes(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Managers/bmc/EthernetInterfaces/")
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
            "/redfish/v1/Managers/bmc/EthernetInterfaces";
        asyncResp->res.jsonValue["Name"] =
            "Ethernet Network Interface Collection";
        asyncResp->res.jsonValue["Description"] =
            "Collection of EthernetInterfaces for this Manager";

        // Get eth interface list, and call the below callback for JSON
        // preparation
        getEthernetIfaceList(
            [asyncResp](const bool& success,
                        const std::vector<std::string>& ifaceList) {
            if (!success)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            nlohmann::json& ifaceArray = asyncResp->res.jsonValue["Members"];
            ifaceArray = nlohmann::json::array();
            for (const std::string& ifaceItem : ifaceList)
            {
                nlohmann::json::object_t iface;
                iface["@odata.id"] = boost::urls::format(
                    "/redfish/v1/Managers/bmc/EthernetInterfaces/{}",
                    ifaceItem);
                ifaceArray.push_back(std::move(iface));
            }

            asyncResp->res.jsonValue["Members@odata.count"] = ifaceArray.size();
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/Managers/bmc/EthernetInterfaces";
        });
    });

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/bmc/EthernetInterfaces/")
        .privileges(redfish::privileges::postEthernetInterfaceCollection)
        .methods(boost::beast::http::verb::post)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }

        bool vlanEnable = false;
        uint32_t vlanId = 0;
        nlohmann::json::array_t relatedInterfaces;

        if (!json_util::readJsonPatch(req, asyncResp->res, "VLAN/VLANEnable",
                                      vlanEnable, "VLAN/VLANId", vlanId,
                                      "Links/RelatedInterfaces",
                                      relatedInterfaces))
        {
            return;
        }

        if (relatedInterfaces.size() != 1)
        {
            messages::arraySizeTooLong(asyncResp->res,
                                       "Links/RelatedInterfaces",
                                       relatedInterfaces.size());
            return;
        }

        std::string parentInterfaceUri;
        if (!json_util::readJson(relatedInterfaces[0], asyncResp->res,
                                 "@odata.id", parentInterfaceUri))
        {
            messages::propertyMissing(asyncResp->res,
                                      "Links/RelatedInterfaces/0/@odata.id");
            return;
        }
        BMCWEB_LOG_INFO("Parent Interface URI: {}", parentInterfaceUri);

        boost::system::result<boost::urls::url_view> parsedUri =
            boost::urls::parse_relative_ref(parentInterfaceUri);
        if (!parsedUri)
        {
            messages::propertyValueFormatError(
                asyncResp->res, parentInterfaceUri,
                "Links/RelatedInterfaces/0/@odata.id");
            return;
        }

        std::string parentInterface;
        if (!crow::utility::readUrlSegments(
                *parsedUri, "redfish", "v1", "Managers", "bmc",
                "EthernetInterfaces", std::ref(parentInterface)))
        {
            messages::propertyValueNotInList(
                asyncResp->res, parentInterfaceUri,
                "Links/RelatedInterfaces/0/@odata.id");
            return;
        }

        if (!vlanEnable)
        {
            // In OpenBMC implementation, VLANEnable cannot be false on
            // create
            messages::propertyValueIncorrect(asyncResp->res, "VLAN/VLANEnable",
                                             "false");
            return;
        }

        std::string vlanInterface = parentInterface + "_" +
                                    std::to_string(vlanId);
        crow::connections::systemBus->async_method_call(
            [asyncResp, parentInterfaceUri,
             vlanInterface](const boost::system::error_code& ec,
                            const sdbusplus::message_t& m) {
            afterVlanCreate(asyncResp, parentInterfaceUri, vlanInterface, ec,
                            m);
        },
            "xyz.openbmc_project.Network", "/xyz/openbmc_project/network",
            "xyz.openbmc_project.Network.VLAN.Create", "VLAN", parentInterface,
            vlanId);
    });

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/bmc/EthernetInterfaces/<str>/")
        .privileges(redfish::privileges::getEthernetInterface)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& ifaceId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        getEthernetIfaceData(
            ifaceId,
            [asyncResp, ifaceId](const bool& success,
                                 const EthernetInterfaceData& ethData,
                                 const std::vector<IPv4AddressData>& ipv4Data,
                                 const std::vector<IPv6AddressData>& ipv6Data) {
            if (!success)
            {
                // TODO(Pawel)consider distinguish between non
                // existing object, and other errors
                messages::resourceNotFound(asyncResp->res, "EthernetInterface",
                                           ifaceId);
                return;
            }

            asyncResp->res.jsonValue["@odata.type"] =
                "#EthernetInterface.v1_9_0.EthernetInterface";
            asyncResp->res.jsonValue["Name"] = "Manager Ethernet Interface";
            asyncResp->res.jsonValue["Description"] =
                "Management Network Interface";

            parseInterfaceData(asyncResp, ifaceId, ethData, ipv4Data, ipv6Data);
        });
    });

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/bmc/EthernetInterfaces/<str>/")
        .privileges(redfish::privileges::patchEthernetInterface)
        .methods(boost::beast::http::verb::patch)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& ifaceId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        std::optional<std::string> hostname;
        std::optional<std::string> fqdn;
        std::optional<std::string> macAddress;
        std::optional<std::string> ipv6DefaultGateway;
        std::optional<nlohmann::json::array_t> ipv4StaticAddresses;
        std::optional<nlohmann::json::array_t> ipv6StaticAddresses;
        std::optional<std::vector<std::string>> staticNameServers;
        std::optional<nlohmann::json> dhcpv4;
        std::optional<nlohmann::json> dhcpv6;
        std::optional<bool> ipv6AutoConfigEnabled;
        std::optional<bool> interfaceEnabled;
        std::optional<size_t> mtuSize;
        DHCPParameters v4dhcpParms;
        DHCPParameters v6dhcpParms;
        // clang-format off
        if (!json_util::readJsonPatch(
                req, asyncResp->res,
                "DHCPv4", dhcpv4,
                "DHCPv6", dhcpv6,
                "FQDN", fqdn,
                "HostName", hostname,
                "IPv4StaticAddresses", ipv4StaticAddresses,
                "IPv6DefaultGateway", ipv6DefaultGateway,
                "IPv6StaticAddresses", ipv6StaticAddresses,
                "InterfaceEnabled", interfaceEnabled,
                "MACAddress", macAddress,
                "MTUSize", mtuSize,
                "StatelessAddressAutoConfig/IPv6AutoConfigEnabled", ipv6AutoConfigEnabled,
                "StaticNameServers", staticNameServers
                )
            )
        {
            return;
        }
        //clang-format on
        if (dhcpv4)
        {
            if (!json_util::readJson(*dhcpv4, asyncResp->res, "DHCPEnabled",
                                     v4dhcpParms.dhcpv4Enabled, "UseDNSServers",
                                     v4dhcpParms.useDnsServers, "UseNTPServers",
                                     v4dhcpParms.useNtpServers, "UseDomainName",
                                     v4dhcpParms.useDomainName))
            {
                return;
            }
        }

        if (dhcpv6)
        {
            if (!json_util::readJson(*dhcpv6, asyncResp->res, "OperatingMode",
                                     v6dhcpParms.dhcpv6OperatingMode,
                                     "UseDNSServers", v6dhcpParms.useDnsServers,
                                     "UseNTPServers", v6dhcpParms.useNtpServers,
                                     "UseDomainName",
                                     v6dhcpParms.useDomainName))
            {
                return;
            }
        }

        // Get single eth interface data, and call the below callback
        // for JSON preparation
        getEthernetIfaceData(
            ifaceId,
            [asyncResp, ifaceId, hostname = std::move(hostname),
             fqdn = std::move(fqdn), macAddress = std::move(macAddress),
             ipv4StaticAddresses = std::move(ipv4StaticAddresses),
             ipv6DefaultGateway = std::move(ipv6DefaultGateway),
             ipv6StaticAddresses = std::move(ipv6StaticAddresses),
             staticNameServers = std::move(staticNameServers),
             dhcpv4 = std::move(dhcpv4), dhcpv6 = std::move(dhcpv6), mtuSize,
             ipv6AutoConfigEnabled, v4dhcpParms = std::move(v4dhcpParms),
             v6dhcpParms = std::move(v6dhcpParms), interfaceEnabled](
                const bool& success, const EthernetInterfaceData& ethData,
                const std::vector<IPv4AddressData>& ipv4Data,
                const std::vector<IPv6AddressData>& ipv6Data) {
            if (!success)
            {
                // ... otherwise return error
                // TODO(Pawel)consider distinguish between non
                // existing object, and other errors
                messages::resourceNotFound(asyncResp->res, "EthernetInterface",
                                           ifaceId);
                return;
            }

            if (dhcpv4 || dhcpv6)
            {
                handleDHCPPatch(ifaceId, ethData, v4dhcpParms, v6dhcpParms,
                                asyncResp);
            }

            if (hostname)
            {
                handleHostnamePatch(*hostname, asyncResp);
            }

            if (ipv6AutoConfigEnabled)
            {
                handleSLAACAutoConfigPatch(ifaceId, *ipv6AutoConfigEnabled,
                                           asyncResp);
            }

            if (fqdn)
            {
                handleFqdnPatch(ifaceId, *fqdn, asyncResp);
            }

            if (macAddress)
            {
                handleMACAddressPatch(ifaceId, *macAddress, asyncResp);
            }

            if (ipv4StaticAddresses)
            {
                // TODO(ed) for some reason the capture of
                // ipv4Addresses above is returning a const value,
                // not a non-const value. This doesn't really work
                // for us, as we need to be able to efficiently move
                // out the intermedia nlohmann::json objects. This
                // makes a copy of the structure, and operates on
                // that, but could be done more efficiently
                nlohmann::json::array_t ipv4Static = *ipv4StaticAddresses;
                handleIPv4StaticPatch(ifaceId, ipv4Static, ipv4Data, asyncResp);
            }

            if (staticNameServers)
            {
                handleStaticNameServersPatch(ifaceId, *staticNameServers,
                                             asyncResp);
            }

            if (ipv6DefaultGateway)
            {
                messages::propertyNotWritable(asyncResp->res,
                                              "IPv6DefaultGateway");
            }

            if (ipv6StaticAddresses)
            {
                handleIPv6StaticAddressesPatch(ifaceId, *ipv6StaticAddresses,
                                               ipv6Data, asyncResp);
            }

            if (interfaceEnabled)
            {
                setEthernetInterfaceBoolProperty(ifaceId, "NICEnabled",
                                                 *interfaceEnabled, asyncResp);
            }

            if (mtuSize)
            {
                handleMTUSizePatch(ifaceId, *mtuSize, asyncResp);
            }
            });
        });

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/bmc/EthernetInterfaces/<str>/")
        .privileges(redfish::privileges::deleteEthernetInterface)
        .methods(boost::beast::http::verb::delete_)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& ifaceId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }

        crow::connections::systemBus->async_method_call(
            [asyncResp, ifaceId](const boost::system::error_code& ec,
                                 const sdbusplus::message_t& m) {
            afterDelete(asyncResp, ifaceId, ec, m);
            },
            "xyz.openbmc_project.Network",
            std::string("/xyz/openbmc_project/network/") + ifaceId,
            "xyz.openbmc_project.Object.Delete", "Delete");
        });
}

} // namespace redfish