#pragma once
#include "app.hpp"
namespace redfish
{

enum class LinkType
{
    Local,
    Global
};

/**
 * Structure for keeping IPv4 data required by Redfish
 */
struct IPv4AddressData
{
    std::string id;
    std::string address;
    std::string domain;
    std::string gateway;
    std::string netmask;
    std::string origin;
    LinkType linktype{};
    bool isActive{};
};

/**
 * Structure for keeping IPv6 data required by Redfish
 */
struct IPv6AddressData
{
    std::string id;
    std::string address;
    std::string origin;
    uint8_t prefixLength = 0;
};

/**
 * Structure for keeping basic single Ethernet Interface information
 * available from DBus
 */
struct EthernetInterfaceData
{
    uint32_t speed;
    size_t mtuSize;
    bool autoNeg;
    bool dnsEnabled;
    bool ntpEnabled;
    bool hostNameEnabled;
    bool linkUp;
    bool nicEnabled;
    bool ipv6AcceptRa;
    std::string dhcpEnabled;
    std::string operatingMode;
    std::string hostName;
    std::string defaultGateway;
    std::string ipv6DefaultGateway;
    std::string macAddress;
    std::optional<uint32_t> vlanId;
    std::vector<std::string> nameServers;
    std::vector<std::string> staticNameServers;
    std::vector<std::string> domainnames;
};

// Helper function that changes bits netmask notation (i.e. /24)
// into full dot notation
std::string getNetmask(unsigned int bits);

bool translateDhcpEnabledToBool(const std::string& inputDHCP, bool isIPv4);

std::string translateAddressOriginDbusToRedfish(const std::string& inputOrigin,
                                                bool isIPv4);

std::string getDhcpEnabledEnumeration(bool isIPv4, bool isIPv6);
bool isHostnameValid(const std::string& hostname);

void requestEthernetInterfacesRoutes(App&);
} // namespace redfish
