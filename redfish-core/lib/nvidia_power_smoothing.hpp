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

#include "bmcweb_config.h"

#include "app.hpp"
#include "dbus_utility.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "utils/collection.hpp"
#include "utils/dbus_utils.hpp"
#include "utils/hex_utils.hpp"
#include "utils/json_utils.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/format.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <utils/conditions_utils.hpp>

#include <array>
#include <string_view>
namespace redfish
{
using DbusProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

inline void getProcessorPowerSmoothingControlData(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& service,
    const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get processor smoothing control data.");
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const DbusProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            if (property.first == "PowerSmoothingEnable")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("PowerSmoothingEnable nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["Enabled"] = *value;
            }
            else if (property.first == "ImmediateRampDownEnabled")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("ImmediateRampDownEnabled nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["ImmediateRampDown"] = *value;
            }
            else if (property.first == "CurrentTempSetting")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("CurrentTempSetting nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["TMPWatts"] = *value;
            }
            else if (property.first == "CurrentTempFloorSetting")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("CurrentTempFloorSetting nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["TMPFloorWatts"] = *value;
            }
            else if (property.first == "MaxAllowedTmpFloorPercent")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("MaxAllowedTmpFloorPercent nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["MaxAllowedTMPFloorPercent"] = *value;
            }
            else if (property.first == "MinAllowedTmpFloorPercent")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("MinAllowedTmpFloorPercent nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["MinAllowedTMPFloorPercent"] = *value;
            }
            else if (property.first == "LifeTimeRemaining")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("LifeTimeRemaining nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RemainingLifetimeCircuitryPercent"] =
                    *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.Cpu.PowerSmoothing");
}

inline void getProcessorCurrentProfileData(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& service,
    const std::string& objPath, const std::string& presetProfileURI)
{
    BMCWEB_LOG_DEBUG("Get processor current profile data.");
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)},
         presetProfileURI](const boost::system::error_code ec,
                           const DbusProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            if (property.first == "RampDownHysteresis")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampDownHysteresis nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampDownHysteresisSeconds"] = *value;
            }
            else if (property.first == "RampDownHysteresisApplied")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampDownHysteresisApplied nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["AdminOverrideActiveMask"]
                                    ["RampDownHysteresisSecondsApplied"] =
                    *value;
            }
            else if (property.first == "RampDownRate")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampDownRate nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampDownWattsPerSecond"] = *value;
            }
            else if (property.first == "RampDownRateApplied")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampDownRateApplied nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["AdminOverrideActiveMask"]
                                    ["RampDownWattsPerSecondApplied"] = *value;
            }
            else if (property.first == "RampUpRate")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampUpRate nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampUpWattsPerSecond"] = *value;
            }
            else if (property.first == "RampUpRateApplied")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampUpRateApplied nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["AdminOverrideActiveMask"]
                                    ["RampUpWattsPerSecondApplied"] = *value;
            }
            else if (property.first == "TMPFloorPecent")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("TMPFloorPecent nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["TMPFloorPercent"] = *value;
            }
            else if (property.first == "TMPFloorPecentApplied")
            {
                const bool* value = std::get_if<bool>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("TMPFloorPecentApplied nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["AdminOverrideActiveMask"]
                                    ["TMPFloorPercentApplied"] = *value;
            }
            else if (property.first == "AppliedProfilePath")
            {
                const sdbusplus::message::object_path* value =
                    std::get_if<sdbusplus::message::object_path>(
                        &property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("AppliedPresetProfile nullptr");
                    messages::internalError(aResp->res);
                    return;
                }

                std::string appliedProfile = presetProfileURI;
                appliedProfile += "/";
                appliedProfile += value->filename();
                aResp->res.jsonValue["AppliedPresetProfile"]["@odata.id"] =
                    appliedProfile;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.Cpu.CurrentPowerProfile");
}

inline void
    getProcessorPowerSmoothingData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                   const std::string& processorId)
{
    BMCWEB_LOG_DEBUG("Get available system processor resource");
    crow::connections::systemBus->async_method_call(
        [processorId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);

            return;
        }
        for (const auto& [path, object] : subtree)
        {
            if (!boost::ends_with(path, processorId))
            {
                continue;
            }
            std::string pwrSmoothingURI =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
            pwrSmoothingURI += processorId;
            pwrSmoothingURI += "/Oem/Nvidia/PowerSmoothing";
            aResp->res.jsonValue["@odata.type"] =
                "#NvidiaPowerSmoothing.v1_0_0.NvidiaPowerSmoothing";
            aResp->res.jsonValue["@odata.id"] = pwrSmoothingURI;
            aResp->res.jsonValue["Id"] = "PowerSmoothing";
            aResp->res.jsonValue["Name"] = processorId + " Power Smoothing";

            std::string presetProfileURI = pwrSmoothingURI;
            presetProfileURI += "/PresetProfiles";
            aResp->res.jsonValue["PresetProfiles"]["@odata.id"] =
                presetProfileURI;

            std::string adminOverrideProfileURI = pwrSmoothingURI;
            adminOverrideProfileURI += "/AdminOverrideProfile";
            aResp->res.jsonValue["AdminOverrideProfile"]["@odata.id"] =
                adminOverrideProfileURI;

            for (const auto& [service, interfaces] : object)
            {
                if (std::find(interfaces.begin(), interfaces.end(),
                              "com.nvidia.Cpu.CurrentPowerProfile") !=
                    interfaces.end())
                {
                    getProcessorCurrentProfileData(aResp, service, path,
                                                   presetProfileURI);
                }
                if (std::find(interfaces.begin(), interfaces.end(),
                              "com.nvidia.Cpu.PowerSmoothing") !=
                    interfaces.end())
                {
                    getProcessorPowerSmoothingControlData(aResp, service, path);
                }
            }
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaPowerSmoothing.v1_0_0.NvidiaPowerSmoothing",
            processorId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.Cpu"});
}

inline void requestRoutesProcessorPowerSmoothing(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
                      "/Processors/<str>/Oem/Nvidia/PowerSmoothing")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& processorId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        getProcessorPowerSmoothingData(asyncResp, processorId);
    });
}

