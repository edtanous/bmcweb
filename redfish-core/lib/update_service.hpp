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

#include "background_copy.hpp"
#include "commit_image.hpp"
#include "persistentstorage_util.hpp"

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
#include <utils/dbus_log_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/fw_utils.hpp>
#include <utils/sw_utils.hpp>

namespace redfish
{

// Match signals added on software path
static std::unique_ptr<sdbusplus::bus::match_t> fwUpdateMatcher;
// Only allow one update at a time
static bool fwUpdateInProgress = false;
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
// Match signals added on software path for staging fw package
static std::unique_ptr<sdbusplus::bus::match_t> fwStageImageMatcher;
// Allow staging, deleting or initializing firmware 
// when staging of another firmware is not in a progress
static bool fwImageIsStaging = false;
// max staged updates per hour
constexpr auto maxStagedUpdatesPerHour = 16;
// rate limit duration for staging update. current rate limit is 16 updates per
// one hour.
constexpr auto stageRateLimitDurationInSeconds = 3600;
// staged update counter for rate limit
static uint8_t stagedUpdateCount = 0;
// staged update time stamp for rate limit
static std::chrono::time_point<std::chrono::steady_clock> stagedUpdateTimeStamp;
// Timer for staging software available
static std::unique_ptr<boost::asio::steady_timer> fwStageAvailableTimer;
/* Match signals added on emmc partition service */
static std::unique_ptr<sdbusplus::bus::match_t> emmcServiceSignalMatch;

/* Exist codes returned by nvidia-emmc partition service after completion.*/
enum EMMCServiceExitCodes
{
    emmcPartitionMounted = 0,
    emmcNotFound = 1,
    emmcDisabled = 2,
    eudaNotProgrammed = 3,
    eudaProgrammed = 4,
    eudaProgrammedNotActivated = 5
};
#endif
// Timer for software available
static std::unique_ptr<boost::asio::steady_timer> fwAvailableTimer;
// match for logging
constexpr auto fwObjectCreationDefaultTimeout = 20;
static std::unique_ptr<sdbusplus::bus::match::match> loggingMatch = nullptr;

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
#define SUPPORTED_RETIMERS 8
/* holds compute digest operation state to allow one operation at a time */
static bool computeDigestInProgress = false;
const std::string hashComputeInterface = "com.Nvidia.ComputeHash";
constexpr auto retimerHashMaxTimeSec =
    180; // 2 mins for 2 attempts and 1 addional min as buffer
#endif

inline static void cleanUp()
{
    fwUpdateInProgress = false;
    fwUpdateMatcher = nullptr;
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
inline static void cleanUpStageObjects()
{
    fwImageIsStaging = false;
    fwStageImageMatcher = nullptr;
}
#endif

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
                else if (propertyMap.first == "Resolution")
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
                           sdbusplus::message_t& m, task::Payload&& payload)
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
static void monitorForSoftwareAvailable(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const crow::Request& req,
    int timeoutTimeSeconds = fwObjectCreationDefaultTimeout,
    const std::string& imagePath = {})
{
    fwAvailableTimer =
        std::make_unique<boost::asio::steady_timer>(*req.ioService);

    fwAvailableTimer->expires_after(std::chrono::seconds(timeoutTimeSeconds));

    fwAvailableTimer->async_wait(
        [asyncResp, imagePath](const boost::system::error_code& ec) {
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
            // remove update package to allow next update
            if (!imagePath.empty())
            {
                std::filesystem::remove(imagePath);
            }
        });

    task::Payload payload(req);
    auto callback = [asyncResp, payload](sdbusplus::message_t& m) mutable {
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
        .methods(boost::beast::http::verb::post)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }

                std::optional<std::string> transferProtocol;
                std::string imageURI;

                BMCWEB_LOG_DEBUG << "Enter UpdateService.SimpleUpdate doPost";

                // User can pass in both TransferProtocol and ImageURI
                // parameters or they can pass in just the ImageURI with the
                // transfer protocol embedded within it. 1)
                // TransferProtocol:TFTP ImageURI:1.1.1.1/myfile.bin 2)
                // ImageURI:tftp://1.1.1.1/myfile.bin

                if (!json_util::readJsonAction(
                        req, asyncResp->res, "TransferProtocol",
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
                        BMCWEB_LOG_ERROR
                            << "ImageURI missing transfer protocol: "
                            << imageURI;
                        return;
                    }
                    transferProtocol = imageURI.substr(0, separator);
                    // Ensure protocol is upper case for a common comparison
                    // path below
                    boost::to_upper(*transferProtocol);
                    BMCWEB_LOG_DEBUG << "Encoded transfer protocol "
                                     << *transferProtocol;

                    // Adjust imageURI to not have the protocol on it for
                    // parsing below ex. tftp://1.1.1.1/myfile.bin
                    // -> 1.1.1.1/myfile.bin
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
                BMCWEB_LOG_DEBUG
                    << "Server: " << tftpServer + " File: " << fwFile;


                if (fwUpdateInProgress != false)
                {
                    if (asyncResp)
                    {
                        std::string resolution =
                            "Another update is in progress. Retry"
                            " the update operation once it is complete.";
                        redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
                    }
                    return;
                }

                // Setup callback for when new software detected
                // Give TFTP 10 minutes to complete
                monitorForSoftwareAvailable(asyncResp, req, 600);

                // TFTP can take up to 10 minutes depending on image size and
                // connection speed. Return to caller as soon as the TFTP
                // operation has been started. The callback above will ensure
                // the activate is started once the download has completed
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
                            BMCWEB_LOG_DEBUG
                                << "Call to DownloaViaTFTP Success";
                        }
                    },
                    "xyz.openbmc_project.Software.Download",
                    "/xyz/openbmc_project/software",
                    "xyz.openbmc_project.Common.TFTP", "DownloadViaTFTP",
                    fwFile, tftpServer);

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
            messages::payloadTooLarge(asyncResp->res, resolution);
        }
        return;
    }

    // Only allow one FW update at a time
    if (fwUpdateInProgress != false)
    {
        if (asyncResp)
        {
            // don't copy the image, update already in progress.
            std::string resolution =
                "Another update is in progress. Retry"
                " the update operation once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Update already in progress.";
        }
        return;
    }

    std::error_code spaceInfoError;
    const std::filesystem::space_info spaceInfo =
        std::filesystem::space(updateServiceImageLocation, spaceInfoError);
    if (!spaceInfoError)
    {
        if (spaceInfo.free < req.body.size())
        {
            BMCWEB_LOG_ERROR
                << "Insufficient storage space. Required: " << req.body.size()
                << " Available: " << spaceInfo.free;
            std::string resolution =
                "Reset the baseboard and retry the operation.";
            messages::insufficientStorage(asyncResp->res, resolution);
            return;
        }
    }   

    std::string filepath(
        updateServiceImageLocation +
        boost::uuids::to_string(boost::uuids::random_generator()()));
    monitorForSoftwareAvailable(asyncResp, req, fwObjectCreationDefaultTimeout,
                                filepath);

    BMCWEB_LOG_DEBUG << "Writing file to " << filepath;
    std::ofstream out(filepath, std::ofstream::out | std::ofstream::binary |
                                    std::ofstream::trunc);
    out << req.body;
    out.close();
    BMCWEB_LOG_DEBUG << "file upload complete!!";

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
            asyncResp->res.jsonValue["Actions"]["Oem"]["Nvidia"]
                                    ["#NvidiaUpdateService.CommitImage"] = {
                {"target",
                 "/redfish/v1/UpdateService/Actions/Oem/NvidiaUpdateService.CommitImage"},
                {"@Redfish.ActionInfo",
                 "/redfish/v1/UpdateService/Oem/Nvidia/CommitImageActionInfo"}};
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            asyncResp->res.jsonValue["Oem"]["OCP"] = {
                {"@odata.type", "#OcpUpdateService.v1_0_0.OcpUpdateService"},
                {"PersistentStorage",
                 {"@odata.id",
                  "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage"}}};
