#pragma once
#include "app.hpp"
namespace redfish
{
void requestRoutesSystemPCIeDeviceCollection(App&);
void requestRoutesSystemPCIeDevice(App&);
void requestRoutesSystemPCIeFunctionCollection(App&);
void requestRoutesSystemPCIeFunction(App&);
} // namespace redfish
