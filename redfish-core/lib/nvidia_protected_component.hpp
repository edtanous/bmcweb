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

#include "utils/dbus_utils.hpp"

namespace redfish::firmware_info
{

static constexpr std::array<std::string_view, 3> interfaces = {
    "xyz.openbmc_project.Software.Slot",
    "xyz.openbmc_project.Software.BuildType",
    "xyz.openbmc_project.Software.State"};

static const std::string chassisDbusPath =
    "/xyz/openbmc_project/inventory/system/chassis/";

static inline std::string getStrBeforeLastDot(const std::string& text)
{
    size_t lastDot = text.find_last_of('.');
    if (lastDot != std::string::npos)
    {
        return text.substr(lastDot + 1);
    }
    else
    {
        return text;
    }
}

static inline bool stringToInt(const std::string& str, int& number)
{
    try
    {
        size_t pos;
        number = std::stoi(str, &pos);
        if (pos != str.size())
        {
            return false;
        }
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
    return true;
}

static inline std::string removeERoTFromStr(const std::string& input)
{
    size_t first_underscore = input.find('_');
    size_t last_underscore = input.rfind('_');
    size_t second_last_underscore = input.rfind('_', last_underscore - 1);
    if (second_last_underscore != std::string::npos &&
        last_underscore != std::string::npos &&
        first_underscore != std::string::npos)
    {
        return input.substr(0, first_underscore + 1) +
               input.substr(second_last_underscore + 1);
    }
    return input;
}

inline void
    updateSlotProperties(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& service,
                         const std::string& objectPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& properties) {
        if (ec)
        {
            if (ec == boost::system::errc::host_unreachable)
            {
                // Service not available, no error, just don't
                // return chassis state info
                BMCWEB_LOG_ERROR("Service not available {}", ec);
                return;
            }
            BMCWEB_LOG_ERROR("DBUS response error {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
        for (const auto& [key, val] : properties)
        {
            if (key == "SlotId")
            {
                if (const uint8_t* value = std::get_if<uint8_t>(&val))
                {
                    asyncResp->res.jsonValue["SlotId"] = *value;
                }
                else
                {
                    BMCWEB_LOG_ERROR("Null value returned for SlotId");
                }
            }
            else if (key == "FirmwareComparisonNumber")
            {
                if (const uint32_t* value = std::get_if<uint32_t>(&val))
                {
                    asyncResp->res.jsonValue["FirmwareComparisonNumber"] =
                        *value;
                }
                else
                {
                    BMCWEB_LOG_ERROR(
                        "Null value returned for FirmwareComparisonNumber");
                }
            }
            else if (key == "ExtendedVersion")
            {
                if (const std::string* value = std::get_if<std::string>(&val))
                {
                    asyncResp->res.jsonValue["Version"] = *value;
                }
                else
                {
                    BMCWEB_LOG_ERROR("Null value returned for {}", key);
                }
            }
            else if (key == "BuildType")
            {
                if (const std::string* value = std::get_if<std::string>(&val))
                {
                    asyncResp->res.jsonValue["BuildType"] =
                        getStrBeforeLastDot(*value);
                }
                else
                {
                    BMCWEB_LOG_ERROR("Null value returned for {}", key);
                }
            }
            else if (key == "State")
            {
                if (const std::string* value = std::get_if<std::string>(&val))
                {
                    asyncResp->res.jsonValue["FirmwareState"] =
                        getStrBeforeLastDot(*value);
                }
                else
                {
                    BMCWEB_LOG_ERROR("Null value returned for {}", key);
                }
            }
            else if (key == "WriteProtected")
            {
                if (const bool* value = std::get_if<bool>(&val))
                {
                    asyncResp->res.jsonValue["WriteProtected"] = *value;
                }
                else
                {
                    BMCWEB_LOG_ERROR("Null value returned for {}", key);
                }
            }
        }
    },
        service, objectPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

inline void handleNvidiaRoTImageSlot(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId, const std::string& fwTypeStr,
    const std::string& slotNumStr)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    int slotNum;
    if (!stringToInt(slotNumStr, slotNum) || (slotNum > 1))
    {
        messages::resourceNotFound(asyncResp->res, "SlotNumber", slotNumStr);
        return;
    }

    dbus::utility::getSubTree(
        chassisDbusPath + chassisId, 0, interfaces,
        [chassisId, slotNum, fwTypeStr, slotNumStr,
         asyncResp](const boost::system::error_code& ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus error: {}, {}", ec, ec.message());
            messages::internalError(asyncResp->res);
            return;
        }
        auto componentId = fwTypeStr != "Self" ? removeERoTFromStr(chassisId)
                                               : "Self";
        if (componentId.find(fwTypeStr) == std::string::npos)
        {
            messages::resourceNotFound(asyncResp->res, "NvidiaRoTImageSlot",
                                       fwTypeStr);
            return;
        }
        for (const auto& [objectPath, serviceMap] : subtree)
        {
            for (const auto& [service, interfaces] : serviceMap)
            {
                sdbusplus::asio::getAllProperties(
                    *crow::connections::systemBus, service, objectPath,
                    "xyz.openbmc_project.Software.Slot",
                    [asyncResp, service, objectPath, chassisId, slotNum,
                     slotNumStr,
                     fwTypeStr](const boost::system::error_code& ec,
                                const dbus::utility::DBusPropertiesMap&
                                    propertiesList) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR("DBUS response error {}", ec);
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    const auto slotType =
                        (fwTypeStr == "Self")
                            ? "xyz.openbmc_project.Software.Slot.FirmwareType.EC"
                            : "xyz.openbmc_project.Software.Slot.FirmwareType.AP";
                    std::optional<uint8_t> slotId;
                    std::optional<bool> isActive;
                    std::optional<std::string> fwType;
                    const bool success = sdbusplus::unpackPropertiesNoThrow(
                        dbus_utils::UnpackErrorPrinter(), propertiesList,
                        "SlotId", slotId, "IsActive", isActive, "Type", fwType);
                    if (!success)
                    {
                        BMCWEB_LOG_ERROR("Unpack Slot properites error");
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    if ((fwType && *fwType == slotType) &&
                        (slotId && *slotId == slotNum))
                    {
                        asyncResp->res.jsonValue["Name"] =
                            chassisId + " RoTProtectedComponent " + fwTypeStr +
                            " ImageSlot " + slotNumStr;
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#NvidiaRoTImageSlot.v1_0_0.NvidiaRoTImageSlot";
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisId +
                            "/NvidiaRoT/RoTProtectedComponents/" + fwTypeStr +
                            "/ImageSlots/" + slotNumStr;
                        updateSlotProperties(asyncResp, service, objectPath);
                    }
                });
            }
        }
    });
}

inline void updateProtectedComponentLink(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    dbus::utility::getSubTreePaths(
        chassisDbusPath + chassisId, 0, interfaces,
        [chassisId, asyncResp](
            const boost::system::error_code& ec,
            const dbus::utility::MapperGetSubTreePathsResponse& subtreePaths) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Service not available {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
        if (subtreePaths.size() > 0)
        {
            asyncResp->res
                .jsonValue["Oem"]["Nvidia"]["RoTProtectedComponents"] = {
                {"@odata.id",
                 boost::urls::format("/redfish/v1/Chassis/{}/Oem/NvidiaRoT/"
                                     "RoTProtectedComponents",
                                     chassisId)}};
        }
    });
}

inline void handleNvidiaRoTProtectedComponentCollection(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    dbus::utility::getSubTree(
        chassisDbusPath + chassisId, 0, interfaces,
        [chassisId,
         asyncResp](const boost::system::error_code& ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
        if (ec)
        {
            if (ec == boost::system::errc::host_unreachable)
            {
                // Service not available, no error, just don't
                // return chassis state info
                BMCWEB_LOG_ERROR("Service not available {}", ec);
                messages::internalError(asyncResp->res);
                return;
            }
            BMCWEB_LOG_ERROR("D-Bus error: {}, {}", ec, ec.message());
            messages::resourceNotFound(asyncResp->res,
                                       "NvidiaRoTProtectedComponentCollection",
                                       chassisId);
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#NvidiaRoTProtectedComponentCollection.NvidiaRoTProtectedComponentCollection";
        asyncResp->res.jsonValue["Name"] = chassisId +
                                           " RoTProtectedComponent Collection";
        asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
        for (const auto& [objectPath, serviceMap] : subtree)
        {
            for (const auto& [service, interfaces] : serviceMap)
            {
                sdbusplus::asio::getAllProperties(
                    *crow::connections::systemBus, service, objectPath,
                    "xyz.openbmc_project.Software.Slot",
                    [asyncResp, objectPath,
                     chassisId](const boost::system::error_code& ec,
                                const dbus::utility::DBusPropertiesMap&
                                    propertiesList) {
                    if (ec)
                    {
                        if (ec == boost::system::errc::host_unreachable)
                        {
                            // Service not available, no error, just don't
                            // return chassis state info
                            BMCWEB_LOG_ERROR("Service not available {}", ec);
                            return;
                        }
                        BMCWEB_LOG_ERROR("DBUS response error {}", ec);
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    std::optional<uint8_t> slotID;
                    std::optional<bool> isActive;
                    std::optional<std::string> fwType;
                    const bool success = sdbusplus::unpackPropertiesNoThrow(
                        dbus_utils::UnpackErrorPrinter(), propertiesList,
                        "SlotId", slotID, "IsActive", isActive, "Type", fwType);
                    if (!success)
                    {
                        BMCWEB_LOG_ERROR("Unpack Slot properites error");
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    if (slotID && fwType)
                    {
                        if (*slotID == 0 &&
                            *fwType ==
                                "xyz.openbmc_project.Software.Slot.FirmwareType.EC")
                        {
                            asyncResp->res.jsonValue["Members"].push_back(
                                {{"@odata.id",
                                  "/redfish/v1/Chassis/" + chassisId +
                                      "/Oem/NvidiaRoT/RoTProtectedComponents/Self"}});
                            asyncResp->res.jsonValue["Members@odata.count"] =
                                asyncResp->res.jsonValue["Members"].size();
                        }
                        else if (
                            *slotID == 0 &&
                            fwType ==
                                "xyz.openbmc_project.Software.Slot.FirmwareType.AP")
                        {
                            asyncResp->res.jsonValue["Members"].push_back(
                                {{"@odata.id",
                                  "/redfish/v1/Chassis/" + chassisId +
                                      "/Oem/NvidiaRoT/RoTProtectedComponents/" +
                                      removeERoTFromStr(chassisId)}});
                            asyncResp->res.jsonValue["Members@odata.count"] =
                                asyncResp->res.jsonValue["Members"].size();
                        }
                    }
                });
                break;
            }
        }
    });
}

inline void handleNvidiaRoTImageSlotCollection(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId, const std::string& fwTypeStr)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    dbus::utility::getSubTree(
        chassisDbusPath + chassisId, 0, interfaces,
        [chassisId, fwTypeStr,
         asyncResp](const boost::system::error_code& ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus error: {}, {}", ec, ec.message());
            messages::internalError(asyncResp->res);
            return;
        }
        auto componentId = fwTypeStr != "Self" ? removeERoTFromStr(chassisId)
                                               : "Self";
        if (componentId.find(fwTypeStr) == std::string::npos)
        {
            messages::resourceNotFound(
                asyncResp->res, "NvidiaRoTImageSlotCollection", fwTypeStr);
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#NvidiaRoTImageSlotCollection.NvidiaRoTImageSlotCollection";
        asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
        asyncResp->res.jsonValue["@odata.id"] =
            "/redfish/v1/Chassis/" + chassisId +
            "/NvidiaRoT/RoTProtectedComponents/" + fwTypeStr + "/ImageSlots";
        asyncResp->res.jsonValue["Name"] =
            chassisId + " RoTProtectedComponent " + fwTypeStr + " ImageSlot";
        for (const auto& [objectPath, serviceMap] : subtree)
        {
            for (const auto& [service, interfaces] : serviceMap)
            {
                sdbusplus::asio::getAllProperties(
                    *crow::connections::systemBus, service, objectPath,
                    "xyz.openbmc_project.Software.Slot",
                    [asyncResp, objectPath, chassisId,
                     fwTypeStr](const boost::system::error_code& ec,
                                const dbus::utility::DBusPropertiesMap&
                                    propertiesList) {
                    if (ec)
                    {
                        if (ec == boost::system::errc::host_unreachable)
                        {
                            // Service not available, no error, just don't
                            // return chassis state info
                            BMCWEB_LOG_ERROR("Service not available {}", ec);
                            return;
                        }
                        BMCWEB_LOG_ERROR("DBUS response error {}", ec);
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    const auto slotType =
                        (fwTypeStr == "Self")
                            ? "xyz.openbmc_project.Software.Slot.FirmwareType.EC"
                            : "xyz.openbmc_project.Software.Slot.FirmwareType.AP";
                    std::optional<uint8_t> slotID;
                    std::optional<bool> isActive;
                    std::optional<std::string> fwType;
                    const bool success = sdbusplus::unpackPropertiesNoThrow(
                        dbus_utils::UnpackErrorPrinter(), propertiesList,
                        "SlotId", slotID, "IsActive", isActive, "Type", fwType);
                    if (!success)
                    {
                        BMCWEB_LOG_ERROR("Unpack Slot properites error");
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    if (fwType && *fwType == slotType)
                    {
                        auto memberId = boost::urls::format(
                            "/redfish/v1/Chassis/{}/Oem/NvidiaRoT/RoTProtectedComponents/{}/ImageSlots/{}",
                            chassisId, fwTypeStr, std::to_string(*slotID));
                        asyncResp->res.jsonValue["Members"].push_back(
                            {{"@odata.id", memberId}});
                        asyncResp->res.jsonValue["Members@odata.count"] =
                            asyncResp->res.jsonValue["Members"].size();
                    }
                });
                break;
            }
        }
    });
}

inline void handleNvidiaRoTProtectedComponent(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId, const std::string& fwTypeStr)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    dbus::utility::getSubTree(
        chassisDbusPath + chassisId, 0, interfaces,
        [chassisId, fwTypeStr, asyncResp](
            const boost::system::error_code& ec,
            [[maybe_unused]] const dbus::utility::MapperGetSubTreeResponse&
                subtree) {
        if (ec)
        {
            if (ec == boost::system::errc::host_unreachable)
            {
                // Service not available, no error, just don't
                // return chassis state info
                BMCWEB_LOG_ERROR("Service not available {}", ec);
                messages::internalError(asyncResp->res);
                return;
            }
            BMCWEB_LOG_ERROR("D-Bus error: {}, {}", ec, ec.message());
            messages::resourceNotFound(
                asyncResp->res, "NvidiaRoTProtectedComponent", fwTypeStr);
            return;
        }
        auto componentId = fwTypeStr != "Self" ? removeERoTFromStr(chassisId)
                                               : "Self";
        if (componentId.find(fwTypeStr) == std::string::npos)
        {
            messages::resourceNotFound(
                asyncResp->res, "NvidiaRoTProtectedComponent", fwTypeStr);
            return;
        }
        asyncResp->res.jsonValue["@odata.id"] = boost::urls::format(
            "/redfish/v1/Chassis/{}/Oem/NvidiaRoT/RoTProtectedComponents/{}",
            chassisId, componentId);
        asyncResp->res.jsonValue["@odata.type"] =
            "#NvidiaRoTProtectedComponent.v1_0_0.NvidiaRoTProtectedComponent";
        asyncResp->res.jsonValue["Name"] =
            chassisId + " RoTProtectedComponent " + fwTypeStr;
        asyncResp->res.jsonValue["Id"] = fwTypeStr;
        asyncResp->res.jsonValue["RoTProtectedComponentType"] =
            fwTypeStr == "Self" ? "Self" : "AP";
        auto slotUrl = boost::urls::format(
            "/redfish/v1/Chassis/{}/Oem/NvidiaRoT/RoTProtectedComponents/{}/ImageSlots",
            chassisId, componentId);
        asyncResp->res.jsonValue["ImageSlots"] = {{"@odata.id", slotUrl}};
    });
}
} // namespace redfish::firmware_info
