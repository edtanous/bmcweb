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

#include <utils/nvidia_async_set_utils.hpp>
#include "utils/nvidia_async_set_callbacks.hpp"

namespace redfish
{
namespace nvidia_control_utils
{
inline void
    changeClockLimitControl(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& path, const uint32_t& value,
                            const std::string& patchProp)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, path, value, patchProp](
            const boost::system::error_code& errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
        if (errorno)
        {
            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}",
                             errorno);
            messages::internalError(asyncResp->res);
            return;
        }
        for (const auto& element : objInfo)
        {
            if (patchProp == "SettingMin")

            {
                nvidia_async_operation_utils::doGenericSetAsyncAndGatherResult(
                    asyncResp, std::chrono::seconds(60), element.first, path,
                    "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
                    "RequestedSpeedLimitMin", std::variant<uint32_t>(value),
                    nvidia_async_operation_utils::
                        PatchClockLimitControlCallback{asyncResp});
            }
            else if (patchProp == "SettingMax")

            {
                nvidia_async_operation_utils::doGenericSetAsyncAndGatherResult(
                    asyncResp, std::chrono::seconds(60), element.first, path,
                    "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
                    "RequestedSpeedLimitMax", std::variant<uint32_t>(value),
                    nvidia_async_operation_utils::
                        PatchClockLimitControlCallback{asyncResp});
            }
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig"});
}

inline void
    patchClockLimitControl(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& chassisID,
                           const std::string& controlID,
                           const crow::Request& req,
                           const std::optional<std::string>& validChassisPath,
                           const std::string& processorName)
{
    if (!validChassisPath)
    {
        BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisID);
        return;
    }
    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisID, controlID, validChassisPath, processorName,
         req](const boost::system::error_code ec,
              std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            BMCWEB_LOG_ERROR(
                "ObjectMapper::Get Associated clock control object call failed: {}",
                ec);
            messages::internalError(asyncResp->res);
            return;
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);
        if (data == nullptr)
        {
            BMCWEB_LOG_ERROR("control id resource not found");
            messages::resourceNotFound(asyncResp->res, "ControlID", controlID);
            return;
        }

        auto validendpoint = false;
        std::optional<uint32_t> settingMin;
        std::optional<uint32_t> settingMax;
        if (!json_util::readJsonAction(req, asyncResp->res, "SettingMin",
                                       settingMin, "SettingMax", settingMax))
        {
            return;
        }
        for (const auto& object : *data)
        {
            sdbusplus::message::object_path objPath(object);
            if (objPath.filename() == controlID)
            {
                if (settingMin)
                {
                    changeClockLimitControl(asyncResp, object, *settingMin,
                                            "SettingMin");
                }
                else if (settingMax)
                {
                    changeClockLimitControl(asyncResp, object, *settingMax,
                                            "SettingMax");
                }
                validendpoint = true;
                break;
            }
        }
        if (!validendpoint)
        {
            BMCWEB_LOG_ERROR("control id resource not found");
            messages::resourceNotFound(asyncResp->res, "ControlID", controlID);
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        *validChassisPath + "/clock_controls",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
};
} // namespace nvidia_processor_utils
}