inline void getAdminProfileData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                const std::string& service,
                                const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get processor current profile data.");
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const DbusProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            if (property.first == "RampDownHysteresis")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampDownHysteresis nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampDownHysteresisSeconds"] = *value;
            }
            else if (property.first == "RampDownRate")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampDownRate nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampDownWattsPerSecond"] = *value;
            }
            else if (property.first == "RampUpRate")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("RampUpRate nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampUpWattsPerSecond"] = *value;
            }
            else if (property.first == "TMPFloorPecent")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("TMPFloorPecent nullptr");
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["TMPFloorPercent"] = *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.Cpu.AdminPowerProfile");
}

inline void getProcessorPowerSmoothingAdminOverrideData(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& processorId)
{
    BMCWEB_LOG_DEBUG("Get available system processor resource");
    crow::connections::systemBus->async_method_call(
        [processorId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        for (const auto& [path, object] : subtree)
        {
            if (!boost::ends_with(path, processorId))
            {
                continue;
            }
            std::string adminOverrideURI =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
            adminOverrideURI += processorId;
            adminOverrideURI +=
                "/Oem/Nvidia/PowerSmoothing/AdminOverrideProfile";
            aResp->res.jsonValue["@odata.type"] =
                "#NvidiaPowerSmoothingPresetProfile.v1_0_0.NvidiaPowerSmoothingPresetProfile";
            aResp->res.jsonValue["@odata.id"] = adminOverrideURI;
            aResp->res.jsonValue["Id"] = "AdminOverrideProfile";
            aResp->res.jsonValue["Name"] =
                processorId + " PowerSmoothing AdminOverrideProfile";

            crow::connections::systemBus->async_method_call(
                [aResp,
                 processorId](const boost::system::error_code ec2,
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

                for (const std::string& profilePath : *data)
                {
                    sdbusplus::message::object_path objectPath(profilePath);
                    std::string processorName = objectPath.filename();
                    if (processorName.empty())
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    crow::connections::systemBus->async_method_call(
                        [processorId, profilePath, aResp{std::move(aResp)}](
                            const boost::system::error_code ec,
                            const boost::container::flat_map<
                                std::string,
                                boost::container::flat_map<
                                    std::string, std::vector<std::string>>>&
                                subtree) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR("DBUS response error");
                            messages::internalError(aResp->res);

                            return;
                        }
                        for (const auto& [path, object] : subtree)
                        {
                            BMCWEB_LOG_ERROR("DBUS path {}", profilePath);
                            if (path != profilePath)
                            {
                                continue;
                            }
                            for (const auto& [service, interfaces] : object)
                            {
                                if (std::find(
                                        interfaces.begin(), interfaces.end(),
                                        "com.nvidia.Cpu.AdminPowerProfile") !=
                                    interfaces.end())
                                {
                                    getAdminProfileData(aResp, service, path);
                                }
                            }
                        }
                    },
                        "xyz.openbmc_project.ObjectMapper",
                        "/xyz/openbmc_project/object_mapper",
                        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                        "/xyz/openbmc_project/inventory", 0,
                        std::array<const char*, 1>{
                            "com.nvidia.Cpu.AdminPowerProfile"});
                }
            },
                "xyz.openbmc_project.ObjectMapper", path + "/admin_override",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaPowerSmoothing.v1_0_0.NvidiaPowerSmoothing",
            processorId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.Cpu"});
}

inline void requestRoutesProcessorPowerSmoothingAdminProfile(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(
        app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
             "/Processors/<str>/Oem/Nvidia/PowerSmoothing/AdminOverrideProfile")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& processorId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        getProcessorPowerSmoothingAdminOverrideData(asyncResp, processorId);
    });
}

