
#include "roles.hpp"

#include "app.hpp"
#include "dbus_utility.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"

#include <boost/url/format.hpp>
#include <sdbusplus/asio/property.hpp>

#include <variant>
namespace redfish
{

std::string getRoleFromPrivileges(std::string_view priv)
{
    if (priv == "priv-admin")
    {
        return "Administrator";
    }
    if (priv == "priv-user")
    {
        return "ReadOnly";
    }
    if (priv == "priv-operator")
    {
        return "Operator";
    }
    return "";
}

bool getAssignedPrivFromRole(std::string_view role, nlohmann::json& privArray)
{
    if (role == "Administrator")
    {
        privArray = {"Login", "ConfigureManager", "ConfigureUsers",
                     "ConfigureSelf", "ConfigureComponents"};
    }
    else if (role == "Operator")
    {
        privArray = {"Login", "ConfigureSelf", "ConfigureComponents"};
    }
    else if (role == "ReadOnly")
    {
        privArray = {"Login", "ConfigureSelf"};
    }
    else
    {
        return false;
    }
    return true;
}

void requestRoutesRoles(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/AccountService/Roles/<str>/")
        .privileges(redfish::privileges::getRole)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& roleId) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        nlohmann::json privArray = nlohmann::json::array();
        if (!getAssignedPrivFromRole(roleId, privArray))
        {
            messages::resourceNotFound(asyncResp->res, "Role", roleId);

            return;
        }

        asyncResp->res.jsonValue["@odata.type"] = "#Role.v1_2_2.Role";
        asyncResp->res.jsonValue["Name"] = "User Role";
        asyncResp->res.jsonValue["Description"] = roleId + " User Role";
        asyncResp->res.jsonValue["OemPrivileges"] = nlohmann::json::array();
        asyncResp->res.jsonValue["IsPredefined"] = true;
        asyncResp->res.jsonValue["Id"] = roleId;
        asyncResp->res.jsonValue["RoleId"] = roleId;
        asyncResp->res.jsonValue["@odata.id"] =
            boost::urls::format("/redfish/v1/AccountService/Roles/{}", roleId);
        asyncResp->res.jsonValue["AssignedPrivileges"] = std::move(privArray);
    });
}

void requestRoutesRoleCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/AccountService/Roles/")
        .privileges(redfish::privileges::getRoleCollection)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }

        asyncResp->res.jsonValue["@odata.id"] =
            "/redfish/v1/AccountService/Roles";
        asyncResp->res.jsonValue["@odata.type"] =
            "#RoleCollection.RoleCollection";
        asyncResp->res.jsonValue["Name"] = "Roles Collection";
        asyncResp->res.jsonValue["Description"] = "BMC User Roles";

        sdbusplus::asio::getProperty<std::vector<std::string>>(
            *crow::connections::systemBus, "xyz.openbmc_project.User.Manager",
            "/xyz/openbmc_project/user", "xyz.openbmc_project.User.Manager",
            "AllPrivileges",
            [asyncResp](const boost::system::error_code& ec,
                        const std::vector<std::string>& privList) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            nlohmann::json& memberArray = asyncResp->res.jsonValue["Members"];
            memberArray = nlohmann::json::array();
            for (const std::string& priv : privList)
            {
                std::string role = getRoleFromPrivileges(priv);
                if (!role.empty())
                {
                    nlohmann::json::object_t member;
                    member["@odata.id"] = boost::urls::format(
                        "/redfish/v1/AccountService/Roles/{}", role);
                    memberArray.emplace_back(std::move(member));
                }
            }
            asyncResp->res.jsonValue["Members@odata.count"] =
                memberArray.size();
        });
    });
}

} // namespace redfish