#endif
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
            if (!json_util::readJsonPatch(req, asyncResp->res,
                                          "HttpPushUriOptions", pushUriOptions,
                                          "HttpPushUriTargets", imgTargets))
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
                        pushUriApplyTime /*, "ForceUpdate", forceUpdate*/))
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
                                    const boost::system::error_code
                errCodePolicy) { if (errCodePolicy)
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
                                messages::invalidObject(
                                    asyncResp->res,
                                    crow::utility::urlFromPieces(
                                        "HttpPushUriTargets"));
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
                    [aResp, objPath,
                     path](const boost::system::error_code errCodeController,
                           const dbus::utility::MapperGetSubTreeResponse&
                               subtree) {
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

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * @brief compute digest method handler invoke retimer hash computation
 *
 * @param[in] req - http request
 * @param[in] asyncResp - http response
 * @param[in] hashComputeObjPath - hash object path
 * @param[in] swId - software id
 */
inline void computeDigest(const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& hashComputeObjPath,
                          const std::string& swId)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, req, hashComputeObjPath, swId](
            const boost::system::error_code ec,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Failed to GetObject for ComputeDigest: "
                                 << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            // Ensure we only got one service back
            if (objInfo.size() != 1)
            {
                BMCWEB_LOG_ERROR << "Invalid Object Size " << objInfo.size();
                messages::internalError(asyncResp->res);
                return;
            }
            const std::string hashComputeService = objInfo[0].first;
            unsigned retimerId;
            try
            {
                retimerId = std::stoul(swId.substr(swId.rfind("_") + 1));
            }
            catch (const std::exception& e)
            {
                BMCWEB_LOG_ERROR << "Error while parsing retimer Id: "
                                 << e.what();
                messages::internalError(asyncResp->res);
                return;
            }
            // callback to reset hash compute state for timeout scenario
            auto timeoutCallback = [](const std::string_view state,
                                      size_t index) {
                nlohmann::json message{};
                if (state == "Started")
                {
                    message = messages::taskStarted(std::to_string(index));
                }
                else if (state == "Aborted")
                {
                    computeDigestInProgress = false;
                    message = messages::taskAborted(std::to_string(index));
                }
                return message;
            };
            // create a task to wait for the hash digest property changed signal
            std::shared_ptr<task::TaskData> task = task::TaskData::createTask(
                [hashComputeObjPath, hashComputeService](
                    boost::system::error_code ec,
                    sdbusplus::message::message& msg,
                    const std::shared_ptr<task::TaskData>& taskData) {
                    if (ec)
                    {
                        if (ec != boost::asio::error::operation_aborted)
                        {
                            taskData->state = "Aborted";
                            taskData->messages.emplace_back(
                                messages::resourceErrorsDetectedFormatError(
                                    "NvidiaSoftwareInventory.ComputeDigest",
                                    ec.message()));
                            taskData->finishTask();
                        }
                        computeDigestInProgress = false;
                        return task::completed;
                    }

                    std::string interface;
                    std::map<std::string, dbus::utility::DbusVariantType> props;

                    msg.read(interface, props);
                    if (interface == hashComputeInterface)
                    {
                        auto it = props.find("Digest");
                        if (it == props.end())
                        {
                            BMCWEB_LOG_ERROR
                                << "Signal doesn't have Digest value";
                            return !task::completed;
                        }
                        auto value = std::get_if<std::string>(&(it->second));
                        if (!value)
                        {
                            BMCWEB_LOG_ERROR
                                << "Digest value is not a string";
                            return !task::completed;
                        }

                        if (!(value->empty()))
                        {
                            std::string hashDigestValue = *value;
                            crow::connections::systemBus->async_method_call(
                                [taskData, hashDigestValue](
                                    const boost::system::error_code ec,
                                    const std::variant<std::string>& property) {
                                    if (ec)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "DBUS response error for Algorithm";
                                        taskData->state = "Exception";
                                        taskData->messages.emplace_back(
                                            messages::taskAborted(
                                                std::to_string(
                                                    taskData->index)));
                                        return;
                                    }
                                    const std::string* hashAlgoValue =
                                        std::get_if<std::string>(&property);
                                    if (hashAlgoValue == nullptr)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Null value returned for Algorithm";
                                        taskData->state = "Exception";
                                        taskData->messages.emplace_back(
                                            messages::taskAborted(
                                                std::to_string(
                                                    taskData->index)));
                                        return;
                                    }

                                    nlohmann::json jsonResponse;
                                    jsonResponse["FirmwareDigest"] =
                                        hashDigestValue;
                                    jsonResponse
                                        ["FirmwareDigestHashingAlgorithm"] =
                                            *hashAlgoValue;
                                    taskData->taskResponse.emplace(
                                        jsonResponse);
                                    std::string location =
                                        "Location: /redfish/v1/TaskService/Tasks/" +
                                        std::to_string(taskData->index) +
                                        "/Monitor";
                                    taskData->payload->httpHeaders.emplace_back(
                                        std::move(location));
                                    taskData->state = "Completed";
                                    taskData->percentComplete = 100;
                                    taskData->messages.emplace_back(
                                        messages::taskCompletedOK(
                                            std::to_string(taskData->index)));
                                    taskData->finishTask();
                                },
                                hashComputeService, hashComputeObjPath,
                                "org.freedesktop.DBus.Properties", "Get",
                                hashComputeInterface, "Algorithm");
                            computeDigestInProgress = false;
                            return task::completed;
                        }
                        else
                        {
                            BMCWEB_LOG_ERROR
                                << "GetHash failed. Digest is empty.";
                            taskData->state = "Exception";
                            taskData->messages.emplace_back(
                                messages::resourceErrorsDetectedFormatError(
                                    "NvidiaSoftwareInventory.ComputeDigest",
                                    "Hash Computation Failed"));
                            taskData->finishTask();
                            computeDigestInProgress = false;
                            return task::completed;
                        }
                    }
                    return !task::completed;
                },
                "type='signal',member='PropertiesChanged',"
                "interface='org.freedesktop.DBus.Properties',"
                "path='" +
                    hashComputeObjPath + "',",
                timeoutCallback);
            task->startTimer(std::chrono::seconds(retimerHashMaxTimeSec));
            task->populateResp(asyncResp->res);
            task->payload.emplace(req);
            computeDigestInProgress = true;
            crow::connections::systemBus->async_method_call(
                [task](const boost::system::error_code ec) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "Failed to ComputeDigest: " << ec;
                        task->state = "Aborted";
                        task->messages.emplace_back(
                            messages::resourceErrorsDetectedFormatError(
                                "NvidiaSoftwareInventory.ComputeDigest",
                                ec.message()));
                        task->finishTask();
                        computeDigestInProgress = false;
                        return;
                    }
                },
                hashComputeService, hashComputeObjPath, hashComputeInterface,
                "GetHash", retimerId);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", hashComputeObjPath,
        std::array<const char*, 1>{hashComputeInterface.c_str()});
}

/**
 * @brief post handler for compute digest method
 *
 * @param req
 * @param asyncResp
 * @param swId
 */
