/*
// Copyright (c) 2022, NVIDIA Corporation
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

#include <app.hpp>
#include <boost/algorithm/string/split.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/json_utils.hpp>

#include <iostream>
#include <regex>
#include <variant>
#include <vector>

namespace redfish
{

/**
 * @brief Get all pcieslot  swithc links info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       chassisId   Chassis that contains the pcieslot.
 */
inline void  updatePCIeSlotsSwitchLinks(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                std::map<std::string, std::variant<int,std::string>>& dbusProperties, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "updatePCIeSlotsSwitchLinks ";

    crow::connections::systemBus->async_method_call(
                [asyncResp, objPath, dbusProperties](const boost::system::error_code ec,
                    std::variant<std::vector<std::string>>& resp) {
                    if (ec)
                    {
                        // no fabric = no switches , Get processor link
                        crow::connections::systemBus->async_method_call(
                            [asyncResp, objPath, dbusProperties](const boost::system::error_code ec,
                                std::variant<std::vector<std::string>>& resp) {

                            if (ec)
                            {
                                BMCWEB_LOG_ERROR << "processor not found for pcieslot ";
                                return; // no processors identified for pcieslotpath
                            }

                            nlohmann::json pcieSlotRes;
                            for (auto const& [key, val] : dbusProperties)
                            {
                                if( key == "Lanes")
                                {
                                    pcieSlotRes[key] = std::get<int>(val);
                                }
                                else
                                {
                                    pcieSlotRes[key] = std::get<std::string>(val);
                                }
                            }
                            std::vector<std::string>* data =
                            std::get_if<std::vector<std::string>>(&resp);
                            if (data == nullptr)
                            {
                                BMCWEB_LOG_ERROR << "processor data null for pcieslot ";
                                return;
                            }
                            // declaring processors array
                            nlohmann::json& prcoessorsLinkArray = pcieSlotRes["Links"]["Processors"];
                            prcoessorsLinkArray = nlohmann::json::array();

                            for (const std::string& processorPath : *data)
                            {
                                sdbusplus::message::object_path dbusObjPath(processorPath);
                                const std::string& prcoessorId = dbusObjPath.filename();
                                std::string processorURI = "/redfish/v1/Systems/system/Processors/";
                                processorURI += prcoessorId;
                                prcoessorsLinkArray.push_back({{"@odata.id", processorURI}});
                            }
                            nlohmann::json& jResp = asyncResp->res.jsonValue["Slots"];
                            jResp.push_back(pcieSlotRes);
                        },
                        "xyz.openbmc_project.ObjectMapper", objPath + "/processor_link",
                        "org.freedesktop.DBus.Properties", "Get",
                        "xyz.openbmc_project.Association", "endpoints");

                        return;
                    }
                    //fabric identified for pcieslot
                    std::vector<std::string>* data =
                        std::get_if<std::vector<std::string>>(&resp);
                    if (data == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "fabric data null  for pcieslot ";
                        return;
                    }
                    std::string fabricId; //pcieslot fabric id
                    for (const std::string& fabricPath : *data)
                    {
                        sdbusplus::message::object_path dbusObjPath(fabricPath);
                        fabricId = dbusObjPath.filename();
                    }
                    // Get Switch links using associtaions
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, objPath, dbusProperties, fabricId](const boost::system::error_code ec,
                            std::variant<std::vector<std::string>>& resp) {

                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "switch not found for pcieslot ";
                            return;
                        }

                        std::vector<std::string>* data =
                            std::get_if<std::vector<std::string>>(&resp);
                        nlohmann::json pcieSlotRes;

                        if (data == nullptr)
                        {
                            BMCWEB_LOG_ERROR << "switch data null for pcieslot ";
                            return;
                        }
                        // update dbus properties to json object
                        for (auto const& [key, val] : dbusProperties)
                        {
                            if( key == "Lanes")
                            {
                                pcieSlotRes[key] = std::get<int>(val);
                            }
                            else
                            {
                                pcieSlotRes[key] = std::get<std::string>(val);
                            }
                        }

                        pcieSlotRes["Links"]["Oem"]["Nvidia"]["@odata.type"] = "#NvidiaPCIeSlots.v1_0_0.NvidiaPCIeSlots";
                        // declaring switches array
                        nlohmann::json& switchesLinkArray = pcieSlotRes["Links"]["Oem"]["Nvidia"]["Switches"];
                        switchesLinkArray = nlohmann::json::array();
                        std::string switchURI;
                        // parse switchlinks and append it to switches array
                        for (const std::string& switchPath : *data)
                        {
                            sdbusplus::message::object_path dbusObjPath(switchPath);
                            const std::string& switchId = dbusObjPath.filename();
                            switchURI = "/redfish/v1/Fabrics/" + fabricId +"/Switches/";
                            switchURI += switchId;
                            switchesLinkArray.push_back({{"@odata.id", switchURI}});
                        }

                        // sending response
                        nlohmann::json& jResp = asyncResp->res.jsonValue["Slots"];
                        jResp.push_back(pcieSlotRes);

                     },
                    "xyz.openbmc_project.ObjectMapper", objPath + "/switch_link",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");

         },
         "xyz.openbmc_project.ObjectMapper", objPath + "/fabric_link",
         "org.freedesktop.DBus.Properties", "Get",
         "xyz.openbmc_project.Association", "endpoints");
}

