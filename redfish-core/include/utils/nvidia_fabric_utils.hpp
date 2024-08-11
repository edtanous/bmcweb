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

#include <atomic>
#include <cstdint>
namespace redfish
{

namespace nvidia_fabric_utils
{
// Map of service name to list of interfaces
using MapperServiceMap =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

// Map of object paths to MapperServiceMaps
using MapperGetSubTreeResponse =
    std::vector<std::pair<std::string, MapperServiceMap>>;
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * Handle the PATCH operation of the Port Disable Future Property. Do basic
 * validation of the input data, and then update using async way.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       fabricId        Fabric's Id.
 * @param[in]       switchId        Switch's Id.
 * @param[in]       propertyValue   New property value to apply.
 * @param[in]       propertyName    Property name for new value
 * @param[in]       ObjectPath      Path of object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchPortDisableFuture(
    const std::shared_ptr<bmcweb::AsyncResp>& resp, const std::string& fabricId,
    const std::string& switchId, const std::string& propertyValue,
    const std::string& propertyName, const uint32_t& portNumber,
    const std::vector<uint8_t>& portsList, const std::string& objectPath,
    const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.NVLink.NVLinkDisableFuture") !=
            interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        BMCWEB_LOG_ERROR(
            "NVLinkDisableFuture interface not found while {} patch",
            propertyName);
        messages::internalError(resp->res);
        return;
    }