inline void
    handlePostComputeDigest(const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& swId)
{
    crow::connections::systemBus->async_method_call(
        [req, asyncResp, swId](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
            if (ec)
            {
                messages::resourceNotFound(
                    asyncResp->res, "NvidiaSoftwareInventory.ComputeDigest",
                    swId);
                BMCWEB_LOG_ERROR << "Invalid object path: " << ec;
                return;
            }
            for (auto& obj : subtree)
            {
                sdbusplus::message::object_path hashPath(obj.first);
                std::string hashId = hashPath.filename();
                if (hashId == swId)
                {
                    computeDigest(req, asyncResp, hashPath, swId);
                    return;
                }
            }
            messages::resourceNotFound(
                asyncResp->res, "NvidiaSoftwareInventory.ComputeDigest", swId);
            return;
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/com/Nvidia/ComputeHash", static_cast<int32_t>(0),
        std::array<const char*, 1>{hashComputeInterface.c_str()});
}

/**
 * @brief app handler for ComputeDigest action
 *
 * @param[in] app
 */
inline void requestRoutesComputeDigestPost(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/UpdateService/FirmwareInventory/<str>/Actions/Oem/"
             "NvidiaSoftwareInventory.ComputeDigest")
        .privileges(redfish::privileges::postUpdateService)
        .methods(
            boost::beast::http::verb::
                post)([&app](
                          const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& param) {
            if (!redfish::setUpRedfishRoute(app, req, asyncResp))
            {
                return;
            }
            BMCWEB_LOG_DEBUG
                << "Enter NvidiaSoftwareInventory.ComputeDigest doPost";
            std::shared_ptr<std::string> swId =
                std::make_shared<std::string>(param);
            // skip input parameter validation

            // 1. Firmware update and retimer hash cannot run in parallel
            if (fwUpdateInProgress)
            {
                redfish::messages::updateInProgressMsg(
                    asyncResp->res,
                    "Retry the operation once firmware update operation is complete.");
                BMCWEB_LOG_ERROR
                    << "Cannot execute ComputeDigest. Update firmware is in progress.";

                return;
            }
            // 2. Only one compute hash allowed at a time due to FPGA limitation
            if (computeDigestInProgress)
            {
                redfish::messages::resourceErrorsDetectedFormatError(
                    asyncResp->res, "NvidiaSoftwareInventory.ComputeDigest",
                    "Another ComputeDigest operation is in progress");
                BMCWEB_LOG_ERROR << "Cannot execute ComputeDigest."
                                 << " Another ComputeDigest is in progress.";
                return;
            }
            handlePostComputeDigest(req, asyncResp, *swId);
            BMCWEB_LOG_DEBUG << "Exit NvidiaUpdateService.ComputeDigest doPost";
        });
}

/**
 * @brief update oem action with ComputeDigest for devices which supports hash
 * compute
 *
 * @param[in] asyncResp
 * @param[in] swId
 */
