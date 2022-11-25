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

#include "health.hpp"
#include "redfish_util.hpp"

#include <app.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time.hpp>
#include <dbus_utility.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/fw_utils.hpp>
#include <utils/dbus_utils.hpp>
#include <utils/systemd_utils.hpp>
#include <utils/hex_utils.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <variant>

namespace redfish
{

// Map of service name to list of interfaces
using MapperServiceMap =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

// Map of object paths to MapperServiceMaps
using MapperGetSubTreeResponse =
    std::vector<std::pair<std::string, MapperServiceMap>>;

const std::string hexPrefix = "0x";

const int invalidDataOutSizeErr = 0x116;

#ifdef BMCWEB_ENABLE_TLS_AUTH_OPT_IN

/**
 * Helper to enable the AuthenticationTLSRequired
 */
inline void enableTLSAuth()
{
    BMCWEB_LOG_DEBUG << "Processing AuthenticationTLSRequired Enable.";

    const char* socket_path = "/lib/systemd/system/bmcweb.socket";
    const char* tmp_path = "/lib/systemd/system/bmcweb.tmp";
    {
        std::ifstream in(socket_path);
        std::ofstream out(tmp_path);
        std::string line;
        while (getline(in, line))
        {
            if (line == "ListenStream=80")
            {
                out << "ListenStream=443" << std::endl;
            }
            else
            {
                out << line << std::endl;
            }
        }
    }
    std::filesystem::rename(tmp_path, socket_path);
    persistent_data::getConfig().enableTLSAuth();

    // restart procedure
    // TODO find nicer and faster way?
    try
    {
        dbus::utility::systemdReload();
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_ERROR << "TLSAuthEnable systemd Reload failed with: "
                         << e.what();
    }

    try
    {
        dbus::utility::systemdRestartUnit("bmcweb_2esocket", "replace");
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_ERROR << "TLSAuthEnable bmcweb.socket Restart failed with: "
                         << e.what();
    }

    try
    {
        dbus::utility::systemdRestartUnit("bmcweb_2eservice", "replace");
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_ERROR << "TLSAuthEnable bmcweb.service Restart failed with: "
                         << e.what();
    }
}

#endif // BMCWEB_ENABLE_TLS_AUTH_OPT_IN

/**
 * Function reboots the BMC.
 *
 * @param[in] asyncResp - Shared pointer for completing asynchronous calls
 */
inline void
    doBMCGracefulRestart(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const char* processName = "xyz.openbmc_project.State.BMC";
    const char* objectPath = "/xyz/openbmc_project/state/bmc0";
    const char* interfaceName = "xyz.openbmc_project.State.BMC";
    const std::string& propertyValue =
        "xyz.openbmc_project.State.BMC.Transition.Reboot";
    const char* destProperty = "RequestedBMCTransition";

    // Create the D-Bus variant for D-Bus call.
    dbus::utility::DbusVariantType dbusPropertyValue(propertyValue);

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec) {
            // Use "Set" method to set the property value.
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "[Set] Bad D-Bus request error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }

            messages::success(asyncResp->res);
        },
        processName, objectPath, "org.freedesktop.DBus.Properties", "Set",
        interfaceName, destProperty, dbusPropertyValue);
}

inline void
    doBMCForceRestart(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const char* processName = "xyz.openbmc_project.State.BMC";
    const char* objectPath = "/xyz/openbmc_project/state/bmc0";
    const char* interfaceName = "xyz.openbmc_project.State.BMC";
    const std::string& propertyValue =
        "xyz.openbmc_project.State.BMC.Transition.HardReboot";
    const char* destProperty = "RequestedBMCTransition";

    // Create the D-Bus variant for D-Bus call.
    dbus::utility::DbusVariantType dbusPropertyValue(propertyValue);

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec) {
            // Use "Set" method to set the property value.
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "[Set] Bad D-Bus request error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }

            messages::success(asyncResp->res);
        },
        processName, objectPath, "org.freedesktop.DBus.Properties", "Set",
        interfaceName, destProperty, dbusPropertyValue);
}

/**
 * Function shutdowns the BMC.
 *
 * @param[in] asyncResp - Shared pointer for completing asynchronous calls
 */
inline void
    doBMCGracefulShutdown(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const char* processName = "xyz.openbmc_project.State.BMC";
    const char* objectPath = "/xyz/openbmc_project/state/bmc0";
    const char* interfaceName = "xyz.openbmc_project.State.BMC";
    const std::string& propertyValue =
        "xyz.openbmc_project.State.BMC.Transition.PowerOff";
    const char* destProperty = "RequestedBMCTransition";

    // Create the D-Bus variant for D-Bus call.
    dbus::utility::DbusVariantType dbusPropertyValue(propertyValue);

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec) {
            // Use "Set" method to set the property value.
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "[Set] Bad D-Bus request error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }

            messages::success(asyncResp->res);
        },
        processName, objectPath, "org.freedesktop.DBus.Properties", "Set",
        interfaceName, destProperty, dbusPropertyValue);
}

/**
 * ManagerResetAction class supports the POST method for the Reset (reboot)
 * action.
 */
inline void requestRoutesManagerResetAction(App& app)
{
    /**
     * Function handles POST method request.
     * Analyzes POST body before sending Reset (Reboot) request data to D-Bus.
     * OpenBMC supports ResetType "GracefulRestart" and "ForceRestart".
     */

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID
                      "/Actions/Manager.Reset/")
        .privileges(redfish::privileges::postManager)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                BMCWEB_LOG_DEBUG << "Post Manager Reset.";

                std::string resetType;

                if (!json_util::readJson(req, asyncResp->res, "ResetType",
                                         resetType))
                {
                    return;
                }

                if (resetType == "GracefulRestart")
                {
                    BMCWEB_LOG_DEBUG << "Proceeding with " << resetType;
                    doBMCGracefulRestart(asyncResp);
                    return;
                }
                if (resetType == "ForceRestart")
                {
                    BMCWEB_LOG_DEBUG << "Proceeding with " << resetType;
                    doBMCForceRestart(asyncResp);
                    return;
                }
                if (resetType == "GracefulShutdown")
                {
                    BMCWEB_LOG_DEBUG << "Proceeding with " << resetType;
                    doBMCGracefulShutdown(asyncResp);
                    return;
                }
                BMCWEB_LOG_DEBUG << "Invalid property value for ResetType: "
                                 << resetType;
                messages::actionParameterNotSupported(asyncResp->res, resetType,
                                                      "ResetType");

                return;
            });
}

/**
 * ManagerResetToDefaultsAction class supports POST method for factory reset
 * action.
 */
inline void requestRoutesManagerResetToDefaultsAction(App& app)
{

    /**
     * Function handles ResetToDefaults POST method request.
     *
     * Analyzes POST body message and factory resets BMC by calling
     * BMC code updater factory reset followed by a BMC reboot.
     *
     * BMC code updater factory reset wipes the whole BMC read-write
     * filesystem which includes things like the network settings.
     *
     * OpenBMC only supports ResetToDefaultsType "ResetAll".
     */

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID
                      "/Actions/Manager.ResetToDefaults/")
        .privileges(redfish::privileges::postManager)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                BMCWEB_LOG_DEBUG << "Post ResetToDefaults.";

                std::string resetType;
                std::string ifnameFactoryReset =
                    "xyz.openbmc_project.Common.FactoryReset";

                if (!json_util::readJson(req, asyncResp->res,
                                         "ResetToDefaultsType", resetType))
                {
                    BMCWEB_LOG_DEBUG << "Missing property ResetToDefaultsType.";

                    messages::actionParameterMissing(asyncResp->res,
                                                     "ResetToDefaults",
                                                     "ResetToDefaultsType");
                    return;
                }

                if (resetType != "ResetAll")
                {
                    BMCWEB_LOG_DEBUG
                        << "Invalid property value for ResetToDefaultsType: "
                        << resetType;
                    messages::actionParameterNotSupported(
                        asyncResp->res, resetType, "ResetToDefaultsType");
                    return;
                }

                crow::connections::systemBus->async_method_call(
                    [asyncResp, ifnameFactoryReset](
                        const boost::system::error_code ec,
                        const std::vector<
                            std::pair<std::string, std::vector<std::string>>>&
                            interfaceNames) {
                        if (ec || interfaceNames.size() <= 0)
                        {
                            BMCWEB_LOG_ERROR << "Can't find object";
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        for (const std::pair<std::string,
                                             std::vector<std::string>>& object :
                             interfaceNames)
                        {
                            crow::connections::systemBus->async_method_call(
                                [asyncResp,
                                 object](const boost::system::error_code ec) {
                                    if (ec)
                                    {
                                        BMCWEB_LOG_DEBUG
                                            << "Failed to ResetToDefaults: "
                                            << ec;
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    // Factory Reset doesn't actually happen
                                    // until a reboot Can't erase what the BMC
                                    // is running on
                                    doBMCGracefulRestart(asyncResp);
                                },
                                object.first, "/xyz/openbmc_project/software",
                                ifnameFactoryReset, "Reset");
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject",
                    "/xyz/openbmc_project/software",
                    std::array<const char*, 1>{ifnameFactoryReset.c_str()});
            });
}

/**
 * NvidiaManagerResetToDefaultsAction class supports POST method for complete
 * reset action.
 */
inline void requestRoutesNvidiaManagerResetToDefaultsAction(App& app)
{

    /**
     * Function handles ResetToDefaults POST method request.
     *
     */

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID
                      "/Actions/Oem/NvidiaManager.ResetToDefaults")
        .privileges(redfish::privileges::postManager)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                BMCWEB_LOG_DEBUG << "Post ResetToDefaults.";
                (void)req;
                std::string ifnameCompleteReset =
                    "com.nvidia.Common.CompleteReset";

                crow::connections::systemBus->async_method_call(
                    [asyncResp, ifnameCompleteReset](
                        const boost::system::error_code ec,
                        const std::vector<
                            std::pair<std::string, std::vector<std::string>>>&
                            interfaceNames) {
                        if (ec || interfaceNames.size() <= 0)
                        {
                            BMCWEB_LOG_ERROR << "Can't find object";
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        for (const std::pair<std::string,
                                             std::vector<std::string>>& object :
                             interfaceNames)
                        {
                            crow::connections::systemBus->async_method_call(
                                [asyncResp,
                                 object](const boost::system::error_code ec) {
                                    if (ec)
                                    {
                                        BMCWEB_LOG_DEBUG
                                            << "Failed to ResetToDefaults: "
                                            << ec;
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    // Factory Reset doesn't actually happen
                                    // until a reboot Can't erase what the BMC
                                    // is running on
                                    doBMCGracefulRestart(asyncResp);
                                },
                                object.first, "/xyz/openbmc_project/software",
                                ifnameCompleteReset, "CompleteReset");
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject",
                    "/xyz/openbmc_project/software",
                    std::array<const char*, 1>{ifnameCompleteReset.c_str()});
            });
}

/**
 * ManagerResetActionInfo derived class for delivering Manager
 * ResetType AllowableValues using ResetInfo schema.
 */
inline void requestRoutesManagerResetActionInfo(App& app)
{
    /**
     * Functions triggers appropriate requests on DBus
     */

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID "/ResetActionInfo/")
        .privileges(redfish::privileges::getActionInfo)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                asyncResp->res.jsonValue = {
                    {"@odata.type", "#ActionInfo.v1_1_2.ActionInfo"},
                    {"@odata.id",
                     "/redfish/v1/Managers/" PLATFORMBMCID "/ResetActionInfo"},
                    {"Name", "Reset Action Info"},
                    {"Id", "ResetActionInfo"},
                    {"Parameters",
                     {{{"Name", "ResetType"},
                       {"Required", true},
                       {"DataType", "String"},
                       {"AllowableValues",
                        {"GracefulRestart", "ForceRestart"}}}}}};
            });
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * SyncOOBRawCommandActionInfo derived class for delivering Managers
 * RawOOBCommands AllowableValues using NvidiaSyncOOBRawCommandAction schema.
 */
inline void requestRoutesNvidiaSyncOOBRawCommandActionInfo(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/Managers/<str>/Oem/Nvidia/SyncOOBRawCommandActionInfo/")
        .privileges(redfish::privileges::getActionInfo)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& bmcId)

            {
                // Process non bmc service manager
                if (bmcId != PLATFORMBMCID)
                {
                    messages::resourceNotFound(
                        asyncResp->res, "#Manager.v1_11_0.Manager", bmcId); 
                    return;
                }
                asyncResp->res.jsonValue = {
                    {"@odata.type", "#ActionInfo.v1_1_2.ActionInfo"},
                    {"@odata.id", "/redfish/v1/Managers/" + bmcId +
                                      "/Oem/Nvidia/SyncOOBRawCommandActionInfo"},
                    {"Name", "SyncOOBRawCommand Action Info"},
                    {"Id", "NvidiaSyncOOBRawCommandActionInfo"},
                    {"Parameters",
                     {{{"Name", "TargetType"},
                       {"Required", true},
                       {"DataType", "String"},
                       {"AllowableValues", {"GPU", "NVSwitch", "Baseboard"}}},
                      {{"Name", "TartgetInstanceId"},
                       {"Required", false},
                       {"DataType", "Number"}},
                      {{"Name", "Opcode"},
                       {"Required", true},
                       {"DataType", "String"}},
                      {{"Name", "Arg1"},
                       {"Required", false},
                       {"DataType", "String"}},
                      {{"Name", "Arg2"},
                       {"Required", false},
                       {"DataType", "String"}},
                      {{"Name", "DataIn"},
                       {"Required", false},
                       {"DataType", "StringArray"},
                       {"ArraySizeMaximum", 4},
                       {"ArraySizeMinimum", 4}},
                      {{"Name", "ExtDataIn"},
                       {"Required", false},
                       {"DataType", "StringArray"},
                       {"ArraySizeMaximum", 4},
                       {"ArraySizeMinimum", 4}}}}};
            });
}