    dbus::utility::getDbusObject(
        objectPath,
        std::array<std::string_view, 1>{
            nvidia_async_operation_utils::setAsyncInterfaceName},
        [resp, propertyValue, propertyName, portNumber, portsList, fabricId,
         switchId, objectPath, service = *inventoryService](
            const boost::system::error_code& ec,
            const dbus::utility::MapperGetObject& object) {
        if (!ec)
        {
            for (const auto& [serv, _] : object)
            {
                if (serv != service)
                {
                    continue;
                }

                std::vector<uint8_t> portListToDisable = portsList;
                auto it = std::find(portListToDisable.begin(),
                                    portListToDisable.end(), portNumber);
                if (propertyValue == "Disabled")
                {
                    if (it == portListToDisable.end())
                        portListToDisable.push_back(
                            static_cast<uint8_t>(portNumber));
                }
                else if (propertyValue == "Enabled")
                {
                    if (it != portListToDisable.end())
                        portListToDisable.erase(it);
                }
                else
                {
                    BMCWEB_LOG_ERROR("Invalid value for patch on property {}",
                                     propertyName);
                    messages::internalError(resp->res);
                    return;
                }

                BMCWEB_LOG_DEBUG(
                    "Performing Patch using Set Async Method Call for {}",
                    propertyName);

                nvidia_async_operation_utils::doGenericSetAsyncAndGatherResult(
                    resp, std::chrono::seconds(60), service, objectPath,
                    "com.nvidia.NVLink.NVLinkDisableFuture", propertyName,
                    std::variant<std::vector<uint8_t>>(portListToDisable),
                    nvidia_async_operation_utils::PatchPortDisableCallback{resp});

                return;
            }
        }
    });
}

/**
 * Find the D-Bus object representing the requested switch, and call the
 * handler with the results. If matching object is not found, add 404 error to
 * response and don't call the handler.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       fabricId        Redfish Fabric Id.
 * @param[in]       switchId        Redfish Switch Id.
 * @param[in]       handler         Callback to continue processing request upon
 *                                  successfully finding object.
 */
template <typename Handler>
inline void getSwitchObjectAndPortNum(
    const std::shared_ptr<bmcweb::AsyncResp>& resp, const std::string& fabricId,
    const std::string& switchId, const std::string& portId, Handler&& handler)
{
    BMCWEB_LOG_DEBUG("Get available switch on fabric resources.");

    crow::connections::systemBus->async_method_call(
        [resp, fabricId, switchId, portId,
         handler = std::forward<Handler>(handler)](
            boost::system::error_code ec,
            const MapperGetSubTreeResponse& subtree) mutable {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error: {}", ec);
            messages::internalError(resp->res);
            return;
        }

        bool isFoundFabricObject = false;
        for (const auto& [objectPath, serviceMap] : subtree)
        {
            // Ignore any objects which don't end with our desired fabric
            if (!boost::ends_with(objectPath, fabricId))
            {
                continue;
            }

            crow::connections::systemBus->async_method_call(
                [resp, fabricId, switchId, portId,
                 handler](const boost::system::error_code ec,
                          std::variant<std::vector<std::string>>& response) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("DBUS response error: {}", ec);
                    messages::internalError(resp->res);
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&response);
                if (data == nullptr)
                {
                    BMCWEB_LOG_ERROR("Response error data null");
                    messages::internalError(resp->res);
                    return;
                }

                bool isFoundSwitchObject = false;
                // Iterate over all retrieved ObjectPaths.
                for (const std::string& switchPath : *data)
                {
                    sdbusplus::message::object_path objPath(switchPath);
                    if (objPath.filename() != switchId)
                    {
                        continue;
                    }

                    crow::connections::systemBus->async_method_call(
                        [resp, fabricId, switchId, portId, switchPath,
                         handler](const boost::system::error_code ec,
                                  const std::vector<std::pair<
                                      std::string, std::vector<std::string>>>&
                                      object) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR(
                                "DBUS response error while getting service name for switches");
                            messages::internalError(resp->res);
                            return;
                        }

                        using PropertyType =
                            std::variant<std::string, bool, double, uint8_t,
                                         size_t, std::vector<uint8_t>>;
                        using PropertiesMap =
                            boost::container::flat_map<std::string,
                                                       PropertyType>;
                        crow::connections::systemBus->async_method_call(
                            [resp, fabricId, switchId, portId, switchPath,
                             object,
                             handler](const boost::system::error_code ec,
                                      const PropertiesMap& properties) {
                            if (ec)
                            {
                                messages::internalError(resp->res);
                                return;
                            }
                            std::vector<uint8_t> portsToDisable;

                            for (const auto& property : properties)
                            {
                                const std::string& propertyName =
                                    property.first;
                                if (propertyName == "PortDisableFuture")
                                {
                                    auto* value =
                                        std::get_if<std::vector<uint8_t>>(
                                            &property.second);
                                    if (value == nullptr)
                                    {
                                        BMCWEB_LOG_ERROR(
                                            "Null value returned "
                                            "for Port Disable Future mask");
                                        messages::internalError(resp->res);
                                        return;
                                    }
                                    portsToDisable = *value;
                                }
                            }

                            crow::connections::systemBus->async_method_call(
                                [resp, fabricId, switchId, portId, handler,
                                 switchPath, object, portsToDisable](
                                    const boost::system::error_code ec,
                                    std::variant<std::vector<std::string>>&
                                        response) {
                                if (ec)
                                {
                                    BMCWEB_LOG_ERROR("DBUS response error");
                                    messages::internalError(resp->res);
                                    return;
                                }
                                std::vector<std::string>* data =
                                    std::get_if<std::vector<std::string>>(
                                        &response);
                                if (data == nullptr)
                                {
                                    BMCWEB_LOG_ERROR(
                                        "DBUS response error while getting ports");
                                    messages::internalError(resp->res);
                                    return;
                                }
                                bool isFoundPortObject = false;
                                for (const std::string& portPath : *data)
                                {
                                    // Get the portId object
                                    sdbusplus::message::object_path pPath(
                                        portPath);
                                    if (pPath.filename() != portId)
                                    {
                                        continue;
                                    }
                                    crow::connections::systemBus->async_method_call(
                                        [resp, fabricId, switchId, switchPath,
                                         object, handler, portPath,
                                         portsToDisable](
                                            const boost::system::error_code ec,
                                            const std::vector<std::pair<
                                                std::string,
                                                std::vector<std::string>>>&
                                                obj) {
                                        if (ec)
                                        {
                                            BMCWEB_LOG_DEBUG(
                                                "No port interface on {}",
                                                portPath);
                                            return;
                                        }

                                        crow::connections::systemBus
                                            ->async_method_call(
                                                [resp, fabricId, switchId,
                                                 switchPath, object, handler,
                                                 portsToDisable](
                                                    const boost::system::
                                                        error_code ec,
                                                    const PropertiesMap&
                                                        properties) {
                                            if (ec)
                                            {
                                                messages::internalError(
                                                    resp->res);
                                                return;
                                            }

                                            uint32_t portNumber;
                                            for (const auto& property :
                                                 properties)
                                            {
                                                const std::string&
                                                    propertyName =
                                                        property.first;
                                                if (propertyName ==
                                                    "PortNumber")
                                                {
                                                    const size_t* value =
                                                        std::get_if<size_t>(
                                                            &property.second);
                                                    if (value == nullptr)
                                                    {
                                                        BMCWEB_LOG_DEBUG(
                                                            "Null value returned "
                                                            "for port number");
                                                        messages::internalError(
                                                            resp->res);
                                                        return;
                                                    }

                                                    portNumber =
                                                        static_cast<uint32_t>(
                                                            *value);
                                                }
                                            }

                                            handler(resp, fabricId, switchId,
                                                    switchPath, object,
                                                    portNumber, portsToDisable);
                                        },
                                                obj.front().first, portPath,
                                                "org.freedesktop.DBus.Properties",
                                                "GetAll", "");
                                    },
                                        "xyz.openbmc_project.ObjectMapper",
                                        "/xyz/openbmc_project/object_mapper",
                                        "xyz.openbmc_project.ObjectMapper",
                                        "GetObject", portPath,
                                        std::array<std::string, 1>(
                                            {"xyz.openbmc_project.Inventory.Item.Port"}));
                                    isFoundPortObject = true;
                                }
                                if (isFoundPortObject == false)
                                {
                                    messages::resourceNotFound(resp->res,
                                                               "Port", portId);
                                }
                            },
                                "xyz.openbmc_project.ObjectMapper",
                                switchPath + "/all_states",
                                "org.freedesktop.DBus.Properties", "Get",
                                "xyz.openbmc_project.Association", "endpoints");
                        },
                            object.front().first, switchPath,
                            "org.freedesktop.DBus.Properties", "GetAll",
                            "com.nvidia.NVLink.NVLinkDisableFuture");
                    },
                        "xyz.openbmc_project.ObjectMapper",
                        "/xyz/openbmc_project/object_mapper",
                        "xyz.openbmc_project.ObjectMapper", "GetObject",
                        switchPath, std::array<const char*, 0>());
                    isFoundSwitchObject = true;
                }
                if (isFoundSwitchObject == false)
                {
                    messages::resourceNotFound(resp->res, "Switch", switchId);
                }
            },
                "xyz.openbmc_project.ObjectMapper",
                objectPath + "/all_switches", "org.freedesktop.DBus.Properties",
                "Get", "xyz.openbmc_project.Association", "endpoints");

            isFoundFabricObject = true;
        }
        if (isFoundFabricObject == false)
        {
            messages::resourceNotFound(resp->res, "Fabric", fabricId);
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.Fabric"});
}

inline void getPortDisableFutureStatus(
    const std::shared_ptr<bmcweb::AsyncResp>& resp, const std::string& fabricId,
    const std::string& switchId, const std::string& portId,
    const uint32_t& portNumber, const std::vector<uint8_t>& portsList)
{
    BMCWEB_LOG_DEBUG("Get port disable future status on switch resources.");

    std::string portSettingURI =
        (boost::format("/redfish/v1/Fabrics/%s/Switches/%s/Ports/"
                       "%s/Settings") %
         fabricId % switchId % portId)
            .str();
    resp->res.jsonValue["@odata.type"] = "#Port.v1_4_0.Port";
    resp->res.jsonValue["@odata.id"] = portSettingURI;
    resp->res.jsonValue["Name"] = switchId + " " + portId + " Pending Settings";
    resp->res.jsonValue["Id"] = "Settings";

    // check in portsList if present
    auto it = std::find(portsList.begin(), portsList.end(), portNumber);
    if (it != portsList.end())
    {
        resp->res.jsonValue["LinkState"] = "Disabled";
    }
    else
    {
        resp->res.jsonValue["LinkState"] = "Enabled";
    }
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

} // namespace nvidia_fabric_utils
} // namespace redfish
