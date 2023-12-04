#include "registries.hpp"

#include "registries/base_message_registry.hpp"
#include "registries/openbmc_message_registry.hpp"
#include "str_utility.hpp"
#include "registries_selector.hpp"
#include <ranges>
#include <string>
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
    std::vector<std::string> fields;
    fields.reserve(4);
    bmcweb::split(fields, messageID, '.');
    const std::string& registryName = fields[0];
    const std::string& messageKey = fields[3];

    // Find the right registry and check it for the MessageKey
    return getMessageFromRegistry(messageKey,
                                  getRegistryFromPrefix(registryName));
}

} // namespace redfish::registries
