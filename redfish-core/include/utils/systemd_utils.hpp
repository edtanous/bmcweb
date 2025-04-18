// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
// SPDX-FileCopyrightText: Copyright 2019 Intel Corporation
#pragma once

#include <systemd/sd-id128.h>

#include <array>
#include <string>

namespace redfish
{

namespace systemd_utils
{

/**
 * @brief Retrieve service root UUID
 *
 * @return Service root UUID
 */

inline std::string getUuid()
{
    std::string ret;
    // This ID needs to match the one in ipmid
    sd_id128_t appId{{0Xe0, 0Xe1, 0X73, 0X76, 0X64, 0X61, 0X47, 0Xda, 0Xa5,
                      0X0c, 0Xd0, 0Xcc, 0X64, 0X12, 0X45, 0X78}};
    sd_id128_t machineId{};

    if (sd_id128_get_machine_app_specific(appId, &machineId) == 0)
    {
        std::array<char, SD_ID128_STRING_MAX> str{};
        ret = sd_id128_to_string(machineId, str.data());
        ret.insert(8, 1, '-');
        ret.insert(13, 1, '-');
        ret.insert(18, 1, '-');
        ret.insert(23, 1, '-');
    }

    return ret;
}

} // namespace systemd_utils
} // namespace redfish
