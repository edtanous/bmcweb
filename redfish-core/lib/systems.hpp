#pragma once
#include "app.hpp"
namespace redfish
{

void afterGetAllowedHostTransitions(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const boost::system::error_code& ec,
    const std::vector<std::string>& allowedHostTransitions);
void requestRoutesSystems(App&);
} // namespace redfish
