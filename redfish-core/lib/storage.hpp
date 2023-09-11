/*
// Copyright (c) 2019 Intel Corporation
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
#include "openbmc_dbus_rest.hpp"
#include "task.hpp"

#include <app.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <utils/dbus_utils.hpp>

namespace redfish
{
const uint8_t maxDriveNum = 32;
// only allow doing drive sanitize once
static bool sanitizeInProgress[maxDriveNum] = { false };
// task uri for long-run drive operation
std::vector<std::string> taskUris;

inline void requestRoutesStorageCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Storage/")
        .privileges(redfish::privileges::getStorageCollection)
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
                    "#StorageCollection.StorageCollection";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Storage";
                asyncResp->res.jsonValue["Name"] = "Storage Collection";
                nlohmann::json::array_t members;
                nlohmann::json::object_t member;
                member["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Storage/1";
                members.emplace_back(member);
                asyncResp->res.jsonValue["Members"] = std::move(members);
                asyncResp->res.jsonValue["Members@odata.count"] = 1;
            });
}

inline void getDrives(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const std::shared_ptr<HealthPopulate>& health)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, health](
            const boost::system::error_code ec,
            const dbus::utility::MapperGetSubTreePathsResponse& driveList) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Drive mapper call error";
                messages::internalError(asyncResp->res);
                return;
            }

            nlohmann::json& driveArray = asyncResp->res.jsonValue["Drives"];
            driveArray = nlohmann::json::array();
            auto& count = asyncResp->res.jsonValue["Drives@odata.count"];
            count = 0;

            health->inventory.insert(health->inventory.end(), driveList.begin(),
                                     driveList.end());

            for (const std::string& drive : driveList)
            {
                sdbusplus::message::object_path object(drive);
                if (object.filename().empty())
                {
                    BMCWEB_LOG_ERROR << "Failed to find filename in " << drive;
                    return;
                }

                nlohmann::json::object_t driveJson;
                driveJson["@odata.id"] = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                         "/Storage/1/Drives/" +
                                         object.filename();
                driveArray.push_back(std::move(driveJson));
            }

            count = driveArray.size();
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", int32_t(0),
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Drive"});
}

inline void
    getStorageControllers(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::shared_ptr<HealthPopulate>& health)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp,
         health](const boost::system::error_code ec,
                 const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec || subtree.empty())
            {
                // doesn't have to be there
                return;
            }

            nlohmann::json& root =
                asyncResp->res.jsonValue["StorageControllers"];
            root = nlohmann::json::array();
            for (const auto& [path, interfaceDict] : subtree)
            {
                sdbusplus::message::object_path object(path);
                std::string id = object.filename();
                if (id.empty())
                {
                    BMCWEB_LOG_ERROR << "Failed to find filename in " << path;
                    return;
                }

                if (interfaceDict.size() != 1)
                {
                    BMCWEB_LOG_ERROR << "Connection size "
                                     << interfaceDict.size()
                                     << ", greater than 1";
                    messages::internalError(asyncResp->res);
                    return;
                }

                const std::string& connectionName = interfaceDict.front().first;

                size_t index = root.size();
                nlohmann::json& storageController =
                    root.emplace_back(nlohmann::json::object());

                storageController["@odata.type"] =
                    "#Storage.v1_7_0.StorageController";
                storageController["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID
                    "/Storage/1#/StorageControllers/" +
                    std::to_string(index);
                storageController["Name"] = id;
                storageController["MemberId"] = id;
                storageController["Status"]["State"] = "Enabled";

                sdbusplus::asio::getProperty<bool>(
                    *crow::connections::systemBus, connectionName, path,
                    "xyz.openbmc_project.Inventory.Item", "Present",
                    [asyncResp, index](const boost::system::error_code ec2,
                                       bool enabled) {
                        // this interface isn't necessary, only check it
                        // if we get a good return
                        if (ec2)
                        {
                            return;
                        }
                        if (!enabled)
                        {
                            asyncResp->res.jsonValue["StorageControllers"]
                                                    [index]["Status"]["State"] =
                                "Disabled";
                        }
                    });

                sdbusplus::asio::getAllProperties(
                    *crow::connections::systemBus, connectionName, path,
                    "xyz.openbmc_project.Inventory.Decorator.Asset",
                    [asyncResp,
                     index](const boost::system::error_code ec2,
                            const std::vector<std::pair<
                                std::string, dbus::utility::DbusVariantType>>&
                                propertiesList) {
                        if (ec2)
                        {
                            // this interface isn't necessary
                            return;
                        }

                        const std::string* partNumber = nullptr;
                        const std::string* serialNumber = nullptr;
                        const std::string* manufacturer = nullptr;
                        const std::string* model = nullptr;

                        const bool success = sdbusplus::unpackPropertiesNoThrow(
                            dbus_utils::UnpackErrorPrinter(), propertiesList,
                            "PartNumber", partNumber, "SerialNumber",
                            serialNumber, "Manufacturer", manufacturer, "Model",
                            model);

                        if (!success)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        nlohmann::json& controller =
                            asyncResp->res
                                .jsonValue["StorageControllers"][index];

                        if (partNumber != nullptr)
                        {
                            controller["PartNumber"] = *partNumber;
                        }

                        if (serialNumber != nullptr)
                        {
                            controller["SerialNumber"] = *serialNumber;
                        }

                        if (manufacturer != nullptr)
                        {
                            controller["Manufacturer"] = *manufacturer;
                        }

                        if (model != nullptr)
                        {
                            controller["Model"] = *model;
                        }
                    });
            }

            // this is done after we know the json array will no longer
            // be resized, as json::array uses vector underneath and we
            // need references to its members that won't change
            size_t count = 0;
            // Pointer based on |asyncResp->res.jsonValue|
            nlohmann::json::json_pointer rootPtr =
                "/StorageControllers"_json_pointer;
            for (const auto& [path, interfaceDict] : subtree)
            {
                auto subHealth = std::make_shared<HealthPopulate>(
                    asyncResp, rootPtr / count / "Status");
                subHealth->inventory.emplace_back(path);
                health->inventory.emplace_back(path);
                health->children.emplace_back(subHealth);
                count++;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", int32_t(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.StorageController"});
}

inline void requestRoutesStorage(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Storage/1/")
        .privileges(redfish::privileges::getStorage)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                asyncResp->res.jsonValue["@odata.type"] =
                    "#Storage.v1_7_1.Storage";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Storage/1";
                asyncResp->res.jsonValue["Name"] = "Storage";
                asyncResp->res.jsonValue["Id"] = "1";
                asyncResp->res.jsonValue["Status"]["State"] = "Enabled";

                auto health = std::make_shared<HealthPopulate>(asyncResp);
                health->populate();

                getDrives(asyncResp, health);
                getStorageControllers(asyncResp, health);
            });
}

inline void getDriveAsset(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& connectionName,
                          const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.Asset",
        [asyncResp](const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string, dbus::utility::DbusVariantType>>&
                        propertiesList) {
            if (ec)
            {
                // this interface isn't necessary
                return;
            }

            const std::string* partNumber = nullptr;
            const std::string* serialNumber = nullptr;
            const std::string* manufacturer = nullptr;
            const std::string* model = nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), propertiesList, "PartNumber",
                partNumber, "SerialNumber", serialNumber, "Manufacturer",
                manufacturer, "Model", model);

            if (!success)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (partNumber != nullptr)
            {
                asyncResp->res.jsonValue["PartNumber"] = *partNumber;
            }

            if (serialNumber != nullptr)
            {
                asyncResp->res.jsonValue["SerialNumber"] = *serialNumber;
            }

            if (manufacturer != nullptr)
            {
                asyncResp->res.jsonValue["Manufacturer"] = *manufacturer;
            }

            if (model != nullptr)
            {
                asyncResp->res.jsonValue["Model"] = *model;
            }
        });
}

inline void getDrivePresent(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& connectionName,
                            const std::string& path)
{
    sdbusplus::asio::getProperty<bool>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Item", "Present",
        [asyncResp, path](const boost::system::error_code ec,
                          const bool enabled) {
            // this interface isn't necessary, only check it if
            // we get a good return
            if (ec)
            {
                return;
            }

            if (!enabled)
            {
                asyncResp->res.jsonValue["Status"]["State"] = "Disabled";
            }
        });
}

inline void getDriveState(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& connectionName,
                          const std::string& path)
{
    sdbusplus::asio::getProperty<bool>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.State.Drive", "Rebuilding",
        [asyncResp](const boost::system::error_code ec, const bool updating) {
            // this interface isn't necessary, only check it
            // if we get a good return
            if (ec)
            {
                return;
            }

            // updating and disabled in the backend shouldn't be
            // able to be set at the same time, so we don't need
            // to check for the race condition of these two
            // calls
            if (updating)
            {
                asyncResp->res.jsonValue["Status"]["State"] = "Updating";
            }
        });
}

inline std::optional<std::string> convertDriveType(const std::string& type)
{
    if (type == "xyz.openbmc_project.Inventory.Item.Drive.DriveType.HDD")
    {
        return "HDD";
    }
    if (type == "xyz.openbmc_project.Inventory.Item.Drive.DriveType.SSD")
    {
        return "SSD";
    }

    return std::nullopt;
}

inline std::optional<std::string> convertDriveProtocol(const std::string& proto)
{
    if (proto == "xyz.openbmc_project.Inventory.Item.Drive.DriveProtocol.SAS")
    {
        return "SAS";
    }
    if (proto == "xyz.openbmc_project.Inventory.Item.Drive.DriveProtocol.SATA")
    {
        return "SATA";
    }
    if (proto == "xyz.openbmc_project.Inventory.Item.Drive.DriveProtocol.NVMe")
    {
        return "NVMe";
    }
    if (proto == "xyz.openbmc_project.Inventory.Item.Drive.DriveProtocol.FC")
    {
        return "FC";
    }

    return std::nullopt;
}

inline std::optional<std::string>
    convertDriveFormFactor(const std::string& formFactor)
{

    if (formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.Drive3_5")
    {
        return "Drive3_5";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.Drive2_5" )
    {
        return "Drive2_5";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.EDSFF_1U_Long" )
    {
        return "EDSFF_1U_Long";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.EDSFF_1U_Short" )
    {
        return "EDSFF_1U_Short";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.EDSFF_E3_Short" )
    {
        return "EDSFF_E3_Short";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.EDSFF_E3_Long" )
    {
        return "EDSFF_E3_Long";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.M2_2230" )
    {
        return "M2_2230";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.M2_2242" )
    {
        return "M2_2242";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.M2_2260" )
    {
        return "M2_2260";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.M2_2280" )
    {
        return "M2_2280";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.M2_22110" )
    {
        return "M2_22110";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.U2" )
    {
        return "U2";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.PCIeSlotFullLength" )
    {
        return "PCIeSlotFullLength";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.PCIeSlotLowProfile" )
    {
        return "PCIeSlotLowProfile";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.PCIeHalfLength" )
    {
        return "PCIeHalfLength";
    }
    else if ( formFactor == "xyz.openbmc_project.Inventory.Item.Drive.DriveFormFactor.OEM" )
    {
        return "OEM";
    }

    return std::nullopt;
}

inline std::optional<std::string>
    convertDriveOperation(const std::string& op)
{
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Sanitize")
    {
        return "Sanitize";
    }
    if (op ==
        "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Deduplicate")
    {
        return "Deduplicate";
    }
    if (op ==
        "xyz.openbmc_project.Inventory.Item.Drive.OperationType.CheckConsistency")
    {
        return "CheckConsistency";
    }
    if (op ==
        "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Initialize")
    {
        return "Initialize";
    }
    if (op ==
        "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Replicate")
    {
        return "Replicate";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Delete")
    {
        return "Delete";
    }
    if (op ==
        "xyz.openbmc_project.Inventory.Item.Drive.OperationType.ChangeRAIDType")
    {
        return "ChangeRAIDType";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Rebuild")
    {
        return "Rebuild";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Encrypt")
    {
        return "Encrypt";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Encrypt")
    {
        return "Decrypt";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Resize")
    {
        return "Resize";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Compress")
    {
        return "Compress";
    }
    if (op == "xyz.openbmc_project.Inventory.Item.Drive.OperationType.Format")
    {
        return "Format";
    }
    if (op ==
        "xyz.openbmc_project.Inventory.Item.Drive.OperationType.ChangeStripSize")
    {
        return "ChangeStripSize";
    }
    return "";
}

inline void
    getDriveItemProperties(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Item.Drive",
        [asyncResp](const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string, dbus::utility::DbusVariantType>>&
                        propertiesList) {
            if (ec)
            {
                // this interface isn't required
                return;
            }
            for (const std::pair<std::string, dbus::utility::DbusVariantType>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Type")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        // illegal property
                        BMCWEB_LOG_ERROR << "Illegal property: Type";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    std::optional<std::string> mediaType =
                        convertDriveType(*value);
                    if (!mediaType)
                    {
                        BMCWEB_LOG_ERROR << "Unsupported DriveType Interface: "
                                         << *value;
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    asyncResp->res.jsonValue["MediaType"] = *mediaType;
                }
                else if (propertyName == "Capacity")
                {
                    const uint64_t* capacity =
                        std::get_if<uint64_t>(&property.second);
                    if (capacity == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Illegal property: Capacity";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    if (*capacity == 0)
                    {
                        // drive capacity not known
                        continue;
                    }

                    asyncResp->res.jsonValue["CapacityBytes"] = *capacity;
                }
                else if (propertyName == "Protocol")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Illegal property: Protocol";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    std::optional<std::string> proto =
                        convertDriveProtocol(*value);
                    if (!proto)
                    {
                        BMCWEB_LOG_ERROR
                            << "Unsupported DrivePrototype Interface: "
                            << *value;
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Protocol"] = *proto;
                }
                else if (propertyName == "FormFactor")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Illegal property: FormFactor";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    std::optional<std::string> formFactor =
                        convertDriveFormFactor(*value);
                    if (!formFactor)
                    {
                        BMCWEB_LOG_ERROR
                            << "Unsupported Drive FormFactor Interface: "
                            << *value;
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["FormFactor"] = *formFactor;
                }
                else if (propertyName == "PredictedMediaLifeLeftPercent")
                {
                    const uint8_t* lifeLeft =
                        std::get_if<uint8_t>(&property.second);
                    if (lifeLeft == nullptr)
                    {
                        BMCWEB_LOG_ERROR
                            << "Illegal property: PredictedMediaLifeLeftPercent";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // 255 means reading the value is not supported
                    if (*lifeLeft != 255)
                    {
                        asyncResp->res
                            .jsonValue["PredictedMediaLifeLeftPercent"] =
                            *lifeLeft;
                    }
                }
                else if (propertyName == "Operation")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Illegal property: Operation";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    std::optional<std::string> prog =
                        convertDriveOperation(*value);
                    asyncResp->res.jsonValue["Operations"]["Operation"] = *prog;
                    asyncResp->res.jsonValue["Operations"]["AssociatedTask"] = taskUris;
                }
            }
        });
}

inline void
    getDrivePortProperties(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Item.Port",
        [asyncResp](const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string, dbus::utility::DbusVariantType>>&
                        propertiesList) {
            if (ec)
            {
                // this interface isn't required
                return;
            }
            for (const std::pair<std::string, dbus::utility::DbusVariantType>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "MaxSpeed")
                {
                    const size_t* maxSpeed =
                        std::get_if<size_t>(&property.second);
                    if (maxSpeed == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Illegal property: MaxSpeed";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    asyncResp->res.jsonValue["CapableSpeedGbs"] = *maxSpeed;
                }
                else if (propertyName == "CurrentSpeed")
                {
                    const size_t* speed = std::get_if<size_t>(&property.second);
                    if (speed == nullptr)
                    {
                        BMCWEB_LOG_ERROR
                            << "Illegal property: NegotiatedSpeedGbs";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["NegotiatedSpeedGbs"] = *speed;
                }
            }
        });
}

inline void getDriveVersion(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& connectionName,
                            const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Software.Version", "Version",
        [asyncResp, path](const boost::system::error_code ec,
                          const std::string version) {
            if (ec)
            {
                return;
            }
            asyncResp->res.jsonValue["FirmwareVersion"] = version;
        });
}

inline void
    getDriveLocation(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& connectionName, const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.LocationCode", "LocationCode",
        [asyncResp, path](const boost::system::error_code ec,
                          const std::string location) {
            if (ec)
            {
                return;
            }
            asyncResp->res.jsonValue["PhysicalLocation"] = location;
        });
}

inline void getDriveStatus(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path, const std::string& sw)
{
    sdbusplus::asio::getProperty<bool>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.State.Decorator.OperationalStatus", "Functional",
        [asyncResp, path, sw](const boost::system::error_code ec,
                          const bool functional) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "fail to get drive status";
                return;
            }
            if (!functional) {
                asyncResp->res.jsonValue["StatusIndicator"] = "Fail";
            }
            else if(sw != "0" && sw != "2")
            {
                // the temperature(2) is excluded and it is not PFA.
                asyncResp->res.jsonValue["StatusIndicator"] =
                    "PredictiveFailureAnalysis";
                asyncResp->res.jsonValue["FailurePredicted"] = true;
            }
            else
            {
                asyncResp->res.jsonValue["StatusIndicator"] = "OK";
                asyncResp->res.jsonValue["FailurePredicted"] = false;
            }
        });
}

inline void
    getDriveSmartWarning(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& connectionName,
                         const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Nvme.Status", "SmartWarnings",
        [asyncResp, connectionName, path](const boost::system::error_code ec,
                          const std::string sw) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "fail to get drive smart";
                return;
            }
            getDriveStatus(asyncResp, connectionName, path, sw);
        });

}

inline void
    getDriveProgress(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<uint8_t>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Common.Progress", "Progress",
        [asyncResp, connectionName, path](const boost::system::error_code ec,
                                          const uint8_t prog) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "fail to get drive progress";
                return;
            }
            asyncResp->res.jsonValue["Operations"]["PercentageComplete"] =
                prog;
        });
}

static void addAllDriveInfo(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& connectionName,
                            const std::string& path,
                            const std::vector<std::string>& interfaces)
{
    for (const std::string& interface : interfaces)
    {
        if (interface == "xyz.openbmc_project.Inventory.Decorator.Asset")
        {
            getDriveAsset(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.Inventory.Item")
        {
            getDrivePresent(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.State.Drive")
        {
            getDriveState(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.Inventory.Item.Drive")
        {
            getDriveItemProperties(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.Inventory.Item.Port")
        {
            getDrivePortProperties(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.Software.Version")
        {
            getDriveVersion(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.Nvme.Status")
        {
            getDriveSmartWarning(asyncResp, connectionName, path);
        }
        else if (interface ==
                 "xyz.openbmc_project.Inventory.Decorator.LocationCode")
        {
            getDriveLocation(asyncResp, connectionName, path);
        }
        else if (interface == "xyz.openbmc_project.Common.Progress")
        {
            getDriveProgress(asyncResp, connectionName, path);
        }
    }
}

inline void getChassisID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& driveId)
{
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};

    crow::connections::systemBus->async_method_call(
        [asyncResp, driveId](const boost::system::error_code ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            // Iterate over all retrieved ObjectPaths.
            for (const auto& [p, connectionNames] : subtree)
            {
                if (connectionNames.empty())
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus,
                    "xyz.openbmc_project.ObjectMapper", p + "/drive",
                    "xyz.openbmc_project.Association", "endpoints",
                    [asyncResp, p](const boost::system::error_code ec3,
                                   const std::vector<std::string>& resp) {
                        if (ec3 || resp.empty())
                        {
                            return; // no drives = no
                                    // failures
                        }
                        sdbusplus::message::object_path path(p);
                        asyncResp->res
                            .jsonValue["Links"]["Chassis"]["@odata.id"] =
                            "/redfish/v1/Chassis/" + path.filename();

                        return;
                    });
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

inline void handleDriveSanitizePost(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& /*chassisId*/, const std::string& driveId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    auto driveIdx = stoi(driveId);
    if (sanitizeInProgress[driveIdx] == true)
    {
        if (asyncResp)
        {
            std::string resolution =
                "Drive sanitize in progress. Retry "
                "the sanitize operation once it is complete.";
                redfish::messages::updateInProgressMsg(asyncResp->res,
                                                       resolution);
            BMCWEB_LOG_ERROR << "Sanitize on drive" << driveId
                             << " already in progress.";
        }
        return;
    }

    std::string sanitizeType;
    uint16_t owPass = 0;
    if (!json_util::readJsonAction(req, asyncResp->res, "SanitizationType",
                                   sanitizeType))
    {
            messages::actionParameterValueError(
                asyncResp->res, "Drive.SecureErase", "SanitizationType");
        return;
    }
    if (sanitizeType == "Overwrite")
    {
        if (!json_util::readJsonAction(req, asyncResp->res, "OverwritePasses",
                                       owPass))
        {
            messages::actionParameterMissing(
                asyncResp->res, "Drive.SecureErase", "OverwritePasses");
            return;
        }
    }
    else if (sanitizeType == "CryptographicErase")
    {
        sanitizeType = "CryptoErase";
    }

    crow::connections::systemBus->async_method_call(
        [req, asyncResp, driveId, sanitizeType,
         owPass](const boost::system::error_code ec,
                 const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Drive mapper call error";
                messages::internalError(asyncResp->res);
                return;
            }

            auto drive = std::find_if(
                subtree.begin(), subtree.end(),
                [&driveId](
                    const std::pair<std::string,
                                    dbus::utility::MapperServiceMap>& object) {
                    return sdbusplus::message::object_path(object.first)
                               .filename() == driveId;
                });
            if (drive == subtree.end())
            {
                messages::resourceNotFound(asyncResp->res, "Drive", driveId);
                return;
            }
            const std::string& path = drive->first;
            const dbus::utility::MapperServiceMap& connNames = drive->second;
            if (connNames.size() != 1)
            {
                BMCWEB_LOG_ERROR << "Connection size " << connNames.size()
                                 << ", not equal to 1";
                messages::internalError(asyncResp->res);
                return;
            }

            auto service = connNames[0].first;
            auto interfaces = connNames[0].second;
            auto driveIdx = stoi(driveId);
            for (const std::string& interface : interfaces)
            {
                if (interface != "xyz.openbmc_project.Inventory.Item.Drive")
                {
                    continue;
                }
                std::shared_ptr<task::TaskData> task = task::TaskData::createTask(
                    [service, path, driveIdx](
                        boost::system::error_code ec, sdbusplus::message_t& msg,
                        const std::shared_ptr<task::TaskData>& taskData) {
                        if (ec)
                        {
                            taskData->finishTask();
                            taskData->state = "Aborted";
                            taskData->messages.emplace_back(
                                messages::resourceErrorsDetectedFormatError(
                                    "Drive SecureErase", ec.message()));
                            return task::completed;
                        }

                        std::string iface;
                        boost::container::flat_map<
                            std::string, dbus::utility::DbusVariantType>
                            values;

                        std::string index = std::to_string(taskData->index);
                        msg.read(iface, values);

                        if (iface != "xyz.openbmc_project.Common.Progress")
                        {
                            return !task::completed;
                        }
                        auto findStatus = values.find("Status");
                        if (findStatus != values.end())
                        {
                            std::string* state =
                                std::get_if<std::string>(&(findStatus->second));
                            if (state == nullptr)
                            {
                                taskData->messages.emplace_back(
                                    messages::internalError());
                                return !task::completed;
                            }

                            if (boost::ends_with(*state, "Aborted") ||
                                boost::ends_with(*state, "Failed"))
                            {
                                taskData->state = "Exception";
                                taskData->messages.emplace_back(
                                    messages::taskAborted(index));
                                sanitizeInProgress[driveIdx] = false;
                                return task::completed;
                            }

                            if (boost::ends_with(*state, "Completed"))
                            {
                                taskData->state = "Completed";
                                taskData->percentComplete = 100;
                                taskData->messages.emplace_back(
                                    messages::taskCompletedOK(index));
                                taskData->finishTask();
                                sanitizeInProgress[driveIdx] = false;
                                return task::completed;
                            }
                        }

                        auto findProgress = values.find("Progress");
                        if (findProgress == values.end())
                        {
                            return !task::completed;
                        }
                        uint8_t* progress =
                            std::get_if<uint8_t>(&(findProgress->second));
                        if (progress == nullptr)
                        {
                            taskData->messages.emplace_back(
                                messages::internalError());
                            return task::completed;
                        }
                        taskData->percentComplete = static_cast<int>(*progress);

                        BMCWEB_LOG_ERROR << taskData->percentComplete;
                        taskData->messages.emplace_back(
                            messages::taskProgressChanged(
                                index, static_cast<size_t>(*progress)));

                        return !task::completed;
                    },
                    "type='signal',interface='org.freedesktop.DBus.Properties',"
                    "member='PropertiesChanged',path='" + path + "'");
                task->startTimer(std::chrono::seconds(60));
                task->payload.emplace(req);
                task->populateResp(asyncResp->res);

                taskUris.clear();
                taskUris.push_back("/redfish/v1/TaskService/Tasks/" +
                                   std::to_string(task->index));
                auto methodName =
                    "xyz.openbmc_project.Inventory.Item.Drive.EraseMethod." +
                    sanitizeType;
                sanitizeInProgress[driveIdx] = true;
                // execute drive sanitize operation
                crow::connections::systemBus->async_method_call(
                    [req, asyncResp, driveIdx, 
                     task](const boost::system::error_code ec) {
                        if (ec)
                        {
                            task->state = "Aborted";
                            task->messages.emplace_back(
                                messages::resourceErrorsDetectedFormatError(
                                    "Drive secureErase", ec.message()));
                            task->finishTask();
                            sanitizeInProgress[driveIdx] = false;
                            return;
                        }
                    },
                    service, path, interface, "Erase", owPass, methodName);
                return;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", int32_t(0),
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Drive"});
}

inline void handleDriveSanitizetActionInfoGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId, const std::string& driveId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] = "#ActionInfo.v1_1_2.ActionInfo";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/Chassis/" + chassisId + "/Drives/"+ driveId +"/SanitizeActionInfo";
    asyncResp->res.jsonValue["Name"] = "Sanitize Action Info";
    asyncResp->res.jsonValue["Id"] = "SanitizeActionInfo";

    crow::connections::systemBus->async_method_call(
        [asyncResp,
         driveId](const boost::system::error_code ec,
                  const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Drive mapper call error";
                messages::internalError(asyncResp->res);
                return;
            }

            auto drive = std::find_if(
                subtree.begin(), subtree.end(),
                [&driveId](
                    const std::pair<std::string,
                                    dbus::utility::MapperServiceMap>& object) {
                    return sdbusplus::message::object_path(object.first)
                               .filename() == driveId;
                });
            if (drive == subtree.end())
            {
                messages::resourceNotFound(asyncResp->res, "Drive", driveId);
                return;
            }
            const std::string& path = drive->first;
            const dbus::utility::MapperServiceMap& connNames = drive->second;
            if (connNames.size() != 1)
            {
                BMCWEB_LOG_ERROR << "Connection size " << connNames.size()
                                 << ", not equal to 1";
                messages::internalError(asyncResp->res);
                return;
            }

            auto service = connNames[0].first;
            auto interfaces = connNames[0].second;
            for (const std::string& interface : interfaces)
            {
                if (interface != "xyz.openbmc_project.Inventory.Item.Drive")
                {
                    continue;
                }
                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus, service, path, interface,
                    "SanitizeCapability",
                    [asyncResp](const boost::system::error_code ec,
                                const std::vector<std::string>& cap) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "fail to get drive Progress";
                            return;
                        }
                        nlohmann::json::array_t parameters;
                        nlohmann::json::object_t parameter;
                        nlohmann::json::array_t allowed;

                        if (std::find(
                                cap.begin(), cap.end(),
                                "xyz.openbmc_project.Inventory.Item.Drive.EraseMethod.Overwrite") !=
                            cap.end())
                        {
                            parameter["Name"] = "OverwritePasses";
                            parameter["DataType"] = "integer";
                            parameters.push_back(std::move(parameter));

                            allowed.push_back("Overwrite");
                        }
                        if (std::find(
                                cap.begin(), cap.end(),
                                "xyz.openbmc_project.Inventory.Item.Drive.EraseMethod.BlockErase") !=
                            cap.end())
                        {
                            allowed.push_back("BlockErase");
                        }
                        if (std::find(
                                cap.begin(), cap.end(),
                                "xyz.openbmc_project.Inventory.Item.Drive.EraseMethod.CryptoErase") !=
                            cap.end())
                        {
                            allowed.push_back("CryptographicErase");
                        }
                        parameter["Name"] = "SanitizationType";
                        parameter["DataType"] = "string";

                        parameter["AllowableValues"] = std::move(allowed);
                        parameters.push_back(std::move(parameter));

                        asyncResp->res.jsonValue["Parameters"] =
                            std::move(parameters);
                    });
                break;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", int32_t(0),
        std::array<const char*, 1>{"xyz.openbmc_project.Inventory.Item.Drive"});
}

