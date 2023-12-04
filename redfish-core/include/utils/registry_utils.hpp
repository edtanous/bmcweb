/*!
 * @file    registry_utils.cpp
 * @brief   Source code for utility functions of handling message registries.
 */

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/beast/core/span.hpp>
#include <registries.hpp>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <utility>

namespace redfish
{
namespace message_registries
{

inline std::string getPrefix(const std::string& messageID)
{
    size_t pos = messageID.find('.');
    return messageID.substr(0, pos);
}

inline bool isMessageIdValid(const std::string_view messageId)
{
    const redfish::registries::Message* msg = redfish::registries::getMessage(messageId);
    (void)msg;
    return msg != nullptr;
}

inline void
    updateResolution(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& property, std::string resolution)
{
    std::string extendInfo = property + "@Message.ExtendedInfo";
    auto& extendedInfoArr = asyncResp->res.jsonValue[extendInfo];
    if (extendedInfoArr.size() > 0)
    {
        std::string oldResolution = extendedInfoArr[0]["Resolution"];
        resolution = oldResolution + resolution;
        extendedInfoArr[0]["Resolution"] = resolution;
    }
}

inline void
    updateMessageSeverity(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& property,
                          const std::string& messageSeverity)
{
    std::string extendInfo = property + "@Message.ExtendedInfo";
    auto& extendedInfoArr = asyncResp->res.jsonValue[extendInfo];
    if (extendedInfoArr.size() > 0)
    {
        extendedInfoArr[0]["MessageSeverity"] = messageSeverity;
    }
}
} // namespace message_registries
} // namespace redfish