/**
 * AsyncOOBRawCommandActionInfo derived class for delivering Managers
 * RawOOBCommands AllowableValues using NvidiaAsyncOOBRawCommandAction schema.
 */
inline void requestRoutesNvidiaAsyncOOBRawCommandActionInfo(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/Managers/<str>/Oem/Nvidia/AsyncOOBRawCommandActionInfo/")
        .privileges(redfish::privileges::getActionInfo)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& bmcId)

            {
                // Process non bmc service manager
                if (bmcId != PLATFORMBMCID)
                {
                    messages::resourceNotFound(
                        asyncResp->res, "#Manager.v1_11_0.Manager", bmcId); 
                    return;
                }

                asyncResp->res.jsonValue = {
                    {"@odata.type", "#ActionInfo.v1_1_2.ActionInfo"},
                    {"@odata.id", "/redfish/v1/Managers/" + bmcId +
                                      "/Oem/Nvidia/AsyncOOBRawCommandActionInfo"},
                    {"Name", "AsyncOOBRawCommand Action Info"},
                    {"Id", "NvidiaAsyncOOBRawCommandActionInfo"},
                    {"Parameters",
                     {{{"Name", "TargetType"},
                       {"Required", true},
                       {"DataType", "String"},
                       {"AllowableValues", {"GPU", "NVSwitch"}}},
                      {{"Name", "TartgetInstanceId"},
                       {"Required", true},
                       {"DataType", "Number"}},
                      {{"Name", "AsyncArg1"},
                       {"Required", true},
                       {"DataType", "String"}},
                      {{"Name", "AsyncDataIn"},
                       {"Required", false},
                       {"DataType", "StringArray"}},
                      {{"Name", "RequestedDataOutBytes"},
                       {"Required", true},
                       {"DataType", "number"}}}}};
            });
}

// convert sync command input request data to raw datain
inline uint32_t formatSyncDataIn(std::vector<std::string>& data)
{
      size_t found;
      std::string dataStr;
      uint32_t dataIn = 0;
      for (auto it =  data.rbegin(); it != data.rend(); ++it)
      {
          found = (*it).find(hexPrefix);
          if (found != std::string::npos)
          {
              (*it).erase(found, hexPrefix.length());
          }
          dataStr.append(*it);
      }

      try
      {
          dataIn = static_cast<uint32_t>(std::stoul(dataStr, nullptr, 16));
      }
      catch (const std::invalid_argument& ia)
      {
          BMCWEB_LOG_ERROR << "stoul conversion exception Invalid argument "
                           << ia.what();
          throw std::runtime_error("Invalid Argument");
      }
      catch (const std::out_of_range& oor)
      {
          BMCWEB_LOG_ERROR << "stoul conversion exception out fo range "
                           << oor.what();
          throw std::runtime_error("Invalid Argument");
      }
      catch (const std::exception& e)
      {
          BMCWEB_LOG_ERROR << "stoul conversion undefined exception"
                           << e.what();
          throw std::runtime_error("Invalid Argument");
      }
      return dataIn;
}

void executeRawSynCommand(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                          const std::string& serviceName,
                          const std::string& objPath, const std::string& Type,
                          uint8_t id, uint8_t opCode, uint8_t arg1,
                          uint8_t arg2, uint32_t dataIn, uint32_t extDataIn)
{
    BMCWEB_LOG_DEBUG << "executeRawAsynCommand fn";
    crow::connections::systemBus->async_method_call(
        [resp, Type,
         id](boost::system::error_code ec, sdbusplus::message::message& msg,
             const std::tuple<int, uint32_t, uint32_t, uint32_t>& res) {
            if (!ec)
            {
                int rc = get<0>(res);
                if (rc != 0)
                {
                    BMCWEB_LOG_ERROR << "asynccommand failed with rc:" << rc;
                    messages::operationFailed(resp->res);
                    return;
                }

                resp->res.jsonValue["StatusRegister"] =
                    intToHexByteArray(get<1>(res));
                resp->res.jsonValue["DataOut"] = intToHexByteArray(get<2>(res));
                resp->res.jsonValue["ExtDataOut"] =
                    intToHexByteArray(get<3>(res));
                return;
            }
            // Read and convert dbus error message to redfish error
            const sd_bus_error* dbusError = msg.get_error();
            if (dbusError == nullptr)
            {
                BMCWEB_LOG_DEBUG << "dbuserror nullptr error";
                messages::internalError(resp->res);
                return;
            }
            if (strcmp(dbusError->name,
                       "xyz.openbmc_project.Common.Error.InvalidArgument") == 0)
            {
                BMCWEB_LOG_ERROR
                    << "xyz.openbmc_project.Common.Error.InvalidArgument error";
                messages::propertyValueIncorrect(resp->res, "TargetInstanceId",
                                                 std::to_string(id));
            }
            else
            {
                BMCWEB_LOG_ERROR
                    << "form executeRawSynCommand failed with error \n";
                BMCWEB_LOG_ERROR << ec;
                messages::internalError(resp->res);
            }
            return;
        },
        serviceName, objPath, "com.nvidia.Protocol.SMBPBI.Raw", "SyncCommand",
        Type, id, opCode, arg1, arg2, dataIn, extDataIn);
}

inline void requestRouteSyncRawOobCommand(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/Managers/" PLATFORMBMCID
        "/Actions/Oem/NvidiaManager.SyncOOBRawCommand")
        .privileges(redfish::privileges::postManager)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {

            uint8_t  targetId;
            std::string targetType;
            std::string opCode;
            std::string arg1;
            std::string arg2;
            uint8_t opCodeRaw;
            uint8_t arg1Raw;
            uint8_t arg2Raw;
            uint32_t dataInRaw = 0;
            uint32_t extDataInRaw = 0;
            std::optional<std::vector<std::string>> dataIn;
            std::optional<std::vector<std::string>> extDataIn;

            if (!json_util::readJson(req, asyncResp->res, "TargetType", targetType,
                            "TargetInstanceId",targetId,"Opcode", opCode,
                            "Arg1", arg1, "Arg2", arg2, "DataIn", dataIn,
                            "ExtDataIn", extDataIn))
            {
                    BMCWEB_LOG_ERROR << "Missing property";
                    return;
            }

            if((dataIn) && ((*dataIn).size()))
            {
                try
                {
                    dataInRaw = formatSyncDataIn(*dataIn);
                }
                catch (const std::runtime_error& e)
                {
                    BMCWEB_LOG_ERROR
                        << "formatSyncDataIn failed with runtime error ";
                    messages::internalError(asyncResp->res);
                    return;
                }
            }

            if((extDataIn) && ((*extDataIn).size()))
            {
                try
                {
                    extDataInRaw = formatSyncDataIn(*extDataIn);
                }
                catch (const std::runtime_error& e)
                {
                    BMCWEB_LOG_ERROR
                        << "formatSyncDataIn failed with runtime error ";
                    messages::internalError(asyncResp->res);
                    return;
                }
            }

            try
            {
                opCodeRaw =
                    static_cast<uint8_t>(std::stoul(opCode, nullptr, 16));
                arg1Raw = static_cast<uint8_t>(std::stoul(arg1, nullptr, 16));
                arg2Raw = static_cast<uint8_t>(std::stoul(arg2, nullptr, 16));
            }
            catch (...)
            {
                BMCWEB_LOG_ERROR
                    << "raw Sync command failed : stoul exception \n";
                messages::internalError(asyncResp->res);
                return;
            }

            crow::connections::systemBus->async_method_call(
                [asyncResp, targetType, targetId, opCodeRaw, arg1Raw, arg2Raw, dataInRaw, extDataInRaw](
                                    const boost::system::error_code ec,
                                    const MapperGetSubTreeResponse& subtree) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "unable to find SMBPBI raw interface";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    for (const auto& [objectPath, serviceMap] : subtree)
                    {
                        if (serviceMap.size() < 1)
                        {
                            BMCWEB_LOG_ERROR << "No service Present";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        const std::string& serviceName = serviceMap[0].first;
                        executeRawSynCommand(asyncResp, serviceName, objectPath,
                                             targetType, targetId, opCodeRaw,
                                             arg1Raw, arg2Raw, dataInRaw,
                                             extDataInRaw);
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", int32_t(0),
                std::array<const char*, 1>{"com.nvidia.Protocol.SMBPBI.Raw"});

            });
}

// function to convert dataInbyte array to dataIn uint32 vector
std::vector<std::uint32_t> formatAsyncDataIn(std::vector<std::string>& asynDataInBytes)
{
    size_t j;
    size_t found;
    std::string temp;
    std::vector<std::uint32_t> asyncDataIn;
    auto dataInSize = asynDataInBytes.size();
    try
    {
        if (dataInSize)
        {
            for (size_t i = 0; i < dataInSize; i++)
            {
                j = i + 1;
                // handle "0x" hex prefix
                found = (asynDataInBytes[i]).find(hexPrefix);
                if (found != std::string::npos)
                {
                    (asynDataInBytes[i]).erase(found, hexPrefix.length());
                }
                temp.insert(0, asynDataInBytes[i]);

                if ((j == dataInSize) || ((j % 4) == 0))
                {
                    // covert string to unit32_t
                    asyncDataIn.push_back(
                        static_cast<uint32_t>(std::stoul(temp, nullptr, 16)));
                    temp.erase();
                }
            }
        }
    }
    catch (const std::invalid_argument& ia)
    {
        BMCWEB_LOG_ERROR
            << "formatAsyncDataIn: stoul conversion exception Invalid argument "
            << ia.what();
        throw std::runtime_error("Invalid Argument");
    }
    catch (const std::out_of_range& oor)
    {
        BMCWEB_LOG_ERROR
            << "formatAsyncDataIn: stoul conversion exception out fo range "
            << oor.what();
        throw std::runtime_error("Argument out of range");
    }
    catch (const std::exception& e)
    {
        BMCWEB_LOG_ERROR
            << "formatAsyncDataIn: stoul conversion undefined exception"
            << e.what();
        throw std::runtime_error("undefined exception");
    }

    return asyncDataIn;
}

void executeRawAsynCommand(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                           const std::string& serviceName,
                           const std::string& objPath, const std::string& Type,
                           uint8_t id, uint8_t argRaw,
                           const std::vector<uint32_t>& asyncDataInRaw,
                           uint32_t requestedDataOutBytes)
{
    BMCWEB_LOG_DEBUG << "executeRawAsynCommand fn";
    crow::connections::systemBus->async_method_call(
        [resp, Type, requestedDataOutBytes,
         id](boost::system::error_code ec, sdbusplus::message::message& msg,
             const std::tuple<int, uint32_t, uint32_t, std::vector<uint32_t>>&
                 res) {
            if (!ec)
            {
                int rc = get<0>(res);

                if (rc == invalidDataOutSizeErr)
                {
                    BMCWEB_LOG_ERROR << "asynccommand failed with rc:" << rc;
                    messages::propertyValueIncorrect(resp->res, "RequestedDataOutBytes",
                            std::to_string(requestedDataOutBytes));
                    return;
                }

                if (rc != 0)
                {
                    BMCWEB_LOG_ERROR << "asynccommand failed with rc:" << rc;
                    messages::operationFailed(resp->res);
                    return;
                }

                resp->res.jsonValue["StatusRegister"] =
                    intToHexByteArray(get<1>(res));
                resp->res.jsonValue["DataOut"] = intToHexByteArray(get<2>(res));
                std::vector<std::uint32_t> asyncDataOut = get<3>(res);
                std::vector<std::string> asyncDataOutBytes;
                for (auto val : asyncDataOut)
                {
                    auto dataOutHex = intToHexByteArray(val);
                    asyncDataOutBytes.insert(asyncDataOutBytes.end(),
                                             dataOutHex.begin(),
                                             dataOutHex.end());
                }
                resp->res.jsonValue["AsyncDataOut"] = asyncDataOutBytes;

                return;
            }
            // Read and convert dbus error message to redfish error
            const sd_bus_error* dbusError = msg.get_error();
            if (dbusError == nullptr)
            {
                BMCWEB_LOG_ERROR << "dbus error nullptr error";
                messages::internalError(resp->res);
                return;
            }
            if (strcmp(dbusError->name,
                       "xyz.openbmc_project.Common.Error.InvalidArgument") == 0)
            {
                BMCWEB_LOG_ERROR
                    << "xyz.openbmc_project.Common.Error.InvalidArgument error";
                messages::propertyValueIncorrect(resp->res, "TargetInstanceId",
                                                 std::to_string(id));
            }
            else
            {
                BMCWEB_LOG_ERROR
                    << "form executeRawAsynCommand failed with error \n";
                BMCWEB_LOG_ERROR << ec;
                messages::internalError(resp->res);
            }
            return;
        },
        serviceName, objPath, "com.nvidia.Protocol.SMBPBI.Raw", "AsyncCommand",
        Type, id, argRaw, asyncDataInRaw, requestedDataOutBytes);
}