inline void requestRoutesDrive(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Storage/1/Drives/<str>/")
        .privileges(redfish::privileges::getDrive)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& systemName, const std::string& driveId) {
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

                crow::connections::systemBus->async_method_call(
                    [asyncResp,
                     driveId](const boost::system::error_code ec,
                              const dbus::utility::MapperGetSubTreeResponse&
                                  subtree) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "Drive mapper call error";
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        auto drive = std::find_if(
                            subtree.begin(), subtree.end(),
                            [&driveId](
                                const std::pair<
                                    std::string,
                                    dbus::utility::MapperServiceMap>& object) {
                                return sdbusplus::message::object_path(
                                           object.first)
                                           .filename() == driveId;
                            });

                        if (drive == subtree.end())
                        {
                            messages::resourceNotFound(asyncResp->res, "Drive",
                                                       driveId);
                            return;
                        }

                        const std::string& path = drive->first;
                        const dbus::utility::MapperServiceMap& connectionNames =
                            drive->second;

                        asyncResp->res.jsonValue["@odata.type"] =
                            "#Drive.v1_7_0.Drive";
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/Systems/" PLATFORMSYSTEMID
                            "/Storage/1/Drives/" +
                            driveId;
                        asyncResp->res.jsonValue["Name"] = driveId;
                        asyncResp->res.jsonValue["Id"] = driveId;

                        asyncResp->res
                            .jsonValue["Actions"]["#Drive.SecureErase"]
                                      ["target"] =
                            "/redfish/v1/Systems/" PLATFORMSYSTEMID
                            "/Storage/1/Drives/" +
                            driveId + "/Actions/Drive.SecureErase";
                        asyncResp->res
                            .jsonValue["Actions"]["#Drive.SecureErase"]
                                      ["@Redfish.ActionInfo"] =
                            "/redfish/v1/Systems/" PLATFORMSYSTEMID
                            "/Storage/1/Drives/" +
                            driveId +"/SanitizeActionInfo";
                        if (connectionNames.size() != 1)
                        {
                            BMCWEB_LOG_ERROR << "Connection size "
                                             << connectionNames.size()
                                             << ", not equal to 1";
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        getChassisID(asyncResp, driveId);

                        // default it to Enabled
                        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";

                        auto health =
                            std::make_shared<HealthPopulate>(asyncResp);
                        health->inventory.emplace_back(path);
                        health->selfPath = path;
                        health->populate();

                        addAllDriveInfo(asyncResp, connectionNames[0].first,
                                        path, connectionNames[0].second);
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                    "/xyz/openbmc_project/inventory", int32_t(0),
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item.Drive"});
            });

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/<str>/Storage/1/Drives/<str>/Actions/Drive.SecureErase")
        .privileges(redfish::privileges::postDrive)
        .methods(boost::beast::http::verb::post)(
            std::bind_front(handleDriveSanitizePost, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/<str>/Storage/1/Drives/<str>/SanitizeActionInfo")
        .privileges(redfish::privileges::getDrive)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleDriveSanitizetActionInfoGet, std::ref(app)));
}

