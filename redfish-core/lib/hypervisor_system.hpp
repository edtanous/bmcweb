#pragma once
#include "app.hpp"
namespace redfish
{

void handleHypervisorSystemGet(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);

void handleHypervisorResetActionGet(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);
void requestRoutesHypervisorSystems(App&);
} // namespace redfish
