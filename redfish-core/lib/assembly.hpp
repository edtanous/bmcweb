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

#include <app.hpp>
#include <boost/algorithm/string/split.hpp>
#include <openbmc_dbus_rest.hpp>
#include <query.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/json_utils.hpp>

#include <iostream>
#include <regex>
#include <variant>

namespace redfish
{

/**
 * @brief Get all assembly info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       chassisId   Chassis that contains the assembly.
 */
inline void
    updateAssemblies(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& service, const std::string& objPath,
                     const std::string& chassisId)
{
    BMCWEB_LOG_DEBUG("Get Assemblies Data");
    // Get Assembly ID
    size_t devStart = objPath.rfind('/');
    if (devStart == std::string::npos)
    {
        BMCWEB_LOG_ERROR("Assembly not found {}", objPath);
        return;
    }
    std::string assemblyName = objPath.substr(devStart + 1);
    if (assemblyName.empty())
    {
        BMCWEB_LOG_ERROR("Empty assembly");
        return;
    }
    std::regex assemblyRegex("[^0-9]*([0-9]+).*");
    std::cmatch match;
    if (!regex_match(assemblyName.c_str(), match, assemblyRegex))
    {
        BMCWEB_LOG_ERROR("Assembly Id not found");
        return;
    }
    const std::string& assemblyId = match[1].first;
    // Get interface properties
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, chassisId, assemblyId](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::variant<std::string>>>& propertiesList) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error for assembly properties");
            messages::internalError(asyncResp->res);
            return;
        }
        // Assemblies data
        nlohmann::json assemblyRes;
        assemblyRes["@odata.id"] = "/redfish/v1/Chassis/" + chassisId +
                                   "/Assembly#/Assemblies/" + assemblyId;
        assemblyRes["MemberId"] = assemblyId;
        for (const std::pair<std::string, std::variant<std::string>>& property :
             propertiesList)
        {
            // Store DBus properties that are also Redfish
            // properties with same name and a string value
            const std::string& propertyName = property.first;
            if ((propertyName == "Model") || (propertyName == "Name") ||
                (propertyName == "PartNumber") ||
                (propertyName == "SerialNumber") ||
                (propertyName == "Version") || (propertyName == "SKU"))
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for asset properties");
                    messages::internalError(asyncResp->res);
                    return;
                }
                assemblyRes[propertyName] = *value;
            }
            else if (propertyName == "Manufacturer")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for manufacturer");
                    messages::internalError(asyncResp->res);
                    return;
                }
                assemblyRes["Vendor"] = *value;
            }
            else if (propertyName == "BuildDate")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for build date");
                    messages::internalError(asyncResp->res);
                    return;
                }
                assemblyRes["ProductionDate"] = *value;
            }
            else if (propertyName == "LocationType")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for LocationType");
                    messages::internalError(asyncResp->res);
                    return;
                }
                assemblyRes["Location"]["PartLocation"]["LocationType"] =
                    redfish::dbus_utils::toLocationType(*value);
            }
            else if (propertyName == "PhysicalContext")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for PhysicalContext");
                    messages::internalError(asyncResp->res);
                    return;
                }
                assemblyRes["PhysicalContext"] =
                    redfish::dbus_utils::toPhysicalContext(*value);
            }
        }
        nlohmann::json& jResp = asyncResp->res.jsonValue["Assemblies"];
        jResp.push_back(assemblyRes);
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

/**
 * Assembly override class for delivering Chassis/Assembly Schema
 * Functions triggers appropriate requests on DBus
 */
inline void requestAssemblyRoutes(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Assembly/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& chassisId) {
        BMCWEB_LOG_DEBUG("Assembly doGet enter");
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        const std::array<const char*, 1> interface = {
            "xyz.openbmc_project.Inventory.Item.Chassis"};
        // Get chassis collection
        crow::connections::systemBus->async_method_call(
            [asyncResp, chassisId(std::string(chassisId))](
                const boost::system::error_code ec,
                const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG("DBUS response error");
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;
                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }
                if (connectionNames.size() < 1)
                {
                    std::cerr << "Got 0 Connection names";
                    continue;
                }
                const std::string& connectionName = connectionNames[0].first;
                // Chassis assembly properties
                asyncResp->res.jsonValue["@odata.type"] =
                    "#Assembly.v1_3_0.Assembly";
                asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Chassis/" +
                                                        chassisId + "/Assembly";
                asyncResp->res.jsonValue["Id"] = "Assembly";
                asyncResp->res.jsonValue["Name"] = "Assembly data for " +
                                                   chassisId;
                // Get Chassis assembly through association
                crow::connections::systemBus->async_method_call(
                    [asyncResp, path, chassisId(std::string(chassisId)),
                     connectionName](
                        const boost::system::error_code ec,
                        std::variant<std::vector<std::string>>& resp) {
                    const std::array<const char*, 1> assemblyIntf = {
                        "xyz.openbmc_project.Inventory.Item.Assembly"};
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR(
                            "Chassis and assembly are not connected through association. ec : {}",
                            ec);

                crow::connections::systemBus->async_method_call(
                    [asyncResp, chassisId(std::string(chassisId)),
                     connectionName](
                        const boost::system::error_code ec,
                        const std::vector<std::string>& assemblyList) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG("DBUS response error");
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Update the Assemblies
                    asyncResp->res.jsonValue["Assemblies"] =
                        nlohmann::json::array();
                    for (const std::string& assembly : assemblyList)
                    {
                                updateAssemblies(asyncResp, connectionName,
                                                 assembly, chassisId);
                            }
                        },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper",
                            "GetSubTreePaths", path + "/", 0, assemblyIntf);
                        return;
                    }

                    else
                    {
                        std::vector<std::string>* assemblyList =
                            std::get_if<std::vector<std::string>>(&resp);

                        for (const std::string& assembly : *assemblyList)
                        {
                            BMCWEB_LOG_DEBUG("Found Assembly Path, {}",
                                             assembly);
                            asyncResp->res.jsonValue["Assemblies"] =
                                nlohmann::json::array();
                            crow::connections::systemBus->async_method_call(
                                [asyncResp, assembly,
                                 chassisId(std::string(chassisId))](
                                    const boost::system::error_code errorCode,
                                    const std::vector<std::pair<
                                        std::string, std::vector<std::string>>>&
                                        objInfo) mutable {
                                if (errorCode)
                                {
                                    BMCWEB_LOG_ERROR("error_code = {}",
                                                     errorCode);

                                    messages::internalError(asyncResp->res);

                                    return;
                                }
                                else
                                {
                                    updateAssemblies(asyncResp,
                                                     objInfo[0].first, assembly,
                                         chassisId);
                    }
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                                "xyz.openbmc_project.ObjectMapper", "GetObject",
                                assembly, assemblyIntf);
                        }
                    }
                },
                    "xyz.openbmc_project.ObjectMapper", path + "/assembly",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");

                return;
            }
            // Couldn't find an object with that name. return an error
            messages::resourceNotFound(asyncResp->res,
                                       "#Chassis.v1_15_0.Chassis", chassisId);
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
            "/xyz/openbmc_project/inventory", 0, interface);
    });
}

} // namespace redfish
