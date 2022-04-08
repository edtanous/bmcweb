#pragma once

namespace redfish
{

namespace dbus_utils
{

inline const char* toPhysicalContext(const std::string& physicalContext)
{
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.Back")
    {
        return "Back";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.Backplane")
    {
        return "Backplane";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.CPU")
    {
        return "CPU";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.Fan")
    {
        return "Fan";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.Front")
    {
        return "Front";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU")
    {
        return "GPU";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPUSubsystem")
    {
        return "GPUSubsystem";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.Memory")
    {
        return "Memory";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.NetworkingDevice")
    {
        return "NetworkingDevice";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.PowerSupply")
    {
        return "PowerSupply";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.StorageDevice")
    {
        return "StorageDevice";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.SystemBoard")
    {
        return "SystemBoard";
    }
    if (physicalContext ==
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.VoltageRegulator")
    {
        return "VoltageRegulator";
    }
    return "";
}

inline std::string toLocationType(const std::string& location)
{
    if (location ==
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot")
    {
        return "Slot";
    }
    if (location ==
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded")
    {
        return "Embedded";
    }
    if (location ==
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Socket")
    {
        return "Socket";
    }
    if (location ==
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Backplane")
    {
        return "Backplane";
    }
    if (location ==
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Unknown")
    {
        return "Unknown";
    }
    return "";
}

} // namespace dbus_utils
} // namespace redfish