/*
// Copyright (c) 2022 Nvidia Corporation
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
#include "error_messages.hpp"

#include <boost/asio.hpp>
#include <boost/process.hpp>

#include <functional>
#include <iostream>
#include <string>

namespace redfish
{

namespace bp = boost::process;

using ExitCode = int32_t;
using ErrorMessage = std::string;
using Resolution = std::string;
using ErrorMapping = std::pair<ErrorMessage, Resolution>;

/* Match signals added on emmc partition service */
static std::unique_ptr<sdbusplus::bus::match_t> emmcServiceSignalMatch;

/* Exist codes returned by nvidia-emmc partition service after completion.*/
enum EMMCServiceExitCodes
{
    emmcPartitionMounted = 0,
    emmcInitFail = 1,
    emmcDisabled = 2,
    eudaProgramFail = 3,
    eudaProgrammedNotActivated = 4,
    emmcPartitionFail = 5,
    emmcFileSystemFormatFail = 6,
    emmcMountFail = 7
};

/* EMMC Service error mapping */
std::unordered_map<ExitCode, ErrorMapping> emmcServiceErrorMapping = {
    {emmcInitFail,
     {"PersistentStorage Initialization Failure",
      "Reset the baseboard and retry the operation."}},
    {eudaProgramFail,
     {"PersistentStorage Configuration Failure", "Retry the operation."}},
    {eudaProgrammedNotActivated,
     {"PersistentStorage Enabled but not activated",
      "Reset the baseboard to activate the PersistentStorage."}},
    {emmcPartitionFail,
     {"PersistentStorage Internal Error: Partition Fail",
      "Reset the baseboard and retry the operation."}},
    {emmcFileSystemFormatFail,
     {"PersistentStorage Internal Error: File System Format Failure",
      "Reset the baseboard and retry the operation."}},
    {emmcMountFail,
     {"PersistentStorage Internal Error: Mount Failure",
      "Reset the baseboard and retry the operation."}},
};

/**
 * @brief get EMMC error message from service exit code
 *
 * @param[in] exitCode
 * @return std::optional<ErrorMapping>
 */
std::optional<ErrorMapping> getEMMCErrorMessageFromExitCode(ExitCode exitCode)
{
    if (emmcServiceErrorMapping.find(exitCode) != emmcServiceErrorMapping.end())
    {
        return emmcServiceErrorMapping[exitCode];
    }

    BMCWEB_LOG_ERROR << "No mapping found for ExitCode: " << exitCode;
    return std::nullopt;
}

using AsyncResponseCallback = std::function<void(
    const crow::Request&, const std::shared_ptr<bmcweb::AsyncResp>&,
    const std::string& /* stdOut*/, const std::string& /* stdErr*/,
    const boost::system::error_code& /* ec */, int /*errorCode */)>;

struct PersistentStorageUtil
{
  public:
    /**
     * @brief updates persistent storage enabled property by reading the uboot
     * env variable
     *
     * @param req
     * @param asyncResp
     * @param command
     * @param respCallback
     */
    void executeEnvCommand(const crow::Request& req,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& command,
                           AsyncResponseCallback responseCallback)
    {
        auto dataOut = std::make_shared<boost::process::ipstream>();
        auto dataErr = std::make_shared<boost::process::ipstream>();
        auto exitCallback = [&req, asyncResp, dataOut, dataErr, command,
                             respCallback = std::move(responseCallback)](
                                const boost::system::error_code& ec,
                                int errorCode) mutable {
            std::string stdOut;
            std::string stdErr;
            if (ec || errorCode)
            {
                BMCWEB_LOG_ERROR
                    << "Error while executing persistent storage command: "
                    << command << " Error Code: " << errorCode;

                while (*dataErr)
                {
                    std::string line;
                    std::getline(*dataErr, line);
                    stdErr += line + "\n";
                }
                dataErr->close();
                BMCWEB_LOG_ERROR << "Command Response: " << stdErr;
                if (ec)
                {
                    BMCWEB_LOG_ERROR
                        << "Error while executing command: " << command
                        << " Message: " << ec.message();
                }
                return;
            }
            while (*dataOut)
            {
                std::string line;
                std::getline(*dataOut, line);
                stdOut += line + "\n";
            }
            dataOut->close();
            respCallback(req, asyncResp, stdOut, stdErr, ec, errorCode);
            return;
        };
        bp::async_system(crow::connections::systemBus->get_io_context(),
                         exitCallback, command, bp::std_in.close(),
                         bp::std_out > *dataOut, bp::std_err > *dataErr);
    }
};