/**
 * Chassis drives, this URL will show all the DriveCollection
 * information
 */
inline void chassisDriveCollectionGet(
    crow::App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    // mapper call lambda
    crow::connections::systemBus->async_method_call(
        [asyncResp,
         chassisId](const boost::system::error_code ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                if (ec == boost::system::errc::host_unreachable)
                {
                    messages::resourceNotFound(asyncResp->res, "Chassis",
                                               chassisId);
                    return;
                }
                messages::internalError(asyncResp->res);
                return;
            }

            // Iterate over all retrieved ObjectPaths.
            for (const auto& [path, connectionNames] : subtree)
            {
                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }

                if (connectionNames.empty())
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

                asyncResp->res.jsonValue["@odata.type"] =
                    "#DriveCollection.DriveCollection";
                asyncResp->res.jsonValue["@odata.id"] =
                    crow::utility::urlFromPieces("redfish", "v1", "Chassis",
                                                 chassisId, "Drives");
                asyncResp->res.jsonValue["Name"] = "Drive Collection";

                // Association lambda
                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus,
                    "xyz.openbmc_project.ObjectMapper", path + "/drive",
                    "xyz.openbmc_project.Association", "endpoints",
                    [asyncResp,
                     chassisId](const boost::system::error_code ec3,
                                const std::vector<std::string>& resp) {
                        if (ec3)
                        {
                            BMCWEB_LOG_ERROR
                                << "Error in chassis Drive association ";
                        }
                        nlohmann::json& members =
                            asyncResp->res.jsonValue["Members"];
                        // important if array is empty
                        members = nlohmann::json::array();

                        std::vector<std::string> leafNames;
                        for (const auto& drive : resp)
                        {
                            sdbusplus::message::object_path drivePath(drive);
                            leafNames.push_back(drivePath.filename());
                        }

                        std::sort(leafNames.begin(), leafNames.end(),
                                  AlphanumLess<std::string>());

                        for (const auto& leafName : leafNames)
                        {
                            nlohmann::json::object_t member;
                            member["@odata.id"] = crow::utility::urlFromPieces(
                                "redfish", "v1", "Chassis", chassisId, "Drives",
                                leafName);
                            members.push_back(std::move(member));
                            // navigation links will be registered in next patch
                            // set
                        }
                        asyncResp->res.jsonValue["Members@odata.count"] =
                            resp.size();
                    }); // end association lambda

            } // end Iterate over all retrieved ObjectPaths
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Board",
            "xyz.openbmc_project.Inventory.Item.Chassis"});
}

