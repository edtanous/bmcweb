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

#include "debug_token/targeted_operation.hpp"

#include <memory>

namespace redfish
{

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
inline void
    getChassisDebugToken(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& chassisId)
{
    constexpr std::array<std::string_view, 1> interfaces = {
        debug_token::debugTokenIntf};
    dbus::utility::getSubTreePaths(
        std::string(debug_token::debugTokenBasePath), 0, interfaces,
        [asyncResp,
         chassisId](const boost::system::error_code& ec,
                    const dbus::utility::MapperGetSubTreePathsResponse& resp) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("GetSubTreePaths error: {}", ec);
            return;
        }
        if (resp.size() == 0)
        {
            return;
        }
        for (const auto& path : resp)
        {
            if (path.find(chassisId) != std::string::npos)
            {
                auto& oemNvidia = asyncResp->res.jsonValue["Oem"]["Nvidia"];
                oemNvidia["@odata.type"] =
                    "#NvidiaChassis.v1_3_0.NvidiaChassis";
                oemNvidia["DebugToken"]["@odata.id"] = boost::urls::format(
                    "/redfish/v1/Chassis/{}/Oem/Nvidia/DebugToken", chassisId);
                return;
            }
        }
    });
}

static std::unique_ptr<debug_token::TargetedOperationHandler> tokenOp;
inline void handleDebugTokenResourceInfo(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    using namespace debug_token;
    using namespace std::string_literals;
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    int timeout;
    if (tokenOp && !tokenOp->finished(timeout))
    {
        messages::serviceTemporarilyUnavailable(asyncResp->res,
                                                std::to_string(timeout));
        return;
    }

    std::string resUri = req.url().buffer();
    asyncResp->res.jsonValue["@odata.type"] =
        "#NvidiaDebugToken.v1_0_0.NvidiaDebugToken";
    asyncResp->res.jsonValue["@odata.id"] = resUri;
    asyncResp->res.jsonValue["Id"] = "DebugToken";
    asyncResp->res.jsonValue["Name"] = chassisId + " Debug Token Resource"s;

    auto& generateAction =
        asyncResp->res.jsonValue["Actions"]["#NvidiaDebugToken.GenerateToken"];
    generateAction["target"] = resUri +
                               "/Actions/NvidiaDebugToken.GenerateToken"s;
    generateAction["@Redfish.ActionInfo"] = resUri +
                                            "/GenerateTokenActionInfo"s;

    auto& installAction =
        asyncResp->res.jsonValue["Actions"]["#NvidiaDebugToken.InstallToken"];
    installAction["target"] = resUri +
                              "/Actions/NvidiaDebugToken.InstallToken"s;
    installAction["@Redfish.ActionInfo"] = resUri + "/InstallTokenActionInfo"s;

    auto& disableAction =
        asyncResp->res.jsonValue["Actions"]["#NvidiaDebugToken.DisableToken"];
    disableAction["target"] = resUri +
                              "/Actions/NvidiaDebugToken.DisableToken"s;

    std::string tokenType = "CRCS";
    tokenOp = std::make_unique<TargetedOperationHandler>(
        chassisId, asyncResp, TargetedOperation::GetTokenStatus, tokenType);
}

inline void handleGenerateTokenActionInfo(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string&)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] = "#ActionInfo.v1_2_0.ActionInfo";
    asyncResp->res.jsonValue["@odata.id"] = req.url();
    asyncResp->res.jsonValue["Id"] = "GenerateTokenActionInfo";
    asyncResp->res.jsonValue["Name"] = "GenerateToken Action Info";

    nlohmann::json::array_t parameters;
    nlohmann::json::object_t parameter;
    parameter["Name"] = "TokenType";
    parameter["Required"] = true;
    parameter["DataType"] = "String";
    nlohmann::json::array_t allowed;
    allowed.emplace_back("CRCS");
    parameter["AllowableValues"] = std::move(allowed);
    parameters.emplace_back(std::move(parameter));

    asyncResp->res.jsonValue["Parameters"] = std::move(parameters);
}

