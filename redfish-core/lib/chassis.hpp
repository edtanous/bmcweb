#pragma once
#include "app.hpp"
namespace redfish
{

void handleChassisResetActionInfoGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId);
void requestRoutesChassisCollection(App&);
void requestRoutesChassis(App&);
void requestRoutesChassisResetAction(App&);
void requestRoutesChassisResetActionInfo(App&);
} // namespace redfish
