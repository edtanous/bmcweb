/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "health.hpp"

#include <app.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_utility.hpp>
#include <nlohmann/json.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <utils/collection.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/hex_utils.hpp>
#include <utils/json_utils.hpp>

namespace redfish
{
using DimmProperties =
    boost::container::flat_map<std::string, dbus::utility::DbusVariantType>;

inline std::string translateMemoryTypeToRedfish(const std::string& memoryType)
{
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR")
    {
        return "DDR";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR2")
    {
        return "DDR2";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR3")
    {
        return "DDR3";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR4")
    {
        return "DDR4";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR4E_SDRAM")
    {
        return "DDR4E_SDRAM";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR5")
    {
        return "DDR5";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.LPDDR4_SDRAM")
    {
        return "LPDDR4_SDRAM";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.LPDDR3_SDRAM")
    {
        return "LPDDR3_SDRAM";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR2_SDRAM_FB_DIMM")
    {
        return "DDR2_SDRAM_FB_DIMM";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR2_SDRAM_FB_DIMM_PROB")
    {
        return "DDR2_SDRAM_FB_DIMM_PROBE";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DDR_SGRAM")
    {
        return "DDR_SGRAM";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.ROM")
    {
        return "ROM";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.SDRAM")
    {
        return "SDRAM";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.EDO")
    {
        return "EDO";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.FastPageMode")
    {
        return "FastPageMode";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.PipelinedNibble")
    {
        return "PipelinedNibble";
    }
    if (memoryType ==
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.Logical")
    {
        return "Logical";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM")
    {
        return "HBM";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM2")
    {
        return "HBM2";
    }
    if (memoryType == "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM3")
    {
        return "HBM3";
    }
    // This is values like Other or Unknown
    // Also D-Bus values:
    // DRAM
    // EDRAM
    // VRAM
    // SRAM
    // RAM
    // FLASH
    // EEPROM
    // FEPROM
    // EPROM
    // CDRAM
    // ThreeDRAM
    // RDRAM
    // FBD2
    // LPDDR_SDRAM
    // LPDDR2_SDRAM
    // LPDDR5_SDRAM
    return "";
}

inline void dimmPropToHex(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                          const char* key, const uint16_t* value,
                          const nlohmann::json::json_pointer& jsonPtr)
{
    if (value == nullptr)
    {
        return;
    }
    aResp->res.jsonValue[jsonPtr][key] = "0x" + intToHexString(*value, 4);
}

inline void getPersistentMemoryProperties(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const dbus::utility::DBusPropertiesMap& properties,
    const nlohmann::json::json_pointer& jsonPtr)
{
    const uint16_t* moduleManufacturerID = nullptr;
    const uint16_t* moduleProductID = nullptr;
    const uint16_t* subsystemVendorID = nullptr;
    const uint16_t* subsystemDeviceID = nullptr;
    const uint64_t* volatileRegionSizeLimitInKiB = nullptr;
    const uint64_t* pmRegionSizeLimitInKiB = nullptr;
    const uint64_t* volatileSizeInKiB = nullptr;
    const uint64_t* pmSizeInKiB = nullptr;
    const uint64_t* cacheSizeInKB = nullptr;
    const uint64_t* voltaileRegionMaxSizeInKib = nullptr;
    const uint64_t* pmRegionMaxSizeInKiB = nullptr;
    const uint64_t* allocationIncrementInKiB = nullptr;
    const uint64_t* allocationAlignmentInKiB = nullptr;
    const uint64_t* volatileRegionNumberLimit = nullptr;
    const uint64_t* pmRegionNumberLimit = nullptr;
    const uint64_t* spareDeviceCount = nullptr;
    const bool* isSpareDeviceInUse = nullptr;
    const bool* isRankSpareEnabled = nullptr;
    const std::vector<uint32_t>* maxAveragePowerLimitmW = nullptr;
    const bool* configurationLocked = nullptr;
    const std::string* allowedMemoryModes = nullptr;
    const std::string* memoryMedia = nullptr;
    const bool* configurationLockCapable = nullptr;
    const bool* dataLockCapable = nullptr;
    const bool* passphraseCapable = nullptr;
    const uint64_t* maxPassphraseCount = nullptr;
    const uint64_t* passphraseLockLimit = nullptr;

    const bool success = sdbusplus::unpackPropertiesNoThrow(
        dbus_utils::UnpackErrorPrinter(), properties, "ModuleManufacturerID",
        moduleManufacturerID, "ModuleProductID", moduleProductID,
        "SubsystemVendorID", subsystemVendorID, "SubsystemDeviceID",
        subsystemDeviceID, "VolatileRegionSizeLimitInKiB",
        volatileRegionSizeLimitInKiB, "PmRegionSizeLimitInKiB",
        pmRegionSizeLimitInKiB, "VolatileSizeInKiB", volatileSizeInKiB,
        "PmSizeInKiB", pmSizeInKiB, "CacheSizeInKB", cacheSizeInKB,
        "VoltaileRegionMaxSizeInKib", voltaileRegionMaxSizeInKib,
        "PmRegionMaxSizeInKiB", pmRegionMaxSizeInKiB,
        "AllocationIncrementInKiB", allocationIncrementInKiB,
        "AllocationAlignmentInKiB", allocationAlignmentInKiB,
        "VolatileRegionNumberLimit", volatileRegionNumberLimit,
        "PmRegionNumberLimit", pmRegionNumberLimit, "SpareDeviceCount",
        spareDeviceCount, "IsSpareDeviceInUse", isSpareDeviceInUse,
        "IsRankSpareEnabled", isRankSpareEnabled, "MaxAveragePowerLimitmW",
        maxAveragePowerLimitmW, "ConfigurationLocked", configurationLocked,
        "AllowedMemoryModes", allowedMemoryModes, "MemoryMedia", memoryMedia,
        "ConfigurationLockCapable", configurationLockCapable, "DataLockCapable",
        dataLockCapable, "PassphraseCapable", passphraseCapable,
        "MaxPassphraseCount", maxPassphraseCount, "PassphraseLockLimit",
        passphraseLockLimit);

    if (!success)
    {
        messages::internalError(aResp->res);
        return;
    }

    dimmPropToHex(aResp, "ModuleManufacturerID", moduleManufacturerID, jsonPtr);
    dimmPropToHex(aResp, "ModuleProductID", moduleProductID, jsonPtr);
    dimmPropToHex(aResp, "MemorySubsystemControllerManufacturerID",
                  subsystemVendorID, jsonPtr);
    dimmPropToHex(aResp, "MemorySubsystemControllerProductID",
                  subsystemDeviceID, jsonPtr);

    if (volatileRegionSizeLimitInKiB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["VolatileRegionSizeLimitMiB"] =
            (*volatileRegionSizeLimitInKiB) >> 10;
    }

    if (pmRegionSizeLimitInKiB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["PersistentRegionSizeLimitMiB"] =
            (*pmRegionSizeLimitInKiB) >> 10;
    }

    if (volatileSizeInKiB != nullptr)
    {

        aResp->res.jsonValue[jsonPtr]["VolatileSizeMiB"] =
            (*volatileSizeInKiB) >> 10;
    }

    if (pmSizeInKiB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["NonVolatileSizeMiB"] =
            (*pmSizeInKiB) >> 10;
    }

    if (cacheSizeInKB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["CacheSizeMiB"] = (*cacheSizeInKB >> 10);
    }

    if (voltaileRegionMaxSizeInKib != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["VolatileRegionSizeMaxMiB"] =
            (*voltaileRegionMaxSizeInKib) >> 10;
    }

    if (pmRegionMaxSizeInKiB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["PersistentRegionSizeMaxMiB"] =
            (*pmRegionMaxSizeInKiB) >> 10;
    }

    if (allocationIncrementInKiB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["AllocationIncrementMiB"] =
            (*allocationIncrementInKiB) >> 10;
    }

    if (allocationAlignmentInKiB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["AllocationAlignmentMiB"] =
            (*allocationAlignmentInKiB) >> 10;
    }

    if (volatileRegionNumberLimit != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["VolatileRegionNumberLimit"] =
            *volatileRegionNumberLimit;
    }

    if (pmRegionNumberLimit != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["PersistentRegionNumberLimit"] =
            *pmRegionNumberLimit;
    }

    if (spareDeviceCount != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["SpareDeviceCount"] = *spareDeviceCount;
    }

    if (isSpareDeviceInUse != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["IsSpareDeviceEnabled"] =
            *isSpareDeviceInUse;
    }

    if (isRankSpareEnabled != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["IsRankSpareEnabled"] =
            *isRankSpareEnabled;
    }

    if (maxAveragePowerLimitmW != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["MaxTDPMilliWatts"] =
            *maxAveragePowerLimitmW;
    }

    if (configurationLocked != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["ConfigurationLocked"] =
            *configurationLocked;
    }

    if (allowedMemoryModes != nullptr)
    {
        constexpr const std::array<const char*, 3> values{"Volatile", "PMEM",
                                                          "Block"};

        for (const char* v : values)
        {
            if (allowedMemoryModes->ends_with(v))
            {
                aResp->res.jsonValue[jsonPtr]["OperatingMemoryModes"].push_back(
                    v);
                break;
            }
        }
    }

    if (memoryMedia != nullptr)
    {
        constexpr const std::array<const char*, 3> values{"DRAM", "NAND",
                                                          "Intel3DXPoint"};

        for (const char* v : values)
        {
            if (memoryMedia->ends_with(v))
            {
                aResp->res.jsonValue[jsonPtr]["MemoryMedia"].push_back(v);
                break;
            }
        }
    }

    if (configurationLockCapable != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["SecurityCapabilities"]
                            ["ConfigurationLockCapable"] =
            *configurationLockCapable;
    }

    if (dataLockCapable != nullptr)
    {
        aResp->res
            .jsonValue[jsonPtr]["SecurityCapabilities"]["DataLockCapable"] =
            *dataLockCapable;
    }

    if (passphraseCapable != nullptr)
    {
        aResp->res
            .jsonValue[jsonPtr]["SecurityCapabilities"]["PassphraseCapable"] =
            *passphraseCapable;
    }

    if (maxPassphraseCount != nullptr)
    {
        aResp->res
            .jsonValue[jsonPtr]["SecurityCapabilities"]["MaxPassphraseCount"] =
            *maxPassphraseCount;
    }

    if (passphraseLockLimit != nullptr)
    {
        aResp->res
            .jsonValue[jsonPtr]["SecurityCapabilities"]["PassphraseLockLimit"] =
            *passphraseLockLimit;
    }
}

inline void
    assembleDimmProperties(std::string_view dimmId,
                           const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const dbus::utility::DBusPropertiesMap& properties,
                           const nlohmann::json::json_pointer& jsonPtr)
{
    aResp->res.jsonValue[jsonPtr]["Id"] = dimmId;
    aResp->res.jsonValue[jsonPtr]["Name"] = "DIMM Slot";
    aResp->res.jsonValue[jsonPtr]["Status"]["State"] = "Enabled";
#ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
    aResp->res.jsonValue[jsonPtr]["Status"]["Health"] = "OK";
#endif // ifndef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
    std::string dimmIdStr{dimmId};
    redfish::conditions_utils::populateServiceConditions(aResp, dimmIdStr);
    const uint16_t* memoryDataWidth = nullptr;
    const size_t* memorySizeInKB = nullptr;
    const std::string* partNumber = nullptr;
    const std::string* serialNumber = nullptr;
    const std::string* manufacturer = nullptr;
    const uint16_t* revisionCode = nullptr;
    const bool* present = nullptr;
    const uint16_t* memoryTotalWidth = nullptr;
    const std::string* ecc = nullptr;
    const std::string* formFactor = nullptr;
    const std::vector<uint16_t>* allowedSpeedsMT = nullptr;
    const uint8_t* memoryAttributes = nullptr;
    const uint16_t* memoryConfiguredSpeedInMhz = nullptr;
    const std::string* memoryType = nullptr;
    const std::string* channel = nullptr;
    const std::string* memoryController = nullptr;
    const std::string* slot = nullptr;
    const std::string* socket = nullptr;
    const std::string* sparePartNumber = nullptr;
    const std::string* model = nullptr;
    const std::string* locationCode = nullptr;
    const std::string* locationType = nullptr;
    const bool* rowMappingFailureState = nullptr;
    const bool* rowMappingPendingState = nullptr;

    const bool success = sdbusplus::unpackPropertiesNoThrow(
        dbus_utils::UnpackErrorPrinter(), properties, "MemoryDataWidth",
        memoryDataWidth, "MemorySizeInKB", memorySizeInKB, "PartNumber",
        partNumber, "SerialNumber", serialNumber, "Manufacturer", manufacturer,
        "RevisionCode", revisionCode, "Present", present, "MemoryTotalWidth",
        memoryTotalWidth, "ECC", ecc, "FormFactor", formFactor,
        "AllowedSpeedsMT", allowedSpeedsMT, "MemoryAttributes",
        memoryAttributes, "MemoryConfiguredSpeedInMhz",
        memoryConfiguredSpeedInMhz, "MemoryType", memoryType, "Channel",
        channel, "MemoryController", memoryController, "Slot", slot, "Socket",
        socket, "SparePartNumber", sparePartNumber, "Model", model,
        "LocationCode", locationCode, "LocationType", locationType,
        "RowRemappingFailureState", rowMappingFailureState,
        "RowRemappingPendingState", rowMappingPendingState);

    if (!success)
    {
        messages::internalError(aResp->res);
        return;
    }

    if (memoryDataWidth != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["DataWidthBits"] = *memoryDataWidth;
    }

    if (memorySizeInKB != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["CapacityMiB"] = (*memorySizeInKB >> 10);
    }

    if (partNumber != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["PartNumber"] = *partNumber;
    }

    if (serialNumber != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["SerialNumber"] = *serialNumber;
    }

    if (manufacturer != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["Manufacturer"] = *manufacturer;
    }

    if (revisionCode != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["FirmwareRevision"] =
            std::to_string(*revisionCode);
    }

    if (present != nullptr && !*present)
    {
        aResp->res.jsonValue[jsonPtr]["Status"]["State"] = "Absent";
    }

    if (memoryTotalWidth != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["BusWidthBits"] = *memoryTotalWidth;
    }

    if (ecc != nullptr)
    {
        constexpr const std::array<const char*, 4> values{
            "NoECC", "SingleBitECC", "MultiBitECC", "AddressParity"};

        for (const char* v : values)
        {
            if (ecc->ends_with(v))
            {
                aResp->res.jsonValue[jsonPtr]["ErrorCorrection"] = v;
                break;
            }
        }
    }

    if (formFactor != nullptr)
    {
        constexpr const std::array<const char*, 11> values{
            "RDIMM",       "UDIMM",       "SO_DIMM",      "LRDIMM",
            "Mini_RDIMM",  "Mini_UDIMM",  "SO_RDIMM_72b", "SO_UDIMM_72b",
            "SO_DIMM_16b", "SO_DIMM_32b", "Die"};

        for (const char* v : values)
        {
            if (formFactor->ends_with(v))
            {
                aResp->res.jsonValue[jsonPtr]["BaseModuleType"] = v;
                break;
            }
        }
    }

    if (allowedSpeedsMT != nullptr)
    {
        nlohmann::json& jValue =
            aResp->res.jsonValue[jsonPtr]["AllowedSpeedsMHz"];
        jValue = nlohmann::json::array();
        for (uint16_t subVal : *allowedSpeedsMT)
        {
            jValue.push_back(subVal);
        }
    }

    if (memoryAttributes != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["RankCount"] =
            static_cast<uint64_t>(*memoryAttributes);
    }

    if (memoryConfiguredSpeedInMhz != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["OperatingSpeedMhz"] =
            *memoryConfiguredSpeedInMhz;
    }

    if (memoryType != nullptr)
    {
        std::string memoryDeviceType =
            translateMemoryTypeToRedfish(*memoryType);
        // Values like "Unknown" or "Other" will return empty
        // so just leave off
        if (!memoryDeviceType.empty())
        {
            aResp->res.jsonValue[jsonPtr]["MemoryDeviceType"] =
                memoryDeviceType;
        }
        if (memoryType->find("DDR") != std::string::npos ||
            (boost::ends_with(*memoryType, "HBM")) ||
            (boost::ends_with(*memoryType, "HBM2")))
        {
            aResp->res.jsonValue[jsonPtr]["MemoryType"] = "DRAM";
        }
        else if (memoryType->ends_with("Logical"))
        {
            aResp->res.jsonValue[jsonPtr]["MemoryType"] = "IntelOptane";
        }
    }

    if (channel != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["MemoryLocation"]["Channel"] = *channel;
    }

    if (memoryController != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["MemoryLocation"]["MemoryController"] =
            *memoryController;
    }

    if (slot != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["MemoryLocation"]["Slot"] = *slot;
    }

    if (socket != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["MemoryLocation"]["Socket"] = *socket;
    }

    if (sparePartNumber != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["SparePartNumber"] = *sparePartNumber;
    }

    if (model != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["Model"] = *model;
    }

    if (locationCode != nullptr)
    {
        aResp->res
            .jsonValue[jsonPtr]["Location"]["PartLocation"]["ServiceLabel"] =
            *locationCode;
    }
    if (locationType != nullptr)
    {
        aResp->res
            .jsonValue[jsonPtr]["Location"]["PartLocation"]["LocationType"] =
            redfish::dbus_utils::toLocationType(*locationType);
    }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
    if (rowMappingFailureState != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["Oem"]["Nvidia"]["RowRemappingFailed"] =
            *rowMappingFailureState;
    }
    if (rowMappingPendingState != nullptr)
    {
        aResp->res.jsonValue[jsonPtr]["Oem"]["Nvidia"]["RowRemappingPending"] =
            *rowMappingPendingState;
    }
    aResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
        "#NvidiaMemory.v1_0_0.NvidiaMemory";
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

    getPersistentMemoryProperties(aResp, properties, jsonPtr);
}

inline void getDimmDataByService(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& dimmId,
                                 const std::string& service,
                                 const std::string& objPath)
{
#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
    std::shared_ptr<HealthRollup> health = std::make_shared<HealthRollup>(
        objPath, [aResp](const std::string& rootHealth,
                         const std::string& healthRollup) {
            aResp->res.jsonValue["Status"]["Health"] = rootHealth;
            aResp->res.jsonValue["Status"]["HealthRollup"] = healthRollup;
        });
    health->start();
#else  // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

    auto health = std::make_shared<HealthPopulate>(aResp);
    health->selfPath = objPath;
    health->populate();
#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE*/

    BMCWEB_LOG_DEBUG << "Get available system components.";
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, objPath, "",
        [dimmId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }
            assembleDimmProperties(dimmId, aResp, properties, ""_json_pointer);
        });
}

inline void assembleDimmPartitionData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const dbus::utility::DBusPropertiesMap& properties,
    const nlohmann::json::json_pointer& regionPtr)
{
    const std::string* memoryClassification = nullptr;
    const uint64_t* offsetInKiB = nullptr;
    const std::string* partitionId = nullptr;
    const bool* passphraseState = nullptr;
    const uint64_t* sizeInKiB = nullptr;

    const bool success = sdbusplus::unpackPropertiesNoThrow(
        dbus_utils::UnpackErrorPrinter(), properties, "MemoryClassification",
        memoryClassification, "OffsetInKiB", offsetInKiB, "PartitionId",
        partitionId, "PassphraseState", passphraseState, "SizeInKiB",
        sizeInKiB);

    if (!success)
    {
        messages::internalError(aResp->res);
        return;
    }

    nlohmann::json::object_t partition;

    if (memoryClassification != nullptr)
    {
        partition["MemoryClassification"] = *memoryClassification;
    }

    if (offsetInKiB != nullptr)
    {
        partition["OffsetMiB"] = (*offsetInKiB >> 10);
    }

    if (partitionId != nullptr)
    {
        partition["RegionId"] = *partitionId;
    }

    if (passphraseState != nullptr)
    {
        partition["PassphraseEnabled"] = *passphraseState;
    }

    if (sizeInKiB != nullptr)
    {
        partition["SizeMiB"] = (*sizeInKiB >> 10);
    }

    aResp->res.jsonValue[regionPtr].emplace_back(std::move(partition));
}

inline void getDimmPartitionData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& service,
                                 const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Inventory.Item.PersistentMemory.Partition",
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);

                return;
            }
            nlohmann::json::json_pointer regionPtr = "/Regions"_json_pointer;
            assembleDimmPartitionData(aResp, properties, regionPtr);
        }

    );
}

/**
 * @brief Fill out links association to parent processor by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getMemoryProcessorLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get parent processor link";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no processors = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                return;
            }
            nlohmann::json& linksArray =
                aResp->res.jsonValue["Links"]["Processors"];
            linksArray = nlohmann::json::array();
            for (const std::string& processorPath : *data)
            {
                sdbusplus::message::object_path objectPath(processorPath);
                std::string processorName = objectPath.filename();
                if (processorName.empty())
                {
                    messages::internalError(aResp->res);
                    return;
                }
                linksArray.push_back(
                    {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                   "/Processors/" +
                                       processorName}});
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_processor",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Fill out links association to parent chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getMemoryChassisLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                         const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get parent chassis link";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no chassis = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr && data->size() > 1)
            {
                // Memory must have single parent chassis
                return;
            }
            const std::string& chassisPath = data->front();
            sdbusplus::message::object_path objectPath(chassisPath);
            std::string chassisName = objectPath.filename();
            if (chassisName.empty())
            {
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["Links"]["Chassis"] = {
                {"@odata.id", "/redfish/v1/Chassis/" + chassisName}};
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getDimmData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                        const std::string& dimmId)
{
    BMCWEB_LOG_DEBUG << "Get available system dimm resources.";
    crow::connections::systemBus->async_method_call(
        [dimmId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);

                return;
            }
            bool found = false;
            for (const auto& [rawPath, object] : subtree)
            {
                sdbusplus::message::object_path path(rawPath);
                for (const auto& [service, interfaces] : object)
                {
                    for (const auto& interface : interfaces)
                    {
                        if (interface ==
                                "xyz.openbmc_project.Inventory.Item.Dimm" &&
                            path.filename() == dimmId)
                        {
                            getDimmDataByService(aResp, dimmId, service,
                                                 rawPath);
                            found = true;
                            // Link association to parent processor
                            getMemoryProcessorLink(aResp, path);
                            // Link association to parent chassis
                            getMemoryChassisLink(aResp, path);
                        }

                        // partitions are separate as there can be multiple
                        // per
                        // device, i.e.
                        // /xyz/openbmc_project/Inventory/Item/Dimm1/Partition1
                        // /xyz/openbmc_project/Inventory/Item/Dimm1/Partition2
                        if (interface ==
                                "xyz.openbmc_project.Inventory.Item.PersistentMemory.Partition" &&
                            path.parent_path().filename() == dimmId)
                        {
                            getDimmPartitionData(aResp, service, rawPath);
                        }
                    }
                }
            }
            // Object not found
            if (!found)
            {
                messages::resourceNotFound(aResp->res, "Memory", dimmId);
                return;
            }
            // Set @odata only if object is found
            aResp->res.jsonValue["@odata.type"] = "#Memory.v1_11_0.Memory";
            aResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory/" + dimmId;
            std::string memoryMetricsURI =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory/";
            memoryMetricsURI += dimmId;
            std::string environmentMetricsURI = memoryMetricsURI;
            memoryMetricsURI += "/MemoryMetrics";
            aResp->res.jsonValue["Metrics"]["@odata.id"] = memoryMetricsURI;
            environmentMetricsURI += "/EnvironmentMetrics";
            aResp->res.jsonValue["EnvironmentMetrics"]["@odata.id"] =
                environmentMetricsURI;

            return;
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Dimm",
            "xyz.openbmc_project.Inventory.Item.PersistentMemory.Partition"});
}

inline void requestRoutesMemoryCollection(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Memory/")
        .privileges(redfish::privileges::getMemoryCollection)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& systemName) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                if (systemName != PLATFORMSYSTEMID)
                {
                    messages::resourceNotFound(asyncResp->res, "ComputerSystem",
                                               systemName);
                    return;
                }

                asyncResp->res.jsonValue["@odata.type"] =
                    "#MemoryCollection.MemoryCollection";
                asyncResp->res.jsonValue["Name"] = "Memory Module Collection";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory";

                collection_util::getCollectionMembers(
                    asyncResp,
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory",
                    {"xyz.openbmc_project.Inventory.Item.Dimm"});
            });
}

inline void requestRoutesMemory(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Memory/<str>/")
        .privileges(redfish::privileges::getMemory)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& systemName, const std::string& dimmId) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                if (systemName != PLATFORMSYSTEMID)
                {
                    messages::resourceNotFound(asyncResp->res, "ComputerSystem",
                                               systemName);
                    return;
                }

                getDimmData(asyncResp, dimmId);
            });
}

inline void getMemoryDataByService(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                   const std::string& service,
                                   const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get memory metrics data.";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const DimmProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "MemoryConfiguredSpeedInMhz")
                {
                    const uint16_t* value =
                        std::get_if<uint16_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["OperatingSpeedMHz"] = *value;
                }
                else if (property.first == "Utilization")
                {
                    const double* value = std::get_if<double>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["BandwidthPercent"] = *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Dimm");
}

inline void getMemoryECCData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                             const std::string& service,
                             const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get memory ecc data.";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const DimmProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "ceCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res
                        .jsonValue["LifeTime"]["CorrectableECCErrorCount"] =
                        *value;
                }
                else if (property.first == "ueCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res
                        .jsonValue["LifeTime"]["UncorrectableECCErrorCount"] =
                        *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Memory.MemoryECC");
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
inline void getMemoryRowRemappings(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                   const std::string& service,
                                   const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get memory row remapping counts.";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const DimmProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "ceRowRemappingCount")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["Oem"]["Nvidia"]["RowRemapping"]
                                        ["CorrectableRowRemappingCount"] =
                        *value;
                }
                else if (property.first == "ueRowRemappingCount")
                {
                    const uint32_t* value =
                        std::get_if<uint32_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["Oem"]["Nvidia"]["RowRemapping"]
                                        ["UncorrectableRowRemappingCount"] =
                        *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.MemoryRowRemapping");
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getMemoryMetricsData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& dimmId)
{
    BMCWEB_LOG_DEBUG << "Get available system memory resources.";
    crow::connections::systemBus->async_method_call(
        [dimmId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << " DBUS response error";
                messages::internalError(aResp->res);

                return;
            }
            for (const auto& [path, object] : subtree)
            {
                if (!boost::ends_with(path, dimmId))
                {
                    continue;
                }
                std::string memoryMetricsURI =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Memory/";
                memoryMetricsURI += dimmId;
                memoryMetricsURI += "/MemoryMetrics";
                aResp->res.jsonValue["@odata.type"] =
                    "#MemoryMetrics.v1_4_1.MemoryMetrics";
                aResp->res.jsonValue["@odata.id"] = memoryMetricsURI;
                aResp->res.jsonValue["Id"] = "MemoryMetrics";
                aResp->res.jsonValue["Name"] = dimmId + " Memory Metrics";
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                aResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaMemoryMetrics.v1_0_0.NvidiaMemoryMetrics";
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                for (const auto& [service, interfaces] : object)
                {
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Inventory.Item.Dimm") !=
                        interfaces.end())
                    {
                        getMemoryDataByService(aResp, service, path);
                    }
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "xyz.openbmc_project.Memory.MemoryECC") !=
                        interfaces.end())
                    {
                        getMemoryECCData(aResp, service, path);
                    }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                    if (std::find(interfaces.begin(), interfaces.end(),
                                  "com.nvidia.MemoryRowRemapping") !=
                        interfaces.end())
                    {
                        getMemoryRowRemappings(aResp, service, path);
                    }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                }
                return;
            }
            // Object not found
            messages::resourceNotFound(aResp->res, "#Memory.v1_11_0.Memory",
                                       dimmId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Dimm"});
}

inline void requestRoutesMemoryMetrics(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Memory/<str>/MemoryMetrics")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& dimmId) {
                getMemoryMetricsData(asyncResp, dimmId);
            });
}

} // namespace redfish