inline void updateOemActionComputeDigest(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& swId)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, swId](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
            if (ec)
            {
                // hash compute interface is not applicable, ignore for the
                // device
                return;
            }
            for (auto& obj : subtree)
            {
                sdbusplus::message::object_path hashPath(obj.first);
                std::string hashId = hashPath.filename();
                if (hashId == swId)
                {
                    std::string computeDigestTarget =
                        "/redfish/v1/UpdateService/FirmwareInventory/" + swId +
                        "/Actions/Oem/NvidiaSoftwareInventory.ComputeDigest";
                    asyncResp->res
                        .jsonValue["Actions"]["Oem"]["Nvidia"]
                                  ["#NvidiaSoftwareInventory.ComputeDigest"] = {
                        {"target", computeDigestTarget}};
                    break;
                }
            }
            return;
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/com/Nvidia/ComputeHash", static_cast<int32_t>(0),
        std::array<const char*, 1>{hashComputeInterface.c_str()});
}
#endif

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


                                it = propertiesList.find("Manufacturer");
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
                            "org.freedesktop.DBus.Properties", "GetAll","");

                        asyncResp->res.jsonValue["Status"]["Health"] = "OK";
                        asyncResp->res.jsonValue["Status"]["HealthRollup"] =
                            "OK";
                        asyncResp->res.jsonValue["Status"]["Conditions"] =
                            nlohmann::json::array();
                    }
                    if (!found)
                    {
                        BMCWEB_LOG_ERROR
                            << "Input swID " + *swId + " not found!";
                        messages::resourceMissingAtURI(
                            asyncResp->res,
                            crow::utility::urlFromPieces(
                                "redfish", "v1", "UpdateService",
                                "FirmwareInventory", *swId));
                        return;
                    }
                    asyncResp->res.jsonValue["@odata.type"] =
                        "#SoftwareInventory.v1_4_0.SoftwareInventory";
                    asyncResp->res.jsonValue["Name"] = "Software Inventory";

                    asyncResp->res.jsonValue["Updateable"] = false;
                    fw_util::getFwUpdateableStatus(asyncResp, swId);
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                    updateOemActionComputeDigest(asyncResp, *swId);
#endif
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/xyz/openbmc_project/software/",
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
                        asyncResp->res.jsonValue["Status"]["Conditions"] =
                            nlohmann::json::array();

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
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/xyz/openbmc_project/software/",
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
    std::vector<CommitImageValueEntry>::iterator it =
        find(allowableValues.begin(), allowableValues.end(),
             static_cast<std::string>(inventoryPath));

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
inline std::pair<bool, CommitImageValueEntry>
    getAllowableValue(const std::string_view inventoryPath)
{
    std::pair<bool, CommitImageValueEntry> result;

    std::vector<CommitImageValueEntry> allowableValues = getAllowableValues();
    std::vector<CommitImageValueEntry>::iterator it =
        find(allowableValues.begin(), allowableValues.end(),
             static_cast<std::string>(inventoryPath));

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
inline void updateParametersForCommitImageInfo(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::vector<std::pair<
        std::string,
        std::vector<std::pair<std::string, std::vector<std::string>>>>>&
        subtree)
{
    asyncResp->res.jsonValue["Parameters"] = nlohmann::json::array();
    nlohmann::json& parameters = asyncResp->res.jsonValue["Parameters"];

    nlohmann::json parameterTargets;
    parameterTargets["Name"] = "Targets";
    parameterTargets["Required"] = "false";
    parameterTargets["DataType"] = "StringArray";
    parameterTargets["AllowableValues"] = nlohmann::json::array();

    nlohmann::json& allowableValues = parameterTargets["AllowableValues"];

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

        if (isInventoryAllowableValue(obj.first))
        {
            allowableValues.push_back(
                "/redfish/v1/UpdateService/FirmwareInventory/" + fwId);
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
inline void handleCommitImagePost(
    const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::vector<std::pair<
        std::string,
        std::vector<std::pair<std::string, std::vector<std::string>>>>>&
        subtree)
{
    std::optional<std::vector<std::string>> targets;

    if (!json_util::readJsonAction(req, asyncResp->res,
        "Targets", targets))
    {
        return;
    }

    bool hasTargets = false;

    if (targets && targets.value().empty() == false)
    {
        hasTargets = true;
    }

    if (hasTargets)
    {
        std::vector<std::string> targetsCollection = targets.value();

        for (auto& target : targetsCollection)
        {
            sdbusplus::message::object_path objectPath(target);
            std::string inventoryPath =
                "/xyz/openbmc_project/software/" + objectPath.filename();
            std::pair<bool, CommitImageValueEntry> result =
                getAllowableValue(inventoryPath);
            if (result.first == true)
            {
                uint32_t eid = result.second.mctpEndpointId;

                initBackgroundCopy(req, asyncResp, eid, result.second.inventoryUri);
            }
            else
            {
                BMCWEB_LOG_DEBUG
                    << "Cannot find firmware inventory in allowable values";
                messages::resourceMissingAtURI(
                    asyncResp->res, crow::utility::urlFromPieces(target));
            }
        }
    }
    else
    {
        for (auto& obj : subtree)
        {
            std::pair<bool, CommitImageValueEntry> result =
                getAllowableValue(obj.first);

            if (result.first == true)
            {
                uint32_t eid = result.second.mctpEndpointId;

                initBackgroundCopy(req, asyncResp, eid, result.second.inventoryUri);
            }
        }
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
                "#ActionInfo.v1_2_0.ActionInfo";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/Oem/Nvidia/CommitImageActionInfo";
            asyncResp->res.jsonValue["Name"] = "CommitImage Action Info";
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

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Actions/Oem/NvidiaUpdateService.CommitImage")
        .privileges(redfish::privileges::postUpdateService)
        .methods(
            boost::beast::http::verb::
                post)([](const crow::Request& req,
                         const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            BMCWEB_LOG_DEBUG << "doPost...";

            if (fwUpdateInProgress == true)
            {
                redfish::messages::updateInProgressMsg(
                    asyncResp->res,
                    "Retry the operation once firmware update operation is complete.");

                // don't copy the image, update already in progress.
                BMCWEB_LOG_ERROR
                    << "Cannot execute commit image. Update firmware is in progress.";

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
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
/**
 * @brief Add dbus watcher to wait till staged firmware object is created
 *
 * @param asyncResp
 * @param req
 * @param timeoutTimeSeconds
 * @param imagePath
 *
 * @return None
 */
static void addDBusWatchForSoftwareObject(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const crow::Request& req,
    int timeoutTimeSeconds = fwObjectCreationDefaultTimeout,
    const std::string& imagePath = {})
{

    fwStageAvailableTimer =
        std::make_unique<boost::asio::steady_timer>(*req.ioService);

    fwStageAvailableTimer->expires_after(
        std::chrono::seconds(timeoutTimeSeconds));

    fwStageAvailableTimer->async_wait(
        [asyncResp, imagePath](const boost::system::error_code& ec) {
            cleanUpStageObjects();

            if (ec == boost::asio::error::operation_aborted)
            {
                // expected, we were canceled before the timer completed.
                return;
            }
            BMCWEB_LOG_ERROR
                << "Timed out waiting for staged firmware object being created";
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Async_wait failed" << ec;
                return;
            }

            if (asyncResp)
            {
                redfish::messages::internalError(asyncResp->res);
            }

            if (!imagePath.empty())
            {
                std::filesystem::remove(imagePath);
            }
        });

    auto callback = [asyncResp](sdbusplus::message_t& m) {
        BMCWEB_LOG_DEBUG << "Match fired";

        sdbusplus::message::object_path path;
        dbus::utility::DBusInteracesMap interfaces;
        m.read(path, interfaces);

        if (std::find_if(
                interfaces.begin(), interfaces.end(), [](const auto& i) {
                    return i.first ==
                           "xyz.openbmc_project.Software.PackageInformation";
                }) != interfaces.end())
        {
            std::string stagedFirmwarePackageURI(
                "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages/0");
            asyncResp->res.result(boost::beast::http::status::created);
            asyncResp->res.jsonValue["StagedFirmwarePackageURI"] =
                stagedFirmwarePackageURI;
            asyncResp->res.addHeader(boost::beast::http::field::location,
                                     stagedFirmwarePackageURI);

            cleanUpStageObjects();
            fwStageAvailableTimer = nullptr;
        }
    };

    fwStageImageMatcher = std::make_unique<sdbusplus::bus::match_t>(
        *crow::connections::systemBus,
        "interface='org.freedesktop.DBus.ObjectManager',type='signal',"
        "member='InterfacesAdded',path='/'",
        callback);
}

/**
 * @brief Add dbus watcher to wait till staged firmware object is created
 * and upload firmware package
 *
 * @param asyncResp
 * @param req
 *
 * @return None
 */
inline void addDBusWatchAndUploadPackage(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const crow::Request& req)
{
    std::string filepath(
        updateServiceStageLocation +
        boost::uuids::to_string(boost::uuids::random_generator()()));

    try
    {
        addDBusWatchForSoftwareObject(asyncResp, req,
                                      fwObjectCreationDefaultTimeout, filepath);

        fwImageIsStaging = true;
        BMCWEB_LOG_DEBUG << "Writing file to " << filepath;
        std::ofstream out(filepath, std::ofstream::out | std::ofstream::binary |
                                        std::ofstream::trunc);
        out << req.body;
        out.close();
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_ERROR << "Error while uploading firmware: " << e.what();
        messages::internalError(asyncResp->res);
        cleanUpStageObjects();
        fwStageAvailableTimer = nullptr;
        return;
    }
}

/**
 * @brief Stage firmware package and fill dbus tree
 *
 * @param asyncResp
 * @param req
 *
 * @return None
 */
inline void
    stageFirmwarePackage(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const crow::Request& req)
{
    crow::connections::systemBus->async_method_call(
        [req, asyncResp](const boost::system::error_code ec,
                         const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            bool found = false;
            std::string foundService;
            std::string foundPath;

            for (const auto& obj : subtree)
            {
                if (obj.second.size() < 1)
                {
                    break;
                }

                foundService = obj.second[0].first;
                foundPath = obj.first;

                found = true;
                break;
            }

            if (found)
            {
                auto respHandler = [req, asyncResp](
                                       const boost::system::error_code ec) {
                    BMCWEB_LOG_DEBUG << "doDelete callback: Done";
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "doDelete respHandler got error "
                                         << ec.message();
                        asyncResp->res.result(
                            boost::beast::http::status::internal_server_error);
                        return;
                    }

                    addDBusWatchAndUploadPackage(asyncResp, req);
                };

                crow::connections::systemBus->async_method_call(
                    respHandler, foundService, foundPath,
                    "xyz.openbmc_project.Object.Delete", "Delete");
            }
            else
            {
                addDBusWatchAndUploadPackage(asyncResp, req);
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/software", static_cast<int32_t>(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Software.PackageInformation"});
}

/**
 * @brief POST handler for staging firmware package
 *
 * @param app
 * @param req
 * @param asyncResp
 *
 * @return None
 */
inline void handleUpdateServiceStageFirmwarePackagePost(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    BMCWEB_LOG_DEBUG << "doPost...";

    if (req.body.size() == 0)
    {
        if (asyncResp)
        {
            BMCWEB_LOG_ERROR << "Image size equals 0.";
            messages::unrecognizedRequestBody(asyncResp->res);
        }
        return;
    }

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
            messages::payloadTooLarge(asyncResp->res, resolution);
        }
        return;
    }

    // staging is not allowed when firmware update is in progress.
    if (fwUpdateInProgress != false)
    {
        if (asyncResp)
        {
            // don't copy the image, update already in progress.
            std::string resolution = "Another update is in progress. Retry"
                                     " the operation once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Update already in progress.";
        }
        return;
    }

    // Only allow one FW staging at a time
    if ((fwStageImageMatcher != nullptr) || (fwImageIsStaging == true))
    {
        if (asyncResp)
        {
            redfish::messages::resourceErrorsDetectedFormatError(
                asyncResp->res,
                "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/stage-firmware-package",
                "Another staging is in progress.");
            BMCWEB_LOG_ERROR << "Another staging is in progress.";
        }
        return;
    }

    // add rate limit by limiting number of updates per hour
    if (stagedUpdateCount == 0)
    {
        stagedUpdateTimeStamp = std::chrono::steady_clock::now();
        stagedUpdateCount += 1;
    }
    else if (stagedUpdateCount >= maxStagedUpdatesPerHour)
    {
        std::chrono::time_point<std::chrono::steady_clock> currentTime =
            std::chrono::steady_clock::now();
        auto elapsedTime = currentTime - stagedUpdateTimeStamp;
        if (elapsedTime <=
            std::chrono::seconds(stageRateLimitDurationInSeconds))
        {
            BMCWEB_LOG_ERROR << "Staging update ratelimit error. Duration: "
                             << elapsedTime.count();
            redfish::messages::resourceErrorsDetectedFormatError(
                asyncResp->res, "StageFirmwarePackage",
                "Staging Update Ratelimit Error");
            return;
        }

        BMCWEB_LOG_INFO << "Resetting the staging update counter. Duration: "
                        << elapsedTime.count();
        stagedUpdateTimeStamp = currentTime;
        stagedUpdateCount = 1; // reset the counter
    }
    else
    {
        stagedUpdateCount += 1;
    }

    std::error_code spaceInfoError;
    const std::filesystem::space_info spaceInfo =
        std::filesystem::space(updateServiceStageLocation, spaceInfoError);
    if (spaceInfoError)
    {
        messages::internalError(asyncResp->res);
        return;
    }
    else
    {
        if (spaceInfo.free < req.body.size())
        {
            BMCWEB_LOG_ERROR
                << "Insufficient storage space. Required: " << req.body.size()
                << " Available: " << spaceInfo.free;
            std::string resolution =
                "Reset the baseboard and retry the operation.";
            messages::insufficientStorage(asyncResp->res, resolution);
            return;
        }
    }

    bool stageLocationExists =
        std::filesystem::exists(updateServiceStageLocation);

    if (!stageLocationExists)
    {
        if (asyncResp)
        {
            messages::internalError(asyncResp->res);
            BMCWEB_LOG_ERROR << "Stage location does not exist. "
                             << updateServiceStageLocation;
        }

        return;
    }

    stageFirmwarePackage(asyncResp, req);
    BMCWEB_LOG_DEBUG << "stage firmware package complete";
}

/**
 * @brief Translate VerificationStatus to Redfish state
 *
 * This function will return the corresponding Redfish state
 *
 * @param[i]   status  The OpenBMC software state
 *
 * @return The corresponding Redfish state
 */
inline std::string getPackageVerificationStatus(const std::string& status)
{
    if (status ==
        "xyz.openbmc_project.Software.PackageInformation.PackageVerificationStatus.Invalid")
    {
        return "Failed";
    }
    if (status ==
        "xyz.openbmc_project.Software.PackageInformation.PackageVerificationStatus.Valid")
    {
        return "Success";
    }

    BMCWEB_LOG_DEBUG << "Unknown status: " << status;
    return "Unknown";
}

/**
 * @brief Fill properties related to StoragePackageInformation in the response
 *
 * @param asyncResp
 * @param service
 * @param path
 *
 * @return None
 */
inline void getPropertiesPersistentStoragePackageInformation(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Software.PackageInformation",
        [asyncResp](const boost::system::error_code errorCode,
                    const dbus::utility::DBusPropertiesMap& propertiesList) {
            if (errorCode)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            const std::string* packageVersion = nullptr;
            const std::string* packageVerificationStatus = nullptr;

            bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), propertiesList,
                "PackageVersion", packageVersion, "VerificationStatus",
                packageVerificationStatus);

            if (!success)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (packageVersion == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (packageVerificationStatus == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            asyncResp->res.jsonValue["FirmwarePackageVersion"] =
                *packageVersion;
            asyncResp->res.jsonValue["VerificationStatus"] =
                getPackageVerificationStatus(*packageVerificationStatus);
        });
}

/**
 * @brief Fill properties related to ComputeHash in the response
 *
 * @param asyncResp
 * @param service
 * @param path
 *
 * @return None
 */
inline void getPropertiesPersistentStorageComputeHash(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, path, "com.nvidia.ComputeHash",
        [asyncResp](const boost::system::error_code errorCode,
                    const dbus::utility::DBusPropertiesMap& propertiesList) {
            if (errorCode)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            const std::string* algorithm = nullptr;
            const std::string* digest = nullptr;

            bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), propertiesList, "Algorithm",
                algorithm, "Digest", digest);

            if (!success)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (algorithm == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (digest == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            asyncResp->res.jsonValue["FirmwarePackageDigestHashAlgorithm"] =
                *algorithm;
            asyncResp->res.jsonValue["FirmwarePackageDigest"] = *digest;
        });
}

/**
 * @brief Fill property StagedTimestamp in the response
 *
 * @param asyncResp
 * @param service
 * @param path
 *
 * @return None
 */
inline void getPropertiesPersistentStorageEpochTime(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Time.EpochTime",
        [asyncResp](const boost::system::error_code errorCode,
                    const dbus::utility::DBusPropertiesMap& propertiesList) {
            if (errorCode)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            const uint64_t* elapsed = nullptr;

            bool success = sdbusplus::unpackPropertiesNoThrow(
                dbus_utils::UnpackErrorPrinter(), propertiesList, "Elapsed",
                elapsed);

            if (!success)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            if (elapsed == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }

            uint64_t elapsedTimeStamp = *elapsed / 1000;

            asyncResp->res.jsonValue["StagedTimestamp"] =
                redfish::time_utils::getDateTimeUint(elapsedTimeStamp);
        });
}

/**
 * @brief GET handler to present list of staged firmware packages
 *
 * @param app
 * @param req
 * @param asyncResp
 *
 * @return None
 */
inline void handleUpdateServicePersistentStorageFwPackagesListGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] =
        "#OcpFirmwarePackageCollection.OcpFirmwarePackageCollection";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages";
    asyncResp->res.jsonValue["Name"] = "Firmware Package Collection";

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
            BMCWEB_LOG_DEBUG << "doGet callback...";
            if (ec)
            {
                asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
                asyncResp->res.jsonValue["Members@odata.count"] = 0;
                return;
            }

            bool found = false;

            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     obj : subtree)
            {
                if (obj.second.size() < 1)
                {
                    break;
                }

                asyncResp->res.jsonValue["Members"] = nlohmann::json::array();

                nlohmann::json& members = asyncResp->res.jsonValue["Members"];

                members.push_back(
                    {{"@odata.id",
                      "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages/0"}});
                asyncResp->res.jsonValue["Members@odata.count"] =
                    members.size();

                found = true;

                break;
            }

            if (!found)
            {
                asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
                asyncResp->res.jsonValue["Members@odata.count"] = 0;
                return;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/software/staged", static_cast<int32_t>(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Software.PackageInformation"});
}

