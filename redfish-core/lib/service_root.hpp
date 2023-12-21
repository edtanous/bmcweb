#pragma once
#include "app.hpp"
namespace redfish
{
void handleServiceRootGetImpl(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);

void requestRoutesServiceRoot(App&);
} // namespace redfish
