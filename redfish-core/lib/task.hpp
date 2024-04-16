#pragma once
#include "app.hpp"

#include <sdbusplus/bus/match.hpp>
namespace redfish
{
void requestRoutesTaskMonitor(App&);
void requestRoutesTask(App&);
void requestRoutesTaskCollection(App&);
void requestRoutesTaskService(App&);
} // namespace redfish
