#include "registries.hpp"

#include "registries/base_message_registry.hpp"
#include "registries/openbmc_message_registry.hpp"
<<<<<<< HEAD
#include "registries_selector.hpp"
=======
#include "registries/telemetry_message_registry.hpp"
>>>>>>> master
#include "str_utility.hpp"

#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace redfish::registries
{

const Message* getMessageFromRegistry(const std::string& messageKey,
                                      std::span<const MessageEntry> registry)
{
    std::span<const MessageEntry>::iterator messageIt = std::ranges::find_if(
        registry, [&messageKey](const MessageEntry& messageEntry) {
        return std::strcmp(messageEntry.first, messageKey.c_str()) == 0;
    });
    if (messageIt != registry.end())
    {
        return &messageIt->second;
    }

    return nullptr;
}

const Message* getMessage(std::string_view messageID)
{
    // Redfish MessageIds are in the form
    // RegistryName.MajorVersion.MinorVersion.MessageKey, so parse it to find
    // the right Message
    if (messageID.empty())
    {
        return nullptr;
    }
    std::vector<std::string> fields;
    fields.reserve(4);
    bmcweb::split(fields, messageID, '.');
    if (fields.size() < 4)
    {
        return nullptr;
    }
    const std::string& registryName = fields[0];
    const std::string& messageKey = fields[3];

    // Find the right registry and check it for the MessageKey
<<<<<<< HEAD
    return getMessageFromRegistry(messageKey,
                                  getRegistryFromPrefix(registryName));
=======
    if (std::string(base::header.registryPrefix) == registryName)
    {
        return getMessageFromRegistry(
            messageKey, std::span<const MessageEntry>(base::registry));
    }
    if (std::string(openbmc::header.registryPrefix) == registryName)
    {
        return getMessageFromRegistry(
            messageKey, std::span<const MessageEntry>(openbmc::registry));
    }
    if (std::string(telemetry::header.registryPrefix) == registryName)
    {
        return getMessageFromRegistry(
            messageKey, std::span<const MessageEntry>(telemetry::registry));
    }
    return nullptr;
>>>>>>> master
}

} // namespace redfish::registries