inline void getProfileData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                           const std::string& service,
                           const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get processor current profile data.");
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const DbusProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            if (property.first == "RampDownHysteresis")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampDownHysteresisSeconds"] = *value;
            }
            else if (property.first == "RampDownRate")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampDownWattsPerSecond"] = *value;
            }
            else if (property.first == "RampUpRate")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["RampUpWattsPerSecond"] = *value;
            }
            else if (property.first == "TMPFloorPecent")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["TMPFloorPercent"] = *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.Cpu.PowerProfile");
}

inline void getProcessorPowerSmoothingPresetProfileData(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& processorId,
    const std::string& profileId)
{
    BMCWEB_LOG_DEBUG("Get available system processor resource");
    crow::connections::systemBus->async_method_call(
        [processorId, profileId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);

            return;
        }
        for (const auto& [path, object] : subtree)
        {
            if (!boost::ends_with(path, processorId))
            {
                continue;
            }
            std::string profileURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                     "/Processors/";
            profileURI += processorId;
            profileURI += "/Oem/Nvidia/PowerSmoothing/PresetProfiles/";
            profileURI += profileId;
            aResp->res.jsonValue["@odata.type"] =
                "#NvidiaPowerSmoothingPresetProfile.v1_0_0.NvidiaPowerSmoothingPresetProfile";
            aResp->res.jsonValue["@odata.id"] = profileURI;
            aResp->res.jsonValue["Id"] = "Presetprofile";

            std::string profileName = processorId;
            profileName += " PowerSmoothing PresetProfile ";
            profileName += profileId;
            aResp->res.jsonValue["Name"] = profileName;

            crow::connections::systemBus->async_method_call(
                [aResp, profileId,
                 processorId](const boost::system::error_code ec2,
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
                bool profileExists = false;

                for (const std::string& profilePath : *data)
                {
                    sdbusplus::message::object_path objectPath(profilePath);
                    std::string profileIdOndbus = objectPath.filename();
                    if (profileIdOndbus != profileId)
                    {
                        continue;
                    }
                    profileExists = true;
                    BMCWEB_LOG_ERROR("Profile ID: {}", profileId);
                    crow::connections::systemBus->async_method_call(
                        [processorId, profileId, profilePath,
                         aResp{std::move(aResp)}](
                            const boost::system::error_code ec,
                            const boost::container::flat_map<
                                std::string,
                                boost::container::flat_map<
                                    std::string, std::vector<std::string>>>&
                                subtree) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR("DBUS response error");
                            messages::internalError(aResp->res);

                            return;
                        }
                        for (const auto& [path, object] : subtree)
                        {
                            sdbusplus::message::object_path objectPath(path);
                            BMCWEB_LOG_ERROR("Profile ID as per objectpath: {}",
                                             objectPath.filename());
                            if (objectPath.filename() != profileId)
                            {
                                continue;
                            }
                            for (const auto& [service, interfaces] : object)
                            {
                                if (std::find(interfaces.begin(),
                                              interfaces.end(),
                                              "com.nvidia.Cpu.PowerProfile") !=
                                    interfaces.end())
                                {
                                    getProfileData(aResp, service, path);
                                }
                            }
                        }
                    },
                        "xyz.openbmc_project.ObjectMapper",
                        "/xyz/openbmc_project/object_mapper",
                        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                        "/xyz/openbmc_project/inventory", 0,
                        std::array<const char*, 1>{
                            "com.nvidia.Cpu.PowerProfile"});
                }
                // Object not found
                if (!profileExists)
                {
                    messages::resourceNotFound(
                        aResp->res,
                        "#NvidiaPowerSmoothingPresetProfile.v1_0_0.NvidiaPowerSmoothingPresetProfile",
                        profileId);
                }
            },
                "xyz.openbmc_project.ObjectMapper", path + "/power_profile",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaPowerSmoothing.v1_0_0.NvidiaPowerSmoothing",
            processorId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.Cpu"});
}