inline void requestRoutesChassisDrive(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Drives/")
        .privileges(redfish::privileges::getDriveCollection)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(chassisDriveCollectionGet, std::ref(app)));
}

inline void buildDrive(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& chassisId,
                       const std::string& driveName,
                       const boost::system::error_code ec,
                       const dbus::utility::MapperGetSubTreeResponse& subtree)
{

    if (ec)
    {
        BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
        messages::internalError(asyncResp->res);
        return;
    }

    // Iterate over all retrieved ObjectPaths.
    for (const auto& [path, connectionNames] : subtree)
    {
        sdbusplus::message::object_path objPath(path);
        if (objPath.filename() != driveName)
        {
            continue;
        }

        if (connectionNames.empty())
        {
            BMCWEB_LOG_ERROR << "Got 0 Connection names";
            continue;
        }

        asyncResp->res.jsonValue["@odata.id"] = crow::utility::urlFromPieces(
            "redfish", "v1", "Chassis", chassisId, "Drives", driveName);

        asyncResp->res.jsonValue["@odata.type"] = "#Drive.v1_7_0.Drive";
        asyncResp->res.jsonValue["Name"] = driveName;
        asyncResp->res.jsonValue["Id"] = driveName;
        // default it to Enabled
        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";

        auto health = std::make_shared<HealthPopulate>(asyncResp);
        health->inventory.emplace_back(path);
        health->selfPath = path;
        health->populate();

        nlohmann::json::object_t linkChassisNav;
        linkChassisNav["@odata.id"] =
            crow::utility::urlFromPieces("redfish", "v1", "Chassis", chassisId);
        asyncResp->res.jsonValue["Links"]["Chassis"] = linkChassisNav;
        asyncResp->res.jsonValue["Actions"]["#Drive.SecureErase"]["target"] =
            "/redfish/v1/Chassis/" + chassisId + "/Drives/" + driveName +
            "/Actions/Drive.SecureErase";
        asyncResp->res
            .jsonValue["Actions"]["#Drive.SecureErase"]["@Redfish.ActionInfo"] =
            "/redfish/v1/Chassis/" + chassisId + "/Drives/" + driveName +
            "/SanitizeActionInfo";

        addAllDriveInfo(asyncResp, connectionNames[0].first, path,
                        connectionNames[0].second);
    }
}

