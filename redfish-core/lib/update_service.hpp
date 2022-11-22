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
#include "commit_image.hpp"
#include "background_copy.hpp"

#include <app.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>
#include <update_messages.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/fw_utils.hpp>
#include <utils/sw_utils.hpp>

namespace redfish
{

// Match signals added on software path
static std::unique_ptr<sdbusplus::bus::match_t> fwUpdateMatcher;
// Only allow one update at a time
static bool fwUpdateInProgress = false;
// Timer for software available
static std::unique_ptr<boost::asio::steady_timer> fwAvailableTimer;
// match for logging
static std::unique_ptr<sdbusplus::bus::match::match> loggingMatch = nullptr;

inline static void cleanUp()
{
    fwUpdateInProgress = false;
    fwUpdateMatcher = nullptr;
}
inline static void activateImage(const std::string& objPath,
                                 const std::string& service)
{
    BMCWEB_LOG_DEBUG << "Activate image for " << objPath << " " << service;
    crow::connections::systemBus->async_method_call(
        [](const boost::system::error_code errorCode) {
            if (errorCode)
            {
                BMCWEB_LOG_DEBUG << "error_code = " << errorCode;
                BMCWEB_LOG_DEBUG << "error msg = " << errorCode.message();
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.Software.Activation", "RequestedActivation",
        dbus::utility::DbusVariantType(
            "xyz.openbmc_project.Software.Activation.RequestedActivations.Active"));
}

static void handleLogMatchCallback(sdbusplus::message_t& m,
                                   nlohmann::json& messages)
{
    std::vector<std::pair<std::string, dbus::utility::DBusPropertiesMap>>
        interfacesProperties;
    sdbusplus::message::object_path objPath;
    m.read(objPath, interfacesProperties);
    for (auto interface : interfacesProperties)
    {
        if (interface.first == "xyz.openbmc_project.Logging.Entry")
        {
            std::string rfMessage = "";
            std::string resolution = "";
            std::string messageNamespace = "";
            std::vector<std::string> rfArgs;
            const std::vector<std::string>* vData = nullptr;
            for (auto& propertyMap : interface.second)
            {
                if (propertyMap.first == "AdditionalData")
                {
                    vData = std::get_if<std::vector<std::string>>(
                        &propertyMap.second);

                    for (auto& kv : *vData)
                    {
                        std::vector<std::string> fields;
                        boost::split(fields, kv, boost::is_any_of("="));
                        if (fields[0] == "REDFISH_MESSAGE_ID")
                        {
                            rfMessage = fields[1];
                        }
                        else if (fields[0] == "REDFISH_MESSAGE_ARGS")
                        {
                            boost::split(rfArgs, fields[1],
                                         boost::is_any_of(","));
                        }
                        else if (fields[0] == "namespace")
                        {
                            messageNamespace = fields[1];
                        }
                    }
                }
                if (propertyMap.first == "Resolution")
                {
                    const std::string* value =
                        std::get_if<std::string>(&propertyMap.second);
                    if (value != nullptr)
                    {
                        resolution = *value;
                    }
                }
            }
            /* we need to have found the id, data, this image needs to
               correspond to the image we are working with right now and the
               message should be update related */
            if (vData == nullptr || messageNamespace != "FWUpdate")
            {
                // something is invalid
                BMCWEB_LOG_DEBUG << "Got invalid log message";
            }
            else
            {
                auto message =
                    redfish::messages::getUpdateMessage(rfMessage, rfArgs);
                if (message.find("Message") != message.end())
                {

                    if (resolution != "")
                    {
                        message["Resolution"] = resolution;
                    }
                    messages.emplace_back(message);
                }
                else
                {
                    BMCWEB_LOG_ERROR << "Unknown message ID: " << rfMessage;
                }
            }
        }
    }
}

static void loggingMatchCallback(const std::shared_ptr<task::TaskData>& task,
                                 sdbusplus::message_t& m)
{
    if (task == nullptr)
    {
        return;
    }
    handleLogMatchCallback(m, task->messages);
}

static nlohmann::json preTaskMessages = {};
static void preTaskLoggingHandler(sdbusplus::message_t& m)
{
    handleLogMatchCallback(m, preTaskMessages);
}

// Note that asyncResp can be either a valid pointer or nullptr. If nullptr
// then no asyncResp updates will occur
static void
    softwareInterfaceAdded(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           sdbusplus::message_t& m,
                           task::Payload&& payload)
{
    dbus::utility::DBusInteracesMap interfacesProperties;

    sdbusplus::message::object_path objPath;

    m.read(objPath, interfacesProperties);

    BMCWEB_LOG_DEBUG << "obj path = " << objPath.str;
    for (auto& interface : interfacesProperties)
    {
        BMCWEB_LOG_DEBUG << "interface = " << interface.first;

        if (interface.first == "xyz.openbmc_project.Software.Activation")
        {
            // Retrieve service and activate
            crow::connections::systemBus->async_method_call(
                [objPath, asyncResp, payload(std::move(payload))](
                    const boost::system::error_code errorCode,
                    const std::vector<
                        std::pair<std::string, std::vector<std::string>>>&
                        objInfo) mutable {
                    if (errorCode)
                    {
                        BMCWEB_LOG_ERROR << "error_code = " << errorCode;
                        BMCWEB_LOG_ERROR << "error msg = "
                                         << errorCode.message();
                        if (asyncResp)
                        {
                            messages::internalError(asyncResp->res);
                        }
                        cleanUp();
                        return;
                    }
                    // Ensure we only got one service back
                    if (objInfo.size() != 1)
                    {
                        BMCWEB_LOG_ERROR << "Invalid Object Size "
                                         << objInfo.size();
                        if (asyncResp)
                        {
                            messages::internalError(asyncResp->res);
                        }
                        cleanUp();
                        return;
                    }
                    // cancel timer only when
                    // xyz.openbmc_project.Software.Activation interface
                    // is added
                    fwAvailableTimer = nullptr;
                    sdbusplus::message::object_path objectPath(objPath.str);
                    std::string swID = objectPath.filename();
                    if (swID.empty())
                    {
                        BMCWEB_LOG_ERROR << "Software Id is empty";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    if (asyncResp)
                    {
                        auto messageCallback =
                            [swID, objPath](const std::string_view state,
                                            [[maybe_unused]] size_t index) {
                                nlohmann::json message{};
                                if (state == "Started")
                                {
                                    message = messages::taskStarted(
                                        std::to_string(index));
                                }
                                else if (state == "Aborted")
                                {
                                    fwUpdateInProgress = false;
                                    message = messages::taskAborted(
                                        std::to_string(index));
                                }
                                else if (state == "Completed")
                                {
                                    message = messages::taskCompletedOK(
                                        std::to_string(index));
                                    // fwupdate status is set in task callback
                                }
                                else
                                {
                                    BMCWEB_LOG_INFO << "State not good";
                                }
                                return message;
                            };
                        std::shared_ptr<task::TaskData> task =
                            task::TaskData::createTask(
                                [](boost::system::error_code ec,
                                   sdbusplus::message_t& msg,
                                   const std::shared_ptr<task::TaskData>&
                                       taskData) {
                                    if (ec)
                                    {
                                        return task::completed;
                                    }

                                    std::string iface;
                                    boost::container::flat_map<
                                        std::string,
                                        dbus::utility::DbusVariantType>
                                        values;

                                    std::string index =
                                        std::to_string(taskData->index);
                                    msg.read(iface, values);

                                    if (iface ==
                                        "xyz.openbmc_project.Software.Activation")
                                    {
                                        auto findActivation =
                                            values.find("Activation");
                                        if (findActivation == values.end())
                                        {
                                            return !task::completed;
                                        }
                                        std::string* state =
                                            std::get_if<std::string>(
                                                &(findActivation->second));
                                        if (state == nullptr)
                                        {
                                            taskData->messages.emplace_back(
                                                messages::internalError());
                                            fwUpdateInProgress = false;
                                            return task::completed;
                                        }

                                        if (boost::ends_with(*state,
                                                             "Invalid") ||
                                            boost::ends_with(*state, "Failed"))
                                        {
                                            taskData->state = "Exception";
                                            taskData->status = "Warning";
                                            taskData->messages.emplace_back(
                                                messages::taskAborted(index));
                                            fwUpdateInProgress = false;
                                            return task::completed;
                                        }

                                        if (boost::ends_with(*state, "Staged"))
                                        {

                                            taskData->state = "Stopping";
                                            taskData->messages.emplace_back(
                                                messages::taskPaused(index));
                                            // its staged, set a long timer to
                                            // allow them time to complete the
                                            // update (probably cycle the
                                            // system) if this expires then
                                            // task will be cancelled
                                            taskData->extendTimer(
                                                std::chrono::hours(5));
                                            fwUpdateInProgress = true;
                                            return !task::completed;
                                        }

                                        if (boost::ends_with(*state,
                                                             "Activating"))
                                        {
                                            // set firmware inventory inprogress
                                            // flag to true during activation.
                                            // this will ensure no furthur
                                            // updates allowed during this time
                                            // from redfish
                                            fwUpdateInProgress = true;
                                            return !task::completed;
                                        }

                                        if (boost::ends_with(*state, "Active"))
                                        {
                                            taskData->state = "Completed";
                                            taskData->messages.emplace_back(
                                                messages::taskCompletedOK(
                                                    index));
                                            fwUpdateInProgress = false;
                                            return task::completed;
                                        }
                                    }
                                    else if (
                                        iface ==
                                        "xyz.openbmc_project.Software.ActivationProgress")
                                    {
                                        auto findProgress =
                                            values.find("Progress");
                                        if (findProgress == values.end())
                                        {
                                            return !task::completed;
                                        }
                                        uint8_t* progress =
                                            std::get_if<uint8_t>(
                                                &(findProgress->second));

                                        if (progress == nullptr)
                                        {
                                            taskData->messages.emplace_back(
                                                messages::internalError());
                                            return task::completed;
                                        }
                                        taskData->percentComplete =
                                            static_cast<int>(*progress);

                                        taskData->messages.emplace_back(
                                            messages::taskProgressChanged(
                                                index, static_cast<size_t>(
                                                           *progress)));

                                        // if we're getting status updates it's
                                        // still alive, update timer
                                        taskData->extendTimer(
                                            std::chrono::minutes(
                                                updateServiceTaskTimeout));
                                    }

                                    // as firmware update often results in a
                                    // reboot, the task  may never "complete"
                                    // unless it is an error

                                    return !task::completed;
                                },
                                "type='signal',interface='org.freedesktop.DBus.Properties',"
                                "member='PropertiesChanged',path='" +
                                    objPath.str + "'",
                                messageCallback);
                        task->startTimer(
                            std::chrono::minutes(updateServiceTaskTimeout));
                        task->populateResp(asyncResp->res);
                        task->payload.emplace(std::move(payload));
                        loggingMatch = std::make_unique<
                            sdbusplus::bus::match::match>(
                            *crow::connections::systemBus,
                            "interface='org.freedesktop.DBus.ObjectManager',type='signal',"
                            "member='InterfacesAdded',"
                            "path='/xyz/openbmc_project/logging'",
                            [task](sdbusplus::message_t& msgLog) {
                                loggingMatchCallback(task, msgLog);
                            });
                        if (preTaskMessages.size() > 0)
                        {
                            task->messages.emplace_back(preTaskMessages);
                        }
                        preTaskMessages = {};
                    }
                    fwUpdateInProgress = false;
                    activateImage(objPath.str, objInfo[0].first);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject", objPath.str,
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Activation"});
            break;
        }
    }
}

// Note that asyncResp can be either a valid pointer or nullptr. If nullptr
// then no asyncResp updates will occur
static bool monitorForSoftwareAvailable(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const crow::Request& req, int timeoutTimeSeconds = 10)
{
    // Only allow one FW update at a time
    if (fwUpdateInProgress != false)
    {
        if (asyncResp)
        {
            std::string resolution =
                "Another update is in progress. Retry"
                " the update operation once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
        }
        return true;
    }

    fwAvailableTimer =
        std::make_unique<boost::asio::steady_timer>(*req.ioService);

    fwAvailableTimer->expires_after(std::chrono::seconds(timeoutTimeSeconds));

    fwAvailableTimer->async_wait(
        [asyncResp](const boost::system::error_code& ec) {
            cleanUp();
            if (ec == boost::asio::error::operation_aborted)
            {
                // expected, we were canceled before the timer completed.
                return;
            }
            BMCWEB_LOG_ERROR
                << "Timed out waiting for firmware object being created";
            BMCWEB_LOG_ERROR
                << "FW image may has already been uploaded to server";
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Async_wait failed" << ec;
                return;
            }
            if (asyncResp)
            {
                redfish::messages::internalError(asyncResp->res);
            }
        });

    task::Payload payload(req);
    auto callback = [asyncResp,
                     payload](sdbusplus::message_t& m) mutable {
        BMCWEB_LOG_DEBUG << "Match fired";
        softwareInterfaceAdded(asyncResp, m, std::move(payload));
    };

    fwUpdateInProgress = true;

    fwUpdateMatcher = std::make_unique<sdbusplus::bus::match::match>(
        *crow::connections::systemBus,
        "interface='org.freedesktop.DBus.ObjectManager',type='signal',"
        "member='InterfacesAdded',path='/'",
        callback);

    loggingMatch = std::make_unique<sdbusplus::bus::match::match>(
        *crow::connections::systemBus,
        "interface='org.freedesktop.DBus.ObjectManager',type='signal',"
        "member='InterfacesAdded',"
        "path='/xyz/openbmc_project/logging'",
        preTaskLoggingHandler);
    return false;
}

/**
 * UpdateServiceActionsSimpleUpdate class supports handle POST method for
 * SimpleUpdate action.
 */
inline void requestRoutesUpdateServiceActionsSimpleUpdate(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/UpdateService/Actions/UpdateService.SimpleUpdate/")
        .privileges(redfish::privileges::postUpdateService)
        .methods(
            boost::beast::http::verb::
                post)([&app](
                          const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }

            std::optional<std::string> transferProtocol;
            std::string imageURI;

            BMCWEB_LOG_DEBUG << "Enter UpdateService.SimpleUpdate doPost";

            // User can pass in both TransferProtocol and ImageURI parameters or
            // they can pass in just the ImageURI with the transfer protocol
            // embedded within it.
            // 1) TransferProtocol:TFTP ImageURI:1.1.1.1/myfile.bin
            // 2) ImageURI:tftp://1.1.1.1/myfile.bin

            if (!json_util::readJsonAction(req, asyncResp->res, "TransferProtocol",
                                     transferProtocol, "ImageURI", imageURI))
            {
                BMCWEB_LOG_DEBUG
                    << "Missing TransferProtocol or ImageURI parameter";
                return;
            }
            if (!transferProtocol)
            {
                // Must be option 2
                // Verify ImageURI has transfer protocol in it
                size_t separator = imageURI.find(':');
                if ((separator == std::string::npos) ||
                    ((separator + 1) > imageURI.size()))
                {
                    messages::actionParameterValueTypeError(
                        asyncResp->res, imageURI, "ImageURI",
                        "UpdateService.SimpleUpdate");
                    BMCWEB_LOG_ERROR << "ImageURI missing transfer protocol: "
                                     << imageURI;
                    return;
                }
                transferProtocol = imageURI.substr(0, separator);
                // Ensure protocol is upper case for a common comparison path
                // below
                boost::to_upper(*transferProtocol);
                BMCWEB_LOG_DEBUG << "Encoded transfer protocol "
                                 << *transferProtocol;

                // Adjust imageURI to not have the protocol on it for parsing
                // below
                // ex. tftp://1.1.1.1/myfile.bin -> 1.1.1.1/myfile.bin
                imageURI = imageURI.substr(separator + 3);
                BMCWEB_LOG_DEBUG << "Adjusted imageUri " << imageURI;
            }
            // OpenBMC currently only supports TFTP
            if (*transferProtocol != "TFTP")
            {
                messages::actionParameterNotSupported(
                    asyncResp->res, "TransferProtocol",
                    "UpdateService.SimpleUpdate");
                BMCWEB_LOG_ERROR << "Request incorrect protocol parameter: "
                                 << *transferProtocol;
                return;
            }

            // Format should be <IP or Hostname>/<file> for imageURI
            size_t separator = imageURI.find('/');
            if ((separator == std::string::npos) ||
                ((separator + 1) > imageURI.size()))
            {
                messages::actionParameterValueTypeError(
                    asyncResp->res, imageURI, "ImageURI",
                    "UpdateService.SimpleUpdate");
                BMCWEB_LOG_ERROR << "Invalid ImageURI: " << imageURI;
                return;
            }

            std::string tftpServer = imageURI.substr(0, separator);
            std::string fwFile = imageURI.substr(separator + 1);
            BMCWEB_LOG_DEBUG << "Server: " << tftpServer + " File: " << fwFile;

            // Setup callback for when new software detected
            // Give TFTP 10 minutes to complete
            (void)monitorForSoftwareAvailable(asyncResp, req, 600);

            // TFTP can take up to 10 minutes depending on image size and
            // connection speed. Return to caller as soon as the TFTP operation
            // has been started. The callback above will ensure the activate
            // is started once the download has completed
            redfish::messages::success(asyncResp->res);

            // Call TFTP service
            crow::connections::systemBus->async_method_call(
                [](const boost::system::error_code ec) {
                    if (ec)
                    {
                        // messages::internalError(asyncResp->res);
                        cleanUp();
                        BMCWEB_LOG_DEBUG << "error_code = " << ec;
                        BMCWEB_LOG_DEBUG << "error msg = " << ec.message();
                    }
                    else
                    {
                        BMCWEB_LOG_DEBUG << "Call to DownloaViaTFTP Success";
                    }
                },
                "xyz.openbmc_project.Software.Download",
                "/xyz/openbmc_project/software",
                "xyz.openbmc_project.Common.TFTP", "DownloadViaTFTP", fwFile,
                tftpServer);

            BMCWEB_LOG_DEBUG << "Exit UpdateService.SimpleUpdate doPost";
        });
}

inline void
    handleUpdateServicePost(App& app, const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    BMCWEB_LOG_DEBUG << "doPost...";

    if (req.body.size() > firmwareImageLimitBytes)
    {
        if (asyncResp)
        {
            BMCWEB_LOG_ERROR << "Large image size: " << req.body.size();
            std::string resolution =
                            "Firmware package size is greater than allowed "
                            "size. Make sure package size is less than "
                            "UpdateService.MaxImageSizeBytes property and "
                            "retry the firmware update operation.";

            messages::resourceExhaustion(asyncResp->res,
                                         "/redfish/v1/UpdateService/", resolution);
        }
        return;
    }
    // Setup callback for when new software detected
    if (monitorForSoftwareAvailable(asyncResp, req))
    {
        // don't copy the image, update already in progress.
        BMCWEB_LOG_ERROR << "Update already in progress.";
    }
    else
    {
        std::string filepath(
            updateServiceImageLocation +
            boost::uuids::to_string(boost::uuids::random_generator()()));
        BMCWEB_LOG_DEBUG << "Writing file to " << filepath;
        std::ofstream out(filepath, std::ofstream::out | std::ofstream::binary |
                                        std::ofstream::trunc);
        out << req.body;
        out.close();
        BMCWEB_LOG_DEBUG << "file upload complete!!";
    }
}

inline void requestRoutesUpdateService(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/")
        .privileges(redfish::privileges::getUpdateService)
        .methods(boost::beast::http::verb::get)([&app](const crow::Request& req,
                                                       const std::shared_ptr<
                                                           bmcweb::AsyncResp>&
                                                           asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] =
                "#UpdateService.v1_5_0.UpdateService";
            asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/UpdateService";
            asyncResp->res.jsonValue["Id"] = "UpdateService";
            asyncResp->res.jsonValue["Description"] =
                "Service for Software Update";
            asyncResp->res.jsonValue["Name"] = "Update Service";

#ifdef BMCWEB_ENABLE_REDFISH_UPDATESERVICE_OLD_POST_URL
            // See note about later on in this file about why this is neccesary
            // This is "Wrong" per the standard, but is done temporarily to
            // avoid noise in failing tests as people transition to having this
            // option disabled
            asyncResp->res.addHeader(boost::beast::http::field::allow,
                                     "GET, PATCH, HEAD");
#endif

            asyncResp->res.jsonValue["HttpPushUri"] =
                "/redfish/v1/UpdateService/update";

            // UpdateService cannot be disabled
            asyncResp->res.jsonValue["ServiceEnabled"] = true;
            asyncResp->res.jsonValue["FirmwareInventory"]["@odata.id"] =
                "/redfish/v1/UpdateService/FirmwareInventory";
            asyncResp->res.jsonValue["SoftwareInventory"] = {
                {"@odata.id", "/redfish/v1/UpdateService/SoftwareInventory"}};
            // Get the MaxImageSizeBytes
            asyncResp->res.jsonValue["MaxImageSizeBytes"] =
                firmwareImageLimitBytes;
            asyncResp->res.jsonValue["Actions"]["Oem"]["Nvidia"]["#NvidiaUpdateService.CommitImage"]= {
                {"target", "/redfish/v1/UpdateService/Actions/Oem/NvidiaUpdateService.CommitImage"},
                {"@Redfish.ActionInfo", "/redfish/v1/UpdateService/Oem/Nvidia/CommitImageActionInfo"}};

#ifdef BMCWEB_INSECURE_ENABLE_REDFISH_FW_TFTP_UPDATE
            // Update Actions object.
            nlohmann::json& updateSvcSimpleUpdate =
                asyncResp->res
                    .jsonValue["Actions"]["#UpdateService.SimpleUpdate"];
            updateSvcSimpleUpdate["target"] =
                "/redfish/v1/UpdateService/Actions/UpdateService.SimpleUpdate";
            updateSvcSimpleUpdate["TransferProtocol@Redfish.AllowableValues"] =
                {"TFTP"};
#endif
            // Get the current ApplyTime value
            sdbusplus::asio::getProperty<std::string>(
                *crow::connections::systemBus, "xyz.openbmc_project.Settings",
                "/xyz/openbmc_project/software/apply_time",
                "xyz.openbmc_project.Software.ApplyTime", "RequestedApplyTime",
                [asyncResp](const boost::system::error_code ec,
                            const std::string& applyTime) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Store the ApplyTime Value
                    if (applyTime == "xyz.openbmc_project.Software.ApplyTime."
                                     "RequestedApplyTimes.Immediate")
                    {
                        asyncResp->res
                            .jsonValue["HttpPushUriOptions"]
                                      ["HttpPushUriApplyTime"]["ApplyTime"] =
                            "Immediate";
                    }
                    else if (applyTime ==
                             "xyz.openbmc_project.Software.ApplyTime."
                             "RequestedApplyTimes.OnReset")
                    {
                        asyncResp->res
                            .jsonValue["HttpPushUriOptions"]
                                      ["HttpPushUriApplyTime"]["ApplyTime"] =
                            "OnReset";
                    }
                });

            crow::connections::systemBus->async_method_call(
                [asyncResp](
                    const boost::system::error_code errorCode,
                    const std::vector<
                        std::pair<std::string, std::vector<std::string>>>&
                        objInfo) mutable {
                    if (errorCode)
                    {
                        BMCWEB_LOG_ERROR << "error_code = " << errorCode;
                        BMCWEB_LOG_ERROR << "error msg = "
                                         << errorCode.message();
                        if (asyncResp)
                        {
                            messages::internalError(asyncResp->res);
                        }
                        return;
                    }
                    // Ensure we only got one service back
                    if (objInfo.size() != 1)
                    {
                        BMCWEB_LOG_ERROR << "Invalid Object Size "
                                         << objInfo.size();
                        if (asyncResp)
                        {
                            messages::internalError(asyncResp->res);
                        }
                        return;
                    }

                    crow::connections::systemBus->async_method_call(
                        [asyncResp](const boost::system::error_code ec,
                                    GetManagedPropertyType& resp) {
                            if (ec)
                            {
                                BMCWEB_LOG_ERROR << "error_code = " << ec;
                                BMCWEB_LOG_ERROR << "error msg = "
                                                 << ec.message();
                                messages::internalError(asyncResp->res);
                                return;
                            }

                            for (auto& propertyMap : resp)
                            {
                                if (propertyMap.first == "Targets")
                                {
                                    auto targets = std::get_if<std::vector<
                                        sdbusplus::message::object_path>>(
                                        &propertyMap.second);
                                    if (targets)
                                    {
                                        std::vector<std::string> pushURITargets;
                                        for (auto& target : *targets)
                                        {
                                            std::string firmwareId =
                                                target.filename();
                                            if (firmwareId.empty())
                                            {
                                                BMCWEB_LOG_ERROR
                                                    << "Unable to parse firmware ID";
                                                messages::internalError(
                                                    asyncResp->res);
                                                return;
                                            }
                                            pushURITargets.push_back(
                                                "/redfish/v1/UpdateService/FirmwareInventory/" +
                                                firmwareId);
                                        }
                                        asyncResp->res
                                            .jsonValue["HttpPushUriTargets"] =
                                            pushURITargets;
                                    }
                                }
                                /*
                                else if (propertyMap.first == "ForceUpdate")
                                {
                                    auto forceUpdate =
                                        std::get_if<bool>(&propertyMap.second);
                                    if (forceUpdate)
                                    {
                                        asyncResp->res
                                            .jsonValue["HttpPushUriOptions"]
                                                      ["ForceUpdate"] =
                                            *forceUpdate;
                                    }
                                }
                                */
                            }
                            return;
                        },
                        objInfo[0].first, "/xyz/openbmc_project/software",
                        "org.freedesktop.DBus.Properties", "GetAll",
                        "xyz.openbmc_project.Software.UpdatePolicy");
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetObject",
                "/xyz/openbmc_project/software",
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.UpdatePolicy"});
        });
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/")
        .privileges(redfish::privileges::patchUpdateService)
        .methods(
            boost::beast::http::verb::patch)([&app](const crow::Request& req,
                                                    const std::shared_ptr<
                                                        bmcweb::AsyncResp>&
                                                        asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            BMCWEB_LOG_DEBUG << "doPatch...";

            std::optional<nlohmann::json> pushUriOptions;
            std::optional<std::vector<std::string>> imgTargets;
            if (!json_util::readJsonPatch(req, asyncResp->res, "HttpPushUriOptions",
                                     pushUriOptions, "HttpPushUriTargets",
                                     imgTargets))
            {
                BMCWEB_LOG_ERROR
                    << "UpdateService doPatch: Invalid request body";
                return;
            }

            if (pushUriOptions)
            {
                /*std::optional<bool> forceUpdate;*/
                std::optional<nlohmann::json> pushUriApplyTime;
                if (!json_util::readJson(
                        *pushUriOptions, asyncResp->res, "HttpPushUriApplyTime",
                        pushUriApplyTime/*, "ForceUpdate", forceUpdate*/))
                {
                    return;
                }

                if (pushUriApplyTime)
                {
                    std::optional<std::string> applyTime;
                    if (!json_util::readJson(*pushUriApplyTime, asyncResp->res,
                                             "ApplyTime", applyTime))
                    {
                        return;
                    }

                    if (applyTime)
                    {
                        std::string applyTimeNewVal;
                        if (applyTime == "Immediate")
                        {
                            applyTimeNewVal =
                                "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.Immediate";
                        }
                        else if (applyTime == "OnReset")
                        {
                            applyTimeNewVal =
                                "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.OnReset";
                        }
                        else
                        {
                            BMCWEB_LOG_INFO
                                << "ApplyTime value is not in the list of acceptable values";
                            messages::propertyValueNotInList(
                                asyncResp->res, *applyTime, "ApplyTime");
                            return;
                        }

                        // Set the requested image apply time value
                        crow::connections::systemBus->async_method_call(
                            [asyncResp](const boost::system::error_code ec) {
                                if (ec)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "D-Bus responses error: " << ec;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                messages::success(asyncResp->res);
                            },
                            "xyz.openbmc_project.Settings",
                            "/xyz/openbmc_project/software/apply_time",
                            "org.freedesktop.DBus.Properties", "Set",
                            "xyz.openbmc_project.Software.ApplyTime",
                            "RequestedApplyTime",
                            dbus::utility::DbusVariantType{applyTimeNewVal});
                    }
                }
                /*
                if (forceUpdate)
                {
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, forceUpdate](
                            const boost::system::error_code errorCode,
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                objInfo) mutable {
                            if (errorCode)
                            {
                                BMCWEB_LOG_ERROR << "error_code = "
                                                 << errorCode;
                                BMCWEB_LOG_ERROR << "error msg = "
                                                 << errorCode.message();
                                if (asyncResp)
                                {
                                    messages::internalError(asyncResp->res);
                                }
                                return;
                            }
                            // Ensure we only got one service back
                            if (objInfo.size() != 1)
                            {
                                BMCWEB_LOG_ERROR << "Invalid Object Size "
                                                 << objInfo.size();
                                if (asyncResp)
                                {
                                    messages::internalError(asyncResp->res);
                                }
                                return;
                            }
                            crow::connections::systemBus->async_method_call(
                                [asyncResp](
                                    const boost::system::error_code errCodePolicy) {
                                    if (errCodePolicy)
                                    {
                                        BMCWEB_LOG_ERROR << "error_code = "
                                                         << errCodePolicy;
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    messages::success(asyncResp->res);
                                },
                                objInfo[0].first,
                                "/xyz/openbmc_project/software",
                                "org.freedesktop.DBus.Properties", "Set",
                                "xyz.openbmc_project.Software.UpdatePolicy",
                                "ForceUpdate",
                                dbus::utility::DbusVariantType(*forceUpdate));
                        },
                        "xyz.openbmc_project.ObjectMapper",
                        "/xyz/openbmc_project/object_mapper",
                        "xyz.openbmc_project.ObjectMapper", "GetObject",
                        "/xyz/openbmc_project/software",
                        std::array<const char*, 1>{
                            "xyz.openbmc_project.Software.UpdatePolicy"});
                }
                */
            }

            if (imgTargets)
            {
                crow::connections::systemBus->async_method_call(
                    [asyncResp, uriTargets{*imgTargets}](
                        const boost::system::error_code ec,
                        const std::vector<std::string>& swInvPaths) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "D-Bus responses error: " << ec;
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        std::vector<sdbusplus::message::object_path>
                            httpPushUriTargets = {};
                        // validate TargetUris if entries are present
                        if (uriTargets.size() != 0)
                        {
                            std::vector<std::string> invalidTargets;
                            for (const std::string& target : uriTargets)
                            {
                                std::string compName =
                                    std::filesystem::path(target).filename();
                                bool validTarget = false;
                                std::string objPath = "software/" + compName;
                                for (const std::string& path : swInvPaths)
                                {
                                    std::size_t idPos = path.rfind(objPath);
                                    if ((idPos == std::string::npos))
                                    {
                                        continue;
                                    }
                                    std::string swId = path.substr(idPos);
                                    if (swId == objPath)
                                    {
                                        sdbusplus::message::object_path objpath(
                                            path);
                                        httpPushUriTargets.emplace_back(
                                            objpath);
                                        validTarget = true;
                                        break;
                                    }
                                }
                                if (!validTarget)
                                {
                                    invalidTargets.emplace_back(target);
                                }
                            }
                            // return HTTP400 - Bad request
                            // when none of the target filters are valid
                            if (invalidTargets.size() == uriTargets.size())
                            {
                                BMCWEB_LOG_ERROR
                                    << "Targetted Device not Found!!";
                                messages::invalidObject(asyncResp->res,
                                    crow::utility::urlFromPieces("HttpPushUriTargets"));
                                return;
                            }
                            // return HTTP200 - Success with errors
                            // when there is partial valid targets
                            if (invalidTargets.size() > 0)
                            {
                                for (const std::string& invalidTarget :
                                     invalidTargets)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "Invalid HttpPushUriTarget: "
                                        << invalidTarget << "\n";
                                    messages::propertyValueFormatError(
                                        asyncResp->res, invalidTarget,
                                        "HttpPushUriTargets");
                                }
                                asyncResp->res.result(
                                    boost::beast::http::status::ok);
                            }
                            // else all targets are valid
                        }
                        crow::connections::systemBus->async_method_call(
                            [asyncResp, httpPushUriTargets](
                                const boost::system::error_code errorCode,
                                const std::vector<std::pair<
                                    std::string, std::vector<std::string>>>&
                                    objInfo) mutable {
                                if (errorCode)
                                {
                                    BMCWEB_LOG_ERROR << "error_code = "
                                                     << errorCode;
                                    BMCWEB_LOG_ERROR << "error msg = "
                                                     << errorCode.message();
                                    if (asyncResp)
                                    {
                                        messages::internalError(asyncResp->res);
                                    }
                                    return;
                                }
                                // Ensure we only got one service back
                                if (objInfo.size() != 1)
                                {
                                    BMCWEB_LOG_ERROR << "Invalid Object Size "
                                                     << objInfo.size();
                                    if (asyncResp)
                                    {
                                        messages::internalError(asyncResp->res);
                                    }
                                    return;
                                }

                                crow::connections::systemBus->async_method_call(
                                    [asyncResp](const boost::system::error_code
                                                    errCodePolicy) {
                                        if (errCodePolicy)
                                        {
                                            BMCWEB_LOG_ERROR << "error_code = "
                                                             << errCodePolicy;
                                            messages::internalError(
                                                asyncResp->res);
                                        }
                                        messages::success(asyncResp->res);
                                    },
                                    objInfo[0].first,
                                    "/xyz/openbmc_project/software",
                                    "org.freedesktop.DBus.Properties", "Set",
                                    "xyz.openbmc_project.Software.UpdatePolicy",
                                    "Targets",
                                    dbus::utility::DbusVariantType(
                                        httpPushUriTargets));
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetObject",
                            "/xyz/openbmc_project/software",
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.Software.UpdatePolicy"});
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                    "/xyz/openbmc_project/software/", static_cast<int32_t>(0),
                    std::array<std::string, 1>{
                        "xyz.openbmc_project.Software.Version"});
            }
        } // namespace redfish
        );
// The "old" behavior of the update service URI causes redfish-service validator
// failures when the Allow header is supported, given that in the spec,
// UpdateService does not allow POST.  in openbmc, we unfortunately reused that
// resource as our HttpPushUri as well.  A number of services, including the
// openbmc tests, and documentation have hardcoded that erroneous API, instead
// of relying on HttpPushUri as the spec requires.  This option will exist
// temporarily to allow the old behavior until Q4 2022, at which time it will be
// removed.
#ifdef BMCWEB_ENABLE_REDFISH_UPDATESERVICE_OLD_POST_URL
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/")
        .privileges(redfish::privileges::postUpdateService)
        .methods(
            boost::beast::http::verb::
                post)([&app](
                          const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            asyncResp->res.addHeader(
                boost::beast::http::field::warning,
                "299 - \"POST to /redfish/v1/UpdateService is deprecated. Use "
                "the value contained within HttpPushUri.\"");
            handleUpdateServicePost(app, req, asyncResp);
        });
#endif
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/update/")
        .privileges(redfish::privileges::postUpdateService)
        .methods(boost::beast::http::verb::post)(
            std::bind_front(handleUpdateServicePost, std::ref(app)));
}

inline void requestRoutesSoftwareInventoryCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/FirmwareInventory/")
        .privileges(redfish::privileges::getSoftwareInventoryCollection)
        .methods(boost::beast::http::verb::get)([&app](const crow::Request& req,
                                                       const std::shared_ptr<
                                                           bmcweb::AsyncResp>&
                                                           asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] =
                "#SoftwareInventoryCollection.SoftwareInventoryCollection";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/FirmwareInventory";
            asyncResp->res.jsonValue["Name"] = "Software Inventory Collection";

            crow::connections::systemBus->async_method_call(
                [asyncResp](
                    const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string,
                                  std::vector<std::pair<
                                      std::string, std::vector<std::string>>>>>&
                        subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Members"] =
                        nlohmann::json::array();
                    asyncResp->res.jsonValue["Members@odata.count"] = 0;

                    for (auto& obj : subtree)
                    {
                        sdbusplus::message::object_path path(obj.first);
                        std::string swId = path.filename();
                        if (swId.empty())
                        {
                            messages::internalError(asyncResp->res);
                            BMCWEB_LOG_DEBUG << "Can't parse firmware ID!!";
                            return;
                        }

                        nlohmann::json& members =
                            asyncResp->res.jsonValue["Members"];
                        members.push_back(
                            {{"@odata.id",
                              "/redfish/v1/UpdateService/FirmwareInventory/" +
                                  swId}});
                        asyncResp->res.jsonValue["Members@odata.count"] =
                            members.size();
                    }
                },
                // Note that only firmware levels associated with a device
                // are stored under /xyz/openbmc_project/software therefore
                // to ensure only real FirmwareInventory items are returned,
                // this full object path must be used here as input to
                // mapper
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/software", static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});
        });
}

inline void requestRoutesInventorySoftwareCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/SoftwareInventory/")
        .privileges(redfish::privileges::getSoftwareInventoryCollection)
        .methods(boost::beast::http::verb::get)([&app](const crow::Request& req,
                                                       const std::shared_ptr<
                                                           bmcweb::AsyncResp>&
                                                           asyncResp) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            asyncResp->res.jsonValue["@odata.type"] =
                "#SoftwareInventoryCollection.SoftwareInventoryCollection";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/SoftwareInventory";
            asyncResp->res.jsonValue["Name"] = "Software Inventory Collection";

            crow::connections::systemBus->async_method_call(
                [asyncResp](
                    const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string,
                                  std::vector<std::pair<
                                      std::string, std::vector<std::string>>>>>&
                        subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Members"] =
                        nlohmann::json::array();
                    asyncResp->res.jsonValue["Members@odata.count"] = 0;

                    for (auto& obj : subtree)
                    {
                        sdbusplus::message::object_path path(obj.first);
                        std::string swId = path.filename();
                        if (swId.empty())
                        {
                            messages::internalError(asyncResp->res);
                            BMCWEB_LOG_DEBUG << "Can't parse software ID!!";
                            return;
                        }

                        nlohmann::json& members =
                            asyncResp->res.jsonValue["Members"];
                        members.push_back(
                            {{"@odata.id",
                              "/redfish/v1/UpdateService/SoftwareInventory/" +
                                  swId}});
                        asyncResp->res.jsonValue["Members@odata.count"] =
                            members.size();
                    }
                },
                // Note that only firmware levels associated with a device
                // are stored under /xyz/openbmc_project/inventory_software
                // therefore to ensure only real SoftwareInventory items are
                // returned, this full object path must be used here as input to
                // mapper
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory_software",
                static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});
        });
}

inline static bool validSubpath([[maybe_unused]] const std::string& objPath,
                                [[maybe_unused]] const std::string& objectPath)
{
    return false;
}
inline static void
    getRelatedItemsDrive(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                         const sdbusplus::message::object_path& objPath)
{
    // Drive is expected to be under a Chassis
    crow::connections::systemBus->async_method_call(
        [aResp, objPath](const boost::system::error_code ec,
                         const std::vector<std::string>& objects) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                return;
            }

            nlohmann::json& relatedItem = aResp->res.jsonValue["RelatedItem"];
            nlohmann::json& relatedItemCount =
                aResp->res.jsonValue["RelatedItem@odata.count"];

            for (const auto& object : objects)
            {
                if (!validSubpath(objPath.str, object))
                {
                    continue;
                }

                sdbusplus::message::object_path path(object);
                relatedItem.push_back(
                    {{"@odata.id", "/redfish/v1/"
                                   "Systems/" PLATFORMSYSTEMID "/"
                                   "Storage/" +
                                       path.filename() + "/Drives/" +
                                       objPath.filename()}});
                break;
            }
            relatedItemCount = relatedItem.size();
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<std::string, 1>{
            "xyz.openbmc_project.Inventory.Item.Storage"});
}