/**
 * @brief GET handler to present details of staged firmware package
 *
 * @param app
 * @param req
 * @param asyncResp
 * @param strParam
 *
 * @return None
 */
inline void handleUpdateServicePersistentStorageFwPackageGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& strParam)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    if ((fwStageImageMatcher != nullptr) || (fwImageIsStaging == true))
    {
        if (asyncResp)
        {
            std::string resolution = "Staging is in progress. Retry"
                                     " the action once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Staging is in progress.";
        }
        return;
    }

    // current implementation assume that the collection of FwPackages has only
    // one item
    if (strParam != "0")
    {
        messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                   strParam);
        return;
    }

    asyncResp->res.jsonValue["@odata.type"] =
        "#OcpFirmwarePackage.v1_0_0.OcpFirmwarePackage";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages/0";
    asyncResp->res.jsonValue["Id"] = 0;
    asyncResp->res.jsonValue["Name"] = "Firmware Package 0 Resource";

    crow::connections::systemBus->async_method_call(
        [asyncResp,
         &strParam](const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
            BMCWEB_LOG_DEBUG << "doGet callback...";
            if (ec)
            {
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
                return;
            }
            // Ensure we find package information, otherwise return an
            // error
            bool found = false;

            for (const auto& obj : subtree)
            {
                if (obj.second.size() < 1)
                {
                    break;
                }

                found = true;

                getPropertiesPersistentStoragePackageInformation(
                    asyncResp, obj.second[0].first, obj.first);
                getPropertiesPersistentStorageComputeHash(
                    asyncResp, obj.second[0].first, obj.first);
                getPropertiesPersistentStorageEpochTime(
                    asyncResp, obj.second[0].first, obj.first);
            }
            if (!found)
            {
                BMCWEB_LOG_ERROR << "PackageInformation not found!";
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
                return;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/software/staged", static_cast<int32_t>(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Software.PackageInformation"});
}

/**
 * @brief Initiate staged firmware package.
 * The function create relevant task to monitor status of the process
 *
 * @param asyncResp
 * @param service
 * @param path
 * @param payload
 *
 * @return None
 */