inline void
    matchAndFillDrive(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const std::string& chassisId,
                      const std::string& driveName,
                      const std::vector<std::string>& resp)
{

    for (const std::string& drivePath : resp)
    {
        sdbusplus::message::object_path path(drivePath);
        std::string leaf = path.filename();
        if (leaf != driveName)
        {
            continue;
        }
        //  mapper call drive
        const std::array<const char*, 1> driveInterface = {
            "xyz.openbmc_project.Inventory.Item.Drive"};

        crow::connections::systemBus->async_method_call(
            [asyncResp, chassisId, driveName](
                const boost::system::error_code ec,
                const dbus::utility::MapperGetSubTreeResponse& subtree) {
                buildDrive(asyncResp, chassisId, driveName, ec, subtree);
            },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
            "/xyz/openbmc_project/inventory", 0, driveInterface);
    }
}

inline void
    handleChassisDriveGet(crow::App& app, const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisId,
                          const std::string& driveName)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};

    // mapper call chassis
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisId,
         driveName](const boost::system::error_code ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            // Iterate over all retrieved ObjectPaths.
            for (const auto& [path, connectionNames] : subtree)
            {
                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }

                if (connectionNames.empty())
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus,
                    "xyz.openbmc_project.ObjectMapper", path + "/drive",
                    "xyz.openbmc_project.Association", "endpoints",
                    [asyncResp, chassisId,
                     driveName](const boost::system::error_code ec3,
                                const std::vector<std::string>& resp) {
                        if (ec3)
                        {
                            return; // no drives = no failures
                        }
                        matchAndFillDrive(asyncResp, chassisId, driveName,
                                          resp);
                    });
                break;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

/**
 * This URL will show the drive interface for the specific drive in the chassis
 */
inline void requestRoutesChassisDriveName(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Drives/<str>/")
        .privileges(redfish::privileges::getChassis)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleChassisDriveGet, std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Drives/<str>/Actions/Drive.SecureErase")
        .privileges(redfish::privileges::postDrive)
        .methods(boost::beast::http::verb::post)(
            std::bind_front(handleDriveSanitizePost, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/Chassis/<str>/Drives/<str>/SanitizeActionInfo")
        .privileges(redfish::privileges::getDrive)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleDriveSanitizetActionInfoGet, std::ref(app)));
}

} // namespace redfish