inline static void getRelatedItemsStorageController(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const sdbusplus::message::object_path& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, objPath](const boost::system::error_code ec,
                         const std::vector<std::string>& objects) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                return;
            }

            for (const auto& object : objects)
            {
                if (!validSubpath(objPath.str, object))
                {
                    continue;
                }

                sdbusplus::message::object_path path(object);

                crow::connections::systemBus->async_method_call(
                    [aResp, objPath, path](
                        const boost::system::error_code errCodeController,
                        const dbus::utility::MapperGetSubTreeResponse& subtree) {
                        if (errCodeController || !subtree.size())
                        {
                            return;
                        }
                        nlohmann::json& relatedItem =
                            aResp->res.jsonValue["RelatedItem"];
                        nlohmann::json& relatedItemCount =
                            aResp->res.jsonValue["RelatedItem@odata.count"];

                        for (size_t i = 0; i < subtree.size(); ++i)
                        {
                            if (subtree[i].first != objPath.str)
                            {
                                continue;
                            }

                            relatedItem.push_back(
                                {{"@odata.id",
                                  "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                  "/Storage/" +
                                      path.filename() +
                                      "#/StorageControllers/" +
                                      std::to_string(i)}});
                            break;
                        }

                        relatedItemCount = relatedItem.size();
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", object,
                    int32_t(0),
                    std::array<const char*, 1>{"xyz.openbmc_project.Inventory."
                                               "Item.StorageController"});
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.Storage"});
}

inline static void getRelatedItemsPowerSupply(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const sdbusplus::message::object_path& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath](const boost::system::error_code& errorCode,
                             std::variant<std::vector<std::string>>& resp) {
            if (errorCode)
            {
                BMCWEB_LOG_DEBUG << "error_code = " << errorCode;
                BMCWEB_LOG_DEBUG << "error msg = " << errorCode.message();
                return;
            }
            std::string chassisName = "chassis";
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_ERROR << "Invalid Object ";
                return;
            }
            for (const std::string& path : *data)
            {
                sdbusplus::message::object_path myLocalPath(path);
                chassisName = myLocalPath.filename();
            }
            nlohmann::json& relatedItem =
                asyncResp->res.jsonValue["RelatedItem"];
            nlohmann::json& relatedItemCount =
                asyncResp->res.jsonValue["RelatedItem@odata.count"];
            relatedItem.push_back(
                {{"@odata.id", "/redfish/v1/Chassis/" + chassisName +
                                   "/PowerSubsystem/PowerSupplies/" +
                                   objPath.filename()}});

            relatedItemCount = relatedItem.size();
            asyncResp->res.jsonValue["Description"] = "Power Supply image";
        },
        "xyz.openbmc_project.ObjectMapper", objPath.str + "/chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline static void getRelatedItemsPCIeDevice(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const sdbusplus::message::object_path& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath](const boost::system::error_code& errorCode,
                             std::variant<std::vector<std::string>>& resp) {
            if (errorCode)
            {
                BMCWEB_LOG_DEBUG << "error_code = " << errorCode;
                BMCWEB_LOG_DEBUG << "error msg = " << errorCode.message();
                return;
            }
            std::string chassisName = "chassis";
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_ERROR << "Invalid Object ";
                return;
            }
            for (const std::string& path : *data)
            {
                sdbusplus::message::object_path myLocalPath(path);
                chassisName = myLocalPath.filename();
            }
            nlohmann::json& relatedItem =
                asyncResp->res.jsonValue["RelatedItem"];
            nlohmann::json& relatedItemCount =
                asyncResp->res.jsonValue["RelatedItem@odata.count"];
            relatedItem.push_back(
                {{"@odata.id", "/redfish/v1/Chassis/" + chassisName +
                                   "/PCIeDevices/" + objPath.filename()}});

            relatedItemCount = relatedItem.size();
        },
        "xyz.openbmc_project.ObjectMapper", objPath.str + "/chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline static void
    getRelatedItemsSwitch(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const sdbusplus::message::object_path& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, objPath](const boost::system::error_code& errorCode,
                             std::variant<std::vector<std::string>>& resp) {
            if (errorCode)
            {
                BMCWEB_LOG_DEBUG << "error_code = " << errorCode;
                BMCWEB_LOG_DEBUG << "error msg = " << errorCode.message();
                return;
            }
            std::string fabricName = "fabric";
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_ERROR << "Invalid Object ";
                return;
            }
            for (const std::string& path : *data)
            {
                sdbusplus::message::object_path myLocalPath(path);
                fabricName = myLocalPath.filename();
            }
            nlohmann::json& relatedItem =
                asyncResp->res.jsonValue["RelatedItem"];
            nlohmann::json& relatedItemCount =
                asyncResp->res.jsonValue["RelatedItem@odata.count"];
            relatedItem.push_back(
                {{"@odata.id", "/redfish/v1/Fabrics/" + fabricName +
                                   "/Switches/" + objPath.filename()}});

            relatedItemCount = relatedItem.size();
        },
        "xyz.openbmc_project.ObjectMapper", objPath.str + "/fabrics",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline static void
    getRelatedItemsOther(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                         const sdbusplus::message::object_path& association)
{
    // Find supported device types.
    crow::connections::systemBus->async_method_call(
        [aResp, association](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objects) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "error_code = " << ec
                                 << ", error msg = " << ec.message();
                return;
            }
            if (objects.empty())
            {
                return;
            }

            nlohmann::json& relatedItem = aResp->res.jsonValue["RelatedItem"];
            nlohmann::json& relatedItemCount =
                aResp->res.jsonValue["RelatedItem@odata.count"];

            for (const auto& object : objects)
            {
                for (const auto& interfaces : object.second)
                {
                    if (interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.Drive")
                    {
                        getRelatedItemsDrive(aResp, association);
                    }

                    if (interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.PCIeDevice")
                    {
                        getRelatedItemsPCIeDevice(aResp, association);
                    }

                    if (interfaces == "xyz.openbmc_project."
                                      "Inventory."
                                      "Item.Accelerator" ||
                        interfaces == "xyz.openbmc_project."
                                      "Inventory.Item.Cpu")
                    {
                        relatedItem.push_back(
                            {{"@odata.id",
                              "/redfish/v1/Systems/" PLATFORMSYSTEMID
                              "/Processors/" +
                                  association.filename()}});
                    }

                    if (interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.Board" ||
                        interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.Chassis")
                    {
                        relatedItem.push_back(
                            {{"@odata.id", "/redfish/v1/Chassis/" +
                                               association.filename()}});
                    }

                    if (interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.StorageController")
                    {
                        getRelatedItemsStorageController(aResp, association);
                    }
                    if (interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.PowerSupply")
                    {
                        getRelatedItemsPowerSupply(aResp, association);
                    }

                    if (interfaces == "xyz.openbmc_project.Inventory."
                                      "Item.Switch")
                    {
                        getRelatedItemsSwitch(aResp, association);
                    }
                }
            }

            relatedItemCount = relatedItem.size();
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", association.str,
        std::array<const char*, 9>{
            "xyz.openbmc_project.Inventory.Item.PowerSupply",
            "xyz.openbmc_project.Inventory.Item.Accelerator",
            "xyz.openbmc_project.Inventory.Item.PCIeDevice",
            "xyz.openbmc_project.Inventory.Item.Switch",
            "xyz.openbmc_project.Inventory.Item.Cpu",
            "xyz.openbmc_project.Inventory.Item.Drive",
            "xyz.openbmc_project.Inventory.Item.Board",
            "xyz.openbmc_project.Inventory.Item.Chassis",
            "xyz.openbmc_project.Inventory.Item.StorageController"});
}

/*
    Fill related item links for Software with other purposes.
    Use other purpose for device level softwares.
*/
inline static void
    getRelatedItemsOthers(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                          const std::string& swId,
                          std::string inventoryPath = "")
{

    BMCWEB_LOG_DEBUG << "getRelatedItemsOthers enter";

    if (inventoryPath.empty())
    {
        inventoryPath = "/xyz/openbmc_project/software/";
    }

    aResp->res.jsonValue["RelatedItem"] = nlohmann::json::array();
    aResp->res.jsonValue["RelatedItem@odata.count"] = 0;

    crow::connections::systemBus->async_method_call(
        [aResp, swId](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
            if (ec)
            {
                messages::internalError(aResp->res);
                return;
            }

            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     obj : subtree)
            {
                sdbusplus::message::object_path path(obj.first);
                if (path.filename() != swId)
                {
                    continue;
                }

                if (obj.second.size() < 1)
                {
                    continue;
                }
                crow::connections::systemBus->async_method_call(
                    [aResp](const boost::system::error_code errCodeAssoc,
                            std::variant<std::vector<std::string>>& resp) {
                        if (errCodeAssoc)
                        {
                            BMCWEB_LOG_ERROR
                                << "error_code = " << errCodeAssoc
                                << ", error msg = " << errCodeAssoc.message();
                            return;
                        }

                        std::vector<std::string>* associations =
                            std::get_if<std::vector<std::string>>(&resp);
                        if ((associations == nullptr) ||
                            (associations->empty()))
                        {
                            BMCWEB_LOG_ERROR
                                << "Zero association for the software";
                            return;
                        }

                        for (const std::string& association : *associations)
                        {
                            if (association.empty())
                            {
                                continue;
                            }
                            sdbusplus::message::object_path associationPath(
                                association);

                            getRelatedItemsOther(aResp, associationPath);
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper", path.str + "/inventory",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", inventoryPath, 0,
        std::array<const char*, 1>{"xyz.openbmc_project.Software.Version"});
}

/* Fill related item links (i.e. bmc, bios) in for inventory */
inline static void
    getRelatedItems(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                    const std::string& swId, const std::string& purpose)
{
    if (purpose == fw_util::biosPurpose)
    {
        nlohmann::json& relatedItem = aResp->res.jsonValue["RelatedItem"];
        relatedItem.push_back(
            {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Bios"}});
        aResp->res.jsonValue["Members@odata.count"] = relatedItem.size();
    }
    else if (purpose == fw_util::otherPurpose || purpose == fw_util::bmcPurpose)
    {
        getRelatedItemsOthers(aResp, swId);
    }
    else
    {
        BMCWEB_LOG_ERROR << "Unknown software purpose " << purpose;
    }
}

inline void
    getSoftwareVersion(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& service, const std::string& path,
                       const std::string& swId)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Software.Version",
        [asyncResp,
         swId](const boost::system::error_code errorCode,
               const dbus::utility::DBusPropertiesMap& propertiesList) {
            if (errorCode)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            const std::string* swInvPurpose = nullptr;
            const std::string* version = nullptr;

            const bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), propertiesList, "Purpose",
                swInvPurpose, "Version", version);

            if (!success)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (swInvPurpose == nullptr)
            {
                BMCWEB_LOG_DEBUG << "Can't find property \"Purpose\"!";
                messages::internalError(asyncResp->res);
                return;
            }

            BMCWEB_LOG_DEBUG << "swInvPurpose = " << *swInvPurpose;

            if (version == nullptr)
            {
                BMCWEB_LOG_DEBUG << "Can't find property \"Version\"!";

                messages::internalError(asyncResp->res);

                return;
            }
            asyncResp->res.jsonValue["Version"] = *version;
            asyncResp->res.jsonValue["Id"] = swId;

            // swInvPurpose is of format:
            // xyz.openbmc_project.Software.Version.VersionPurpose.ABC
            // Translate this to "ABC image"
            size_t endDesc = swInvPurpose->rfind('.');
            if (endDesc == std::string::npos)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            endDesc++;
            if (endDesc >= swInvPurpose->size())
            {
                messages::internalError(asyncResp->res);
                return;
            }

            std::string formatDesc = swInvPurpose->substr(endDesc);
            asyncResp->res.jsonValue["Description"] = formatDesc + " image";
            getRelatedItems(asyncResp, swId, *swInvPurpose);
        });
}

inline void requestRoutesSoftwareInventory(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/FirmwareInventory/<str>/")
        .privileges(redfish::privileges::getSoftwareInventory)
        .methods(
            boost::beast::http::verb::
                get)([&app](const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& param) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            std::shared_ptr<std::string> swId =
                std::make_shared<std::string>(param);

            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/FirmwareInventory/" + *swId;

            crow::connections::systemBus->async_method_call(
                [asyncResp, swId](
                    const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string,
                                  std::vector<std::pair<
                                      std::string, std::vector<std::string>>>>>&
                        subtree) {
                    BMCWEB_LOG_DEBUG << "doGet callback...";
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    // Ensure we find our input swId, otherwise return an
                    // error
                    bool found = false;
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<
                                 std::string, std::vector<std::string>>>>& obj :
                         subtree)
                    {
                        sdbusplus::message::object_path objPath(obj.first);
                        if (boost::equals(objPath.filename(), *swId) != true)
                        {
                            continue;
                        }

                        if (obj.second.size() < 1)
                        {
                            continue;
                        }

                        found = true;
                        fw_util::getFwStatus(asyncResp, swId,
                                             obj.second[0].first);
                        fw_util::getFwWriteProtectedStatus(asyncResp, swId,
                                                           obj.second[0].first);
                        crow::connections::systemBus->async_method_call(
                            [asyncResp,
                             swId](const boost::system::error_code errorCode,
                                   const boost::container::flat_map<
                                       std::string,
                                       dbus::utility::DbusVariantType>&
                                       propertiesList) {
                                if (errorCode)
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                boost::container::flat_map<
                                    std::string,
                                    dbus::utility::DbusVariantType>::
                                    const_iterator it =
                                        propertiesList.find("Purpose");
                                if (it == propertiesList.end())
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "Can't find property \"Version\"!";
                                    messages::propertyMissing(asyncResp->res,
                                                              "Purpose");
                                    return;
                                }
                                const std::string* swInvPurpose =
                                    std::get_if<std::string>(&it->second);
                                if (swInvPurpose == nullptr)
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "wrong types for property\"Purpose\"!";
                                    messages::propertyValueTypeError(
                                        asyncResp->res, "", "Purpose");
                                    return;
                                }

                                BMCWEB_LOG_DEBUG << "swInvPurpose = "
                                                 << *swInvPurpose;
                                it = propertiesList.find("Version");
                                if (it == propertiesList.end())
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "Can't find property \"Version\"!";
                                    messages::propertyMissing(asyncResp->res,
                                                              "Version");
                                    return;
                                }

                                BMCWEB_LOG_DEBUG << "Version found!";

                                const std::string* version =
                                    std::get_if<std::string>(&it->second);

                                if (version == nullptr)
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "Can't find property \"Version\"!";

                                    messages::propertyValueTypeError(
                                        asyncResp->res, "", "Version");
                                    return;
                                }

                                it = propertiesList.find("SoftwareId");
                                if (it != propertiesList.end())
                                {
                                    const std::string* softwareId =
                                        std::get_if<std::string>(&it->second);

                                    if (softwareId == nullptr)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Can't find property \"softwareId\"!";
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    asyncResp->res.jsonValue["SoftwareId"] =
                                        *softwareId;
                                }

                                asyncResp->res.jsonValue["Version"] = *version;
                                asyncResp->res.jsonValue["Id"] = *swId;

                                // swInvPurpose is of format:
                                // xyz.openbmc_project.Software.Version.VersionPurpose.ABC
                                // Translate this to "ABC image"
                                size_t endDesc = swInvPurpose->rfind('.');
                                if (endDesc == std::string::npos)
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                endDesc++;
                                if (endDesc >= swInvPurpose->size())
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }

                                std::string formatDesc =
                                    swInvPurpose->substr(endDesc);
                                asyncResp->res.jsonValue["Description"] =
                                    formatDesc + " image";
                                getRelatedItems(asyncResp, *swId,
                                                *swInvPurpose);
                            },
                            obj.second[0].first, obj.first,
                            "org.freedesktop.DBus.Properties", "GetAll",
                            "xyz.openbmc_project.Software.Version");

                        const std::vector<std::string>& interfaces2 =
                            obj.second[0].second;
                        const std::string assetIntf =
                            "xyz.openbmc_project.Inventory.Decorator.Asset";
                        if (std::find(interfaces2.begin(), interfaces2.end(),
                                      assetIntf) != interfaces2.end())
                        {
                            crow::connections::systemBus->async_method_call(
                                [asyncResp, swId](
                                    const boost::system::error_code errorCode,
                                    const boost::container::flat_map<
                                        std::string,
                                        dbus::utility::DbusVariantType>&
                                        propertiesList) {
                                    if (errorCode)
                                    {
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    boost::container::flat_map<
                                        std::string,
                                        dbus::utility::DbusVariantType>::
                                        const_iterator it =
                                            propertiesList.find("Manufacturer");
                                    if (it != propertiesList.end())
                                    {
                                        const std::string* manufacturer =
                                            std::get_if<std::string>(
                                                &it->second);

                                        if (manufacturer == nullptr)
                                        {
                                            BMCWEB_LOG_ERROR
                                                << "Can't find property \"Manufacturer\"!";
                                            messages::internalError(
                                                asyncResp->res);
                                            return;
                                        }
                                        asyncResp->res
                                            .jsonValue["Manufacturer"] =
                                            *manufacturer;
                                    }
                                },
                                obj.second[0].first, obj.first,
                                "org.freedesktop.DBus.Properties", "GetAll",
                                "xyz.openbmc_project.Inventory.Decorator.Asset");
                        }

#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                        std::shared_ptr<HealthRollup> health =
                            std::make_shared<HealthRollup>(
                                crow::connections::systemBus, objPath,
                                [asyncResp](const std::string& rootHealth,
                                            const std::string& healthRollup) {
                                    asyncResp->res
                                        .jsonValue["Status"]["Health"] =
                                        rootHealth;
                                    asyncResp->res
                                        .jsonValue["Status"]["HealthRollup"] =
                                        healthRollup;
                                },
                                &health_state::ok);
                        health->start();
#else  // BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                        asyncResp->res.jsonValue["Status"]["Health"] = "OK";
                        asyncResp->res.jsonValue["Status"]["HealthRollup"] =
                            "OK";
#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                    }
                    if (!found)
                    {
                        BMCWEB_LOG_ERROR
                            << "Input swID " + *swId + " not found!";
                        messages::resourceMissingAtURI(
                            asyncResp->res, crow::utility::urlFromPieces(
                            "redfish", "v1", "UpdateService", "FirmwareInventory",
                            *swId));
                        return;
                    }
                    asyncResp->res.jsonValue["@odata.type"] =
                        "#SoftwareInventory.v1_4_0.SoftwareInventory";
                    asyncResp->res.jsonValue["Name"] = "Software Inventory";

                    asyncResp->res.jsonValue["Updateable"] = false;
                    fw_util::getFwUpdateableStatus(asyncResp, swId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/",
                static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});
        });
}

inline void requestRoutesInventorySoftware(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/SoftwareInventory/<str>/")
        .privileges(redfish::privileges::getSoftwareInventory)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& param) {
            std::string searchPath = "/xyz/openbmc_project/inventory_software/";
            std::shared_ptr<std::string> swId =
                std::make_shared<std::string>(param);

            crow::connections::systemBus->async_method_call(
                [asyncResp, swId, searchPath](
                    const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string,
                                  std::vector<std::pair<
                                      std::string, std::vector<std::string>>>>>&
                        subtree) {
                    BMCWEB_LOG_DEBUG << "doGet callback...";
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    // Ensure we find our input swId, otherwise return an
                    // error
                    for (const std::pair<
                             std::string,
                             std::vector<std::pair<
                                 std::string, std::vector<std::string>>>>& obj :
                         subtree)
                    {
                        const std::string& path = obj.first;
                        sdbusplus::message::object_path objPath(path);
                        if (objPath.filename() != *swId)
                        {
                            continue;
                        }

                        if (obj.second.size() < 1)
                        {
                            continue;
                        }

                        asyncResp->res.jsonValue["Id"] = *swId;
                        asyncResp->res.jsonValue["Status"]["Health"] = "OK";
                        asyncResp->res.jsonValue["Status"]["HealthRollup"] =
                            "OK";

#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                        std::shared_ptr<HealthRollup> health =
                            std::make_shared<HealthRollup>(
                                crow::connections::systemBus, path,
                                [asyncResp](const std::string& rootHealth,
                                            const std::string& healthRollup) {
                                    asyncResp->res
                                        .jsonValue["Status"]["Health"] =
                                        rootHealth;
                                    asyncResp->res
                                        .jsonValue["Status"]["HealthRollup"] =
                                        healthRollup;
                                });
                        health->start();

#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

                        crow::connections::systemBus->async_method_call(
                            [asyncResp, swId, path, searchPath](
                                const boost::system::error_code errorCode,
                                const boost::container::flat_map<
                                    std::string,
                                    dbus::utility::DbusVariantType>&
                                    propertiesList) {
                                if (errorCode)
                                {
                                    BMCWEB_LOG_DEBUG << "properties not found ";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }

                                for (const auto& property : propertiesList)
                                {
                                    if (property.first == "Manufacturer")
                                    {
                                        const std::string* manufacturer =
                                            std::get_if<std::string>(
                                                &property.second);
                                        if (manufacturer != nullptr)
                                        {
                                            asyncResp->res
                                                .jsonValue["Manufacturer"] =
                                                *manufacturer;
                                        }
                                    }
                                    else if (property.first == "Version")
                                    {
                                        const std::string* version =
                                            std::get_if<std::string>(
                                                &property.second);
                                        if (version != nullptr)
                                        {
                                            asyncResp->res
                                                .jsonValue["Version"] =
                                                *version;
                                        }
                                    }
                                    else if (property.first == "Functional")
                                    {
                                        const bool* swInvFunctional =
                                            std::get_if<bool>(&property.second);
                                        if (swInvFunctional != nullptr)
                                        {
                                            BMCWEB_LOG_DEBUG
                                                << " Functinal "
                                                << *swInvFunctional;
                                            if (*swInvFunctional)
                                            {
                                                asyncResp->res
                                                    .jsonValue["Status"]
                                                              ["State"] =
                                                    "Enabled";
                                            }
                                            else
                                            {
                                                asyncResp->res
                                                    .jsonValue["Status"]
                                                              ["State"] =
                                                    "Disabled";
                                            }
                                        }
                                    }
                                }
                                getRelatedItemsOthers(asyncResp, *swId,
                                                      searchPath);
                                fw_util::getFwUpdateableStatus(asyncResp, swId,
                                                               searchPath);
                            },
                            obj.second[0].first, obj.first,
                            "org.freedesktop.DBus.Properties", "GetAll", "");
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/UpdateService/SoftwareInventory/" +
                            *swId;
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#SoftwareInventory.v1_4_0.SoftwareInventory";
                        asyncResp->res.jsonValue["Name"] = "Software Inventory";
                        redfish::conditions_utils::populateServiceConditions(
                            asyncResp, *swId);
                        return;
                    }
                    // Couldn't find an object with that name.  return an error
                    BMCWEB_LOG_DEBUG << "Input swID " + *swId + " not found!";
                    messages::resourceNotFound(
                        asyncResp->res,
                        "SoftwareInventory.v1_4_0.SoftwareInventory", *swId);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree", searchPath,
                static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});
        });

    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/FirmwareInventory/<str>/")
        .privileges(redfish::privileges::patchUpdateService)
        .methods(
            boost::beast::http::verb::
                patch)([](const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& param) {
            BMCWEB_LOG_DEBUG << "doPatch...";
            std::shared_ptr<std::string> swId =
                std::make_shared<std::string>(param);

            std::optional<bool> writeProtected;
            if (!json_util::readJsonPatch(req, asyncResp->res, "WriteProtected",
                                     writeProtected))
            {
                return;
            }

            if (writeProtected)
            {
                crow::connections::systemBus->async_method_call(
                    [asyncResp, swId, writeProtected](
                        const boost::system::error_code ec,
                        const std::vector<std::pair<
                            std::string,
                            std::vector<std::pair<std::string,
                                                  std::vector<std::string>>>>>&
                            subtree) {
                        if (ec)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 obj : subtree)
                        {
                            const std::string& path = obj.first;
                            sdbusplus::message::object_path objPath(path);
                            if (objPath.filename() != *swId)
                            {
                                continue;
                            }

                            if (obj.second.size() < 1)
                            {
                                continue;
                            }
                            fw_util::patchFwWriteProtectedStatus(
                                asyncResp, swId, obj.second[0].first,
                                *writeProtected);

                            return;
                        }
                        // Couldn't find an object with that name.  return an
                        // error
                        BMCWEB_LOG_DEBUG
                            << "Input swID " + *swId + " not found!";
                        messages::resourceNotFound(
                            asyncResp->res,
                            "SoftwareInventory.v1_4_0.SoftwareInventory",
                            *swId);
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/",
                    static_cast<int32_t>(0),
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Software.Version"});
            }
        });
}

/**
 * @brief Check whether firmware inventory is allowable 
 * The function gets allowable values from config file 
 * /usr/share/bmcweb/fw_mctp_mapping.json.
 * and check if the firmware inventory is in this collection
 * 
 * @param[in] inventoryPath - firmware inventory path.
 * @returns boolean value indicates whether firmware inventory 
 * is allowable.
 */
inline bool isInventoryAllowableValue(const std::string_view inventoryPath)
{
    bool isAllowable = false;

    std::vector<CommitImageValueEntry> allowableValues = getAllowableValues();
    std::vector<CommitImageValueEntry>::iterator it 
     = find(allowableValues.begin(), allowableValues.end(), static_cast<std::string>(inventoryPath));

    if (it != allowableValues.end())
    {
        isAllowable = true;
    }
    else
    {
        isAllowable = false;
    }

    return isAllowable;
}


/**
 * @brief Get allowable value for particular firmware inventory 
 * The function gets allowable values from config file 
 * /usr/share/bmcweb/fw_mctp_mapping.json.
 * and returns the allowable value if exists in the collection
 * 
 * @param[in] inventoryPath - firmware inventory path.
 * @returns Pair of boolean value if the allowable value exists 
 * and the object of AllowableValue who contains inventory path 
 * and assigned to its MCTP EID.
 */
inline std::pair<bool, CommitImageValueEntry> getAllowableValue(const std::string_view inventoryPath)
{
    std::pair<bool, CommitImageValueEntry> result;

    std::vector<CommitImageValueEntry> allowableValues = getAllowableValues();
    std::vector<CommitImageValueEntry>::iterator it 
     = find(allowableValues.begin(), allowableValues.end(), static_cast<std::string>(inventoryPath));

    if (it != allowableValues.end())
    {
        result.second = *it;
        result.first = true;
    }
    else
    {
        result.first = false;
    }

    return result;
}

/**
 * @brief Update parameters for GET Method CommitImageInfo
 *
 * @param[in] asyncResp Shared pointer to the response message
 * @param[in] subtree  Collection of objectmappers for 
 * "/xyz/openbmc_project/software"
 * 
 * @return None
 */
inline void
    updateParametersForCommitImageInfo(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                const std::vector<
                std::pair<std::string,
                    std::vector<std::pair<
                    std::string, std::vector<std::string>>>>>&
                subtree)
{
    asyncResp->res.jsonValue["Parameters"] = nlohmann::json::array();
    nlohmann::json& parameters = asyncResp->res.jsonValue["Parameters"];

    nlohmann::json parameterTargets;
    parameterTargets["Name"] = "Targets";
    parameterTargets["Required"] = "false";
    parameterTargets["DataType"] = "StringArray";
    parameterTargets["Targets@Redfish.AllowableValues"] = nlohmann::json::array();

    nlohmann::json& allowableValues = parameterTargets["Targets@Redfish.AllowableValues"];

    for (auto& obj : subtree)
    {
        sdbusplus::message::object_path path(obj.first);
        std::string fwId = path.filename();
        if (fwId.empty())
        {
            messages::internalError(asyncResp->res);
            BMCWEB_LOG_DEBUG << "Cannot parse firmware ID";
            return;
        }

        if(isInventoryAllowableValue(obj.first))
        {
            allowableValues.push_back("/redfish/v1/UpdateService/FirmwareInventory/" + fwId);
        }
    }

    parameters.push_back(parameterTargets);
}

/**
 * @brief Handles request POST 
 * The function triggers Commit Image action
 * for the list of delivered in the body of request
 * firmware inventories
 * 
 * @param resp Async HTTP response.
 * @param asyncResp Pointer to object holding response data
 * @param[in] subtree  Collection of objectmappers for 
 * "/xyz/openbmc_project/software"
 * 
 * @return None
 */
inline void handleCommitImagePost( const crow::Request& req,
                const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                const std::vector<
                std::pair<std::string,
                    std::vector<std::pair<
                    std::string, std::vector<std::string>>>>>&
                subtree)
{
    std::optional<std::vector<std::string>> targets;
    json_util::readJsonAction(req, asyncResp->res,
                        "Targets", targets);

    bool hasTargets = false;
    bool hasError = false;

    if(targets && targets.value().empty() == false)
    {
        hasTargets = true;
    }

    if(hasTargets)
    {
        std::vector<std::string> targetsCollection = targets.value();

        for (auto& target : targetsCollection)
        {
            std::pair<bool, CommitImageValueEntry> result = getAllowableValue(target);
            if (result.first == true)
            {
                uint32_t eid = result.second.mctpEndpointId;

                if(initBackgroundCopy(eid) != 0)
                {
                    BMCWEB_LOG_DEBUG << "mctp-vdm-util could not execute backgroundcopy_init.";

                    messages::resourceErrorsDetectedFormatError(asyncResp->res,
                        result.second.inventoryUri,
                        "backgroundCopy_init");

                    hasError = true;
                }
            }
            else
            {
                BMCWEB_LOG_DEBUG << "Cannot find firmware inventory in allowable values";
                messages::resourceMissingAtURI(asyncResp->res, crow::utility::urlFromPieces(target));

                hasError = true;
            }
        }
    }
    else
    {
        for (auto& obj : subtree)
        {       
            std::pair<bool, CommitImageValueEntry> result = getAllowableValue(obj.first);

            if (result.first == true)
            {
                uint32_t eid = result.second.mctpEndpointId;
                
                if(initBackgroundCopy(eid) != 0)
                {
                    BMCWEB_LOG_DEBUG << "mctp-vdm-util could not execute backgroundcopy_init.";

                    messages::resourceErrorsDetectedFormatError(asyncResp->res,
                        result.second.inventoryUri,
                        "backgroundCopy_init");

                    hasError = true;
                }
            }
        }
    }   

    if(hasError == false)
    {
        messages::success(asyncResp->res);
    }
}

/**
 * @brief Register Web Api endpoints for Commit Image functionality
 * 
 * @return None
 */
inline void requestRoutesUpdateServiceCommitImage(App& app)
{
    BMCWEB_ROUTE(app, 
        "/redfish/v1/UpdateService/Oem/Nvidia/CommitImageActionInfo")
        .privileges(redfish::privileges::getSoftwareInventoryCollection)
        .methods(
            boost::beast::http::verb::
                get)([](const crow::Request&,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {

            asyncResp->res.jsonValue["@odata.type"] =
               "#UpdateService.v1_11_0.CommitImageActionInfo";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/Oem/Nvidia/CommitImageActionInfo";
            asyncResp->res.jsonValue["Name"] = "Commit Image Action Information";
            asyncResp->res.jsonValue["Id"] = "CommitImageActionInfo";

            crow::connections::systemBus->async_method_call(
                [asyncResp{asyncResp}](
                    const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string,
                                  std::vector<std::pair<
                                      std::string, std::vector<std::string>>>>>&
                        subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    updateParametersForCommitImageInfo(asyncResp, subtree);
                },
                // Note that only firmware levels associated with a device
                // are stored under /xyz/openbmc_project/software therefore
                // to ensure only real FirmwareInventory items are returned,
                // this full object path must be used here as input to
                // mapper
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/software", static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});
        });

    BMCWEB_ROUTE(app,
        "/redfish/v1/UpdateService/Oem/Nvidia/CommitImage/")
        .privileges(redfish::privileges::postUpdateService)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                
            BMCWEB_LOG_DEBUG << "doPost...";

            if (fwUpdateInProgress == true)
            {
                redfish::messages::updateInProgressMsg(
                    asyncResp->res, 
                    "Retry the operation once firmware update operation is complete.");

                // don't copy the image, update already in progress.
                BMCWEB_LOG_ERROR << "Cannot execute commit image. Update firmware is in progress.";

                return;
            }

            crow::connections::systemBus->async_method_call(
                [req, asyncResp{asyncResp}](
                    const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string,
                                  std::vector<std::pair<
                                      std::string, std::vector<std::string>>>>>&
                        subtree) {
                    if (ec)
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    handleCommitImagePost(req, asyncResp, subtree);

                },
                // Note that only firmware levels associated with a device
                // are stored under /xyz/openbmc_project/software therefore
                // to ensure only real FirmwareInventory items are returned,
                // this full object path must be used here as input to
                // mapper
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/software", static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});

        });
}

} // namespace redfish
