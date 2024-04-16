#pragma once
#include "app.hpp"
namespace redfish
{

void doThermalSubsystemCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath);

void requestRoutesThermalSubsystem(App&);
} // namespace redfish
