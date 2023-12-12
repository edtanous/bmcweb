#pragma once

#include <app.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/conditions_utils.hpp>
#include "logging.hpp"
namespace redfish
{

inline void requestRoutesServiceConditions(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/ServiceConditions/")
        .privileges(redfish::privileges::privilegeSetLogin)
        .methods(boost::beast::http::verb::get)(
            []([[maybe_unused]] const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        //TODO: Fix log message
        //BMCWEB_LOG_DEBUG("session {}",req.session);
        asyncResp->res.jsonValue = {
            {"@odata.type", "#ServiceConditions.v1_0_0.ServiceConditions"},
            {"@odata.id", "/redfish/v1/ServiceConditions"},
            {"Id", "ServiceConditions"},
            {"Name", "Redfish Service Conditions"},
            {"HealthRollup", "OK"}};

        asyncResp->res.jsonValue["Oem"] = nlohmann::json::object();

        redfish::conditions_utils::populateServiceConditions(asyncResp,
                                                             std::string(""));
    });
}

} // namespace redfish