inline void requestRouteAsyncRawOobCommand(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/Managers/" PLATFORMBMCID
        "/Actions/Oem/NvidiaManager.AsyncOOBRawCommand")
        .privileges(redfish::privileges::postManager)
        .methods(boost::beast::http::verb::post)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {

            uint8_t  targetId;
            uint8_t  argRaw;
            uint32_t requestedDataOutBytes;
            std::string targetType;
            std::string arg;
            std::optional<std::vector<std::string>> asynDataIn;
            std::vector<uint32_t> asyncDataInRaw;

            if (!json_util::readJson(req, asyncResp->res, "TargetType",
                                     targetType, "TargetInstanceId", targetId,
                                     "AsyncArg1", arg, "RequestedDataOutBytes",
                                     requestedDataOutBytes, "AsyncDataIn",
                                     asynDataIn))
            {
                    BMCWEB_LOG_ERROR << "Missing property";
                    return;
            }

            if ((asynDataIn) && ((*asynDataIn).size()))
            {
                try
                {
                    asyncDataInRaw = formatAsyncDataIn(*asynDataIn);
                }
                catch (const std::runtime_error& e)
                {
                    BMCWEB_LOG_ERROR
                        << "formatAsyncDataIn failed with runtime error ";
                    messages::internalError(asyncResp->res);
                    return;
                }
            }
            try
            {
                argRaw = static_cast<uint8_t>(std::stoul(arg, nullptr, 16));
            }
            catch (...)
            {
                BMCWEB_LOG_ERROR
                    << "raw Async command failed : stoul exception \n";
                messages::internalError(asyncResp->res);
                return;
            }

            crow::connections::systemBus->async_method_call(
                [asyncResp, targetType, targetId, argRaw, asyncDataInRaw,
                 requestedDataOutBytes](
                    const boost::system::error_code ec,
                    const MapperGetSubTreeResponse& subtree) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "unable to find SMBPBI raw interface";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    for (const auto& [objectPath, serviceMap] : subtree)
                    {
                        if (serviceMap.size() < 1)
                        {
                            BMCWEB_LOG_ERROR << "No service Present";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        const std::string& serviceName = serviceMap[0].first;
                        executeRawAsynCommand(asyncResp, serviceName,
                                              objectPath, targetType, targetId,
                                              argRaw, asyncDataInRaw,
                                              requestedDataOutBytes);
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", int32_t(0),
                std::array<const char*, 1>{"com.nvidia.Protocol.SMBPBI.Raw"});

            });
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

static constexpr const char* objectManagerIface =
    "org.freedesktop.DBus.ObjectManager";
static constexpr const char* pidConfigurationIface =
    "xyz.openbmc_project.Configuration.Pid";
static constexpr const char* pidZoneConfigurationIface =
    "xyz.openbmc_project.Configuration.Pid.Zone";
static constexpr const char* stepwiseConfigurationIface =
    "xyz.openbmc_project.Configuration.Stepwise";
static constexpr const char* thermalModeIface =
    "xyz.openbmc_project.Control.ThermalMode";

inline void
    asyncPopulatePid(const std::string& connection, const std::string& path,
                     const std::string& currentProfile,
                     const std::vector<std::string>& supportedProfiles,
                     const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{

    crow::connections::systemBus->async_method_call(
        [asyncResp, currentProfile, supportedProfiles](
            const boost::system::error_code ec,
            const dbus::utility::ManagedObjectType& managedObj) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << ec;
                asyncResp->res.jsonValue.clear();
                messages::internalError(asyncResp->res);
                return;
            }
            nlohmann::json& configRoot =
                asyncResp->res.jsonValue["Oem"]["OpenBmc"]["Fan"];
            nlohmann::json& fans = configRoot["FanControllers"];
            fans["@odata.type"] = "#OemManager.FanControllers";
            fans["@odata.id"] = "/redfish/v1/Managers/" PLATFORMBMCID
                                "/Oem/OpenBmc/Fan/FanControllers";

            nlohmann::json& pids = configRoot["PidControllers"];
            pids["@odata.type"] = "#OemManager.PidControllers";
            pids["@odata.id"] = "/redfish/v1/Managers/" PLATFORMBMCID
                                "/Oem/OpenBmc/Fan/PidControllers";

            nlohmann::json& stepwise = configRoot["StepwiseControllers"];
            stepwise["@odata.type"] = "#OemManager.StepwiseControllers";
            stepwise["@odata.id"] = "/redfish/v1/Managers/" PLATFORMBMCID
                                    "/Oem/OpenBmc/Fan/StepwiseControllers";

            nlohmann::json& zones = configRoot["FanZones"];
            zones["@odata.id"] = "/redfish/v1/Managers/" PLATFORMBMCID
                                 "/Oem/OpenBmc/Fan/FanZones";
            zones["@odata.type"] = "#OemManager.FanZones";
            configRoot["@odata.id"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/OpenBmc/Fan";
            configRoot["@odata.type"] = "#OemManager.Fan";
            configRoot["Profile@Redfish.AllowableValues"] = supportedProfiles;

            if (!currentProfile.empty())
            {
                configRoot["Profile"] = currentProfile;
            }
            BMCWEB_LOG_ERROR << "profile = " << currentProfile << " !";

            for (const auto& pathPair : managedObj)
            {
                for (const auto& intfPair : pathPair.second)
                {
                    if (intfPair.first != pidConfigurationIface &&
                        intfPair.first != pidZoneConfigurationIface &&
                        intfPair.first != stepwiseConfigurationIface)
                    {
                        continue;
                    }

                    std::string name;

                    for (const std::pair<std::string,
                                         dbus::utility::DbusVariantType>&
                             propPair : intfPair.second)
                    {
                        if (propPair.first == "Name")
                        {
                            const std::string* namePtr =
                                std::get_if<std::string>(&propPair.second);
                            if (namePtr == nullptr)
                            {
                                BMCWEB_LOG_ERROR << "Pid Name Field illegal";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            std::string name = *namePtr;
                            dbus::utility::escapePathForDbus(name);
                        }
                        else if (propPair.first == "Profiles")
                        {
                            const std::vector<std::string>* profiles =
                                std::get_if<std::vector<std::string>>(
                                    &propPair.second);
                            if (profiles == nullptr)
                            {
                                BMCWEB_LOG_ERROR
                                    << "Pid Profiles Field illegal";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            if (std::find(profiles->begin(), profiles->end(),
                                          currentProfile) == profiles->end())
                            {
                                BMCWEB_LOG_INFO
                                    << name
                                    << " not supported in current profile";
                                continue;
                            }
                        }
                    }
                    nlohmann::json* config = nullptr;
                    const std::string* classPtr = nullptr;

                    for (const std::pair<std::string,
                                         dbus::utility::DbusVariantType>&
                             propPair : intfPair.second)
                    {
                        if (intfPair.first == "Class")
                        {
                            classPtr =
                                std::get_if<std::string>(&propPair.second);
                        }
                    }

                    if (intfPair.first == pidZoneConfigurationIface)
                    {
                        std::string chassis;
                        if (!dbus::utility::getNthStringFromPath(
                                pathPair.first.str, 5, chassis))
                        {
                            chassis = "#IllegalValue";
                        }
                        nlohmann::json& zone = zones[name];
                        zone["Chassis"] = {
                            {"@odata.id", "/redfish/v1/Chassis/" + chassis}};
                        zone["@odata.id"] =
                            "/redfish/v1/Managers/" PLATFORMBMCID
                            "/Oem/OpenBmc/Fan/FanZones/" +
                            name;
                        zone["@odata.type"] = "#OemManager.FanZone";
                        config = &zone;
                    }

                    else if (intfPair.first == stepwiseConfigurationIface)
                    {
                        if (classPtr == nullptr)
                        {
                            BMCWEB_LOG_ERROR << "Pid Class Field illegal";
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        nlohmann::json& controller = stepwise[name];
                        config = &controller;

                        controller["@odata.id"] =
                            "/redfish/v1/Managers/" PLATFORMBMCID
                            "/Oem/OpenBmc/Fan/StepwiseControllers/" +
                            name;
                        controller["@odata.type"] =
                            "#OemManager.StepwiseController";

                        controller["Direction"] = *classPtr;
                    }

                    // pid and fans are off the same configuration
                    else if (intfPair.first == pidConfigurationIface)
                    {

                        if (classPtr == nullptr)
                        {
                            BMCWEB_LOG_ERROR << "Pid Class Field illegal";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        bool isFan = *classPtr == "fan";
                        nlohmann::json& element =
                            isFan ? fans[name] : pids[name];
                        config = &element;
                        if (isFan)
                        {
                            element["@odata.id"] =
                                "/redfish/v1/Managers/" PLATFORMBMCID
                                "/Oem/OpenBmc/Fan/FanControllers/" +
                                name;
                            element["@odata.type"] =
                                "#OemManager.FanController";
                        }
                        else
                        {
                            element["@odata.id"] =
                                "/redfish/v1/Managers/" PLATFORMBMCID
                                "/Oem/OpenBmc/Fan/PidControllers/" +
                                name;
                            element["@odata.type"] =
                                "#OemManager.PidController";
                        }
                    }
                    else
                    {
                        BMCWEB_LOG_ERROR << "Unexpected configuration";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    // used for making maps out of 2 vectors
                    const std::vector<double>* keys = nullptr;
                    const std::vector<double>* values = nullptr;

                    for (const auto& propertyPair : intfPair.second)
                    {
                        if (propertyPair.first == "Type" ||
                            propertyPair.first == "Class" ||
                            propertyPair.first == "Name")
                        {
                            continue;
                        }

                        // zones
                        if (intfPair.first == pidZoneConfigurationIface)
                        {
                            const double* ptr =
                                std::get_if<double>(&propertyPair.second);
                            if (ptr == nullptr)
                            {
                                BMCWEB_LOG_ERROR << "Field Illegal "
                                                 << propertyPair.first;
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            (*config)[propertyPair.first] = *ptr;
                        }

                        if (intfPair.first == stepwiseConfigurationIface)
                        {
                            if (propertyPair.first == "Reading" ||
                                propertyPair.first == "Output")
                            {
                                const std::vector<double>* ptr =
                                    std::get_if<std::vector<double>>(
                                        &propertyPair.second);

                                if (ptr == nullptr)
                                {
                                    BMCWEB_LOG_ERROR << "Field Illegal "
                                                     << propertyPair.first;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }

                                if (propertyPair.first == "Reading")
                                {
                                    keys = ptr;
                                }
                                else
                                {
                                    values = ptr;
                                }
                                if (keys && values)
                                {
                                    if (keys->size() != values->size())
                                    {
                                        BMCWEB_LOG_ERROR
                                            << "Reading and Output size don't match ";
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    nlohmann::json& steps = (*config)["Steps"];
                                    steps = nlohmann::json::array();
                                    for (size_t ii = 0; ii < keys->size(); ii++)
                                    {
                                        steps.push_back(
                                            {{"Target", (*keys)[ii]},
                                             {"Output", (*values)[ii]}});
                                    }
                                }
                            }
                            if (propertyPair.first == "NegativeHysteresis" ||
                                propertyPair.first == "PositiveHysteresis")
                            {
                                const double* ptr =
                                    std::get_if<double>(&propertyPair.second);
                                if (ptr == nullptr)
                                {
                                    BMCWEB_LOG_ERROR << "Field Illegal "
                                                     << propertyPair.first;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                (*config)[propertyPair.first] = *ptr;
                            }
                        }

                        // pid and fans are off the same configuration
                        if (intfPair.first == pidConfigurationIface ||
                            intfPair.first == stepwiseConfigurationIface)
                        {

                            if (propertyPair.first == "Zones")
                            {
                                const std::vector<std::string>* inputs =
                                    std::get_if<std::vector<std::string>>(
                                        &propertyPair.second);

                                if (inputs == nullptr)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "Zones Pid Field Illegal";
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                auto& data = (*config)[propertyPair.first];
                                data = nlohmann::json::array();
                                for (std::string itemCopy : *inputs)
                                {
                                    dbus::utility::escapePathForDbus(itemCopy);
                                    data.push_back(
                                        {{"@odata.id",
                                          "/redfish/v1/Managers/" PLATFORMBMCID
                                          "/Oem/OpenBmc/Fan/FanZones/" +
                                              itemCopy}});
                                }
                            }
                            // todo(james): may never happen, but this
                            // assumes configuration data referenced in the
                            // PID config is provided by the same daemon, we
                            // could add another loop to cover all cases,
                            // but I'm okay kicking this can down the road a
                            // bit

                            else if (propertyPair.first == "Inputs" ||
                                     propertyPair.first == "Outputs")
                            {
                                auto& data = (*config)[propertyPair.first];
                                const std::vector<std::string>* inputs =
                                    std::get_if<std::vector<std::string>>(
                                        &propertyPair.second);

                                if (inputs == nullptr)
                                {
                                    BMCWEB_LOG_ERROR << "Field Illegal "
                                                     << propertyPair.first;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                data = *inputs;
                            }
                            else if (propertyPair.first == "SetPointOffset")
                            {
                                const std::string* ptr =
                                    std::get_if<std::string>(
                                        &propertyPair.second);

                                if (ptr == nullptr)
                                {
                                    BMCWEB_LOG_ERROR << "Field Illegal "
                                                     << propertyPair.first;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                // translate from dbus to redfish
                                if (*ptr == "WarningHigh")
                                {
                                    (*config)["SetPointOffset"] =
                                        "UpperThresholdNonCritical";
                                }
                                else if (*ptr == "WarningLow")
                                {
                                    (*config)["SetPointOffset"] =
                                        "LowerThresholdNonCritical";
                                }
                                else if (*ptr == "CriticalHigh")
                                {
                                    (*config)["SetPointOffset"] =
                                        "UpperThresholdCritical";
                                }
                                else if (*ptr == "CriticalLow")
                                {
                                    (*config)["SetPointOffset"] =
                                        "LowerThresholdCritical";
                                }
                                else
                                {
                                    BMCWEB_LOG_ERROR << "Value Illegal "
                                                     << *ptr;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                            }
                            // doubles
                            else if (propertyPair.first ==
                                         "FFGainCoefficient" ||
                                     propertyPair.first == "FFOffCoefficient" ||
                                     propertyPair.first == "ICoefficient" ||
                                     propertyPair.first == "ILimitMax" ||
                                     propertyPair.first == "ILimitMin" ||
                                     propertyPair.first ==
                                         "PositiveHysteresis" ||
                                     propertyPair.first ==
                                         "NegativeHysteresis" ||
                                     propertyPair.first == "OutLimitMax" ||
                                     propertyPair.first == "OutLimitMin" ||
                                     propertyPair.first == "PCoefficient" ||
                                     propertyPair.first == "SetPoint" ||
                                     propertyPair.first == "SlewNeg" ||
                                     propertyPair.first == "SlewPos")
                            {
                                const double* ptr =
                                    std::get_if<double>(&propertyPair.second);
                                if (ptr == nullptr)
                                {
                                    BMCWEB_LOG_ERROR << "Field Illegal "
                                                     << propertyPair.first;
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                (*config)[propertyPair.first] = *ptr;
                            }
                        }
                    }
                }
            }
        },
        connection, path, objectManagerIface, "GetManagedObjects");
}

enum class CreatePIDRet
{
    fail,
    del,
    patch
};

inline bool
    getZonesFromJsonReq(const std::shared_ptr<bmcweb::AsyncResp>& response,
                        std::vector<nlohmann::json>& config,
                        std::vector<std::string>& zones)
{
    if (config.empty())
    {
        BMCWEB_LOG_ERROR << "Empty Zones";
        messages::propertyValueFormatError(response->res,
                                           nlohmann::json::array(), "Zones");
        return false;
    }
    for (auto& odata : config)
    {
        std::string path;
        if (!redfish::json_util::readJson(odata, response->res, "@odata.id",
                                          path))
        {
            return false;
        }
        std::string input;

        // 8 below comes from
        // /redfish/v1/Managers/PLATFORMBMCID/Oem/OpenBmc/Fan/FanZones/Left
        //     0    1     2      3    4    5      6     7      8
        if (!dbus::utility::getNthStringFromPath(path, 8, input))
        {
            BMCWEB_LOG_ERROR << "Got invalid path " << path;
            BMCWEB_LOG_ERROR << "Illegal Type Zones";
            messages::propertyValueFormatError(response->res, odata.dump(),
                                               "Zones");
            return false;
        }
        boost::replace_all(input, "_", " ");
        zones.emplace_back(std::move(input));
    }
    return true;
}

inline const dbus::utility::ManagedObjectType::value_type*
    findChassis(const dbus::utility::ManagedObjectType& managedObj,
                const std::string& value, std::string& chassis)
{
    BMCWEB_LOG_DEBUG << "Find Chassis: " << value << "\n";

    std::string escaped = boost::replace_all_copy(value, " ", "_");
    escaped = "/" + escaped;
    auto it = std::find_if(
        managedObj.begin(), managedObj.end(), [&escaped](const auto& obj) {
            if (boost::algorithm::ends_with(obj.first.str, escaped))
            {
                BMCWEB_LOG_DEBUG << "Matched " << obj.first.str << "\n";
                return true;
            }
            return false;
        });

    if (it == managedObj.end())
    {
        return nullptr;
    }
    // 5 comes from <chassis-name> being the 5th element
    // /xyz/openbmc_project/inventory/system/chassis/<chassis-name>
    if (dbus::utility::getNthStringFromPath(it->first.str, 5, chassis))
    {
        return &(*it);
    }

    return nullptr;
}

inline CreatePIDRet createPidInterface(
    const std::shared_ptr<bmcweb::AsyncResp>& response, const std::string& type,
    const nlohmann::json::iterator& it, const std::string& path,
    const dbus::utility::ManagedObjectType& managedObj, bool createNewObject,
    boost::container::flat_map<std::string, dbus::utility::DbusVariantType>&
        output,
    std::string& chassis, const std::string& profile)
{

    // common deleter
    if (it.value() == nullptr)
    {
        std::string iface;
        if (type == "PidControllers" || type == "FanControllers")
        {
            iface = pidConfigurationIface;
        }
        else if (type == "FanZones")
        {
            iface = pidZoneConfigurationIface;
        }
        else if (type == "StepwiseControllers")
        {
            iface = stepwiseConfigurationIface;
        }
        else
        {
            BMCWEB_LOG_ERROR << "Illegal Type " << type;
            messages::propertyUnknown(response->res, type);
            return CreatePIDRet::fail;
        }

        BMCWEB_LOG_DEBUG << "del " << path << " " << iface << "\n";
        // delete interface
        crow::connections::systemBus->async_method_call(
            [response, path](const boost::system::error_code ec) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << "Error patching " << path << ": " << ec;
                    messages::internalError(response->res);
                    return;
                }
                messages::success(response->res);
            },
            "xyz.openbmc_project.EntityManager", path, iface, "Delete");
        return CreatePIDRet::del;
    }

    const dbus::utility::ManagedObjectType::value_type* managedItem = nullptr;
    if (!createNewObject)
    {
        // if we aren't creating a new object, we should be able to find it on
        // d-bus
        managedItem = findChassis(managedObj, it.key(), chassis);
        if (managedItem == nullptr)
        {
            BMCWEB_LOG_ERROR << "Failed to get chassis from config patch";
            messages::invalidObject(response->res, it.key());
            return CreatePIDRet::fail;
        }
    }

    if (profile.size() &&
        (type == "PidControllers" || type == "FanControllers" ||
         type == "StepwiseControllers"))
    {
        if (managedItem == nullptr)
        {
            output["Profiles"] = std::vector<std::string>{profile};
        }
        else
        {
            std::string interface;
            if (type == "StepwiseControllers")
            {
                interface = stepwiseConfigurationIface;
            }
            else
            {
                interface = pidConfigurationIface;
            }
            bool ifaceFound = false;
            for (const auto& iface : managedItem->second)
            {
                if (iface.first == interface)
                {
                    ifaceFound = true;
                    for (const auto& prop : iface.second)
                    {
                        if (prop.first == "Profiles")
                        {
                            const std::vector<std::string>* curProfiles =
                                std::get_if<std::vector<std::string>>(
                                    &(prop.second));
                            if (curProfiles == nullptr)
                            {
                                BMCWEB_LOG_ERROR
                                    << "Illegal profiles in managed object";
                                messages::internalError(response->res);
                                return CreatePIDRet::fail;
                            }
                            if (std::find(curProfiles->begin(),
                                          curProfiles->end(),
                                          profile) == curProfiles->end())
                            {
                                std::vector<std::string> newProfiles =
                                    *curProfiles;
                                newProfiles.push_back(profile);
                                output["Profiles"] = newProfiles;
                            }
                        }
                    }
                }
            }

            if (!ifaceFound)
            {
                BMCWEB_LOG_ERROR
                    << "Failed to find interface in managed object";
                messages::internalError(response->res);
                return CreatePIDRet::fail;
            }
        }
    }

    if (type == "PidControllers" || type == "FanControllers")
    {
        if (createNewObject)
        {
            output["Class"] = type == "PidControllers" ? std::string("temp")
                                                       : std::string("fan");
            output["Type"] = std::string("Pid");
        }

        std::optional<std::vector<nlohmann::json>> zones;
        std::optional<std::vector<std::string>> inputs;
        std::optional<std::vector<std::string>> outputs;
        std::map<std::string, std::optional<double>> doubles;
        std::optional<std::string> setpointOffset;
        if (!redfish::json_util::readJson(
                it.value(), response->res, "Inputs", inputs, "Outputs", outputs,
                "Zones", zones, "FFGainCoefficient",
                doubles["FFGainCoefficient"], "FFOffCoefficient",
                doubles["FFOffCoefficient"], "ICoefficient",
                doubles["ICoefficient"], "ILimitMax", doubles["ILimitMax"],
                "ILimitMin", doubles["ILimitMin"], "OutLimitMax",
                doubles["OutLimitMax"], "OutLimitMin", doubles["OutLimitMin"],
                "PCoefficient", doubles["PCoefficient"], "SetPoint",
                doubles["SetPoint"], "SetPointOffset", setpointOffset,
                "SlewNeg", doubles["SlewNeg"], "SlewPos", doubles["SlewPos"],
                "PositiveHysteresis", doubles["PositiveHysteresis"],
                "NegativeHysteresis", doubles["NegativeHysteresis"]))
        {
            BMCWEB_LOG_ERROR
                << "Illegal Property "
                << it.value().dump(2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
            return CreatePIDRet::fail;
        }
        if (zones)
        {
            std::vector<std::string> zonesStr;
            if (!getZonesFromJsonReq(response, *zones, zonesStr))
            {
                BMCWEB_LOG_ERROR << "Illegal Zones";
                return CreatePIDRet::fail;
            }
            if (chassis.empty() &&
                !findChassis(managedObj, zonesStr[0], chassis))
            {
                BMCWEB_LOG_ERROR << "Failed to get chassis from config patch";
                messages::invalidObject(response->res, it.key());
                return CreatePIDRet::fail;
            }

            output["Zones"] = std::move(zonesStr);
        }
        if (inputs || outputs)
        {
            std::array<std::optional<std::vector<std::string>>*, 2> containers =
                {&inputs, &outputs};
            size_t index = 0;
            for (const auto& containerPtr : containers)
            {
                std::optional<std::vector<std::string>>& container =
                    *containerPtr;
                if (!container)
                {
                    index++;
                    continue;
                }

                for (std::string& value : *container)
                {
                    boost::replace_all(value, "_", " ");
                }
                std::string key;
                if (index == 0)
                {
                    key = "Inputs";
                }
                else
                {
                    key = "Outputs";
                }
                output[key] = *container;
                index++;
            }
        }

        if (setpointOffset)
        {
            // translate between redfish and dbus names
            if (*setpointOffset == "UpperThresholdNonCritical")
            {
                output["SetPointOffset"] = std::string("WarningLow");
            }
            else if (*setpointOffset == "LowerThresholdNonCritical")
            {
                output["SetPointOffset"] = std::string("WarningHigh");
            }
            else if (*setpointOffset == "LowerThresholdCritical")
            {
                output["SetPointOffset"] = std::string("CriticalLow");
            }
            else if (*setpointOffset == "UpperThresholdCritical")
            {
                output["SetPointOffset"] = std::string("CriticalHigh");
            }
            else
            {
                BMCWEB_LOG_ERROR << "Invalid setpointoffset "
                                 << *setpointOffset;
                messages::invalidObject(response->res, it.key());
                return CreatePIDRet::fail;
            }
        }

        // doubles
        for (const auto& pairs : doubles)
        {
            if (!pairs.second)
            {
                continue;
            }
            BMCWEB_LOG_DEBUG << pairs.first << " = " << *pairs.second;
            output[pairs.first] = *(pairs.second);
        }
    }

    else if (type == "FanZones")
    {
        output["Type"] = std::string("Pid.Zone");

        std::optional<nlohmann::json> chassisContainer;
        std::optional<double> failSafePercent;
        std::optional<double> minThermalOutput;
        if (!redfish::json_util::readJson(it.value(), response->res, "Chassis",
                                          chassisContainer, "FailSafePercent",
                                          failSafePercent, "MinThermalOutput",
                                          minThermalOutput))
        {
            BMCWEB_LOG_ERROR
                << "Illegal Property "
                << it.value().dump(2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
            return CreatePIDRet::fail;
        }

        if (chassisContainer)
        {

            std::string chassisId;
            if (!redfish::json_util::readJson(*chassisContainer, response->res,
                                              "@odata.id", chassisId))
            {
                BMCWEB_LOG_ERROR
                    << "Illegal Property "
                    << chassisContainer->dump(
                           2, ' ', true,
                           nlohmann::json::error_handler_t::replace);
                return CreatePIDRet::fail;
            }

            // /redfish/v1/chassis/chassis_name/
            if (!dbus::utility::getNthStringFromPath(chassisId, 3, chassis))
            {
                BMCWEB_LOG_ERROR << "Got invalid path " << chassisId;
                messages::invalidObject(response->res, chassisId);
                return CreatePIDRet::fail;
            }
        }
        if (minThermalOutput)
        {
            output["MinThermalOutput"] = *minThermalOutput;
        }
        if (failSafePercent)
        {
            output["FailSafePercent"] = *failSafePercent;
        }
    }
    else if (type == "StepwiseControllers")
    {
        output["Type"] = std::string("Stepwise");

        std::optional<std::vector<nlohmann::json>> zones;
        std::optional<std::vector<nlohmann::json>> steps;
        std::optional<std::vector<std::string>> inputs;
        std::optional<double> positiveHysteresis;
        std::optional<double> negativeHysteresis;
        std::optional<std::string> direction; // upper clipping curve vs lower
        if (!redfish::json_util::readJson(
                it.value(), response->res, "Zones", zones, "Steps", steps,
                "Inputs", inputs, "PositiveHysteresis", positiveHysteresis,
                "NegativeHysteresis", negativeHysteresis, "Direction",
                direction))
        {
            BMCWEB_LOG_ERROR
                << "Illegal Property "
                << it.value().dump(2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
            return CreatePIDRet::fail;
        }

        if (zones)
        {
            std::vector<std::string> zonesStrs;
            if (!getZonesFromJsonReq(response, *zones, zonesStrs))
            {
                BMCWEB_LOG_ERROR << "Illegal Zones";
                return CreatePIDRet::fail;
            }
            if (chassis.empty() &&
                !findChassis(managedObj, zonesStrs[0], chassis))
            {
                BMCWEB_LOG_ERROR << "Failed to get chassis from config patch";
                messages::invalidObject(response->res, it.key());
                return CreatePIDRet::fail;
            }
            output["Zones"] = std::move(zonesStrs);
        }
        if (steps)
        {
            std::vector<double> readings;
            std::vector<double> outputs;
            for (auto& step : *steps)
            {
                double target = 0.0;
                double out = 0.0;

                if (!redfish::json_util::readJson(step, response->res, "Target",
                                                  target, "Output", out))
                {
                    BMCWEB_LOG_ERROR
                        << "Illegal Property "
                        << it.value().dump(
                               2, ' ', true,
                               nlohmann::json::error_handler_t::replace);
                    return CreatePIDRet::fail;
                }
                readings.emplace_back(target);
                outputs.emplace_back(out);
            }
            output["Reading"] = std::move(readings);
            output["Output"] = std::move(outputs);
        }
        if (inputs)
        {
            for (std::string& value : *inputs)
            {
                boost::replace_all(value, "_", " ");
            }
            output["Inputs"] = std::move(*inputs);
        }
        if (negativeHysteresis)
        {
            output["NegativeHysteresis"] = *negativeHysteresis;
        }
        if (positiveHysteresis)
        {
            output["PositiveHysteresis"] = *positiveHysteresis;
        }
        if (direction)
        {
            constexpr const std::array<const char*, 2> allowedDirections = {
                "Ceiling", "Floor"};
            if (std::find(allowedDirections.begin(), allowedDirections.end(),
                          *direction) == allowedDirections.end())
            {
                messages::propertyValueTypeError(response->res, "Direction",
                                                 *direction);
                return CreatePIDRet::fail;
            }
            output["Class"] = *direction;
        }
    }
    else
    {
        BMCWEB_LOG_ERROR << "Illegal Type " << type;
        messages::propertyUnknown(response->res, type);
        return CreatePIDRet::fail;
    }
    return CreatePIDRet::patch;
}
struct GetPIDValues : std::enable_shared_from_this<GetPIDValues>
{

    GetPIDValues(const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn) :
        asyncResp(asyncRespIn)

    {}

    void run()
    {
        std::shared_ptr<GetPIDValues> self = shared_from_this();

        // get all configurations
        crow::connections::systemBus->async_method_call(
            [self](const boost::system::error_code ec,
                   const crow::openbmc_mapper::GetSubTreeType& subtreeLocal) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << ec;
                    messages::internalError(self->asyncResp->res);
                    return;
                }
                self->subtree = subtreeLocal;
            },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", 0,
            std::array<const char*, 4>{
                pidConfigurationIface, pidZoneConfigurationIface,
                objectManagerIface, stepwiseConfigurationIface});

        // at the same time get the selected profile
        crow::connections::systemBus->async_method_call(
            [self](const boost::system::error_code ec,
                   const crow::openbmc_mapper::GetSubTreeType& subtreeLocal) {
                if (ec || subtreeLocal.empty())
                {
                    return;
                }
                if (subtreeLocal[0].second.size() != 1)
                {
                    // invalid mapper response, should never happen
                    BMCWEB_LOG_ERROR << "GetPIDValues: Mapper Error";
                    messages::internalError(self->asyncResp->res);
                    return;
                }

                const std::string& path = subtreeLocal[0].first;
                const std::string& owner = subtreeLocal[0].second[0].first;
                crow::connections::systemBus->async_method_call(
                    [path, owner,
                     self](const boost::system::error_code ec2,
                           const boost::container::flat_map<
                               std::string, dbus::utility::DbusVariantType>&
                               resp) {
                        if (ec2)
                        {
                            BMCWEB_LOG_ERROR
                                << "GetPIDValues: Can't get thermalModeIface "
                                << path;
                            messages::internalError(self->asyncResp->res);
                            return;
                        }
                        const std::string* current = nullptr;
                        const std::vector<std::string>* supported = nullptr;
                        for (auto& [key, value] : resp)
                        {
                            if (key == "Current")
                            {
                                current = std::get_if<std::string>(&value);
                                if (current == nullptr)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "GetPIDValues: thermal mode iface invalid "
                                        << path;
                                    messages::internalError(
                                        self->asyncResp->res);
                                    return;
                                }
                            }
                            if (key == "Supported")
                            {
                                supported =
                                    std::get_if<std::vector<std::string>>(
                                        &value);
                                if (supported == nullptr)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "GetPIDValues: thermal mode iface invalid"
                                        << path;
                                    messages::internalError(
                                        self->asyncResp->res);
                                    return;
                                }
                            }
                        }
                        if (current == nullptr || supported == nullptr)
                        {
                            BMCWEB_LOG_ERROR
                                << "GetPIDValues: thermal mode iface invalid "
                                << path;
                            messages::internalError(self->asyncResp->res);
                            return;
                        }
                        self->currentProfile = *current;
                        self->supportedProfiles = *supported;
                    },
                    owner, path, "org.freedesktop.DBus.Properties", "GetAll",
                    thermalModeIface);
            },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", 0,
            std::array<const char*, 1>{thermalModeIface});
    }

    ~GetPIDValues()
    {
        if (asyncResp->res.result() != boost::beast::http::status::ok)
        {
            return;
        }
        // create map of <connection, path to objMgr>>
        boost::container::flat_map<std::string, std::string> objectMgrPaths;
        boost::container::flat_set<std::string> calledConnections;
        for (const auto& pathGroup : subtree)
        {
            for (const auto& connectionGroup : pathGroup.second)
            {
                auto findConnection =
                    calledConnections.find(connectionGroup.first);
                if (findConnection != calledConnections.end())
                {
                    break;
                }
                for (const std::string& interface : connectionGroup.second)
                {
                    if (interface == objectManagerIface)
                    {
                        objectMgrPaths[connectionGroup.first] = pathGroup.first;
                    }
                    // this list is alphabetical, so we
                    // should have found the objMgr by now
                    if (interface == pidConfigurationIface ||
                        interface == pidZoneConfigurationIface ||
                        interface == stepwiseConfigurationIface)
                    {
                        auto findObjMgr =
                            objectMgrPaths.find(connectionGroup.first);
                        if (findObjMgr == objectMgrPaths.end())
                        {
                            BMCWEB_LOG_DEBUG << connectionGroup.first
                                             << "Has no Object Manager";
                            continue;
                        }

                        calledConnections.insert(connectionGroup.first);

                        asyncPopulatePid(findObjMgr->first, findObjMgr->second,
                                         currentProfile, supportedProfiles,
                                         asyncResp);
                        break;
                    }
                }
            }
        }
    }

    GetPIDValues(const GetPIDValues&) = delete;
    GetPIDValues(GetPIDValues&&) = delete;
    GetPIDValues& operator=(const GetPIDValues&) = delete;
    GetPIDValues& operator=(GetPIDValues&&) = delete;

    std::vector<std::string> supportedProfiles;
    std::string currentProfile;
    crow::openbmc_mapper::GetSubTreeType subtree;
    std::shared_ptr<bmcweb::AsyncResp> asyncResp;
};

struct SetPIDValues : std::enable_shared_from_this<SetPIDValues>
{

    SetPIDValues(const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn,
                 nlohmann::json& data) :
        asyncResp(asyncRespIn)
    {

        std::optional<nlohmann::json> pidControllers;
        std::optional<nlohmann::json> fanControllers;
        std::optional<nlohmann::json> fanZones;
        std::optional<nlohmann::json> stepwiseControllers;

        if (!redfish::json_util::readJson(
                data, asyncResp->res, "PidControllers", pidControllers,
                "FanControllers", fanControllers, "FanZones", fanZones,
                "StepwiseControllers", stepwiseControllers, "Profile", profile))
        {
            BMCWEB_LOG_ERROR
                << "Illegal Property "
                << data.dump(2, ' ', true,
                             nlohmann::json::error_handler_t::replace);
            return;
        }
        configuration.emplace_back("PidControllers", std::move(pidControllers));
        configuration.emplace_back("FanControllers", std::move(fanControllers));
        configuration.emplace_back("FanZones", std::move(fanZones));
        configuration.emplace_back("StepwiseControllers",
                                   std::move(stepwiseControllers));
    }

    SetPIDValues(const SetPIDValues&) = delete;
    SetPIDValues(SetPIDValues&&) = delete;
    SetPIDValues& operator=(const SetPIDValues&) = delete;
    SetPIDValues& operator=(SetPIDValues&&) = delete;

    void run()
    {
        if (asyncResp->res.result() != boost::beast::http::status::ok)
        {
            return;
        }

        std::shared_ptr<SetPIDValues> self = shared_from_this();

        // todo(james): might make sense to do a mapper call here if this
        // interface gets more traction
        crow::connections::systemBus->async_method_call(
            [self](const boost::system::error_code ec,
                   const dbus::utility::ManagedObjectType& mObj) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << "Error communicating to Entity Manager";
                    messages::internalError(self->asyncResp->res);
                    return;
                }
                const std::array<const char*, 3> configurations = {
                    pidConfigurationIface, pidZoneConfigurationIface,
                    stepwiseConfigurationIface};

                for (const auto& [path, object] : mObj)
                {
                    for (const auto& [interface, _] : object)
                    {
                        if (std::find(configurations.begin(),
                                      configurations.end(),
                                      interface) != configurations.end())
                        {
                            self->objectCount++;
                            break;
                        }
                    }
                }
                self->managedObj = mObj;
            },
            "xyz.openbmc_project.EntityManager", "/", objectManagerIface,
            "GetManagedObjects");

        // at the same time get the profile information
        crow::connections::systemBus->async_method_call(
            [self](const boost::system::error_code ec,
                   const crow::openbmc_mapper::GetSubTreeType& subtree) {
                if (ec || subtree.empty())
                {
                    return;
                }
                if (subtree[0].second.empty())
                {
                    // invalid mapper response, should never happen
                    BMCWEB_LOG_ERROR << "SetPIDValues: Mapper Error";
                    messages::internalError(self->asyncResp->res);
                    return;
                }

                const std::string& path = subtree[0].first;
                const std::string& owner = subtree[0].second[0].first;
                crow::connections::systemBus->async_method_call(
                    [self, path, owner](
                        const boost::system::error_code ec2,
                        const boost::container::flat_map<
                            std::string, dbus::utility::DbusVariantType>& r) {
                        if (ec2)
                        {
                            BMCWEB_LOG_ERROR
                                << "SetPIDValues: Can't get thermalModeIface "
                                << path;
                            messages::internalError(self->asyncResp->res);
                            return;
                        }
                        const std::string* current = nullptr;
                        const std::vector<std::string>* supported = nullptr;
                        for (auto& [key, value] : r)
                        {
                            if (key == "Current")
                            {
                                current = std::get_if<std::string>(&value);
                                if (current == nullptr)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "SetPIDValues: thermal mode iface invalid "
                                        << path;
                                    messages::internalError(
                                        self->asyncResp->res);
                                    return;
                                }
                            }
                            if (key == "Supported")
                            {
                                supported =
                                    std::get_if<std::vector<std::string>>(
                                        &value);
                                if (supported == nullptr)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "SetPIDValues: thermal mode iface invalid"
                                        << path;
                                    messages::internalError(
                                        self->asyncResp->res);
                                    return;
                                }
                            }
                        }
                        if (current == nullptr || supported == nullptr)
                        {
                            BMCWEB_LOG_ERROR
                                << "SetPIDValues: thermal mode iface invalid "
                                << path;
                            messages::internalError(self->asyncResp->res);
                            return;
                        }
                        self->currentProfile = *current;
                        self->supportedProfiles = *supported;
                        self->profileConnection = owner;
                        self->profilePath = path;
                    },
                    owner, path, "org.freedesktop.DBus.Properties", "GetAll",
                    thermalModeIface);
            },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", 0,
            std::array<const char*, 1>{thermalModeIface});
    }
    void pidSetDone()
    {
        if (asyncResp->res.result() != boost::beast::http::status::ok)
        {
            return;
        }
        std::shared_ptr<bmcweb::AsyncResp> response = asyncResp;
        if (profile)
        {
            if (std::find(supportedProfiles.begin(), supportedProfiles.end(),
                          *profile) == supportedProfiles.end())
            {
                messages::actionParameterUnknown(response->res, "Profile",
                                                 *profile);
                return;
            }
            currentProfile = *profile;
            crow::connections::systemBus->async_method_call(
                [response](const boost::system::error_code ec) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "Error patching profile" << ec;
                        messages::internalError(response->res);
                    }
                },
                profileConnection, profilePath,
                "org.freedesktop.DBus.Properties", "Set", thermalModeIface,
                "Current", dbus::utility::DbusVariantType(*profile));
        }

        for (auto& containerPair : configuration)
        {
            auto& container = containerPair.second;
            if (!container)
            {
                continue;
            }
            BMCWEB_LOG_DEBUG << *container;

            std::string& type = containerPair.first;

            for (nlohmann::json::iterator it = container->begin();
                 it != container->end(); ++it)
            {
                const auto& name = it.key();
                BMCWEB_LOG_DEBUG << "looking for " << name;

                auto pathItr =
                    std::find_if(managedObj.begin(), managedObj.end(),
                                 [&name](const auto& obj) {
                                     return boost::algorithm::ends_with(
                                         obj.first.str, "/" + name);
                                 });
                boost::container::flat_map<std::string,
                                           dbus::utility::DbusVariantType>
                    output;

                output.reserve(16); // The pid interface length

                // determines if we're patching entity-manager or
                // creating a new object
                bool createNewObject = (pathItr == managedObj.end());
                BMCWEB_LOG_DEBUG << "Found = " << !createNewObject;

                std::string iface;
                /*
                if (type == "PidControllers" || type == "FanControllers")
                {
                    iface = pidConfigurationIface;
                    if (!createNewObject &&
                        pathItr->second.find(pidConfigurationIface) ==
                            pathItr->second.end())
                    {
                        createNewObject = true;
                    }
                }
                else if (type == "FanZones")
                {
                    iface = pidZoneConfigurationIface;
                    if (!createNewObject &&
                        pathItr->second.find(pidZoneConfigurationIface) ==
                            pathItr->second.end())
                    {

                        createNewObject = true;
                    }
                }
                else if (type == "StepwiseControllers")
                {
                    iface = stepwiseConfigurationIface;
                    if (!createNewObject &&
                        pathItr->second.find(stepwiseConfigurationIface) ==
                            pathItr->second.end())
                    {
                        createNewObject = true;
                    }
                }*/

                if (createNewObject && it.value() == nullptr)
                {
                    // can't delete a non-existent object
                    messages::invalidObject(response->res, name);
                    continue;
                }

                std::string path;
                if (pathItr != managedObj.end())
                {
                    path = pathItr->first.str;
                }

                BMCWEB_LOG_DEBUG << "Create new = " << createNewObject << "\n";

                // arbitrary limit to avoid attacks
                constexpr const size_t controllerLimit = 500;
                if (createNewObject && objectCount >= controllerLimit)
                {
                    messages::resourceExhaustion(response->res, type);
                    continue;
                }

                output["Name"] = boost::replace_all_copy(name, "_", " ");

                std::string chassis;
                CreatePIDRet ret = createPidInterface(
                    response, type, it, path, managedObj, createNewObject,
                    output, chassis, currentProfile);
                if (ret == CreatePIDRet::fail)
                {
                    return;
                }
                if (ret == CreatePIDRet::del)
                {
                    continue;
                }

                if (!createNewObject)
                {
                    for (const auto& property : output)
                    {
                        crow::connections::systemBus->async_method_call(
                            [response,
                             propertyName{std::string(property.first)}](
                                const boost::system::error_code ec) {
                                if (ec)
                                {
                                    BMCWEB_LOG_ERROR << "Error patching "
                                                     << propertyName << ": "
                                                     << ec;
                                    messages::internalError(response->res);
                                    return;
                                }
                                messages::success(response->res);
                            },
                            "xyz.openbmc_project.EntityManager", path,
                            "org.freedesktop.DBus.Properties", "Set", iface,
                            property.first, property.second);
                    }
                }
                else
                {
                    if (chassis.empty())
                    {
                        BMCWEB_LOG_ERROR << "Failed to get chassis from config";
                        messages::invalidObject(response->res, name);
                        return;
                    }

                    bool foundChassis = false;
                    for (const auto& obj : managedObj)
                    {
                        if (boost::algorithm::ends_with(obj.first.str, chassis))
                        {
                            chassis = obj.first.str;
                            foundChassis = true;
                            break;
                        }
                    }
                    if (!foundChassis)
                    {
                        BMCWEB_LOG_ERROR << "Failed to find chassis on dbus";
                        messages::resourceMissingAtURI(
                            response->res, "/redfish/v1/Chassis/" + chassis);
                        return;
                    }

                    crow::connections::systemBus->async_method_call(
                        [response](const boost::system::error_code ec) {
                            if (ec)
                            {
                                BMCWEB_LOG_ERROR << "Error Adding Pid Object "
                                                 << ec;
                                messages::internalError(response->res);
                                return;
                            }
                            messages::success(response->res);
                        },
                        "xyz.openbmc_project.EntityManager", chassis,
                        "xyz.openbmc_project.AddObject", "AddObject", output);
                }
            }
        }
    }

    ~SetPIDValues()
    {
        try
        {
            pidSetDone();
        }
        catch (...)
        {
            BMCWEB_LOG_CRITICAL << "pidSetDone threw exception";
        }
    }

    std::shared_ptr<bmcweb::AsyncResp> asyncResp;
    std::vector<std::pair<std::string, std::optional<nlohmann::json>>>
        configuration;
    std::optional<std::string> profile;
    dbus::utility::ManagedObjectType managedObj;
    std::vector<std::string> supportedProfiles;
    std::string currentProfile;
    std::string profileConnection;
    std::string profilePath;
    size_t objectCount = 0;
};

inline std::string getStateType(const std::string& stateType)
{
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.Absent")
    {
        return "Absent";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.Deferring")
    {
        return "Deferring";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.Disabled")
    {
        return "Disabled";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.Enabled")
    {
        return "Enabled";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.StandbyOffline")
    {
        return "StandbyOffline";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.Starting")
    {
        return "Starting";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.UnavailableOffline")
    {
        return "UnavailableOffline";
    }
    if (stateType == "xyz.openbmc_project.State.Decorator.OperationalStatus."
                     "StateType.Updating")
    {
        return "Updating";
    }
    // Unknown or others
    return "";
}

/**
 * @brief Retrieves manager service state data over DBus
 *
 * @param[in] aResp Shared pointer for completing asynchronous calls
 * @param[in] connectionName - service name
 * @param[in] path - object path
 * @return none
 */
inline void getManagerState(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& connectionName,
                            const std::string& path)
{
    BMCWEB_LOG_DEBUG << "Get manager service state.";

    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const std::vector<std::pair<
                    std::string, std::variant<std::string>>>& propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Error in getting manager service state";
                messages::internalError(aResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<std::string>>&
                     property : propertiesList)
            {
                if (property.first == "State")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for manager service state";
                        messages::internalError(aResp->res);
                        return;
                    }
                    std::string state = getStateType(*value);
                    aResp->res.jsonValue["Status"]["State"] = state;
                    if (state == "Enabled")
                    {
                        aResp->res.jsonValue["Status"]["Health"] = "OK";
                    }
                    else
                    {
                        aResp->res.jsonValue["Status"]["Health"] = "Critical";
                    }
                }
            }
        },
        connectionName, path, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.State.Decorator.OperationalStatus");
}

/**
 * @brief Retrieves BMC asset properties data over DBus
 *
 * @param[in] aResp Shared pointer for completing asynchronous calls
 * @param[in] connectionName - service name
 * @param[in] path - object path
 * @return none
 */
inline void getBMCAssetData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                            const std::string& connectionName,
                            const std::string& path)
{
    BMCWEB_LOG_DEBUG << "Get BMC manager asset data.";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const std::vector<std::pair<
                    std::string, std::variant<std::string>>>& propertiesList) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "Can't get bmc asset!";
                messages::internalError(aResp->res);
                return;
            }
            for (const std::pair<std::string, std::variant<std::string>>&
                     property : propertiesList)
            {
                const std::string& propertyName = property.first;

                if ((propertyName == "PartNumber") ||
                    (propertyName == "SerialNumber") ||
                    (propertyName == "Manufacturer") ||
                    (propertyName == "Model") ||
                    (propertyName == "SparePartNumber") ||
                    (propertyName == "Name"))
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        // illegal property
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue[propertyName] = *value;
                }
            }
        },
        connectionName, path, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator."
        "Asset");
}

/**
 * @brief Retrieves BMC manager location data over DBus
 *
 * @param[in] aResp Shared pointer for completing asynchronous calls
 * @param[in] connectionName - service name
 * @param[in] path - object path
 * @return none
 */
inline void getLocation(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                        const std::string& connectionName,
                        const std::string& path)
{
    BMCWEB_LOG_DEBUG << "Get BMC manager Location data.";

    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.LocationCode", "LocationCode",
        [aResp](const boost::system::error_code ec,
                const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for "
                                    "Location";
                messages::internalError(aResp->res);
                return;
            }

            aResp->res.jsonValue["Location"]["PartLocation"]["ServiceLabel"] =
                property;
        });
}
// avoid name collision systems.hpp
inline void
    managerGetLastResetTime(const std::shared_ptr<bmcweb::AsyncResp>& aResp)
{
    BMCWEB_LOG_DEBUG << "Getting Manager Last Reset Time";

    sdbusplus::asio::getProperty<uint64_t>(
        *crow::connections::systemBus, "xyz.openbmc_project.State.BMC",
        "/xyz/openbmc_project/state/bmc0", "xyz.openbmc_project.State.BMC",
        "LastRebootTime",
        [aResp](const boost::system::error_code ec,
                const uint64_t lastResetTime) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "D-BUS response error " << ec;
                return;
            }

            // LastRebootTime is epoch time, in milliseconds
            // https://github.com/openbmc/phosphor-dbus-interfaces/blob/7f9a128eb9296e926422ddc312c148b625890bb6/xyz/openbmc_project/State/BMC.interface.yaml#L19
            uint64_t lastResetTimeStamp = lastResetTime / 1000;

            // Convert to ISO 8601 standard
            aResp->res.jsonValue["LastResetTime"] =
                crow::utility::getDateTimeUint(lastResetTimeStamp);
        });
}