inline void initiateStagedFirmwareUpdate(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& path,
    task::Payload&& payload)
{
    auto messageCallback = [](const std::string_view state,
                              [[maybe_unused]] size_t index) {
        nlohmann::json message{};
        if (state == "Started")
        {
            message = messages::taskStarted(std::to_string(index));
        }
        else if (state == "Aborted")
        {
            fwUpdateInProgress = false;
            message = messages::taskAborted(std::to_string(index));
        }
        else if (state == "Completed")
        {
            message = messages::taskCompletedOK(std::to_string(index));
            // fwupdate status is set in task callback
        }
        else
        {
            BMCWEB_LOG_INFO << "State not good";
        }
        return message;
    };

    std::shared_ptr<task::TaskData> task = task::TaskData::createTask(
        [](boost::system::error_code ec, sdbusplus::message_t& msg,
           const std::shared_ptr<task::TaskData>& taskData) {
            if (ec)
            {
                return task::completed;
            }

            std::string iface;
            boost::container::flat_map<std::string,
                                       dbus::utility::DbusVariantType>
                values;

            std::string index = std::to_string(taskData->index);
            msg.read(iface, values);

            if (iface == "xyz.openbmc_project.Software.Activation")
            {
                auto findActivation = values.find("Activation");
                if (findActivation == values.end())
                {
                    return !task::completed;
                }
                std::string* state =
                    std::get_if<std::string>(&(findActivation->second));
                if (state == nullptr)
                {
                    taskData->messages.emplace_back(messages::internalError());
                    fwUpdateInProgress = false;
                    return task::completed;
                }

                if (boost::ends_with(*state, "Invalid") ||
                    boost::ends_with(*state, "Failed"))
                {
                    taskData->state = "Exception";
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
                    taskData->extendTimer(std::chrono::hours(5));
                    fwUpdateInProgress = true;
                    return !task::completed;
                }

                if (boost::ends_with(*state, "Activating"))
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
                        messages::taskCompletedOK(index));
                    fwUpdateInProgress = false;
                    return task::completed;
                }
            }
            else if (iface == "xyz.openbmc_project.Software.ActivationProgress")
            {
                auto findProgress = values.find("Progress");
                if (findProgress == values.end())
                {
                    return !task::completed;
                }
                uint8_t* progress =
                    std::get_if<uint8_t>(&(findProgress->second));

                if (progress == nullptr)
                {
                    taskData->messages.emplace_back(messages::internalError());
                    return task::completed;
                }
                taskData->percentComplete = static_cast<int>(*progress);

                taskData->messages.emplace_back(messages::taskProgressChanged(
                    index, static_cast<size_t>(*progress)));

                // if we're getting status updates it's
                // still alive, update timer
                taskData->extendTimer(
                    std::chrono::minutes(updateServiceTaskTimeout));
            }

            return !task::completed;
        },
        "type='signal',interface='org.freedesktop.DBus.Properties',"
        "member='PropertiesChanged',path='" +
            path + "'",
        messageCallback);

    task->startTimer(std::chrono::minutes(updateServiceTaskTimeout));
    task->populateResp(asyncResp->res);
    task->payload.emplace(std::move(payload));
    loggingMatch = std::make_unique<sdbusplus::bus::match::match>(
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

    fwAvailableTimer = nullptr;
    activateImage(path, service);
}

/**
 * @brief Stage firmware package and fill dbus tree
 *
 * @param asyncResp
 * @param req
 *
 * @return None
 */
