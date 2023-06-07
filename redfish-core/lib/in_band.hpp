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
#include "mctp_vdm_util_wrapper.hpp"

/**
 *@brief Updates InbandUpdatePolicyEnabled property
 *
 * @param req - Pointer to object holding request data
 * @param asyncResp - Pointer to object holding response data
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 *
 * @return
 */
inline void
    updateInBandEnabled(const crow::Request& req,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        uint32_t endpointId)
{

    MctpVdmUtil mctpVdmUtilWrapper(endpointId);
    auto responseCallback =
        []([[maybe_unused]] const crow::Request& req,
           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
           [[maybe_unused]] uint32_t endpointId, const std::string& stdOut,
           [[maybe_unused]] const std::string& stdErr,
           const boost::system::error_code& ec, int errorCode) -> void {
        if (ec || errorCode)
        {
            redfish::messages::internalError(asyncResp->res);
            return;
        }
        nlohmann::json& oem = asyncResp->res.jsonValue["Oem"]["Nvidia"];

        std::string rxTemplate = "(.|\n)*RX:( \\d\\d){9} 01(.|\n)*";
        if (std::regex_match(stdOut, std::regex(rxTemplate)))
        {
            oem["InbandUpdatePolicyEnabled"] = true;
        }
        else
        {
            oem["InbandUpdatePolicyEnabled"] = false;
        }
        return;
    };

    mctpVdmUtilWrapper.run(MctpVdmUtilCommand::INBAND_STATUS, req, asyncResp,
                           responseCallback);

    return;
}

/**
 *@brief Enable or Disable in-band update policy
 *
 * @param req - Pointer to object holding request data
 * @param asyncResp - Pointer to object holding response data
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 * @param enabled Enable or disable the in-band
 * @param chassisId - chassis Id
 *
 * @return
 */
inline void enableInBand(const crow::Request& req,
                         const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         uint32_t endpointId, bool enabled,
                         const std::string& chassisId)
{
    MctpVdmUtil mctpVdmUtilWrapper(endpointId);
    auto responseCallback =
        [enabled, chassisId](
            [[maybe_unused]] const crow::Request& req,
            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
            [[maybe_unused]] uint32_t endpointId,
            [[maybe_unused]] const std::string& stdOut,
            [[maybe_unused]] const std::string& stdErr,
            const boost::system::error_code& ec, int errorCode) -> void {
        if (ec || errorCode)
        {
            const std::string errorMessage =
                (enabled == true) ? "MCTP Command Failure: In-Band Enable"
                                  : "MCTP Command Failure: In-Band Disable";

            redfish::messages::resourceErrorsDetectedFormatError(
                asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                errorMessage);
            return;
        }
        if (asyncResp->res.jsonValue.empty())
        {
            redfish::messages::success(asyncResp->res);
        }
    };
    if (enabled)
    {
        mctpVdmUtilWrapper.run(MctpVdmUtilCommand::INBAND_ENABLE, req,
                               asyncResp, responseCallback);
    }
    else
    {
        mctpVdmUtilWrapper.run(MctpVdmUtilCommand::INBAND_DISABLE, req,
                               asyncResp, responseCallback);
    }
}