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

#include "mctp_vdm_util_wrapper.hpp"

/**
 *@brief Checks whether the in-band is enabled
 *
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 *
 * @return Object of MctpVdmUtilStatusResponse
 * contains info:
 * whether the MCTP command was performed successfully and
 * whether in-band is enabled or not.
 */
inline MctpVdmUtilStatusResponse isInBandEnabled(uint32_t endpointId)
{
    MctpVdmUtilStatusResponse status;
    status.isSuccess = false;
    status.enabled = false;

    MctpVdmUtil mctpVdmUtilWrapper(endpointId);

    mctpVdmUtilWrapper.run(MctpVdmUtilCommand::INBAND_STATUS);

    if (mctpVdmUtilWrapper.getReturnStatus() == 0)
    {
        status.isSuccess = true;

        std::string rxTemplate = "(.|\n)*RX:( \\d\\d){9} 01(.|\n)*";

        if (std::regex_match(mctpVdmUtilWrapper.getStdOut(),
                             std::regex(rxTemplate)))
        {
            status.enabled = true;
        }
    }

    return status;
}

/**
 *@brief Enabled or Disabled in-band
 *
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 * @param enabled Enable or disable the in-band
 *
 * @return exit code form mctp-vdm-tool.
 */
inline int enableInBand(uint32_t endpointId, bool enabled)
{
    MctpVdmUtil mctpVdmUtilWrapper(endpointId);

    if (enabled)
    {
        mctpVdmUtilWrapper.run(MctpVdmUtilCommand::INBAND_ENABLE);
    }
    else
    {
        mctpVdmUtilWrapper.run(MctpVdmUtilCommand::INBAND_DISABLE);
    }

    return mctpVdmUtilWrapper.getReturnStatus();
}
