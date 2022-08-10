#pragma once

namespace redfish
{

namespace dbus_utils
{

constexpr const char* dbusObjManagerIntf = "org.freedesktop.DBus.ObjectManager";
constexpr const char* propertyInterface = "org.freedesktop.DBus.Properties";
constexpr const char* associationInterface = "xyz.openbmc_project.Association";
constexpr const char* mapperBusName = "xyz.openbmc_project.ObjectMapper";
constexpr const char* mapperObjectPath = "/xyz/openbmc_project/object_mapper";
constexpr const char* mapperIntf = "xyz.openbmc_project.ObjectMapper";
constexpr char const* objDeleteIntf = "xyz.openbmc_project.Object.Delete";
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

inline std::string toReasonType(const std::string& reason)
{
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.Idle")
    {
        return "Idle";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.UserDefined")
    {
        return "UserDefined";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.AppClock")
    {
        return "AppClock";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.SWPowerCap")
    {
        return "SWPowerCap";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.HWSlowdown")
    {
        return "HWSlowdown";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.HWThermalSlowdown")
    {
        return "HWThermalSlowdown";
    }

    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.HWPowerBrakeSlowdown")
    {
        return "HWPowerBrakeSlowdown";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.SyncBoost")
    {
        return "SyncBoost";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.SWThermalSlowdownTavg")
    {
        return "SWThermalSlowdownTavg";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.SWThermalSlowdownTlimit")
    {
        return "SWThermalSlowdownTlimit";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.DisplayClock")
    {
        return "DisplayClock";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.None")
    {
        return "NA";
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
