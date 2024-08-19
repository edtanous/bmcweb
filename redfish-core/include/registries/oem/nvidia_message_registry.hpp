#pragma once
/****************************************************************
 *                 READ THIS WARNING FIRST
 * This is an auto-generated header which contains definitions
 * for Redfish DMTF defined messages.
 * DO NOT modify this registry outside of running the
 * parse_registries.py script.  The definitions contained within
 * this file are owned by DMTF.  Any modifications to these files
 * should be first pushed to the relevant registry in the DMTF
 * github organization.
 ***************************************************************/
#include "registries.hpp"

#include <array>

// clang-format off

namespace redfish::registries::nvidia
{
const Header header = {
    "Copyright 2024 Nvidia. All rights reserved.",
    "#MessageRegistry.v1_4_0.MessageRegistry",
    "NvidiaUpdate.1.0.0",
    "Nvidia Message Registry",
    "en",
    "This registry defines the update messages for Nvidia.",
    "NvidiaUpdate",
    "1.0.0",
    "Nvidia",
};
constexpr const char* url =
    "";

constexpr std::array registry =
{
    MessageEntry{
        "ComponentUpdateSkipped",
        {
            "Indicates that update of component has been skipped",
            "The update operation for the component %1 is skipped because %2.",
            "OK",
            2,
            {
                "string",
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "DebugTokenInstallationSuccess",
        {
            "Signifies the successful completion of debug token installation.",
            "The operation to install a debug token for device '%1' has been successfully completed.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "DebugTokenRequestSuccess",
        {
            "Signifies the successful completion of the debug token request.",
            "The operation to request a debug token for device '%1' has been successfully completed.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "DebugTokenStatusSuccess",
        {
            "Signifies the successful completion of the debug token status request.",
            "The operation to obtain a token status for device '%1' has been successfully completed.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "DebugTokenUnsupported",
        {
            "Indicates that the device does not support debug token functionality.",
            "Device '%1' does not support debug token functionality.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "FirmwareNotInRecovery",
        {
            "Indicates that a firmware is not in Recovery Mode",
            "Firmware %1 is not in Recovery.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "RecoveryStarted",
        {
            "Indicates that recovery has started on a component",
            "Firmware Recovery Started on %1.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},
    MessageEntry{
        "RecoverySuccessful",
        {
            "Indicates that recovery has successfully completed on a component",
            "Firmware %1 is successfully recovered.",
            "OK",
            1,
            {
                "string",
            },
            "None.",
        }},

};

enum class Index
{
    componentUpdateSkipped = 0,
    debugTokenInstallationSuccess = 1,
    debugTokenRequestSuccess = 2,
    debugTokenStatusSuccess = 3,
    debugTokenUnsupported = 4,
    firmwareNotInRecovery = 5,
    recoveryStarted = 6,
    recoverySuccessful = 7,
};
} // namespace redfish::registries::nvidia
