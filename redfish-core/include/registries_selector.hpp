#pragma once
#include "registries/base_message_registry.hpp"
#include "registries/openbmc_message_registry.hpp"
#include "registries/platform_message_registry.hpp"
#include "registries/resource_event_message_registry.hpp"
#include "registries/task_event_message_registry.hpp"
#include "registries/update_message_registry.hpp"

#include <span>
#include <string_view>

namespace redfish::registries
{
inline std::span<const MessageEntry>
    getRegistryFromPrefix(std::string_view registryName)
{
    if (task_event::header.registryPrefix == registryName)
    {
        return {task_event::registry};
    }
    if (openbmc::header.registryPrefix == registryName)
    {
        return {openbmc::registry};
    }
    if (base::header.registryPrefix == registryName)
    {
        return {base::registry};
    }
    if (resource_event::header.registryPrefix == registryName)
    {
        return {resource_event::registry};
    }
    if (update::header.registryPrefix == registryName)
    {
        return {update::registry};
    }
    if (platform::header.registryPrefix == registryName)
    {
        return {platform::registry};
    }

    return {openbmc::registry};
}
} // namespace redfish::registries