inline void requestRoutesProcessorPowerSmoothingPresetProfile(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(
        app, "/redfish/v1/Systems/" PLATFORMSYSTEMID
             "/Processors/<str>/Oem/Nvidia/PowerSmoothing/PresetProfiles/<str>")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& processorId,
                   const std::string& profileId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        getProcessorPowerSmoothingPresetProfileData(asyncResp, processorId,
                                                    profileId);
    });
}

inline void getProcessorPowerSmoothingPresetProfileCollectionData(
    std::shared_ptr<bmcweb::AsyncResp> aResp, const std::string& processorId)
{
    BMCWEB_LOG_DEBUG("Get available system processor resource");
    crow::connections::systemBus->async_method_call(
        [processorId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);

            return;
        }
        for (const auto& [path, object] : subtree)
        {
            if (!boost::ends_with(path, processorId))
            {
                continue;
            }
            std::string profileCollectionURI =
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
            profileCollectionURI += processorId;
            profileCollectionURI += "/Oem/Nvidia/PowerSmoothing/PresetProfiles";
            aResp->res.jsonValue["@odata.type"] =
                "#NvidiaPowerSmoothingPresetProfileCollection.NvidiaPowerSmoothingPresetProfileCollection";
            aResp->res.jsonValue["@odata.id"] = profileCollectionURI;

            std::string name = processorId;
            name += " PowerSmoothing PresetProfile Collection";
            aResp->res.jsonValue["Name"] = name;

            crow::connections::systemBus->async_method_call(
                [aResp, profileCollectionURI,
                 processorId](const boost::system::error_code ec2,
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
                nlohmann::json& addMembers = aResp->res.jsonValue["Members"];
                for (const std::string& profilePath : *data)
                {
                    sdbusplus::message::object_path objectPath(profilePath);
                    std::string profileUri = profileCollectionURI;
                    profileUri += "/";
                    profileUri += objectPath.filename();
                    addMembers.push_back({{"@odata.id", profileUri}});
                }
                aResp->res.jsonValue["Members@odata.count"] = addMembers.size();
            },
                "xyz.openbmc_project.ObjectMapper", path + "/power_profile",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaPowerSmoothing.v1_0_0.NvidiaPowerSmoothing",
            processorId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.Cpu"});
}

inline void
    requestRoutesProcessorPowerSmoothingPresetProfileCollection(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID
                 "/Processors/<str>/Oem/Nvidia/PowerSmoothing/PresetProfiles")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& processorId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        getProcessorPowerSmoothingPresetProfileCollectionData(asyncResp,
                                                              processorId);
    });
}

} // namespace redfish
