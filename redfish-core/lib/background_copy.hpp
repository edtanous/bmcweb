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
 *@brief Checks whether the background copy is enabled
 *
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 *
 * @return Object of MctpVdmUtilStatusResponse
 * contains info:
 * whether the MCTP command was performed successfully and
 * whether background copy is enabled or not.
 */
inline MctpVdmUtilStatusResponse isBackgroundCopyEnabled(uint32_t endpointId)
{
    MctpVdmUtilStatusResponse status;
    status.isSuccess = false;
    status.enabled = false;

    MctpVdmUtil mctpVdmUtilWrapper(endpointId);

    mctpVdmUtilWrapper.run(MctpVdmUtilCommand::BACKGROUNDCOPY_STATUS);

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
 *@brief Checks whether status of background copy is pending
 *
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 *
 * @return boolean value
 * returns true when status is pending
 * otherwise returns false
 */
inline bool isBackgroundCopyStatusPending(uint32_t endpointId)
{
    bool isPending = false;

    std::string rxTemplatePending = "(.|\n)*RX:( \\d\\d){9} 02(.|\n)*";

    MctpVdmUtil mctpVdmUtilWrapper(endpointId);

    mctpVdmUtilWrapper.run(MctpVdmUtilCommand::BACKGROUNDCOPY_QUERY_PENDING);

    if (mctpVdmUtilWrapper.getReturnStatus() == 0)
    {
        std::string bgcResp = mctpVdmUtilWrapper.getStdOut();

        if (std::regex_match(bgcResp, std::regex(rxTemplatePending)))
        {
            isPending = true;
        }
    }

    return isPending;
}

/**
 *@brief Checks status of background copy
 *
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 *
 * @return Object of MctpVdmUtilProgressStatusResponse
 * contains info:
 * whether the MCTP command was performed successfully and
 * the status of background copy.
 */
inline MctpVdmUtilProgressStatusResponse
    getBackgroundCopyStatus(uint32_t endpointId)
{
    std::string rxTemplateCompleted = "(.|\n)*RX:( \\d\\d){9} 01(.|\n)*";
    std::string rxTemplateInProgress = "(.|\n)*RX:( \\d\\d){9} 02(.|\n)*";

    MctpVdmUtilProgressStatusResponse status;
    status.isSuccess = false;
    status.status = "Unknown";

    MctpVdmUtil mctpVdmUtilWrapper(endpointId);

    mctpVdmUtilWrapper.run(MctpVdmUtilCommand::BACKGROUNDCOPY_QUERY_PROGRESS);

    if (mctpVdmUtilWrapper.getReturnStatus() == 0)
    {
        status.isSuccess = true;

        std::string bgcResp = mctpVdmUtilWrapper.getStdOut();

        if (std::regex_match(bgcResp, std::regex(rxTemplateCompleted)))
        {
            if (isBackgroundCopyStatusPending(endpointId))
            {
                status.status = "Pending";
            }
            else
            {
                status.status = "Completed";
            }
        }
        else if (std::regex_match(bgcResp, std::regex(rxTemplateInProgress)))
        {
            status.status = "InProgress";
        }
    }

    return status;
}

/**
 *@brief Enable or Disable background copy
 *
 * @param endpointId the EID which is used
 * by mctp-vdm-util tool to call request on MCTP
 * @param enabled Enable or disable the background copy
 *
 * @return exit code form mctp-vdm-tool.
 */
inline int enableBackgroundCopy(uint32_t endpointId, bool enabled)
{
    MctpVdmUtil mctpVdmUtilWrapper(endpointId);

    if (enabled)
    {
        mctpVdmUtilWrapper.run(MctpVdmUtilCommand::BACKGROUNDCOPY_ENABLE);
    }
    else
    {
        mctpVdmUtilWrapper.run(MctpVdmUtilCommand::BACKGROUNDCOPY_DISABLE);
    }

    return mctpVdmUtilWrapper.getReturnStatus();
}
