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
using OperatingConfigProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * @brief Fill out error injection processor nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       capability  Capability name
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getErrorInjectionCapabilityData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const std::string& capability, const std::string& service,
    const std::string& objPath)

{
    crow::connections::systemBus->async_method_call(
        [aResp, capability](const boost::system::error_code ec,
                            const OperatingConfigProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        auto& json =
            aResp->res.jsonValue["ErrorInjectionCapabilities"][capability];
        for (const auto& property : properties)
        {
            if (property.first == "Supported")
            {
                const bool* supported = std::get_if<bool>(&property.second);
                if (supported == nullptr)
                {
                    BMCWEB_LOG_ERROR("Get Supported property failed");
                    messages::internalError(aResp->res);
                    return;
                }
                json["Supported"] = *supported;
            }
            else if (property.first == "Enabled")
            {
                const bool* enabled = std::get_if<bool>(&property.second);
                if (enabled == nullptr)
                {
                    BMCWEB_LOG_ERROR("Get Enabled property failed");
                    messages::internalError(aResp->res);
                    return;
                }
                json["Enabled"] = *enabled;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.ErrorInjection.ErrorInjectionCapability");
}

/**
 * @brief Fill out error injection nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       baseUri     Redfish base uri
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getErrorInjectionData(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp, const std::string& baseUri,
    const std::string& service, const std::string& objPath)

{
    crow::connections::systemBus->async_method_call(
        [aResp, baseUri, service,
         objPath](const boost::system::error_code ec,
                  const OperatingConfigProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        nlohmann::json& json = aResp->res.jsonValue;
        json["@odata.type"] =
            "#NvidiaErrorInjection.v1_0_0.NvidiaErrorInjection";
        json["@odata.id"] = baseUri + "/Oem/Nvidia/ErrorInjection";
        json["Id"] = "ErrorInjection";
        json["Name"] = baseUri.substr(baseUri.find_last_of('/') + 1) +
                       " Error Injection";
        for (const auto& property : properties)
        {
            if (property.first == "ErrorInjectionModeEnabled")
            {
                const bool* errorInjectionModeEnabled =
                    std::get_if<bool>(&property.second);
                if (errorInjectionModeEnabled == nullptr)
                {
                    BMCWEB_LOG_ERROR(
                        "Get ErrorInjectionModeEnabled property failed");
                    messages::internalError(aResp->res);
                    return;
                }
                json["ErrorInjectionModeEnabled"] = *errorInjectionModeEnabled;
            }
            else if (property.first == "PersistentDataModified")
            {
                const bool* persistentDataModified =
                    std::get_if<bool>(&property.second);
                if (persistentDataModified == nullptr)
                {
                    BMCWEB_LOG_ERROR(
                        "Get PersistentDataModified property failed");
                    messages::internalError(aResp->res);
                    return;
                }
                json["PersistentDataModified"] = *persistentDataModified;
            }
        }
        std::vector<std::string> capabilities = {
            "MemoryErrors", "PCIeErrors", "NVLinkErrors", "ThermalErrors"};
        for (auto& cap : capabilities)
        {
            getErrorInjectionCapabilityData(aResp, cap, service,
                                            objPath + "/" + cap);
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.ErrorInjection.ErrorInjection");
}

inline void getErrorInjectionService(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                     const std::string& path,
                                     const std::string& baseUri)
{
    crow::connections::systemBus->async_method_call(
        [aResp, baseUri, path](const boost::system::error_code ec,
                               const MapperServiceMap& serviceMap) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Error while fetching service for {}", path);
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& [service, interfaces] : serviceMap)
        {
            if (std::find(interfaces.begin(), interfaces.end(),
                          "com.nvidia.ErrorInjection.ErrorInjection") ==
                interfaces.end())
            {
                continue;
            }
            getErrorInjectionData(aResp, baseUri, service, path);
            return;
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 0>());
}

inline void
    getProcessorErrorInjectionData(App& app, const crow::Request& req,
                                   std::shared_ptr<bmcweb::AsyncResp> aResp,
                                   const std::string& processorId)
{
    BMCWEB_LOG_DEBUG("Get available system processor resource");
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }
    crow::connections::systemBus->async_method_call(
        [processorId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& paths) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);

            return;
        }
        for (const auto& path : paths)
        {
            if (!boost::ends_with(path, processorId))
            {
                continue;
            }

            getErrorInjectionService(
                aResp, path + "/ErrorInjection",
                "/redfish/v1/Systems/" +
                    std::string(BMCWEB_REDFISH_SYSTEM_URI_NAME) +
                    "/Processors/" + processorId);
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaErrorInjection.v1_0_0.NvidiaErrorInjection",
            processorId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.Cpu"});
}
inline void getNetworkAdapterErrorInjectionData(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const std::string& chassisId, const std::string& networkAdapterId)
{
    BMCWEB_LOG_DEBUG("Get available system network adapters resource");
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }
    crow::connections::systemBus->async_method_call(
        [chassisId, networkAdapterId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& paths) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);

            return;
        }
        for (const auto& path : paths)
        {
            if (!boost::ends_with(path, networkAdapterId) ||
                path.find(chassisId) == path.npos)
            {
                continue;
            }

            getErrorInjectionService(aResp, path + "/ErrorInjection",
                                     "/redfish/v1/Chassis/" + chassisId +
                                         "/NetworkAdapters/" +
                                         networkAdapterId);
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaErrorInjection.v1_0_0.NvidiaErrorInjection",
            networkAdapterId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory/", 0,
        std::array<std::string, 1>{
            "xyz.openbmc_project.Inventory.Item.NetworkInterface"});
}
inline void
    getSwitchErrorInjectionData(App& app, const crow::Request& req,
                                std::shared_ptr<bmcweb::AsyncResp> aResp,
                                const std::string& fabricId,
                                const std::string& switchId)
{
    BMCWEB_LOG_DEBUG("Get available system switches resource");
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }
    crow::connections::systemBus->async_method_call(
        [fabricId, switchId, aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& paths) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);

            return;
        }
        for (const auto& path : paths)
        {
            if (!boost::ends_with(path, switchId) ||
                path.find(fabricId) == path.npos)
            {
                continue;
            }

            getErrorInjectionService(aResp, path + "/ErrorInjection",
                                     "/redfish/v1/Fabrics/" + fabricId +
                                         "/Switches/" + switchId);
            return;
        }
        // Object not found
        messages::resourceNotFound(
            aResp->res, "#NvidiaErrorInjection.v1_0_0.NvidiaErrorInjection",
            switchId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory/", 0,
        std::array<std::string, 1>{
            "xyz.openbmc_project.Inventory.Item.NvSwitch"});
}

inline void requestRoutesErrorInjection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" +
                          std::string(BMCWEB_REDFISH_SYSTEM_URI_NAME) +
                          "/Processors/<str>/Oem/Nvidia/ErrorInjection/")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(getProcessorErrorInjectionData, std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/NetworkAdapters/<str>"
                      "/Oem/Nvidia/ErrorInjection/")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(std::bind_front(
            getNetworkAdapterErrorInjectionData, std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/Fabrics/<str>/Switches/<str>"
                      "/Oem/Nvidia/ErrorInjection/")
        .privileges(redfish::privileges::getProcessor)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(getSwitchErrorInjectionData, std::ref(app)));
}

#endif

} // namespace redfish