/**
 * @brief reset emmc variable when enabling emmc service fails
 *
 * @param[in] req
 * @param[in] asyncResp
 */
inline void resetEMMCEnvironmentVariable(
    const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    std::string setCommand = "/sbin/fw_setenv emmc";
    PersistentStorageUtil persistentStorageUtil;
    auto resetEMMCCallback =
        []([[maybe_unused]] const crow::Request& req,
           [[maybe_unused]] const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
           [[maybe_unused]] const std::string& stdOut,
           [[maybe_unused]] const std::string& stdErr,
           [[maybe_unused]] const boost::system::error_code& ec,
           [[maybe_unused]] int errorCode) -> void {
        BMCWEB_LOG_INFO << "Resetting PersistentStorage env";
        return;
    };
    persistentStorageUtil.executeEnvCommand(req, asyncResp, setCommand,
                                            std::move(resetEMMCCallback));
}

/**
 * @brief start emmc partition service and waits for completion
 *
 * @param[in] req
 * @param[in] asyncResp
 */
inline void startEMMCPartitionService(
    const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    std::string serviceUnit = "nvidia-emmc-partition.service";
    auto emmcServiceSignalCallback = [req, asyncResp, serviceUnit](
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
                    [req, asyncResp](const boost::system::error_code ec,
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
                        if (serviceStatus == nullptr)
                        {
                            BMCWEB_LOG_ERROR
                                << "Invalid service exit status code";
                            redfish::messages::internalError(asyncResp->res);
                            return;
                        }
                        if (*serviceStatus == emmcPartitionMounted ||
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
                            std::optional<ErrorMapping> errorMapping =
                                getEMMCErrorMessageFromExitCode(*serviceStatus);
                            if (errorMapping)
                            {
                                BMCWEB_LOG_ERROR
                                    << "PersistentStorage.Enable Error Message: "
                                    << (*errorMapping).first;
                                redfish::messages::
                                    resourceErrorsDetectedFormatError(
                                        asyncResp->res,
                                        "PersistentStorage.Enable",
                                        (*errorMapping).first,
                                        (*errorMapping).second);
                            }
                            else
                            {
                                redfish::messages::internalError(
                                    asyncResp->res);
                            }
                            resetEMMCEnvironmentVariable(req, asyncResp);
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
        startEMMCPartitionService(req, asyncResp);
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
    const crow::Request& req, bool enabled,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{

    if (enabled == false)
    {
        BMCWEB_LOG_ERROR << "Disabling PersistentStorage is not allowed.";
        messages::propertyValueIncorrect(
            asyncResp->res, "PersistentStorage.Enable", "false");
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
                redfish::messages::noOperation(asyncResp->res);
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

    return;
}

/**
 * @brief populate Enabled and Status.State property based on EMMC enablement and EMMC service exit code
 * 
 * @param asyncResp 
 */
inline void
    populatePersistentStorageSettingStatus(const crow::Request& req, const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
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
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["PersistentStorageSettings"]["Enabled"] = true;
                }
                else
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["PersistentStorageSettings"]["Enabled"] = false;
                }
        };
    persistentStorageUtil.executeEnvCommand(req, asyncResp, getCommand,
                                                    respCallback);
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const std::variant<int32_t>& property) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "DBUS response error getting service status: "
                    << ec.message();
                redfish::messages::internalError(asyncResp->res);
                return;
            }
            const int32_t* serviceStatus = std::get_if<int32_t>(&property);
            if (serviceStatus == nullptr)
            {
                BMCWEB_LOG_ERROR << "Invalid service exit status code";
                redfish::messages::internalError(asyncResp->res);
                return;
            }
            if (*serviceStatus == emmcPartitionMounted)
            {
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["PersistentStorageSettings"]["Status"]["State"] = "Enabled";
            }
            else if (*serviceStatus == eudaProgrammedNotActivated)
            {
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["PersistentStorageSettings"]["Status"]["State"] = "StandbyOffline";
            }
            else
            {
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["PersistentStorageSettings"]["Status"]["State"] = "Disabled";
            }
        },
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1/unit/nvidia_2demmc_2dpartition_2eservice",
        "org.freedesktop.DBus.Properties", "Get",
        "org.freedesktop.systemd1.Service", "ExecMainStatus");
}

} // namespace redfish