inline void handleInstallTokenActionInfo(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string&)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    asyncResp->res.jsonValue["@odata.type"] = "#ActionInfo.v1_2_0.ActionInfo";
    asyncResp->res.jsonValue["@odata.id"] = req.url();
    asyncResp->res.jsonValue["Id"] = "InstallTokenActionInfo";
    asyncResp->res.jsonValue["Name"] = "InstallToken Action Info";

    nlohmann::json::array_t parameters;
    nlohmann::json::object_t parameter;
    parameter["Name"] = "TokenData";
    parameter["Required"] = true;
    parameter["DataType"] = "String";
    parameters.emplace_back(std::move(parameter));

    asyncResp->res.jsonValue["Parameters"] = std::move(parameters);
}

template <debug_token::TargetedOperation op>
inline void
    handleTargetedTokenOp(App& app, const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisId)
{
    using namespace debug_token;

    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    int timeout;
    if (tokenOp && !tokenOp->finished(timeout))
    {
        messages::serviceTemporarilyUnavailable(asyncResp->res,
                                                std::to_string(timeout));
        return;
    }

    TargetedOperationArgument arg;
    if (op == TargetedOperation::DisableTokens)
    {
        arg = std::monostate();
    }
    if (op == TargetedOperation::GenerateTokenRequest)
    {
        std::string tokenType;
        if (!redfish::json_util::readJsonAction(req, asyncResp->res,
                                                "TokenType", tokenType))
        {
            return;
        }
        if (tokenType != "CRCS")
        {
            messages::actionParameterValueNotInList(
                asyncResp->res, tokenType, "TokenType", "GenerateToken");
            return;
        }
        arg = tokenType;
    }
    if (op == TargetedOperation::InstallToken)
    {
        std::string tokenData;
        if (!redfish::json_util::readJsonAction(req, asyncResp->res,
                                                "TokenData", tokenData))
        {
            return;
        }
        std::string binaryData;
        if (!crow::utility::base64Decode(tokenData, binaryData))
        {
            messages::actionParameterValueFormatError(
                asyncResp->res, tokenData, "TokenData", "InstallToken");
            return;
        }
        arg = std::vector<uint8_t>(binaryData.begin(), binaryData.end());
    }

    tokenOp = std::make_unique<TargetedOperationHandler>(chassisId, asyncResp,
                                                         op, arg);
}

inline void requestRoutesChassisDebugToken(App& app)
{
    using namespace debug_token;
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Oem/Nvidia/DebugToken")
        .privileges(redfish::privileges::getChassis)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleDebugTokenResourceInfo, std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Oem/Nvidia/DebugToken"
                      "/GenerateTokenActionInfo")
        .privileges(redfish::privileges::getChassis)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleGenerateTokenActionInfo, std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Oem/Nvidia/DebugToken"
                      "/InstallTokenActionInfo")
        .privileges(redfish::privileges::getChassis)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handleInstallTokenActionInfo, std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Oem/Nvidia/DebugToken"
                      "/Actions/NvidiaDebugToken.DisableToken")
        .privileges(redfish::privileges::postChassis)
        .methods(boost::beast::http::verb::post)(std::bind_front(
            handleTargetedTokenOp<TargetedOperation::DisableTokens>,
            std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Oem/Nvidia/DebugToken"
                      "/Actions/NvidiaDebugToken.GenerateToken")
        .privileges(redfish::privileges::postChassis)
        .methods(boost::beast::http::verb::post)(std::bind_front(
            handleTargetedTokenOp<TargetedOperation::GenerateTokenRequest>,
            std::ref(app)));
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Oem/Nvidia/DebugToken"
                      "/Actions/NvidiaDebugToken.InstallToken")
        .privileges(redfish::privileges::postChassis)
        .methods(boost::beast::http::verb::post)(std::bind_front(
            handleTargetedTokenOp<TargetedOperation::InstallToken>,
            std::ref(app)));
}
#endif

} // namespace redfish
