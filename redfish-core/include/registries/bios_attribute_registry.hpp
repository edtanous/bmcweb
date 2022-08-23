#pragma once
#include <registries.hpp>

namespace redfish::message_registries::bios
{
const Header header = {
    "Copyright 2022 OpenBMC. All rights reserved.",
    "#MessageRegistry.v1_4_0.MessageRegistry",
    "BiosAttributeRegistry.1.0.0",
    "Bios Attribute Registry",
    "en",
    "This registry defines the base messages for Bios Attribute Registry.",
    "BiosAttributeRegistry",
    "1.0.0",
    "OpenBMC",
};
constexpr std::array<MessageEntry, 0> registry = {};
} // namespace redfish::message_registries::bios