/**
 * @brief Get all cpieslot info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 * @param[in]       chassisId   Chassis that contains the pcieslot.
 */
inline void  updatePCIeSlots(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& service, const std::string& objPath,
                     const std::string& chassisId)
{
    BMCWEB_LOG_DEBUG << "updatePCIeSlots ";

    // Get interface properties
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, chassisId, objPath](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::variant<std::string, int>>>& propertiesList)
        {

            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "DBUS response error for pcieslot properties";
                messages::internalError(asyncResp->res);
                return;
            }

            std::map<std::string, std::variant<int,std::string>> dbusProperties; //map of all pcislot dbus properties
            // pcieslots  data
            for (const std::pair<std::string, std::variant<std::string, int>>&
                property : propertiesList)
            {
                // Store DBus properties that are also Redfish
                // properties with same name and a string value
                const std::string& propertyName = property.first;
                if (propertyName == "Generation")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                            "for Generation ";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    dbusProperties.insert(std::pair<std::string, std::variant<int,std::string>>("PCIeType",*value));
                }
                else if (propertyName == "SlotType")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                            "for SlotType";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    dbusProperties.insert(std::pair<std::string, std::variant<int,std::string>>(propertyName,*value));
                }
                else if (propertyName == "Lanes")
                {
                    const int* value = std::get_if<int>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Null value returned "
                            "for Lanes";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    dbusProperties.insert(std::pair<std::string, std::variant<int,std::string>>(propertyName,*value));
                }
            }

            // Update switch links
            updatePCIeSlotsSwitchLinks(asyncResp, dbusProperties, objPath);

        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");
}

/**
 * PCIeSlots override class for delivering Chassis/PCIeSlots Schema
 * Functions triggers appropriate requests on DBus
 */
inline void requestPcieSlotsRoutes(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/PCIeSlots/")
        .privileges({{"Login"}})
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& chassisId) {
            BMCWEB_LOG_DEBUG << "PCIeSlot doGet enter";
            const std::array<const char*, 1> interface = {
                "xyz.openbmc_project.Inventory.Item.Chassis"};
            // Get chassis collection
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisId(std::string(chassisId))](
                    const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
                    const std::array<const char*, 1> pcieslotIntf = {
                        "xyz.openbmc_project.Inventory.Item.PCIeSlot"};
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "DBUS response error";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Iterate over all retrieved ObjectPaths.
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<std::string,
                                                   std::vector<std::string>>>>&
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
                            continue;
                        }
                        const std::string& connectionName =
                            connectionNames[0].first;
                        // Chassis pcieslot properties
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#PCIeSlots.v1_5_0.PCIeSlots";
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisId + "/PCIeSlots";
                        asyncResp->res.jsonValue["Id"] = "PCIeSlots";
                        asyncResp->res.jsonValue["Name"] =
                            "PCIeSlots for " + chassisId;
                        // Get chassis pcieSlots
                        crow::connections::systemBus->async_method_call(
                            [asyncResp, chassisId(std::string(chassisId)),
                             connectionName](
                                const boost::system::error_code ec,
                                const std::vector<std::string>& pcieSlotList) {
                                if (ec)
                                {
                                    BMCWEB_LOG_DEBUG << "DBUS response error";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                // Update the pcieslots
                                for (const std::string& pcieslot : pcieSlotList)
                                {
                                    updatePCIeSlots(asyncResp, connectionName,
                                                     pcieslot, chassisId);
                                }
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper",
                            "GetSubTreePaths", path + "/", 0, pcieslotIntf);
                        return;
                    }
                    // Couldn't find an object with that name. return an error
                    messages::resourceNotFound(
                        asyncResp->res, "#Chassis.v1_15_0.Chassis", chassisId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", 0, interface);
        });
}

} // namespace redfish

