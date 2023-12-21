#pragma once
#include "app.hpp"
namespace redfish
{
void doPowerSubsystemCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath);

void requestRoutesPowerSubsystem(App&);
} // namespace redfish
