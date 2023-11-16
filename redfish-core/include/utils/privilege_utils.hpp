#pragma once

#include <async_resp.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/property.hpp>
#include <utils/dbus_utils.hpp>
namespace redfish
{

namespace privilege_utils
{

/**
 * @brief Check if the user has redfish-hostiface group
 * @param username  Username
 * @param callback  A callback function, [](boost::system::error_code ec,
 * bool isRedfishHostInterface).
 */
template <typename Callback>
inline void isRedfishHostInterfaceUser(const std::string& username,
                                       Callback&& callback)
{
    BMCWEB_LOG_DEBUG("isRedfishHostInterfaceUser enter {}", username);

    auto respHandler =
        [callback{std::forward<Callback>(callback)}](
            const boost::system::error_code ec,
            const std::map<std::string, dbus::utility::DbusVariantType>&
                userInfo) {
        BMCWEB_LOG_DEBUG("isRedfishHostInterfaceUser respHandler enter");

        if (ec)
        {
            BMCWEB_LOG_ERROR("isRedfishHostInterfaceUser respHandler DBUS error: {}", ec);
            callback(ec, false);
            return;
        }

        // Get UserGroups
        const std::vector<std::string>* userGroupPtr = nullptr;
        auto userInfoIter = userInfo.find("UserGroups");
        if (userInfoIter != userInfo.end())
        {
            userGroupPtr =
                std::get_if<std::vector<std::string>>(&userInfoIter->second);
        }

        if (userGroupPtr == nullptr)
        {
            BMCWEB_LOG_ERROR("User Group not found");
            callback(boost::system::errc::make_error_code(
                         boost::system::errc::function_not_supported),
                     false);
            return;
        }

        // Check if user in redfish-hostiface group
        auto found = std::find_if(userGroupPtr->begin(), userGroupPtr->end(),
                                  [](const auto& group) {
            return (group == "redfish-hostiface") ? true : false;
        });
        if (found == userGroupPtr->end())
        {
            callback({}, false);
            return;
        }

        callback({}, true);
    };

    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.User.Manager",
        "/xyz/openbmc_project/user", "xyz.openbmc_project.User.Manager",
        "GetUserInfo", username);

    BMCWEB_LOG_DEBUG("isRedfishHostInterfaceUser exit");
}

/**
 * @brief Check if the request from BIOS
 * @param req  crow::Request
 * @param callback  A callback function, [](boost::system::error_code ec,
 * bool isBios).
 */
template <typename Callback>
inline void isBiosPrivilege(const crow::Request& req, Callback&& callback)
{
    // We assume the request from a redfish-hostiface user is from BIOS
    isRedfishHostInterfaceUser(req.session->username,
                               std::forward<Callback>(callback));
}

} // namespace privilege_utils
} // namespace redfish
