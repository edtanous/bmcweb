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

class PatchMigModeCallback
{
  public:
    explicit PatchMigModeCallback(std::shared_ptr<bmcweb::AsyncResp> resp) :
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
                "SMBPBI Command failed with error busy, please try after 60 seconds";

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
