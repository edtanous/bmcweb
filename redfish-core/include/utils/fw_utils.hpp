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
#include <async_resp.hpp>
#include <dbus_utility.hpp>
#include <sdbusplus/asio/property.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace redfish
{
namespace fw_util
{
/* @brief String that indicates a bios firmware instance */
constexpr const char* biosPurpose =
    "xyz.openbmc_project.Software.Version.VersionPurpose.Host";

/* @brief String that indicates a BMC firmware instance */
constexpr const char* bmcPurpose =
    "xyz.openbmc_project.Software.Version.VersionPurpose.BMC";

/* @brief String that indicates other firmware instance */
constexpr const char* otherPurpose =
    "xyz.openbmc_project.Software.Version.VersionPurpose.Other";

/**
 * @brief Populate the running firmware version and image links
 *
 * @param[i,o] aResp             Async response object
 * @param[i]   fwVersionPurpose  Indicates what target to look for
 * @param[i]   activeVersionPropName  Index in aResp->res.jsonValue to write
 * the running firmware version to
 * @param[i]   populateLinkToImages  Populate aResp->res "Links"
 * "ActiveSoftwareImage" with a link to the running firmware image and
 * "SoftwareImages" with a link to the all its firmware images
 *
 * @return void
 */
inline void
    populateFirmwareInformation(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& fwVersionPurpose,
                                const std::string& activeVersionPropName,
                                const bool populateLinkToImages)
{
    // Used later to determine running (known on Redfish as active) FW images
    sdbusplus::asio::getProperty<std::vector<std::string>>(
        *crow::connections::systemBus, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/software/functional",
        "xyz.openbmc_project.Association", "endpoints",
        [aResp, fwVersionPurpose, activeVersionPropName,
         populateLinkToImages](const boost::system::error_code ec,
                               const std::vector<std::string>& functionalFw) {
        BMCWEB_LOG_DEBUG("populateFirmwareInformation enter");
        if (ec)
        {
            BMCWEB_LOG_ERROR("error_code = {}", ec);
            BMCWEB_LOG_ERROR("error msg = {}", ec.message());
            messages::internalError(aResp->res);
            return;
        }

        if (functionalFw.size() == 0)
        {
            // Could keep going and try to populate SoftwareImages but
            // something is seriously wrong, so just fail
            BMCWEB_LOG_ERROR("Zero functional software in system");
            messages::internalError(aResp->res);
            return;
        }

        std::vector<std::string> functionalFwIds;
        // example functionalFw:
        // v as 2 "/xyz/openbmc_project/software/ace821ef"
        //        "/xyz/openbmc_project/software/230fb078"
        for (auto& fw : functionalFw)
        {
            sdbusplus::message::object_path path(fw);
            std::string leaf = path.filename();
            if (leaf.empty())
            {
                continue;
            }

            functionalFwIds.push_back(leaf);
        }

        crow::connections::systemBus->async_method_call(
            [aResp, fwVersionPurpose, activeVersionPropName,
             populateLinkToImages, functionalFwIds](
                const boost::system::error_code ec2,
                const std::vector<std::pair<
                    std::string, std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>>&
                    subtree) {
            if (ec2)
            {
                BMCWEB_LOG_ERROR("error_code = {}", ec2);
                BMCWEB_LOG_ERROR("error msg = {}", ec2.message());
                messages::internalError(aResp->res);
                return;
            }

            BMCWEB_LOG_DEBUG("Found {} images", subtree.size());

            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     obj : subtree)
            {
                sdbusplus::message::object_path path(obj.first);
                std::string swId = path.filename();
                if (swId.empty())
                {
                    messages::internalError(aResp->res);
                    BMCWEB_LOG_ERROR("Invalid firmware ID");

                    return;
                }

                bool runningImage = false;
                // Look at Ids from
                // /xyz/openbmc_project/software/functional
                // to determine if this is a running image
                if (std::find(functionalFwIds.begin(), functionalFwIds.end(),
                              swId) != functionalFwIds.end())
                {
                    runningImage = true;
                }

                // Now grab its version info
                crow::connections::systemBus->async_method_call(
                    [aResp, swId, runningImage, fwVersionPurpose,
                     activeVersionPropName, populateLinkToImages](
                        const boost::system::error_code ec3,
                        const boost::container::flat_map<
                            std::string, dbus::utility::DbusVariantType>&
                            propertiesList) {
                    if (ec3)
                    {
                        BMCWEB_LOG_ERROR("error_code = {}", ec3);
                        BMCWEB_LOG_ERROR("error msg = {}", ec3.message());
                        messages::internalError(aResp->res);
                        return;
                    }
                    // example propertiesList
                    // a{sv} 2 "Version" s
                    // "IBM-witherspoon-OP9-v2.0.10-2.22" "Purpose"
                    // s
                    // "xyz.openbmc_project.Software.Version.VersionPurpose.Host"

                    boost::container::flat_map<
                        std::string,
                        dbus::utility::DbusVariantType>::const_iterator it =
                        propertiesList.find("Purpose");
                    if (it == propertiesList.end())
                    {
                        BMCWEB_LOG_ERROR("Can't find property \"Purpose\"!");
                        messages::internalError(aResp->res);
                        return;
                    }
                    const std::string* swInvPurpose =
                        std::get_if<std::string>(&it->second);
                    if (swInvPurpose == nullptr)
                    {
                        BMCWEB_LOG_ERROR("wrong types for "
                                         "property \"Purpose\"!");
                        messages::internalError(aResp->res);
                        return;
                    }

                    BMCWEB_LOG_DEBUG("Image ID: {}", swId);
                    BMCWEB_LOG_DEBUG("Image purpose: {}", *swInvPurpose);
                    BMCWEB_LOG_DEBUG("Running image: {}", runningImage);

                    if (*swInvPurpose != fwVersionPurpose)
                    {
                        // Not purpose we're looking for
                        return;
                    }

                    if (populateLinkToImages)
                    {
                        nlohmann::json& softwareImageMembers =
                            aResp->res.jsonValue["Links"]["SoftwareImages"];
                        // Firmware images are at
                        // /redfish/v1/UpdateService/FirmwareInventory/<Id>
                        // e.g. .../FirmwareInventory/82d3ec86
                        softwareImageMembers.push_back(
                            {{"@odata.id", "/redfish/v1/UpdateService/"
                                           "FirmwareInventory/" +
                                               swId}});
                        aResp->res
                            .jsonValue["Links"]["SoftwareImages@odata.count"] =
                            softwareImageMembers.size();

                        if (runningImage)
                        {
                            // Create the link to the running image
                            aResp->res
                                .jsonValue["Links"]["ActiveSoftwareImage"] = {
                                {"@odata.id", "/redfish/v1/UpdateService/"
                                              "FirmwareInventory/" +
                                                  swId}};
                        }
                    }
                    if (!activeVersionPropName.empty() && runningImage)
                    {
                        it = propertiesList.find("Version");
                        if (it == propertiesList.end())
                        {
                            BMCWEB_LOG_ERROR("Can't find property "
                                             "\"Version\"!");
                            messages::internalError(aResp->res);
                            return;
                        }
                        const std::string* version =
                            std::get_if<std::string>(&it->second);
                        if (version == nullptr)
                        {
                            BMCWEB_LOG_ERROR("Error getting fw version");
                            messages::internalError(aResp->res);
                            return;
                        }

                        aResp->res.jsonValue[activeVersionPropName] = *version;
                    }
                },
                    obj.second[0].first, obj.first,
                    "org.freedesktop.DBus.Properties", "GetAll",
                    "xyz.openbmc_project.Software.Version");
            }
        },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
            "/xyz/openbmc_project/software", static_cast<int32_t>(0),
            std::array<const char*, 1>{"xyz.openbmc_project.Software.Version"});
    });

    return;
}

/**
 * @brief Translate input fwState to Redfish state
 *
 * This function will return the corresponding Redfish state
 *
 * @param[i]   fwState  The OpenBMC firmware state
 *
 * @return The corresponding Redfish state
 */
inline std::string getRedfishFWState(const std::string& fwState)
{
    if (fwState == "xyz.openbmc_project.Software.Activation.Activations.Active")
    {
        return "Enabled";
    }
    if (fwState == "xyz.openbmc_project.Software.Activation."
                   "Activations.Activating")
    {
        return "Updating";
    }
    if (fwState == "xyz.openbmc_project.Software.Activation."
                   "Activations.StandbySpare")
    {
        return "StandbySpare";
    }
    BMCWEB_LOG_DEBUG("Default fw state {} to Disabled", fwState);
    return "Disabled";
}

/**
 * @brief Translate input fwState to Redfish health state
 *
 * This function will return the corresponding Redfish health state
 *
 * @param[i]   fwState  The OpenBMC firmware state
 *
 * @return The corresponding Redfish health state
 */
inline std::string getRedfishFWHealth(const std::string& fwState)
{
    if ((fwState ==
         "xyz.openbmc_project.Software.Activation.Activations.Active") ||
        (fwState == "xyz.openbmc_project.Software.Activation.Activations."
                    "Activating") ||
        (fwState ==
         "xyz.openbmc_project.Software.Activation.Activations.Ready"))
    {
        return "OK";
    }
    BMCWEB_LOG_DEBUG("FW state {} to Warning", fwState);
    return "Warning";
}

/**
 * @brief Put recovery status of input swId into json response
 *
 * This function will put the appropriate Redfish state of the input
 * firmware id to ["Status"]["State"] within the json response
 *
 * @param[i,o] aResp    Async response object
 * @param[i]   swId     The software ID to get status for
 * @param[i]   dbusSvc  The dbus service implementing the software object
 *
 * @return void
 */
inline void
    getFwRecoveryStatus(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::shared_ptr<std::string>& swId,
                        const std::string& dbusSvc)
{
    auto getLastSegnmentFromDotterString =
        [](const std::string input) -> std::string {
        size_t pos = input.rfind(".");
        if (pos == std::string::npos)
        {
            BMCWEB_LOG_ERROR("Unable to extract last segment from input {}",
                             input);
            return "";
        }
        return input.substr(pos + 1);
    };

    BMCWEB_LOG_DEBUG("getFwRecoveryStatus: swId {} svc {}", *swId, dbusSvc);

    crow::connections::systemBus->async_method_call(
        [asyncResp, swId, &getLastSegnmentFromDotterString](
            const boost::system::error_code errorCode,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& propertiesList) {
        if (errorCode)
        {
            // OK since not all fwtypes support recovery
            return;
        }

        const auto& it = propertiesList.find("Health");
        if (it == propertiesList.end())
        {
            BMCWEB_LOG_ERROR(
                "Can't find D-Bus property \"xyz.openbmc_project.State.Decorator.Health.Health\"!");
            messages::propertyMissing(asyncResp->res, "Health");
            return;
        }

        const std::string* health = std::get_if<std::string>(&it->second);
        if (health == nullptr)
        {
            BMCWEB_LOG_ERROR(
                "wrong types for D-Bus property \"xyz.openbmc_project.State.Decorator.Health.Health\"!");
            messages::propertyValueTypeError(asyncResp->res, "", "Health");
            return;
        }
        BMCWEB_LOG_DEBUG("getFwRecoveryStatus: Health {}", *health);
        asyncResp->res.jsonValue["Status"]["Health"] =
            getLastSegnmentFromDotterString(*health);
    },
        dbusSvc, "/xyz/openbmc_project/software/" + *swId,
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.State.Decorator.Health");

    crow::connections::systemBus->async_method_call(
        [asyncResp, swId, &getLastSegnmentFromDotterString](
            const boost::system::error_code errorCode,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& propertiesList) {
        if (errorCode)
        {
            // OK since not all fwtypes support recovery
            return;
        }

        const auto& it = propertiesList.find("State");
        if (it == propertiesList.end())
        {
            BMCWEB_LOG_ERROR(
                "Can't find property \"xyz.openbmc_project.State.Decorator.OperationalStatus.State\"!");
            messages::propertyMissing(asyncResp->res, "State");
            return;
        }

        const std::string* state = std::get_if<std::string>(&it->second);
        if (state == nullptr)
        {
            BMCWEB_LOG_ERROR(
                "wrong types for property\"xyz.openbmc_project.State.Decorator.OperationalStatus.State\"!");
            messages::propertyValueTypeError(asyncResp->res, "", "State");
            return;
        }
        BMCWEB_LOG_DEBUG("getFwRecoveryStatus: State {}", *state);
        asyncResp->res.jsonValue["Status"]["State"] =
            getLastSegnmentFromDotterString(*state);
    },
        dbusSvc, "/xyz/openbmc_project/software/" + *swId,
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.State.Decorator.OperationalStatus");
}

/**
 * @brief Put status of input swId into json response
 *
 * This function will put the appropriate Redfish state of the input
 * firmware id to ["Status"]["State"] within the json response
 *
 * @param[i,o] aResp    Async response object
 * @param[i]   swId     The software ID to get status for
 * @param[i]   dbusSvc  The dbus service implementing the software object
 *
 * @return void
 */
inline void getFwStatus(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::shared_ptr<std::string>& swId,
                        const std::string& dbusSvc)
{
    BMCWEB_LOG_DEBUG("getFwStatus: swId {} svc {}", *swId, dbusSvc);

    crow::connections::systemBus->async_method_call(
        [asyncResp, swId](
            const boost::system::error_code errorCode,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& propertiesList) {
        if (errorCode)
        {
            // not all fwtypes are updateable, this is ok
            asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
            return;
        }
        boost::container::flat_map<
            std::string, dbus::utility::DbusVariantType>::const_iterator it =
            propertiesList.find("Activation");
        if (it == propertiesList.end())
        {
            BMCWEB_LOG_DEBUG("Can't find property \"Activation\"!");
            messages::propertyMissing(asyncResp->res, "Activation");
            return;
        }
        const std::string* swInvActivation =
            std::get_if<std::string>(&it->second);
        if (swInvActivation == nullptr)
        {
            BMCWEB_LOG_DEBUG("wrong types for property\"Activation\"!");
            messages::propertyValueTypeError(asyncResp->res, "", "Activation");
            return;
        }
        BMCWEB_LOG_DEBUG("getFwStatus: Activation {}", *swInvActivation);
        asyncResp->res.jsonValue["Status"]["State"] =
            getRedfishFWState(*swInvActivation);
        asyncResp->res.jsonValue["Status"]["Health"] =
            getRedfishFWHealth(*swInvActivation);
    },
        dbusSvc, "/xyz/openbmc_project/software/" + *swId,
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Activation");
}

/**
 * @brief Get status WriteProtected of input swId into json response
 *
 * This function will put the appropriate Redfish state of the input
 * firmware id to ["WriteProtected"] within the json response
 *
 * @param[i,o] aResp    Async response object
 * @param[i]   swId     The software ID to get status for
 * @param[i]   dbusSvc  The dbus service implementing the software object
 *
 * @return void
 */
inline void getFwWriteProtectedStatus(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::shared_ptr<std::string>& swId, const std::string& dbusSvc)
{
    BMCWEB_LOG_DEBUG("getFwWriteProtectedStatus: swId {} serviceName {}", *swId,
                     dbusSvc);

    crow::connections::systemBus->async_method_call(
        [asyncResp, swId](
            const boost::system::error_code errorCode,
            const boost::container::flat_map<
                std::string, dbus::utility::DbusVariantType>& propertiesList) {
        if (errorCode)
        {
            return;
        }
        boost::container::flat_map<
            std::string, dbus::utility::DbusVariantType>::const_iterator it =
            propertiesList.find("WriteProtected");
        if (it == propertiesList.end())
        {
            BMCWEB_LOG_DEBUG("Can't find property \"WriteProtected\"!");
            return;
        }
        const bool* writeProtected = std::get_if<bool>(&it->second);
        if (writeProtected == nullptr)
        {
            BMCWEB_LOG_DEBUG("wrong types for property\"WriteProtected\"!");
            messages::propertyValueTypeError(asyncResp->res, "",
                                             "WriteProtected");
            return;
        }
        asyncResp->res.jsonValue["WriteProtected"] = *writeProtected;
    },
        dbusSvc, "/xyz/openbmc_project/software/" + *swId,
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Settings");
}

/**
 * @brief Put status WriteProtected of input swId into json response
 *
 * This function will put the appropriate Redfish state of the input
 * firmware id to ["WriteProtected"] within the json response
 *
 * @param[i,o] aResp    Async response object
 * @param[i]   swId     The software ID to get status for
 * @param[i]   dbusSvc  The dbus service implementing the software object
 *
 * @return void
 */
inline void patchFwWriteProtectedStatus(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::shared_ptr<std::string>& swId, const std::string& dbusSvc,
    const bool writeProtected)
{
    BMCWEB_LOG_DEBUG("patchFwWriteProtectedStatus: swId {} serviceName {}",
                     *swId, dbusSvc);
    crow::connections::systemBus->async_method_call(
        [asyncResp, swId](const boost::system::error_code ec,
                          sdbusplus::message::message& msg) {
        if (!ec)
        {
            BMCWEB_LOG_DEBUG("Set WriteProtect succeeded");
            messages::success(asyncResp->res);
            return;
        }

        BMCWEB_LOG_DEBUG("SWInventory:{} set writeprotect property failed: {}",
                         *swId, ec);
        // Read and convert dbus error message to redfish error
        const sd_bus_error* dbusError = msg.get_error();
        if (dbusError == nullptr)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                    "Device.Error.WriteFailure") == 0)
        {
            // Service failed to change writeproteect
            messages::operationFailed(asyncResp->res);
        }
        if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                    "Error.NotAllowed") == 0)
        {
            // pcieswitch wp error
            messages::propertyNotWritable(asyncResp->res, "WriteProtected");
        }
        else
        {
            messages::internalError(asyncResp->res);
        }
    },
        dbusSvc, "/xyz/openbmc_project/software/" + *swId,
        "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Software.Settings", "WriteProtected",
        dbus::utility::DbusVariantType(writeProtected));
}

/**
 * @brief Updates programmable status of input swId into json response
 *
 * This function checks whether firmware inventory component
 * can be programmable or not and fill's the "Updateable"
 * Property.
 *
 * @param[i,o] asyncResp  Async response object
 * @param[i]   fwId       The firmware ID
 */
inline void
    getFwUpdateableStatus(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::shared_ptr<std::string>& fwId,
                          std::string inventoryPath = "")
{
    if (inventoryPath.empty())
    {
        inventoryPath = "/xyz/openbmc_project/software/";
    }
    sdbusplus::asio::getProperty<std::vector<std::string>>(
        *crow::connections::systemBus, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/software/updateable",
        "xyz.openbmc_project.Association", "endpoints",
        [asyncResp, fwId,
         inventoryPath](const boost::system::error_code ec,
                        const std::vector<std::string>& objPaths) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG(" error_code = {} error msg =  {}", ec,
                             ec.message());
            // System can exist with no updateable firmware,
            // so don't throw error here.
            return;
        }
        std::string reqFwObjPath = inventoryPath + *fwId;

        if (std::find(objPaths.begin(), objPaths.end(), reqFwObjPath) !=
            objPaths.end())
        {
            asyncResp->res.jsonValue["Updateable"] = true;
            return;
        }
    });

    return;
}

} // namespace fw_util
} // namespace redfish
