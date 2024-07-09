/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <boost/interprocess/streams/bufferstream.hpp>

#include <optional>
#include <sstream>
#include <vector>

namespace redfish::debug_token
{

constexpr const size_t vdmUtilWrapperOutputEidIndex = 0;
constexpr const size_t vdmUtilWrapperOutputVersionIndex = 1;
constexpr const size_t vdmUtilWrapperOutputTxIndex = 2;
constexpr const size_t vdmUtilWrapperOutputRxIndex = 3;

constexpr const size_t vdmStatusDeviceIdLength = 8;
constexpr const size_t vdmStatusErrorCodeOffset = 8;
constexpr const size_t vdmStatusErrorCodeSuccess = 0x00;
constexpr const size_t vdmStatusErrorCodeNotSupported = 0x05;

enum class VdmResponseStatus
{
    INVALID_LENGTH,
    PROCESSING_ERROR,
    NOT_SUPPORTED,
    ERROR,
    STATUS
};

enum class VdmTokenInstallationStatus
{
    NOT_INSTALLED = 0x00,
    INSTALLED = 0x01,
    INVALID
};

enum class VdmTokenFuseType
{
    DEBUG = 0x01,
    PRODUCTION = 0x02,
    INVALID
};

enum class VdmTokenType
{
    UNDEFINED = 0x00,
    DEBUG_FW = 0x01,
    JTAG_UNLOCK = 0x02,
    HW_UNLOCK = 0x04,
    RUNTIME_DEBUG = 0x08,
    FEATURE_UNLOCK = 0x10
};

enum class VdmTokenLifecycle
{
    PERSISTENT = 0,
    TEMPORAL = (1 << 0)
};

enum class VdmTokenActivation
{
    ON_BOOT = 0,
    MANUAL = (1 << 1)
};

enum class VdmTokenRevocation
{
    MANUAL = 0,
    AUTOMATIC = (1 << 2)
};

enum class VdmTokenDevIdStatus
{
    DISABLED = 0,
    ENABLED = (1 << 3)
};

enum class VdmTokenAntiReplay
{
    NONCE_DISABLED = 0,
    NONCE_ENABLED = (1 << 4)
};

enum class VdmTokenResetPostInstall
{
    NOT_MANDATED = 0,
    MANDATED = (1 << 5)
};

enum class VdmTokenProcessingStatus
{
    NOT_PROCESSED = 0x00,
    PROCESSED = 0x01,
    VERIFICATION_FAILURE = 0x02,
    RUNTIME_ERROR = 0x03,
    INVALID
};

struct VdmTokenStatus
{
    VdmResponseStatus responseStatus;
    std::optional<uint8_t> errorCode;
    VdmTokenInstallationStatus tokenStatus;
    VdmTokenFuseType fuseType;
    std::vector<uint8_t> deviceId;
    std::optional<uint32_t> tokenType;
    std::optional<uint16_t> validityCounter;
    std::optional<VdmTokenLifecycle> tokenLifecycle;
    std::optional<VdmTokenActivation> tokenActivation;
    std::optional<VdmTokenRevocation> tokenRevocation;
    std::optional<VdmTokenDevIdStatus> tokenDevIdStatus;
    std::optional<VdmTokenAntiReplay> tokenAntiReplay;
    std::optional<VdmTokenResetPostInstall> tokenResetPostInstall;
    std::optional<VdmTokenProcessingStatus> tokenProcessingStatus;
};

#pragma pack(1)
struct VdmStatusV1
{
    uint8_t tokenInstallationStatus;
    uint8_t deviceId[vdmStatusDeviceIdLength];
    uint8_t fuseType;
};
#pragma pack()
#pragma pack(1)
struct VdmStatusV2
{
    uint8_t tokenInstallationStatus;
    uint8_t deviceId[vdmStatusDeviceIdLength];
    uint8_t fuseType;
    uint32_t tokenType;
    uint16_t validityCounter;
    uint16_t tokenConfig;
    uint16_t processingStatus;
    uint8_t reserved[8];
};
#pragma pack()

static VdmTokenStatus parseVdmTokenStatus(std::string vdmResponse, int version)
{
    VdmTokenStatus resp;
    if (vdmResponse.empty())
    {
        resp.responseStatus = VdmResponseStatus::INVALID_LENGTH;
        return resp;
    }
    std::istringstream iss{vdmResponse};
    std::vector<std::string> bytes{std::istream_iterator<std::string>{iss},
                                   std::istream_iterator<std::string>{}};
    if (bytes.size() < vdmStatusErrorCodeOffset + 1)
    {
        resp.responseStatus = VdmResponseStatus::INVALID_LENGTH;
        BMCWEB_LOG_DEBUG("{}", __LINE__);
        return resp;
    }
    uint8_t errorCode;
    try
    {
        errorCode = static_cast<uint8_t>(
            stoul(bytes[vdmStatusErrorCodeOffset], nullptr, 16));
    }
    catch (const std::exception&)
    {
        resp.responseStatus = VdmResponseStatus::PROCESSING_ERROR;
        return resp;
    }
    if (errorCode == vdmStatusErrorCodeNotSupported)
    {
        resp.responseStatus = VdmResponseStatus::NOT_SUPPORTED;
        return resp;
    }
    if (errorCode != vdmStatusErrorCodeSuccess)
    {
        resp.responseStatus = VdmResponseStatus::ERROR;
        resp.errorCode = errorCode;
        return resp;
    }

    std::vector<uint8_t> statusData;
    statusData.reserve(bytes.size() - vdmStatusErrorCodeOffset - 1);
    auto itr = bytes.begin() + vdmStatusErrorCodeOffset + 1;
    while (itr != bytes.end())
    {
        try
        {
            statusData.push_back(
                static_cast<uint8_t>(stoul(*itr++, nullptr, 16)));
        }
        catch (const std::exception&)
        {
            resp.responseStatus = VdmResponseStatus::PROCESSING_ERROR;
            return resp;
        }
    }
    if (version == 1)
    {
        if (statusData.size() != sizeof(VdmStatusV1))
        {
            resp.responseStatus = VdmResponseStatus::INVALID_LENGTH;
            return resp;
        }
        VdmStatusV1* status = reinterpret_cast<VdmStatusV1*>(statusData.data());
        if (status->tokenInstallationStatus ==
            static_cast<uint8_t>(VdmTokenInstallationStatus::NOT_INSTALLED))
        {
            resp.tokenStatus = VdmTokenInstallationStatus::NOT_INSTALLED;
        }
        else if (status->tokenInstallationStatus ==
                 static_cast<uint8_t>(VdmTokenInstallationStatus::INSTALLED))
        {
            resp.tokenStatus = VdmTokenInstallationStatus::INSTALLED;
        }
        else
        {
            resp.tokenStatus = VdmTokenInstallationStatus::INVALID;
        }

        if (status->fuseType == static_cast<uint8_t>(VdmTokenFuseType::DEBUG))
        {
            resp.fuseType = VdmTokenFuseType::DEBUG;
        }
        else if (status->fuseType ==
                 static_cast<uint8_t>(VdmTokenFuseType::PRODUCTION))
        {
            resp.fuseType = VdmTokenFuseType::PRODUCTION;
        }
        else
        {
            resp.fuseType = VdmTokenFuseType::INVALID;
        }

        resp.deviceId = std::vector<uint8_t>(vdmStatusDeviceIdLength);
        std::memcpy(resp.deviceId.data(), status->deviceId,
                    vdmStatusDeviceIdLength);

        resp.responseStatus = VdmResponseStatus::STATUS;
        return resp;
    }
    if (version == 2)
    {
        if (statusData.size() != sizeof(VdmStatusV2))
        {
            resp.responseStatus = VdmResponseStatus::INVALID_LENGTH;
            return resp;
        }
        VdmStatusV2* status = reinterpret_cast<VdmStatusV2*>(statusData.data());
        if (status->tokenInstallationStatus ==
            static_cast<uint8_t>(VdmTokenInstallationStatus::NOT_INSTALLED))
        {
            resp.tokenStatus = VdmTokenInstallationStatus::NOT_INSTALLED;
        }
        else if (status->tokenInstallationStatus ==
                 static_cast<uint8_t>(VdmTokenInstallationStatus::INSTALLED))
        {
            resp.tokenStatus = VdmTokenInstallationStatus::INSTALLED;
        }
        else
        {
            resp.tokenStatus = VdmTokenInstallationStatus::INVALID;
        }

        if (status->fuseType == static_cast<uint8_t>(VdmTokenFuseType::DEBUG))
        {
            resp.fuseType = VdmTokenFuseType::DEBUG;
        }
        else if (status->fuseType ==
                 static_cast<uint8_t>(VdmTokenFuseType::PRODUCTION))
        {
            resp.fuseType = VdmTokenFuseType::PRODUCTION;
        }
        else
        {
            resp.fuseType = VdmTokenFuseType::INVALID;
        }

        resp.deviceId = std::vector<uint8_t>(vdmStatusDeviceIdLength);
        std::memcpy(resp.deviceId.data(), status->deviceId,
                    vdmStatusDeviceIdLength);

        resp.tokenType = status->tokenType;

        resp.validityCounter = status->validityCounter;
        if (status->tokenConfig &
            static_cast<uint16_t>(VdmTokenLifecycle::TEMPORAL))
        {
            resp.tokenLifecycle = VdmTokenLifecycle::TEMPORAL;
        }
        else
        {
            resp.tokenLifecycle = VdmTokenLifecycle::PERSISTENT;
        }

        if (status->tokenConfig &
            static_cast<uint16_t>(VdmTokenActivation::MANUAL))
        {
            resp.tokenActivation = VdmTokenActivation::MANUAL;
        }
        else
        {
            resp.tokenActivation = VdmTokenActivation::ON_BOOT;
        }

        if (status->tokenConfig &
            static_cast<uint16_t>(VdmTokenRevocation::AUTOMATIC))
        {
            resp.tokenRevocation = VdmTokenRevocation::AUTOMATIC;
        }
        else
        {
            resp.tokenRevocation = VdmTokenRevocation::MANUAL;
        }

        if (status->tokenConfig &
            static_cast<uint16_t>(VdmTokenDevIdStatus::ENABLED))
        {
            resp.tokenDevIdStatus = VdmTokenDevIdStatus::ENABLED;
        }
        else
        {
            resp.tokenDevIdStatus = VdmTokenDevIdStatus::DISABLED;
        }

        if (status->tokenConfig &
            static_cast<uint16_t>(VdmTokenAntiReplay::NONCE_ENABLED))
        {
            resp.tokenAntiReplay = VdmTokenAntiReplay::NONCE_ENABLED;
        }
        else
        {
            resp.tokenAntiReplay = VdmTokenAntiReplay::NONCE_DISABLED;
        }

        if (status->tokenConfig &
            static_cast<uint16_t>(VdmTokenResetPostInstall::MANDATED))
        {
            resp.tokenResetPostInstall = VdmTokenResetPostInstall::MANDATED;
        }
        else
        {
            resp.tokenResetPostInstall = VdmTokenResetPostInstall::NOT_MANDATED;
        }

        if (status->processingStatus ==
            static_cast<uint16_t>(VdmTokenProcessingStatus::NOT_PROCESSED))
        {
            resp.tokenProcessingStatus =
                VdmTokenProcessingStatus::NOT_PROCESSED;
        }
        else if (status->processingStatus ==
                 static_cast<uint16_t>(VdmTokenProcessingStatus::PROCESSED))
        {
            resp.tokenProcessingStatus = VdmTokenProcessingStatus::PROCESSED;
        }
        else if (status->processingStatus ==
                 static_cast<uint16_t>(
                     VdmTokenProcessingStatus::VERIFICATION_FAILURE))
        {
            resp.tokenProcessingStatus =
                VdmTokenProcessingStatus::VERIFICATION_FAILURE;
        }
        else if (status->processingStatus ==
                 static_cast<uint16_t>(VdmTokenProcessingStatus::RUNTIME_ERROR))
        {
            resp.tokenProcessingStatus =
                VdmTokenProcessingStatus::RUNTIME_ERROR;
        }
        else
        {
            resp.tokenProcessingStatus = VdmTokenProcessingStatus::INVALID;
        }

        resp.responseStatus = VdmResponseStatus::STATUS;
        return resp;
    }
    resp.responseStatus = VdmResponseStatus::PROCESSING_ERROR;
    return resp;
}

inline std::map<int, VdmTokenStatus>
    parseVdmUtilWrapperOutput(std::vector<char>& output)
{
    boost::interprocess::bufferstream outputStream(output.data(),
                                                   output.size());
    std::string line;
    std::map<int, VdmTokenStatus> outputMap;
    while (std::getline(outputStream, line))
    {
        if (line.empty())
        {
            continue;
        }
        std::stringstream lineStream{line};
        std::string elem;
        std::vector<std::string> lineElements;
        // each line of the wrapper script output has the following format:
        // EID;VERSION;TXDATA;RXDATA
        while (std::getline(lineStream, elem, ';'))
        {
            lineElements.push_back(elem);
        }
        if (lineElements.size() < vdmUtilWrapperOutputRxIndex)
        {
            BMCWEB_LOG_ERROR("Invalid data: ", line);
            continue;
        }
        int eid, version;
        try
        {
            eid = std::stoi(lineElements[vdmUtilWrapperOutputEidIndex]);
            version = std::stoi(lineElements[vdmUtilWrapperOutputVersionIndex]);
        }
        catch (const std::exception&)
        {
            BMCWEB_LOG_ERROR("Invalid data: ", line);
            continue;
        }
        auto& txLine = lineElements[vdmUtilWrapperOutputTxIndex];
        auto& rxLine = lineElements[vdmUtilWrapperOutputRxIndex];
        BMCWEB_LOG_DEBUG("EID: {} TX: {}", eid, txLine);
        BMCWEB_LOG_DEBUG("EID: {} RX: {}", eid, rxLine);
        VdmTokenStatus status = parseVdmTokenStatus(rxLine, version);
        // if more than one query was executed, use the one which contained
        // correct status output
        if (outputMap.find(eid) != outputMap.end())
        {
            auto& prevStatus = outputMap[eid];
            if (prevStatus.responseStatus != VdmResponseStatus::STATUS &&
                status.responseStatus == VdmResponseStatus::STATUS)
            {
                outputMap[eid] = status;
            }
        }
        else
        {
            outputMap[eid] = status;
        }
    }
    return outputMap;
}

static std::string setOrAppend(std::string str, std::string in)
{
    if (str.empty())
    {
        return in;
    }
    return str + ", " + in;
}

inline void vdmTokenStatusToJson(VdmTokenStatus& status, nlohmann::json& json)
{
    if (status.tokenStatus == VdmTokenInstallationStatus::INSTALLED)
    {
        json["TokenInstalled"] = true;
    }
    else
    {
        json["TokenInstalled"] = false;
    }
    if (status.fuseType == VdmTokenFuseType::PRODUCTION)
    {
        json["FirmwareFuseType"] = "Production";
    }
    else if (status.fuseType == VdmTokenFuseType::DEBUG)
    {
        json["FirmwareFuseType"] = "Debug";
    }
    else
    {
        json["FirmwareFuseType"] = "Invalid";
    }
    std::ostringstream oss;
    oss << "0x";
    oss << std::hex << std::uppercase << std::setfill('0');
    auto itr = status.deviceId.begin();
    while (itr != status.deviceId.end())
    {
        oss << std::setw(2) << static_cast<int>(*itr++);
    }
    json["DeviceID"] = oss.str();
    if (status.tokenType)
    {
        if (*status.tokenType == static_cast<uint32_t>(VdmTokenType::UNDEFINED))
        {
            json["TokenType"] = "Undefined";
        }
        else
        {
            std::string tokenType;
            if (*status.tokenType &
                static_cast<uint32_t>(VdmTokenType::DEBUG_FW))
            {
                tokenType = setOrAppend(tokenType, "DebugFw");
            }
            if (*status.tokenType &
                static_cast<uint32_t>(VdmTokenType::JTAG_UNLOCK))
            {
                tokenType = setOrAppend(tokenType, "JtagUnlock");
            }
            if (*status.tokenType &
                static_cast<uint32_t>(VdmTokenType::HW_UNLOCK))
            {
                tokenType = setOrAppend(tokenType, "HwUnlock");
            }
            if (*status.tokenType &
                static_cast<uint32_t>(VdmTokenType::RUNTIME_DEBUG))
            {
                tokenType = setOrAppend(tokenType, "RuntimeDebug");
            }
            if (*status.tokenType &
                static_cast<uint32_t>(VdmTokenType::FEATURE_UNLOCK))
            {
                tokenType = setOrAppend(tokenType, "FeatureUnlock");
            }
            json["TokenType"] = tokenType;
        }
    }
    if (status.validityCounter)
    {
        json["ValidityCounter"] = *status.validityCounter;
    }
    if (status.tokenLifecycle)
    {
        if (*status.tokenLifecycle == VdmTokenLifecycle::PERSISTENT)
        {
            json["Lifecycle"] = "Persistent";
        }
        else
        {
            json["Lifecycle"] = "Temporal";
        }
    }
    if (status.tokenActivation)
    {
        if (*status.tokenActivation == VdmTokenActivation::ON_BOOT)
        {
            json["Activation"] = "OnBoot";
        }
        else
        {
            json["Activation"] = "Manual";
        }
    }
    if (status.tokenRevocation)
    {
        if (*status.tokenRevocation == VdmTokenRevocation::MANUAL)
        {
            json["Revocation"] = "Manual";
        }
        else
        {
            json["Revocation"] = "Automatic";
        }
    }
    if (status.tokenDevIdStatus)
    {
        if (*status.tokenDevIdStatus == VdmTokenDevIdStatus::DISABLED)
        {
            json["DevIdStatus"] = "Disabled";
        }
        else
        {
            json["Activation"] = "Enabled";
        }
    }
    if (status.tokenAntiReplay)
    {
        if (*status.tokenAntiReplay == VdmTokenAntiReplay::NONCE_DISABLED)
        {
            json["AntiReplay"] = "NonceDisabled";
        }
        else
        {
            json["AntiReplay"] = "NonceEnabled";
        }
    }
    if (status.tokenResetPostInstall)
    {
        if (*status.tokenResetPostInstall == VdmTokenResetPostInstall::MANDATED)
        {
            json["ResetPostInstall"] = "Mandated";
        }
        else
        {
            json["ResetPostInstall"] = "NotMandated";
        }
    }
    if (status.tokenProcessingStatus)
    {
        if (*status.tokenProcessingStatus ==
            VdmTokenProcessingStatus::NOT_PROCESSED)
        {
            json["ProcessingStatus"] = "NotProcessed";
        }
        else if (*status.tokenProcessingStatus ==
                 VdmTokenProcessingStatus::PROCESSED)
        {
            json["ProcessingStatus"] = "Processed";
        }
        else if (*status.tokenProcessingStatus ==
                 VdmTokenProcessingStatus::VERIFICATION_FAILURE)
        {
            json["ProcessingStatus"] = "VerificationFailure";
        }
        else if (*status.tokenProcessingStatus ==
                 VdmTokenProcessingStatus::RUNTIME_ERROR)
        {
            json["ProcessingStatus"] = "RuntimeError";
        }
        else
        {
            json["ProcessingStatus"] = "Invalid";
        }
    }
}

} // namespace redfish::debug_token