/**
 * @brief Set the running firmware image
 *
 * @param[i,o] aResp - Async response object
 * @param[i] runningFirmwareTarget - Image to make the running image
 *
 * @return void
 */
inline void
    setActiveFirmwareImage(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& runningFirmwareTarget)
{
    // Get the Id from /redfish/v1/UpdateService/FirmwareInventory/<Id>
    std::string::size_type idPos = runningFirmwareTarget.rfind('/');
    if (idPos == std::string::npos)
    {
        messages::propertyValueNotInList(aResp->res, runningFirmwareTarget,
                                         "@odata.id");
        BMCWEB_LOG_DEBUG << "Can't parse firmware ID!";
        return;
    }
    idPos++;
    if (idPos >= runningFirmwareTarget.size())
    {
        messages::propertyValueNotInList(aResp->res, runningFirmwareTarget,
                                         "@odata.id");
        BMCWEB_LOG_DEBUG << "Invalid firmware ID.";
        return;
    }
    std::string firmwareId = runningFirmwareTarget.substr(idPos);

    // Make sure the image is valid before setting priority
    crow::connections::systemBus->async_method_call(
        [aResp, firmwareId,
         runningFirmwareTarget](const boost::system::error_code ec,
                                dbus::utility::ManagedObjectType& subtree) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "D-Bus response error getting objects.";
                messages::internalError(aResp->res);
                return;
            }

            if (subtree.size() == 0)
            {
                BMCWEB_LOG_DEBUG << "Can't find image!";
                messages::internalError(aResp->res);
                return;
            }

            bool foundImage = false;
            for (auto& object : subtree)
            {
                const std::string& path =
                    static_cast<const std::string&>(object.first);
                std::size_t idPos2 = path.rfind('/');

                if (idPos2 == std::string::npos)
                {
                    continue;
                }

                idPos2++;
                if (idPos2 >= path.size())
                {
                    continue;
                }

                if (path.substr(idPos2) == firmwareId)
                {
                    foundImage = true;
                    break;
                }
            }

            if (!foundImage)
            {
                messages::propertyValueNotInList(
                    aResp->res, runningFirmwareTarget, "@odata.id");
                BMCWEB_LOG_DEBUG << "Invalid firmware ID.";
                return;
            }

            BMCWEB_LOG_DEBUG
                << "Setting firmware version " + firmwareId + " to priority 0.";

            // Only support Immediate
            // An addition could be a Redfish Setting like
            // ActiveSoftwareImageApplyTime and support OnReset
            crow::connections::systemBus->async_method_call(
                [aResp](const boost::system::error_code ec) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "D-Bus response error setting.";
                        messages::internalError(aResp->res);
                        return;
                    }
                    doBMCGracefulRestart(aResp);
                },

                "xyz.openbmc_project.Software.BMC.Updater",
                "/xyz/openbmc_project/software/" + firmwareId,
                "org.freedesktop.DBus.Properties", "Set",
                "xyz.openbmc_project.Software.RedundancyPriority", "Priority",
                dbus::utility::DbusVariantType(static_cast<uint8_t>(0)));
        },
        "xyz.openbmc_project.Software.BMC.Updater",
        "/xyz/openbmc_project/software", "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects");
}

