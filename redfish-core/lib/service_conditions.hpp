#pragma once

#include <app.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/conditions_utils.hpp>

namespace redfish
{

inline void requestRoutesServiceConditions(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/ServiceConditions/")
        .privileges(redfish::privileges::privilegeSetLogin)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                BMCWEB_LOG_DEBUG << "session " << req.session;
                asyncResp->res.jsonValue = {
                    {"@odata.type",
                     "#ServiceConditions.v1_0_0.ServiceConditions"},
                    {"@odata.id", "/redfish/v1/ServiceConditions"},
                    {"Id", "ServiceConditions"},
                    {"Name", "Redfish Service Conditions"}};
                asyncResp->res.jsonValue["Oem"] =  nlohmann::json::object();
#ifndef BMCWEB_DISABLE_HEALTH_ROLLUP
                asyncResp->res.jsonValue["HealthRollup"] = "OK";
#endif // BMCWEB_DISABLE_HEALTH_ROLLUP
#ifndef BMCWEB_DISABLE_CONDITIONS_ARRAY
                redfish::conditions_utils::populateServiceConditions(
                                    asyncResp, std::string(""));
#endif // BMCWEB_DISABLE_CONDITIONS_ARRAY
            });
}

} // namespace redfish
