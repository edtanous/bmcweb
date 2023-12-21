#pragma once

#include "registries.hpp"

#include <span>
#include <string_view>

namespace redfish::registries
{
std::span<const MessageEntry>
    getRegistryFromPrefix(std::string_view registryName);
} // namespace redfish::registries