inline void setDateTime(std::shared_ptr<bmcweb::AsyncResp> aResp,
                        std::string datetime)
{
    BMCWEB_LOG_DEBUG << "Set date time: " << datetime;

    std::stringstream stream(datetime);
    // Convert from ISO 8601 to boost local_time
    // (BMC only has time in UTC)
    boost::posix_time::ptime posixTime;
    boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
    // Facet gets deleted with the stringsteam
    auto ifc = std::make_unique<boost::local_time::local_time_input_facet>(
        "%Y-%m-%d %H:%M:%S%F %ZP");
    stream.imbue(std::locale(stream.getloc(), ifc.release()));

    boost::local_time::local_date_time ldt(boost::local_time::not_a_date_time);

    if (stream >> ldt)
    {
        posixTime = ldt.utc_time();
        boost::posix_time::time_duration dur = posixTime - epoch;
        uint64_t durMicroSecs = static_cast<uint64_t>(dur.total_microseconds());
        crow::connections::systemBus->async_method_call(
            [aResp{std::move(aResp)}, datetime{std::move(datetime)}](
                const boost::system::error_code ec) {
                if (ec)
                {
                    BMCWEB_LOG_DEBUG << "Failed to set elapsed time. "
                                        "DBUS response error "
                                     << ec;
                    messages::internalError(aResp->res);
                    return;
                }
                aResp->res.jsonValue["DateTime"] = datetime;
            },
            "xyz.openbmc_project.Time.Manager", "/xyz/openbmc_project/time/bmc",
            "org.freedesktop.DBus.Properties", "Set",
            "xyz.openbmc_project.Time.EpochTime", "Elapsed",
            dbus::utility::DbusVariantType(durMicroSecs));
    }
    else
    {
        messages::propertyValueFormatError(aResp->res, datetime, "DateTime");
        return;
    }
}

