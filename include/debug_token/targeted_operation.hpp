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

#include "dbus_utility.hpp"
#include "debug_token/base.hpp"
#include "debug_token/request_utils.hpp"
#include "debug_token/status_utils.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace redfish::debug_token
{

constexpr const int targetedOpTimeoutSeconds = 2;

enum class TargetedOperation
{
    DisableTokens,
    GenerateTokenRequest,
    GetTokenStatus,
    InstallToken
};

using TargetedOperationArgument =
    std::variant<std::monostate, std::string, std::vector<uint8_t>>;

class TargetedOperationHandler
{
  public:
    TargetedOperationHandler(const std::string& chassisId,
                             const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             TargetedOperation op,
                             TargetedOperationArgument arg = std::monostate()) :
        operation(op),
        argument(arg), asyncResp(aResp)
    {
        constexpr std::array<std::string_view, 1> interfaces = {debugTokenIntf};
        dbus::utility::getSubTree(
            std::string(debugTokenBasePath), 0, interfaces,
            [this,
             chassisId](const boost::system::error_code& ec,
                        const dbus::utility::MapperGetSubTreeResponse& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("GetSubTreePaths error: {}", ec);
                errorHandler();
                return;
            }
            if (resp.size() == 0)
            {
                BMCWEB_LOG_ERROR("No objects with DebugToken interface found");
                errorHandler();
                return;
            }
            std::string objectPath;
            std::string service;
            for (const auto& [path, serviceMap] : resp)
            {
                if (path.find(chassisId) != std::string::npos)
                {
                    objectPath = path;
                    service = serviceMap[0].first;
                    break;
                }
            }
            if (objectPath.empty())
            {
                BMCWEB_LOG_ERROR("DebugToken interface not implemented for {}",
                                 chassisId);
                asyncResp->res.result(boost::beast::http::status::not_found);
                cleanup();
                return;
            }
            tokenOperationTimer = std::make_unique<boost::asio::steady_timer>(
                crow::connections::systemBus->get_io_context());
            tokenOperationTimer->expires_after(
                std::chrono::seconds(targetedOpTimeoutSeconds));
            tokenOperationTimer->async_wait(
                [this](const boost::system::error_code& ec) {
                match.reset(nullptr);
                if (!ec)
                {
                    BMCWEB_LOG_ERROR("Debug token operation timeout");
                    errorHandler();
                    return;
                }
                if (ec != boost::asio::error::operation_aborted)
                {
                    BMCWEB_LOG_ERROR("async_wait error {}", ec);
                    errorHandler();
                }
            });
            std::string matchRule("type='signal',"
                                  "interface='org.freedesktop.DBus.Properties',"
                                  "path='" +
                                  objectPath +
                                  "',"
                                  "member='PropertiesChanged'");
            match = std::make_unique<sdbusplus::bus::match_t>(
                *crow::connections::systemBus, matchRule,
                [this, objectPath, service](sdbusplus::message_t& msg) {
                std::string interface;
                std::map<std::string, dbus::utility::DbusVariantType> props;
                msg.read(interface, props);
                std::string opStatus;
                if (interface == "xyz.openbmc_project.Common.Progress")
                {
                    auto it = props.find("Status");
                    if (it != props.end())
                    {
                        auto status = std::get_if<std::string>(&(it->second));
                        if (status)
                        {
                            opStatus = *status;
                        }
                    }
                }
                if (opStatus.empty())
                {
                    return;
                }
                tokenOperationTimer.reset(nullptr);
                if (opStatus !=
                    "xyz.openbmc_project.Common.Progress.OperationStatus.Completed")
                {
                    BMCWEB_LOG_ERROR("End status: {}", opStatus);
                    errorHandler();
                    return;
                }
                switch (operation)
                {
                    case TargetedOperation::GenerateTokenRequest:
                        requestHandler(objectPath, service);
                        return;

                    case TargetedOperation::GetTokenStatus:
                        statusHandler(objectPath, service);
                        return;

                    default:
                        genericHandler();
                        return;
                }
            });
            auto dbusErrorHandler =
                [this](const boost::system::error_code& ec) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("DBus error: {}", ec.message());
                    errorHandler();
                }
            };
            switch (operation)
            {
                case TargetedOperation::DisableTokens:
                {
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "DisableTokens");
                    break;
                }

                case TargetedOperation::GenerateTokenRequest:
                {
                    std::string* tokenOpcode =
                        std::get_if<std::string>(&argument);
                    if (!tokenOpcode)
                    {
                        BMCWEB_LOG_ERROR("Invalid argument");
                        errorHandler();
                        return;
                    }
                    std::string arg = std::string(debugTokenOpcodesEnumPrefix) +
                                      *tokenOpcode;
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "GetRequest", arg);
                    break;
                }

                case TargetedOperation::GetTokenStatus:
                {
                    std::string* tokenType =
                        std::get_if<std::string>(&argument);
                    if (!tokenType)
                    {
                        BMCWEB_LOG_ERROR("Invalid argument");
                        errorHandler();
                        return;
                    }
                    std::string arg = std::string(debugTokenTypesEnumPrefix) +
                                      *tokenType;
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "GetStatus", arg);
                    break;
                }

                case TargetedOperation::InstallToken:
                {
                    std::vector<uint8_t>* token =
                        std::get_if<std::vector<uint8_t>>(&argument);
                    if (!token)
                    {
                        BMCWEB_LOG_ERROR("Invalid argument");
                        errorHandler();
                        return;
                    }
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "InstallToken", *token);
                    break;
                }

                default:
                {
                    BMCWEB_LOG_ERROR("Invalid token operation");
                    errorHandler();
                    break;
                }
            }
        });
    }

    bool finished(int& timeout) const
    {
        BMCWEB_LOG_DEBUG("asyncResp: {}, match: {}, timeout: {}",
                         static_cast<bool>(asyncResp), static_cast<bool>(match),
                         static_cast<bool>(tokenOperationTimer));
        bool isFinished = !asyncResp && !match && !tokenOperationTimer;
        timeout = isFinished ? 0 : targetedOpTimeoutSeconds;
        return isFinished;
    }

  private:
    TargetedOperation operation;
    TargetedOperationArgument argument;
    std::shared_ptr<bmcweb::AsyncResp> asyncResp;

    std::unique_ptr<boost::asio::steady_timer> tokenOperationTimer;
    std::unique_ptr<sdbusplus::bus::match_t> match;

    void requestHandler(const std::string& objectPath,
                        const std::string& service)
    {
        sdbusplus::asio::getProperty<sdbusplus::message::unix_fd>(
            *crow::connections::systemBus, service, objectPath,
            std::string(debugTokenIntf), "RequestFd",
            [this](const boost::system::error_code ec,
                   const sdbusplus::message::unix_fd& unixfd) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBus Get error {}", ec);
                errorHandler();
                return;
            }
            BMCWEB_LOG_DEBUG("Received fd: {}", unixfd.fd);
            std::vector<uint8_t> request;
            if (!readNsmTokenRequestFd(unixfd.fd, request))
            {
                errorHandler();
                return;
            }
            std::string_view binaryData(
                reinterpret_cast<const char*>(request.data()), request.size());
            asyncResp->res.jsonValue["Token"] =
                crow::utility::base64encode(binaryData);
            cleanup();
        });
    }

    void statusHandler(const std::string& objectPath,
                       const std::string& service)
    {
        sdbusplus::asio::getProperty<NsmDbusTokenStatus>(
            *crow::connections::systemBus, service, objectPath,
            std::string(debugTokenIntf), "TokenStatus",
            [this](const boost::system::error_code ec,
                   const NsmDbusTokenStatus& dbusStatus) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBus Get error {}", ec);
                errorHandler();
                return;
            }
            try
            {
                NsmTokenStatus nsmStatus(dbusStatus);
                nsmTokenStatusToJson(nsmStatus, asyncResp->res.jsonValue);
                cleanup();
            }
            catch (const std::exception&)
            {
                errorHandler();
            }
        });
    }

    void genericHandler()
    {
        messages::success(asyncResp->res);
        cleanup();
    }

    void errorHandler()
    {
        messages::internalError(asyncResp->res);
        cleanup();
    }

    void cleanup()
    {
        asyncResp = nullptr;
        boost::asio::post(crow::connections::systemBus->get_io_context(),
                          [this] {
            match.reset(nullptr);
            tokenOperationTimer.reset(nullptr);
        });
    }
};

} // namespace redfish::debug_token