inline void initiateFirmwarePackage(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const crow::Request& req,
    const std::optional<std::vector<std::string>>& targets)
{
    crow::connections::systemBus->async_method_call(
        [req, asyncResp,
         targets](const boost::system::error_code ec,
                  const crow::openbmc_mapper::GetSubTreeType& subtree) {
            BMCWEB_LOG_DEBUG << "doGet callback...";
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error code = " << ec;
                BMCWEB_LOG_DEBUG << "DBUS response error msg = "
                                 << ec.message();
                messages::internalError(asyncResp->res);
                return;
            }

            bool found = false;
            std::string foundService;
            std::string foundPath;

            for (const auto& obj : subtree)
            {
                if (obj.second.size() < 1)
                {
                    break;
                }

                found = true;
                foundService = obj.second[0].first;
                foundPath = obj.first;

                break;
            }

            if (found)
            {

                fwAvailableTimer =
                    std::make_unique<boost::asio::steady_timer>(*req.ioService);

                fwAvailableTimer->expires_after(
                    std::chrono::seconds(fwObjectCreationDefaultTimeout));

                fwAvailableTimer->async_wait([asyncResp](const boost::system::
                                                             error_code& ec) {
                    cleanUp();
                    if (ec == boost::asio::error::operation_aborted)
                    {
                        // expected, we were canceled before the timer
                        // completed.
                        return;
                    }
                    BMCWEB_LOG_ERROR
                        << "Timed out waiting for firmware object being created";

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

                if (targets)
                {
                    crow::connections::systemBus->async_method_call(
                        [req, asyncResp, foundService, foundPath,
                         uriTargets{*targets}](
                            const boost::system::error_code ec,
                            const std::vector<std::string>& swInvPaths) {
                            if (ec)
                            {
                                BMCWEB_LOG_ERROR << "D-Bus responses error: "
                                                 << ec;
                                messages::internalError(asyncResp->res);
                                return;
                            }

                            std::vector<sdbusplus::message::object_path>
                                httpUriTargets = {};
                            // validate TargetUris if entries are present
                            if (uriTargets.size() != 0)
                            {
                                std::vector<std::string> invalidTargets;
                                for (const std::string& target : uriTargets)
                                {
                                    std::string compName =
                                        std::filesystem::path(target)
                                            .filename();

                                    bool validTarget = false;
                                    std::string objPath =
                                        "software/" + compName;

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
                                            sdbusplus::message::object_path
                                                objpath(path);
                                            httpUriTargets.emplace_back(
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
                                    messages::invalidObject(
                                        asyncResp->res,
                                        crow::utility::urlFromPieces(
                                            "Targets"));
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
                                            << "Invalid UriTarget: "
                                            << invalidTarget << "\n";
                                        messages::propertyValueFormatError(
                                            asyncResp->res, invalidTarget,
                                            "Targets");
                                    }
                                    asyncResp->res.result(
                                        boost::beast::http::status::ok);
                                }
                                // else all targets are valid
                            }
                            crow::connections::systemBus->async_method_call(
                                [req, asyncResp, foundService, foundPath,
                                 httpUriTargets](
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
                                            messages::internalError(
                                                asyncResp->res);
                                        }
                                        return;
                                    }
                                    // Ensure we only got one service back
                                    if (objInfo.size() != 1)
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Invalid Object Size "
                                            << objInfo.size();
                                        if (asyncResp)
                                        {
                                            messages::internalError(
                                                asyncResp->res);
                                        }
                                        return;
                                    }

                                    crow::connections::systemBus->async_method_call(
                                        [req, asyncResp, foundService,
                                         foundPath](
                                            const boost::system::error_code
                                                errCodePolicy) {
                                            if (errCodePolicy)
                                            {
                                                BMCWEB_LOG_ERROR
                                                    << "error_code = "
                                                    << errCodePolicy;
                                                messages::internalError(
                                                    asyncResp->res);
                                            }

                                            task::Payload payload(req);
                                            initiateStagedFirmwareUpdate(
                                                asyncResp, foundService,
                                                foundPath, std::move(payload));

                                            fwUpdateInProgress = true;
                                        },
                                        objInfo[0].first, foundPath,
                                        "org.freedesktop.DBus.Properties",
                                        "Set",
                                        "xyz.openbmc_project.Software.UpdatePolicy",
                                        "Targets",
                                        dbus::utility::DbusVariantType(
                                            httpUriTargets));
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
                        "/xyz/openbmc_project/software/",
                        static_cast<int32_t>(0),
                        std::array<std::string, 1>{
                            "xyz.openbmc_project.Software.Version"});
                }
                else
                {
                    task::Payload payload(req);

                    initiateStagedFirmwareUpdate(asyncResp, foundService,
                                                 foundPath, std::move(payload));

                    fwUpdateInProgress = true;
                }
            }
            else
            {
                BMCWEB_LOG_ERROR << "PackageInformation not found!";
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
                return;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/software/staged", static_cast<int32_t>(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Software.PackageInformation"});
}

/**
 * @brief POST handler for initiating firmware package
 *
 * @param app
 * @param req
 * @param asyncResp
 *
 * @return None
 */
inline void handleUpdateServiceInitiateFirmwarePackagePost(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    BMCWEB_LOG_DEBUG << "doPost...";

    std::optional<std::string> firmwarePackageURI;
    std::optional<std::vector<std::string>> targets;
    if (!json_util::readJsonPatch(req, asyncResp->res, "StagedFirmwarePackageURI",
                                  firmwarePackageURI, "Targets", targets))
    {
        BMCWEB_LOG_ERROR << "UpdateService doPatch: Invalid request body";
        return;
    }

    if (firmwarePackageURI)
    {
        if (firmwarePackageURI !=
            "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages/0")
        {
            BMCWEB_LOG_ERROR << "Invalid StagedFirmwarePackageURI:  "
                             << firmwarePackageURI.value();
            messages::propertyValueIncorrect(asyncResp->res,
                                             "StagedFirmwarePackageURI",
                                             firmwarePackageURI.value());
            return;
        }
    }
    else
    {
        BMCWEB_LOG_ERROR << "StagedFirmwarePackageURI is empty.";
        messages::propertyValueIncorrect(asyncResp->res, "StagedFirmwarePackageURI",
                                         "empty");
        return;
    }

    if (fwUpdateInProgress != false)
    {
        if (asyncResp)
        {
            std::string resolution = "Update is in progress. Retry"
                                     " the action once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Update is in progress.";
        }
        return;
    }

    if ((fwStageImageMatcher != nullptr) || (fwImageIsStaging == true))
    {
        if (asyncResp)
        {
            std::string resolution = "Staging is in progress. Retry"
                                     " the action once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Staging is in progress.";
        }
        return;
    }

    crow::connections::systemBus->async_method_call(
        [req, asyncResp, uriTargets{*targets}](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            BMCWEB_LOG_DEBUG << "doGet callback...";
            if (ec)
            {
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
                return;
            }
            // Ensure we find package information, otherwise return an
            // error
            bool found = false;

            for (const auto& obj : subtree)
            {
                if (obj.second.size() < 1)
                {
                    break;
                }

                found = true;
                initiateFirmwarePackage(asyncResp, req, uriTargets);
            }
            if (!found)
            {
                BMCWEB_LOG_ERROR << "PackageInformation not found!";
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
                return;
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/software/staged", static_cast<int32_t>(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Software.PackageInformation"});
}

/**
 * @brief Update parameters for GET Method InitiateFirmwareUpdateActionInfo
 *
 * @param[in] asyncResp Shared pointer to the response message
 * @param[in] subtree  Collection of objectmappers for
 * "/xyz/openbmc_project/software"
 *
 * @return None
 */
inline void updateParametersForInitiateActionInfo(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const crow::openbmc_mapper::GetSubTreeType& subtree)
{
    asyncResp->res.jsonValue["Parameters"] = nlohmann::json::array();
    nlohmann::json& parameters = asyncResp->res.jsonValue["Parameters"];

    nlohmann::json parameterPackageURI;
    parameterPackageURI["Name"] = "StagedFirmwarePackageURI";
    parameterPackageURI["Required"] = "true";
    parameterPackageURI["DataType"] = "String";

    parameters.push_back(parameterPackageURI);

    nlohmann::json parameterTargets;
    parameterTargets["Name"] = "Targets";
    parameterTargets["Required"] = "false";
    parameterTargets["DataType"] = "StringArray";
    parameterTargets["AllowableValues"] = nlohmann::json::array();

    nlohmann::json& allowableValues = parameterTargets["AllowableValues"];

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

        if (isInventoryAllowableValue(obj.first))
        {
            allowableValues.push_back(
                "/redfish/v1/UpdateService/FirmwareInventory/" + fwId);
        }
    }

    parameters.push_back(parameterTargets);
}

/**
 * @brief DELETE handler for deleting firmware package
 *
 * @param app
 * @param req
 * @param asyncResp
 * @param strParam
 *
 * @return None
 */
inline void handleUpdateServiceDeleteFirmwarePackage(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& strParam)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    // current implementation assume that the collection of
    // FwPackages has only one item
    if (strParam != "0")
    {
        messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                   strParam);
        return;
    }

    if (fwUpdateInProgress != false)
    {
        if (asyncResp)
        {
            std::string resolution = "Update is in progress. Retry"
                                     " the action once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Update is in progress.";
        }
        return;
    }

    if ((fwStageImageMatcher != nullptr) || (fwImageIsStaging == true))
    {
        if (asyncResp)
        {
            std::string resolution = "Staging is in progress. Retry"
                                     " the action once it is complete.";
            redfish::messages::updateInProgressMsg(asyncResp->res, resolution);
            BMCWEB_LOG_ERROR << "Staging is in progress.";
        }
        return;
    }

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const crow::openbmc_mapper::GetSubTreeType& subtree) {
            BMCWEB_LOG_DEBUG << "doDelete callback...";
            if (ec)
            {
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
                return;
            }

            // Ensure we find package information, otherwise return
            // an error
            bool found = false;
            std::string foundService;
            std::string foundPath;

            for (const auto& obj : subtree)
            {
                if (obj.second.size() < 1)
                {
                    break;
                }

                foundService = obj.second[0].first;
                foundPath = obj.first;

                found = true;
                break;
            }

            if (found)
            {
                auto respHandler = [asyncResp](
                                       const boost::system::error_code ec) {
                    BMCWEB_LOG_DEBUG << "doDelete callback: Done";
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "doDelete respHandler got error "
                                         << ec.message();
                        asyncResp->res.result(
                            boost::beast::http::status::internal_server_error);
                        return;
                    }

                    asyncResp->res.result(boost::beast::http::status::ok);
                };

                crow::connections::systemBus->async_method_call(
                    respHandler, foundService, foundPath,
                    "xyz.openbmc_project.Object.Delete", "Delete");
            }
            else
            {
                BMCWEB_LOG_ERROR << "PackageInformation not found!";
                messages::resourceNotFound(asyncResp->res, "FirmwarePackages",
                                           "0");
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/software/staged", static_cast<int32_t>(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Software.PackageInformation"});
}

/**
 * @brief start emmc partition service and waits for completion
 *
 * @param[in] asyncResp
 */
inline void startEMMCPartitionService(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    std::string serviceUnit = "nvidia-emmc-partition.service";
    auto emmcServiceSignalCallback = [asyncResp, serviceUnit](
                                         sdbusplus::message_t& msg) mutable {
        BMCWEB_LOG_DEBUG
            << "Received signal for emmc partition service state change";
        uint32_t newStateID{};
        sdbusplus::message::object_path newStateObjPath;
        std::string newStateUnit{};
        std::string newStateResult{};

        // Read the msg and populate each variable
        msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);
        if (newStateUnit == serviceUnit)
        {
            if (newStateResult == "done" || newStateResult == "failed" ||
                newStateResult == "dependency")
            {
                crow::connections::systemBus->async_method_call(
                    [asyncResp](const boost::system::error_code ec,
                                const std::variant<int32_t>& property) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR
                                << "DBUS response error getting service status: "
                                << ec.message();
                            redfish::messages::internalError(asyncResp->res);
                            emmcServiceSignalMatch = nullptr;
                            return;
                        }
                        const int32_t* serviceStatus =
                            std::get_if<int32_t>(&property);
                        if (*serviceStatus == emmcPartitionMounted ||
                            *serviceStatus == eudaProgrammed ||
                            *serviceStatus == eudaProgrammedNotActivated)
                        {
                            std::string resolution =
                                "PersistentStorage Enable operation is successful. "
                                "Reset the baseboard to activate the PersistentStorage";
                            BMCWEB_LOG_INFO
                                << "PersistentStorage enable success.";
                            redfish::messages::success(asyncResp->res,
                                                       resolution);
                            emmcServiceSignalMatch = nullptr;
                        }
                        else
                        {
                            BMCWEB_LOG_ERROR
                                << "EMMC Service failed with error: "
                                << *serviceStatus;
                            redfish::messages::internalError(asyncResp->res);
                            emmcServiceSignalMatch = nullptr;
                        }
                    },
                    "org.freedesktop.systemd1",
                    "/org/freedesktop/systemd1/unit/nvidia_2demmc_2dpartition_2eservice",
                    "org.freedesktop.DBus.Properties", "Get",
                    "org.freedesktop.systemd1.Service", "ExecMainStatus");
            }
        }
    };
    emmcServiceSignalMatch = std::make_unique<sdbusplus::bus::match::match>(
        *crow::connections::systemBus,
        "interface='org.freedesktop.systemd1.Manager',type='signal',"
        "member='JobRemoved',path='/org/freedesktop/systemd1'",
        emmcServiceSignalCallback);
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}](const boost::system::error_code ec) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "Error while starting EMMC partition service";
                BMCWEB_LOG_ERROR << "DBUS response error code = " << ec;
                BMCWEB_LOG_ERROR << "DBUS response error msg = "
                                 << ec.message();
                emmcServiceSignalMatch = nullptr;
                messages::internalError(asyncResp->res);
                return;
            }
        },
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "RestartUnit", serviceUnit,
        "replace");
}