inline void getLinkManagerForSwitches(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                   std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // no fabric = no failures
            }
            std::vector<std::string>* objects =
                std::get_if<std::vector<std::string>>(&resp);
            if (objects == nullptr)
            {
                return;
            }
            asyncResp->res.jsonValue["Links"]["ManagerForSwitches"] =
                nlohmann::json::array();
            for (const std::string& fabric : *objects)
            {
                sdbusplus::message::object_path path(fabric);
                std::string fabricId = path.filename();
                crow::connections::systemBus->async_method_call(
                    [asyncResp, fabric, fabricId](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                        if (ec)
                        {
                            messages::internalError(asyncResp->res);
                        }
                        nlohmann::json& tempArray =
                            asyncResp->res
                                .jsonValue["Links"]["ManagerForSwitches"];
                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            const std::string& path = object.first;
                            sdbusplus::message::object_path objPath(path);
                            std::string switchId = objPath.filename();
                            std::string managerUri = "/redfish/v1/Fabrics/";
                            managerUri += fabricId + "/Switches/";
                            managerUri += switchId;

                            tempArray.push_back({{"@odata.id", managerUri}});
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree", fabric, 0,
                    std::array<const char*, 1>{
                        "xyz.openbmc_project.Inventory.Item.Switch"});
            }
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/fabric",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
inline void getFencingPrivilege(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    crow::connections::systemBus
        ->async_method_call(
            [asyncResp](const boost::system::error_code ec,
                        const MapperGetSubTreeResponse& subtree) {
                if (ec)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                for (const auto& [objectPath, serviceMap] : subtree)
                {
                    if (serviceMap.size() < 1)
                    {
                        BMCWEB_LOG_ERROR << "Got 0 service "
                                            "names";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    const std::string& serviceName = serviceMap[0].first;
                    // Get SMBPBI Fencing Privilege
                    crow::connections::
                        systemBus
                            ->async_method_call(
                                [asyncResp](
                                    const boost::system::error_code ec,
                                    const std::vector<std::pair<
                                        std::string,
                                        std::variant<std::string, uint8_t>>>&
                                        propertiesList) {
                                    if (ec)
                                    {
                                        BMCWEB_LOG_ERROR
                                        << "DBUS response error: Unable to get the smbpbi fencing privilege "
                                        << ec;
                                        messages::internalError(asyncResp->res);
                                    }

                                    for (const std::pair<
                                             std::string,
                                             std::variant<std::string,
                                                          uint8_t>>& property :
                                         propertiesList)
                                    {

                                        if (property.first ==
                                            "SMBPBIFencingState")
                                        {
                                            const uint8_t* fencingPrivilege =
                                                std::get_if<uint8_t>(
                                                    &property.second);
                                            if (fencingPrivilege == nullptr)
                                            {
                                                BMCWEB_LOG_DEBUG
                                                    << "Null value returned "
                                                       "for SMBPBI privilege";
                                                messages::internalError(
                                                    asyncResp->res);
                                                return;
                                            }

                                            nlohmann::json& json =
                                                asyncResp->res.jsonValue;
                                            json["Oem"]["Nvidia"]["@odata.type"] =
                                                "#NvidiaManager.v1_0_0.NvidiaManager";
                                            json["Oem"]["Nvidia"]
                                                ["SMBPBIFencingPrivilege"] =
                                                    redfish::dbus_utils::
                                                        toSMPBIPrivilegeString(
                                                            *fencingPrivilege);
                                        }
                                    }
                                },
                                serviceName, objectPath,
                                "org.freedesktop.DBus.Properties", "GetAll",
                                "xyz.openbmc_project.GpuOobRecovery.Server");
                }
            },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", int32_t(0),
            std::array<const char*, 1>{
                "xyz.openbmc_project.GpuOobRecovery.Server"});
}

inline void
    patchFencingPrivilege(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                          const std::string& privilegeType,
                          const std::string& serviceName,
                          const std::string& objPath)
{
    uint8_t privilege =
        redfish::dbus_utils::toSMPBIPrivilegeType(privilegeType);

    // Validate privilege type
    if (privilege == 0)
    {
        messages::invalidObject(resp->res, privilegeType);
        return;
    }

    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, privilege, privilegeType](boost::system::error_code ec,
                                         sdbusplus::message::message& msg) {
            if (!ec)
            {
                BMCWEB_LOG_DEBUG << "Set SMBPBI privilege  property succeeded";
                return;
            }
            BMCWEB_LOG_DEBUG << " set SMBPBI privilege  property failed: " << ec;
            // Read and convert dbus error message to redfish error
            const sd_bus_error* dbusError = msg.get_error();
            if (dbusError == nullptr)
            {
                messages::internalError(resp->res);
                return;
            }
            if (strcmp(dbusError->name,
                       "xyz.openbmc_project.Common.Error.InvalidArgument") == 0)
            {
                // Invalid value
                messages::propertyValueIncorrect(resp->res, "SMBPBIFencingPrivilege",
                                                 privilegeType);
            }

            if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                        "Device.Error.WriteFailure") == 0)
            {
                // Service failed to change the config
                messages::operationFailed(resp->res);
            }
            else
            {
                messages::internalError(resp->res);
            }
        },
        serviceName, objPath, "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.GpuOobRecovery.Server", "SMBPBIFencingState",
        std::variant<uint8_t>(privilege));
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void requestRoutesManager(App& app)
{
    std::string uuid = persistent_data::getConfig().systemUuid;

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/<str>/")
        .privileges(redfish::privileges::getManager)
        .methods(
            boost::beast::http::verb::
                get)([uuid](const crow::Request&,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& bmcId) {
            // Process non bmc service manager
            if (bmcId != PLATFORMBMCID)
            {
                crow::connections::systemBus->async_method_call(
                    [asyncResp, bmcId](
                        const boost::system::error_code ec,
                        const std::vector<std::pair<
                            std::string,
                            std::vector<std::pair<std::string,
                                                  std::vector<std::string>>>>>&
                            subtree) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG
                                << "D-Bus response error on GetSubTree " << ec;
                            return;
                        }
                        // Iterate over all retrieved ObjectPaths.
                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            const std::string& path = object.first;
                            const std::vector<std::pair<
                                std::string, std::vector<std::string>>>&
                                connectionNames = object.second;

                            if (!boost::ends_with(path, bmcId))
                            {
                                continue;
                            }
                            if (connectionNames.size() < 1)
                            {
                                BMCWEB_LOG_ERROR << "Got 0 Connection names";
                                continue;
                            }

                            asyncResp->res.jsonValue["@odata.id"] =
                                "/redfish/v1/Managers/" + bmcId;
                            asyncResp->res.jsonValue["@odata.type"] =
                                "#Manager.v1_11_0.Manager";
                            asyncResp->res.jsonValue["Id"] = bmcId;
                            asyncResp->res.jsonValue["Name"] =
                                "OpenBmc Manager Service";
                            asyncResp->res.jsonValue["Description"] =
                                "Software Service for Baseboard Management "
                                "Functions";
                            asyncResp->res.jsonValue["ManagerType"] = "Service";
                            const std::string& connectionName =
                                connectionNames[0].first;
                            const std::vector<std::string>& interfaces =
                                connectionNames[0].second;

                            for (const auto& interfaceName : interfaces)
                            {
                                if (interfaceName ==
                                    "xyz.openbmc_project.Inventory.Decorator."
                                    "Asset")
                                {
                                    getBMCAssetData(asyncResp, connectionName,
                                                    path);
                                }
                                else if (interfaceName ==
                                         "xyz.openbmc_project.State."
                                         "Decorator.OperationalStatus")
                                {
                                    getManagerState(asyncResp, connectionName,
                                                    path);
                                }
                            }
                            getLinkManagerForSwitches(asyncResp, path);

                            redfish::conditions_utils::
                                populateServiceConditions(asyncResp, bmcId);

#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                            auto health = std::make_shared<HealthRollup>(
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
#endif

                            return;
                        }
                        messages::resourceNotFound(
                            asyncResp->res, "#Manager.v1_11_0.Manager", bmcId);
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                    "/xyz/openbmc_project/inventory", int32_t(0),
                    std::array<const char*, 1>{"xyz.openbmc_project.Inventory."
                                               "Item.ManagementService"});
                return;
            }
            // Process BMC manager
            asyncResp->res.jsonValue["@odata.id"] =
                "/redfish/v1/Managers/" PLATFORMBMCID;
            asyncResp->res.jsonValue["@odata.type"] =
                "#Manager.v1_11_0.Manager";
            asyncResp->res.jsonValue["Id"] = "bmc";
            asyncResp->res.jsonValue["Name"] = "OpenBmc Manager";
            asyncResp->res.jsonValue["Description"] =
                "Baseboard Management Controller";
            asyncResp->res.jsonValue["PowerState"] = "On";
            asyncResp->res.jsonValue["Status"] = {{"State", "Enabled"},
                                                  {"Health", "OK"}};
            asyncResp->res.jsonValue["ManagerType"] = "BMC";
            asyncResp->res.jsonValue["UUID"] = systemd_utils::getUuid();
            asyncResp->res.jsonValue["ServiceEntryPointUUID"] = uuid;
            asyncResp->res.jsonValue["Model"] =
                "OpenBmc"; // TODO(ed), get model

            asyncResp->res.jsonValue["LogServices"] = {
                {"@odata.id",
                 "/redfish/v1/Managers/" PLATFORMBMCID "/LogServices"}};

            asyncResp->res.jsonValue["NetworkProtocol"] = {
                {"@odata.id",
                 "/redfish/v1/Managers/" PLATFORMBMCID "/NetworkProtocol"}};

            asyncResp->res.jsonValue["EthernetInterfaces"] = {
                {"@odata.id",
                 "/redfish/v1/Managers/" PLATFORMBMCID "/EthernetInterfaces"}};

            redfish::conditions_utils::populateServiceConditions(asyncResp,
                                                                 PLATFORMBMCID);

#ifdef BMCWEB_ENABLE_HOST_IFACE
            asyncResp->res.jsonValue["HostInterfaces"] = {
                {"@odata.id",
                 "/redfish/v1/Managers/" PLATFORMBMCID "/HostInterfaces"}};
#endif

#ifdef BMCWEB_ENABLE_RMEDIA
            asyncResp->res.jsonValue["VirtualMedia"] = {
                {"@odata.id",
                 "/redfish/v1/Managers/" PLATFORMBMCID "/VirtualMedia"}};
#endif

            // default oem data
            nlohmann::json& oem = asyncResp->res.jsonValue["Oem"];
            nlohmann::json& oemOpenbmc = oem["OpenBmc"];
            oem["@odata.type"] = "#OemManager.Oem";
            oem["@odata.id"] = "/redfish/v1/Managers/" PLATFORMBMCID "/Oem";
            oemOpenbmc["@odata.type"] = "#OemManager.OpenBmc";
            oemOpenbmc["@odata.id"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/OpenBmc";
            oemOpenbmc["Certificates"] = {{"@odata.id",
                                           "/redfish/v1/Managers/" PLATFORMBMCID
                                           "/Truststore/Certificates"}};

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            // NvidiaManager
            nlohmann::json& oemNvidia = oem["Nvidia"];
            oemNvidia["@odata.type"] = "#OemManager.Nvidia";
            oemNvidia["@odata.id"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia";
            nlohmann::json& oemResetToDefaults =
                asyncResp->res.jsonValue["Actions"]["Oem"]
                                        ["#NvidiaManager.ResetToDefaults"];
            oemResetToDefaults["target"] =
                "/redfish/v1/Managers/" PLATFORMBMCID
                "/Actions/Oem/NvidiaManager.ResetToDefaults";

            // build type
            nlohmann::json& buildType = oemNvidia["FirmwareBuildType"];
            std::ifstream buildDescriptionFile(buildDescriptionFilePath);
            if (buildDescriptionFile.good())
            {
                std::string line;
                std::string const prefix = "BUILD_DESC=";
                std::string descriptionContent;
                while (getline(buildDescriptionFile, line) &&
                       descriptionContent.size() == 0)
                {
                    if (line.rfind(prefix, 0) == 0)
                    {
                        descriptionContent = line.substr(prefix.size());
                        descriptionContent.erase(
                            std::remove(descriptionContent.begin(),
                                        descriptionContent.end(), '"'),
                            descriptionContent.end());
                    }
                }
                if (descriptionContent.rfind("debug-prov", 0) == 0)
                {
                    buildType = "ProvisioningDebug";
                }
                if (descriptionContent.rfind("prod-prov", 0) == 0)
                {
                    buildType = "ProvisioningProduction";
                }
                if (descriptionContent.rfind("debug-platform", 0) == 0)
                {
                    buildType = "PlatformDebug";
                }
                if (descriptionContent.rfind("prod-platform", 0) == 0)
                {
                    buildType = "PlatformProduction";
                }
            }

            // OTP provisioning status
            nlohmann::json& otpProvisioned = oemNvidia["OTPProvisioned"];
            std::ifstream otpStatusFile(otpProvisioningStatusFilePath);
            if (otpStatusFile.good())
            {
                std::string statusLine;
                if (getline(otpStatusFile, statusLine))
                {
                    if (statusLine != "0" && statusLine != "1")
                    {
                        BMCWEB_LOG_ERROR << "Invalid OTP provisioning status - "
                                         << statusLine << "\n";
                    }
                    otpProvisioned = (statusLine == "1");
                }
                else
                {
                    BMCWEB_LOG_ERROR
                        << "Failed to read OTP provisioning status\n";
                    otpProvisioned = false;
                }
            }
            else
            {
                BMCWEB_LOG_ERROR
                    << "Failed to open OTP provisioning status file\n";
                otpProvisioned = false;
            }
            getFencingPrivilege(asyncResp);

#ifdef BMCWEB_ENABLE_TLS_AUTH_OPT_IN
            oemNvidia["AuthenticationTLSRequired"] =
                persistent_data::getConfig().isTLSAuthEnabled();
#endif
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            // Manager.Reset (an action) can be many values, OpenBMC only
            // supports BMC reboot.
            nlohmann::json& managerReset =
                asyncResp->res.jsonValue["Actions"]["#Manager.Reset"];
            managerReset["target"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Actions/Manager.Reset";
            managerReset["@Redfish.ActionInfo"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/ResetActionInfo";

            // ResetToDefaults (Factory Reset) has values like
            // PreserveNetworkAndUsers and PreserveNetwork that aren't supported
            // on OpenBMC
            nlohmann::json& resetToDefaults =
                asyncResp->res.jsonValue["Actions"]["#Manager.ResetToDefaults"];
            resetToDefaults["target"] = "/redfish/v1/Managers/" PLATFORMBMCID
                                        "/Actions/Manager.ResetToDefaults";
            resetToDefaults["ResetType@Redfish.AllowableValues"] = {"ResetAll"};

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            nlohmann::json& oemActions = asyncResp->res.jsonValue["Actions"]["Oem"];
            nlohmann::json& oemActionsNvidia = oemActions["Nvidia"];

            oemActionsNvidia["#NvidiaManager.SyncOOBRawCommand"]["target"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Actions/Oem/NvidiaManager.SyncOOBRawCommand";
            oemActionsNvidia["#NvidiaManager.SyncOOBRawCommand"]["@Redfish.ActionInfo"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia/SyncOOBRawCommandActionInfo";

            oemActionsNvidia["#NvidiaManager.AsyncOOBRawCommand"]["target"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Actions/Oem/NvidiaManager.AsyncOOBRawCommand";
            oemActionsNvidia["#NvidiaManager.AsyncOOBRawCommand"]["@Redfish.ActionInfo"] =
                "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia/AsyncOOBRawCommandActionInfo";

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

            std::pair<std::string, std::string> redfishDateTimeOffset =
                crow::utility::getDateTimeOffsetNow();

            asyncResp->res.jsonValue["DateTime"] = redfishDateTimeOffset.first;
            asyncResp->res.jsonValue["DateTimeLocalOffset"] =
                redfishDateTimeOffset.second;

            // TODO (Gunnar): Remove these one day since moved to ComputerSystem
            // Still used by OCP profiles
            // https://github.com/opencomputeproject/OCP-Profiles/issues/23
            // Fill in SerialConsole info
            asyncResp->res.jsonValue["SerialConsole"]["ServiceEnabled"] = true;
            asyncResp->res.jsonValue["SerialConsole"]["MaxConcurrentSessions"] =
                15;
#ifdef BMCWEB_ENABLE_IPMI
            asyncResp->res.jsonValue["SerialConsole"]["ConnectTypesSupported"] =
                {"IPMI", "SSH"};
#else
            asyncResp->res.jsonValue["SerialConsole"]["ConnectTypesSupported"] =
                {"SSH"};
#endif
#ifdef BMCWEB_ENABLE_KVM
            // Fill in GraphicalConsole info
            asyncResp->res.jsonValue["GraphicalConsole"]["ServiceEnabled"] =
                true;
            asyncResp->res
                .jsonValue["GraphicalConsole"]["MaxConcurrentSessions"] = 4;
            asyncResp->res.jsonValue["GraphicalConsole"]
                                    ["ConnectTypesSupported"] = {"KVMIP"};
#endif // BMCWEB_ENABLE_KVM

            asyncResp->res.jsonValue["Links"]["ManagerForServers@odata.count"] =
                1;
            asyncResp->res.jsonValue["Links"]["ManagerForServers"] = {
                {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID}}};

            auto health = std::make_shared<HealthPopulate>(asyncResp);
            health->isManagersHealth = true;
            health->populate();

            fw_util::populateFirmwareInformation(asyncResp, fw_util::bmcPurpose,
                                                 "FirmwareVersion", true);

            managerGetLastResetTime(asyncResp);

            auto pids = std::make_shared<GetPIDValues>(asyncResp);
            pids->run();

            getMainChassisId(
                asyncResp, [](const std::string& chassisId,
                              const std::shared_ptr<bmcweb::AsyncResp>& aRsp) {
                    aRsp->res
                        .jsonValue["Links"]["ManagerForChassis@odata.count"] =
                        1;
                    aRsp->res.jsonValue["Links"]["ManagerForChassis"] = {
                        {{"@odata.id", "/redfish/v1/Chassis/" + chassisId}}};
                    aRsp->res.jsonValue["Links"]["ManagerInChassis"] = {
                        {"@odata.id", "/redfish/v1/Chassis/" + chassisId}};
                });

            static bool started = false;

            if (!started)
            {
                sdbusplus::asio::getProperty<double>(
                    *crow::connections::systemBus, "org.freedesktop.systemd1",
                    "/org/freedesktop/systemd1",
                    "org.freedesktop.systemd1.Manager", "Progress",
                    [asyncResp](const boost::system::error_code ec,
                                const double& val) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "Error while getting progress";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        if (val < 1.0)
                        {
                            asyncResp->res.jsonValue["Status"]["State"] =
                                "Starting";
                            started = true;
                        }
                    });
            }

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
                        BMCWEB_LOG_DEBUG
                            << "D-Bus response error on GetSubTree " << ec;
                        return;
                    }
                    if (subtree.size() == 0)
                    {
                        BMCWEB_LOG_DEBUG << "Can't find bmc D-Bus object!";
                        return;
                    }
                    // Assume only 1 bmc D-Bus object
                    // Throw an error if there is more than 1
                    if (subtree.size() > 1)
                    {
                        BMCWEB_LOG_DEBUG
                            << "Found more than 1 bmc D-Bus object!";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    if (subtree[0].first.empty() ||
                        subtree[0].second.size() != 1)
                    {
                        BMCWEB_LOG_DEBUG << "Error getting bmc D-Bus object!";
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    const std::string& path = subtree[0].first;
                    const std::string& connectionName =
                        subtree[0].second[0].first;

                    for (const auto& interfaceName :
                         subtree[0].second[0].second)
                    {
                        if (interfaceName ==
                            "xyz.openbmc_project.Inventory.Decorator.Asset")
                        {
                            getBMCAssetData(asyncResp, connectionName, path);
                        }
                        else if (
                            interfaceName ==
                            "xyz.openbmc_project.Inventory.Decorator.LocationCode")
                        {
                            getLocation(asyncResp, connectionName, path);
                        }
                    }
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                "/xyz/openbmc_project/inventory", int32_t(0),
                std::array<const char*, 1>{
                    "xyz.openbmc_project.Inventory.Item.Bmc"});
        });

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID "/")
        .privileges(redfish::privileges::patchManager)
        .methods(
            boost::beast::http::verb::
                patch)([](const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            std::optional<nlohmann::json> oem;
            std::optional<nlohmann::json> links;
            std::optional<std::string> datetime;

            if (!json_util::readJson(req, asyncResp->res, "Oem", oem,
                                     "DateTime", datetime, "Links", links))
            {
                return;
            }

            if (oem)
            {
                std::optional<nlohmann::json> openbmc;
                std::optional<nlohmann::json> nvidia;
                if (!redfish::json_util::readJson(*oem, asyncResp->res,
                                                  "OpenBmc", openbmc, "Nvidia",
                                                  nvidia))
                {
                    BMCWEB_LOG_ERROR
                        << "Illegal Property "
                        << oem->dump(2, ' ', true,
                                     nlohmann::json::error_handler_t::replace);
                    return;
                }
                if (openbmc)
                {
                    std::optional<nlohmann::json> fan;
                    if (!redfish::json_util::readJson(*openbmc, asyncResp->res,
                                                      "Fan", fan))
                    {
                        BMCWEB_LOG_ERROR
                            << "Illegal Property "
                            << openbmc->dump(
                                   2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
                        return;
                    }
                    if (fan)
                    {
                        auto pid =
                            std::make_shared<SetPIDValues>(asyncResp, *fan);
                        pid->run();
                    }
                }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                if (nvidia)
                {
                    std::optional<std::string> privilege;
                    std::optional<bool> tlsAuth;
                    if (!redfish::json_util::readJson(*nvidia, asyncResp->res,
                                                      "AuthenticationTLSRequired", tlsAuth,
                                                       "SMBPBIFencingPrivilege", privilege))
                    {
                        BMCWEB_LOG_ERROR
                            << "Illegal Property "
                            << oem->dump(
                                   2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
                        return;
                    }
                    if(privilege)
                    {
                        crow::connections::systemBus->async_method_call(
                            [asyncResp, privilege](
                                const boost::system::error_code ec,
                                const MapperGetSubTreeResponse& subtree) {
                                if (ec)
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                for (const auto& [objectPath, serviceMap] :
                                     subtree)
                                {
                                    if (serviceMap.size() < 1)
                                    {
                                        BMCWEB_LOG_ERROR << "Got 0 service "
                                                            "names";
                                        messages::internalError(asyncResp->res);
                                        return;
                                    }
                                    const std::string& serviceName =
                                        serviceMap[0].first;
                                    // Patch SMBPBI Fencing Privilege
                                    patchFencingPrivilege(asyncResp, *privilege,
                                                          serviceName,
                                                          objectPath);
                                }
                            },
                            "xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                            "/", int32_t(0),
                            std::array<const char*, 1>{
                                "xyz.openbmc_project.GpuOobRecovery.Server"});
                    }

#ifdef BMCWEB_ENABLE_TLS_AUTH_OPT_IN
                    if (tlsAuth)
                    {
                        if (*tlsAuth ==
                            persistent_data::getConfig().isTLSAuthEnabled())
                        {
                            BMCWEB_LOG_DEBUG
                                << "Ignoring redundant patch of AuthenticationTLSRequired.";
                        }
                        else if (!*tlsAuth)
                        {
                            BMCWEB_LOG_ERROR
                                << "Disabling AuthenticationTLSRequired is not allowed.";
                            messages::propertyValueIncorrect(
                                asyncResp->res, "AuthenticationTLSRequired",
                                "false");
                            return;
                        }
                        else
                        {
                            enableTLSAuth();
                        }
                    }
#endif // BMCWEB_ENABLE_TLS_AUTH_OPT_IN
                }
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            }
            if (links)
            {
                std::optional<nlohmann::json> activeSoftwareImage;
                if (!redfish::json_util::readJson(*links, asyncResp->res,
                                                  "ActiveSoftwareImage",
                                                  activeSoftwareImage))
                {
                    return;
                }
                if (activeSoftwareImage)
                {
                    std::optional<std::string> odataId;
                    if (!json_util::readJson(*activeSoftwareImage,
                                             asyncResp->res, "@odata.id",
                                             odataId))
                    {
                        return;
                    }

                    if (odataId)
                    {
                        setActiveFirmwareImage(asyncResp, *odataId);
                    }
                }
            }
            if (datetime)
            {
                setDateTime(asyncResp, std::move(*datetime));
            }
        });
}

inline void requestRoutesManagerCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Managers/")
        .privileges(redfish::privileges::getManagerCollection)
        .methods(
            boost::beast::http::verb::
                get)([](const crow::Request&,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
            const std::array<const char*, 1> interface = {
                "xyz.openbmc_project.Inventory.Item.ManagementService"};
            const std::string collectionPath = "/redfish/v1/Managers";
            // Collections don't include the static data added by SubRoute
            // because it has a duplicate entry for members
            asyncResp->res.jsonValue["@odata.id"] = collectionPath;
            asyncResp->res.jsonValue["@odata.type"] =
                "#ManagerCollection.ManagerCollection";
            asyncResp->res.jsonValue["Name"] = "Manager Collection";
            nlohmann::json& members = asyncResp->res.jsonValue["Members"];
            members = nlohmann::json::array();
            // Add bmc path
            members.push_back(
                {{"@odata.id", "/redfish/v1/Managers/" PLATFORMBMCID}});
            // Collect additional management services
            crow::connections::systemBus->async_method_call(
                [collectionPath, asyncResp{asyncResp}](
                    const boost::system::error_code ec,
                    const std::vector<std::string>& objects) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG << "DBUS response error";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    nlohmann::json& members =
                        asyncResp->res.jsonValue["Members"];
                    for (const auto& object : objects)
                    {
                        sdbusplus::message::object_path path(object);
                        std::string leaf = path.filename();
                        if (leaf.empty())
                        {
                            continue;
                        }
                        std::string newPath = collectionPath;
                        newPath += '/';
                        newPath += leaf;
                        members.push_back({{"@odata.id", std::move(newPath)}});
                    }
                    asyncResp->res.jsonValue["Members@odata.count"] =
                        members.size();
                },
                "xyz.openbmc_project.ObjectMapper",
                "/xyz/openbmc_project/object_mapper",
                "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                "/xyz/openbmc_project/inventory", 0, interface);
        });
}
} // namespace redfish
