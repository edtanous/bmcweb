/*!
 * @file    registry_utils.cpp
 * @brief   Source code for utility functions of handling message registries.
 */

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/beast/core/span.hpp>
#include <registries.hpp>
#include <registries/base_message_registry.hpp>
#include <registries/bios_attribute_registry.hpp>
#include <registries/openbmc_message_registry.hpp>
#include <registries/resource_event_message_registry.hpp>
#include <registries/task_event_message_registry.hpp>
#include <registries/update_event_message_registry.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <utility>

namespace redfish
{
namespace message_registries
{
inline std::span<const redfish::registries::MessageEntry>
    getRegistryFromPrefix(const std::string_view registryName)
{
    if (redfish::registries::task_event::header.registryPrefix == registryName)
    {
        return std::span<const redfish::registries::MessageEntry>{
            redfish::registries::task_event::registry};
    }
    if (redfish::registries::openbmc::header.registryPrefix == registryName)
    {
        return std::span<const redfish::registries::MessageEntry>{
            redfish::registries::openbmc::registry};
    }
    if (redfish::registries::base::header.registryPrefix == registryName)
    {
        return std::span<const redfish::registries::MessageEntry>{
            redfish::registries::base::registry};
    }
    if (redfish::registries::resource_event::header.registryPrefix ==
        registryName)
    {
        return std::span<const redfish::registries::MessageEntry>{
            redfish::registries::resource_event::registry};
    }
    if (redfish::registries::update_event::header.registryPrefix ==
        registryName)
    {
        return std::span<const redfish::registries::MessageEntry>{
            redfish::registries::update_event::registry};
    }

    return {};
}

inline const redfish::registries::Message* getMessageFromRegistry(
    const std::string& messageKey,
    const std::span<const redfish::registries::MessageEntry> registry)
{
    std::span<const redfish::registries::MessageEntry>::iterator messageIt =
        std::find_if(
            registry.begin(), registry.end(),
            [&messageKey](
                const redfish::registries::MessageEntry& messageEntry) {
                return !std::strcmp(messageEntry.first, messageKey.c_str());
            });
    if (messageIt != registry.end())
    {
        return &messageIt->second;
    }

    return nullptr;
}

inline std::string getPrefix(const std::string& messageID)
{
    size_t pos = messageID.find('.');
    return messageID.substr(0, pos);
}

inline const redfish::registries::Message*
    getMessage(const std::string_view& messageID)
{
    // Redfish MessageIds are in the form
    // RegistryName.MajorVersion.MinorVersion.MessageKey, so parse it to
    // find the right Message
    std::vector<std::string> fields;
    fields.reserve(4);
    boost::split(fields, messageID, boost::is_any_of("."));

    if (fields.size() != 4)
    {
        return nullptr;
    }
    std::string& registryName = fields[0];
    std::string& messageKey = fields[3];

    return getMessageFromRegistry(messageKey,
                                  getRegistryFromPrefix(registryName));
}

inline bool isMessageIdValid(const std::string_view messageId)
{
    const redfish::registries::Message* msg = getMessage(messageId);
    (void)msg;
    return msg != nullptr;
}
} // namespace message_registries
} // namespace redfish