/**
 * @brief enable EMMC
 *
 * @param[in] req
 * @param[in] asyncResp
 */
inline void enableEMMC(const crow::Request& req,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    std::string setCommand = "/sbin/fw_setenv emmc enable";
    PersistentStorageUtil persistentStorageUtil;
    auto setEMMCCallback =
        []([[maybe_unused]] const crow::Request& req,
           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
           [[maybe_unused]] const std::string& stdOut,
           [[maybe_unused]] const std::string& stdErr,
           [[maybe_unused]] const boost::system::error_code& ec,
           [[maybe_unused]] int errorCode) -> void {
        BMCWEB_LOG_INFO << "PersistentStorage setting env is success";
        startEMMCPartitionService(asyncResp);
        return;
    };
    persistentStorageUtil.executeEnvCommand(req, asyncResp, setCommand,
                                            std::move(setEMMCCallback));    
    return;
}

/**
 * @brief patch handler for persistent storage service
 *
 * @param[in] app
 * @param[in] req
 * @param[in] asyncResp
 */
inline void handleUpdateServicePersistentStoragePatch(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    std::optional<bool> enabled;
    if (!json_util::readJsonPatch(req, asyncResp->res, "Enabled", enabled))
    {
        BMCWEB_LOG_ERROR << "PersistentStorage doPatch: Invalid request body";
        return;
    }
    if (enabled)
    {
        if (*enabled == false)
        {
            redfish::messages::resourceErrorsDetectedFormatError(
                asyncResp->res, "PersistentStorage.Enable",
                "Disabling PersistentStorage is not allowed");
            BMCWEB_LOG_ERROR << "Disabling PersistentStorage is not allowed.";
        }
        else
        {            
            std::string getCommand = "/sbin/fw_printenv";
            PersistentStorageUtil persistentStorageUtil;
            auto getEMMCCallback =
                [](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& stdOut,
                   [[maybe_unused]] const std::string& stdErr,
                   [[maybe_unused]] const boost::system::error_code& ec,
                   [[maybe_unused]] int errorCode) -> void {
                if (stdOut.find("emmc=enable") != std::string::npos)
                {
                    BMCWEB_LOG_ERROR << "PersistentStorage already enabled";
                    std::string resolution =
                        "PersistentStorage is already enabled. If "
                        "PesistentStorage is not activated, reset the baseboard"
                        " to activate the PersistentStorage";
                    redfish::messages::resourceErrorsDetectedFormatError(
                        asyncResp->res, "PersistentStorage.Enable",
                        "PersistentStorage already enabled", resolution);
                }
                else
                {
                    BMCWEB_LOG_INFO << "PersistentStorage is not enabled."
                                     << " Enabling PersistentStorage";
                    enableEMMC(req, asyncResp);
                }
                return;
            };
            persistentStorageUtil.executeEnvCommand(req, asyncResp, getCommand,
                                                    std::move(getEMMCCallback));
        }
    }
    return;
}
/**
 * @brief Register Web Api endpoints for Split Update Firmware Package
 * functionality
 *
 * @param app
 *
 * @return None
 */
inline void requestRoutesSplitUpdateService(App& app)
{

    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage")
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
                "#OcpPersistentStorage.v1_0_0.OcpPersistentStorage";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage";
            asyncResp->res.jsonValue["Id"] = "PersistentStorage";
            asyncResp->res.jsonValue["Name"] = "Persistent Storage Resource";
            asyncResp->res.jsonValue["StageFirmwarePackageHttpPushUri"] =
                "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/stage-firmware-package";
            asyncResp->res.jsonValue["MaxFirmwarePackages"] = 1;
            asyncResp->res.jsonValue["FirmwarePackages"] = {
                {"@odata.id",
                 "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages"}};
            asyncResp->res.jsonValue
                ["Actions"]
                ["#OcpPersistentStorage.InitiateFirmwareUpdate"] = {
                {"target",
                 "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/Actions/OcpPersistentStorage.InitiateFirmwareUpdate"},
                {"@Redfish.ActionInfo",
                 "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/InitiateFirmwareUpdateActionInfo"}};
            std::string getCommand = "/sbin/fw_printenv";
            PersistentStorageUtil persistentStorageUtil;
            auto respCallback =
                []([[maybe_unused]] const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& stdOut,
                   [[maybe_unused]] const std::string& stdErr,
                   [[maybe_unused]] const boost::system::error_code& ec,
                   [[maybe_unused]] int errorCode) -> void {
                if (stdOut.find("emmc=enable") != std::string::npos)
                {
                    asyncResp->res.jsonValue["Enabled"] = true;
                    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
                }
                else
                {
                    asyncResp->res.jsonValue["Enabled"] = false;
                    asyncResp->res.jsonValue["Status"]["State"] = "Disabled";
                }
            };
            persistentStorageUtil.executeEnvCommand(req, asyncResp, getCommand,
                                                    respCallback);
        });
    BMCWEB_ROUTE(app, "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage")
        .privileges(redfish::privileges::patchUpdateService)
        .methods(boost::beast::http::verb::patch)(std::bind_front(
            handleUpdateServicePersistentStoragePatch, std::ref(app)));
    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/stage-firmware-package")
        .privileges(redfish::privileges::postUpdateService)
        .methods(boost::beast::http::verb::post)(std::bind_front(
            handleUpdateServiceStageFirmwarePackagePost, std::ref(app)));

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages")
        .privileges(redfish::privileges::getUpdateService)
        .methods(boost::beast::http::verb::get)(std::bind_front(
            handleUpdateServicePersistentStorageFwPackagesListGet,
            std::ref(app)));

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages/<str>/")
        .privileges(redfish::privileges::getUpdateService)
        .methods(boost::beast::http::verb::get)(std::bind_front(
            handleUpdateServicePersistentStorageFwPackageGet, std::ref(app)));

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/Actions/OcpPersistentStorage.InitiateFirmwareUpdate")
        .privileges(redfish::privileges::postUpdateService)
        .methods(boost::beast::http::verb::post)(std::bind_front(
            handleUpdateServiceInitiateFirmwarePackagePost, std::ref(app)));

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/InitiateFirmwareUpdateActionInfo")
        .privileges(redfish::privileges::getUpdateService)
        .methods(
            boost::beast::http::verb::
                get)([](const crow::Request&,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            asyncResp->res.jsonValue["@odata.type"] =
                "#ActionInfo.v1_2_0.ActionInfo";
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/InitiateFirmwareUpdateActionInfo";
            asyncResp->res.jsonValue["Name"] =
                "InitiateFirmwareUpdate Action Info";
            asyncResp->res.jsonValue["Id"] = "InitiateFirmwareUpdateActionInfo";

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
                        BMCWEB_LOG_DEBUG << "DBUS response error code = " << ec;
                        BMCWEB_LOG_DEBUG << "DBUS response error msg = "
                                         << ec.message();
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    updateParametersForInitiateActionInfo(asyncResp, subtree);
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/software", static_cast<int32_t>(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Software.Version"});
        });

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/UpdateService/Oem/OCP/PersistentStorage/FirmwarePackages/<str>/")
        .privileges(redfish::privileges::deleteUpdateService)
        .methods(boost::beast::http::verb::delete_)(std::bind_front(
            handleUpdateServiceDeleteFirmwarePackage, std::ref(app)));
}

#endif

} // namespace redfish
