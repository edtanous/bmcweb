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

#include "async_resp.hpp"
#include "utils/nvidia_async_set_utils.hpp"

namespace redfish
{
namespace nvidia_async_operation_utils
{

class PatchGenericCallback
{
  public:
    explicit PatchGenericCallback(std::shared_ptr<bmcweb::AsyncResp> resp) :
        resp(std::move(resp))
    {}

    void operator()(const std::string& status) const
    {
        if (status == nvidia_async_operation_utils::asyncStatusValueSuccess)
        {
            return;
        }

        if (status ==
            nvidia_async_operation_utils::asyncStatusValueWriteFailure)
        {
            // Service failed to change the config
            messages::operationFailed(resp->res);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueUnavailable)
        {
            std::string errBusy = "0x50A";
            std::string errBusyResolution =
                "Command failed with error busy, please try after 60 seconds";

            // busy error
            messages::asyncError(resp->res, errBusy, errBusyResolution);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueTimeout)
        {
            std::string errTimeout = "0x600";
            std::string errTimeoutResolution =
                "Settings may/maynot have applied, please check get response before patching";

            // timeout error
            messages::asyncError(resp->res, errTimeout, errTimeoutResolution);
        }
        else
        {
            messages::internalError(resp->res);
        }
    }

  private:
    std::shared_ptr<bmcweb::AsyncResp> resp;
};

using PatchMigModeCallback = PatchGenericCallback;
using PatchEccModeCallback = PatchGenericCallback;

class PatchSpeedConfigCallback
{
  public:
    explicit PatchSpeedConfigCallback(std::shared_ptr<bmcweb::AsyncResp> resp,
                                      uint32_t speedLimit) :
        resp(std::move(resp)), speedLimit(speedLimit)
    {}

    void operator()(const std::string& status) const
    {
        if (status == nvidia_async_operation_utils::asyncStatusValueSuccess)
        {
            return;
        }

        if (status ==
            nvidia_async_operation_utils::asyncStatusValueWriteFailure)
        {
            // Service failed to change the config
            messages::operationFailed(resp->res);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueUnavailable)
        {
            std::string errBusy = "0x50A";
            std::string errBusyResolution =
                "Command failed with error busy, please try after 60 seconds";

            // busy error
            messages::asyncError(resp->res, errBusy, errBusyResolution);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueTimeout)
        {
            std::string errTimeout = "0x600";
            std::string errTimeoutResolution =
                "Settings may/maynot have applied, please check get response before patching";

            // timeout error
            messages::asyncError(resp->res, errTimeout, errTimeoutResolution);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueInvalidArgument)
        {
            // Invalid value
            messages::propertyValueIncorrect(resp->res, "SpeedLimitMHz",
                                             std::to_string(speedLimit));
        }
        else
        {
            messages::internalError(resp->res);
        }
    }

  private:
    std::shared_ptr<bmcweb::AsyncResp> resp;
    uint32_t speedLimit;
};

class PatchPowerCapCallback
{
  public:
    explicit PatchPowerCapCallback(std::shared_ptr<bmcweb::AsyncResp> resp,
                                   int64_t setpoint) :
        resp(std::move(resp)), setpoint(setpoint)
    {}

    void operator()(const std::string& status) const
    {
        if (status == nvidia_async_operation_utils::asyncStatusValueSuccess)
        {
            return;
        }

        if (status ==
            nvidia_async_operation_utils::asyncStatusValueWriteFailure)
        {
            // Service failed to change the config
            messages::operationFailed(resp->res);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueUnavailable)
        {
            std::string errBusy = "0x50A";
            std::string errBusyResolution =
                "Command failed with error busy, please try after 60 seconds";

            // busy error
            messages::asyncError(resp->res, errBusy, errBusyResolution);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueTimeout)
        {
            std::string errTimeout = "0x600";
            std::string errTimeoutResolution =
                "Settings may/maynot have applied, please check get response before patching";

            // timeout error
            messages::asyncError(resp->res, errTimeout, errTimeoutResolution);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueInvalidArgument)
        {
            // Invalid value
            messages::propertyValueIncorrect(resp->res, "setpoint",
                                             std::to_string(setpoint));
        }
        else
        {
            messages::internalError(resp->res);
        }
    }

  private:
    std::shared_ptr<bmcweb::AsyncResp> resp;
    int64_t setpoint;
};

class PatchClockLimitControlCallback
{
  public:
    explicit PatchClockLimitControlCallback(
        std::shared_ptr<bmcweb::AsyncResp> resp) : resp(std::move(resp))
    {}

    void operator()(const std::string& status) const
    {
        if (status == nvidia_async_operation_utils::asyncStatusValueSuccess)
        {
            messages::success(resp->res);
            return;
        }

        if (status ==
            nvidia_async_operation_utils::asyncStatusValueWriteFailure)
        {
            // Service failed to change the config
            messages::operationFailed(resp->res);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueUnavailable)
        {
            std::string errBusy = "0x50A";
            std::string errBusyResolution =
                "NSM Command failed with error busy, please try after 60 seconds";

            // busy error
            messages::asyncError(resp->res, errBusy, errBusyResolution);
        }
        else if (status ==
                 nvidia_async_operation_utils::asyncStatusValueTimeout)
        {
            std::string errTimeout = "0x600";
            std::string errTimeoutResolution =
                "Settings may/maynot have applied, please check get response before patching";

            // timeout error

            messages::asyncError(resp->res, errTimeout, errTimeoutResolution);
        }
        else
        {
            messages::internalError(resp->res);
        }
    }

  private:
    std::shared_ptr<bmcweb::AsyncResp> resp;
};

} // namespace nvidia_async_operation_utils
} // namespace redfish
