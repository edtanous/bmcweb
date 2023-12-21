#pragma once
#include "app.hpp"
namespace redfish
{
void requestRoutesStorageCollection(App&);
void requestRoutesStorage(App&);
void requestRoutesDrive(App&);
void requestRoutesChassisDrive(App&);
void requestRoutesChassisDriveName(App&);
void requestRoutesStorageControllerCollection(App&);
void requestRoutesStorageController(App&);
} // namespace redfish
