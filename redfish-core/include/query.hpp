#pragma once

#include "app.hpp"
#include "utils/query_param.hpp"

namespace redfish
{

// Sets up the Redfish Route and delegates some of the query parameter
// processing. |queryCapabilities| stores which query parameters will be
// handled by redfish-core/lib codes, then default query parameter handler won't
// process these parameters.
[[nodiscard]] bool setUpRedfishRouteWithDelegation(
    crow::App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    query_param::Query& delegated,
    const query_param::QueryCapabilities& queryCapabilities);

// Sets up the Redfish Route. All parameters are handled by the default handler.
[[nodiscard]] bool
    setUpRedfishRoute(crow::App& app, const crow::Request& req,
                      const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);
} // namespace redfish
