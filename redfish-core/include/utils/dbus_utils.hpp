#pragma once

#include "logging.hpp"

#include <sdbusplus/server.hpp>
#include <sdbusplus/unpack_properties.hpp>

namespace redfish
{
namespace dbus_utils
{

using PropertyVal = std::variant<uint8_t, uint16_t, std::string,
                                 std::vector<std::string>, bool>;

struct UnpackErrorPrinter
{
    void operator()(const sdbusplus::UnpackErrorReason reason,
                    const std::string& property) const noexcept
    {
        BMCWEB_LOG_ERROR( "DBUS property error in property: {}, reason: {}", property, static_cast<std::underlying_type_t<sdbusplus::UnpackErrorReason>>( reason));
    }
};

constexpr const char* dbusObjManagerIntf = "org.freedesktop.DBus.ObjectManager";
constexpr const char* propertyInterface = "org.freedesktop.DBus.Properties";
constexpr const char* associationInterface = "xyz.openbmc_project.Association";
constexpr const char* mapperBusName = "xyz.openbmc_project.ObjectMapper";
constexpr const char* mapperObjectPath = "/xyz/openbmc_project/object_mapper";
constexpr const char* mapperIntf = "xyz.openbmc_project.ObjectMapper";
constexpr const char* objDeleteIntf = "xyz.openbmc_project.Object.Delete";

inline std::string getRedfishIstMode(const std::string& mode)
{
    if (mode == "xyz.openbmc_project.Control.Mode.StateOfISTMode.Disabled")
    {
        return "Disabled";
    }
    if (mode == "xyz.openbmc_project.Control.Mode.StateOfISTMode.Enabled")
    {
        return "Enabled";
    }
    if (mode == "xyz.openbmc_project.Control.Mode.StateOfISTMode.InProgress")
    {
        return "InProgress";
    }
    return "";
}

inline std::string toIstmgrStatus(const std::string& mode)
{
    if (mode == "com.Nvidia.IstModeManager.Server.StateOfISTMode.Disabled")
    {
        return "Disabled";
    }
    if (mode == "com.Nvidia.IstModeManager.Server.StateOfISTMode.Enabled")
    {
        return "Enabled";
    }
    if (mode == "com.Nvidia.IstModeManager.Server.StateOfISTMode.InProgress")
    {
        return "InProgress";
    }
    return "";
}

inline std::string getIstmgrParam(const bool& enabled)
{
    std::string val =
        "com.Nvidia.IstModeManager.Server.StateOfISTMode.Disabled";
    if (enabled)
    {
        val = "com.Nvidia.IstModeManager.Server.StateOfISTMode.Enabled";
    }
    return val;
}

inline std::string getReqMode(const bool& enabled)
{
    std::string val = "Disabled";
    if (enabled)
    {
        val = "Enabled";
    }
    return val;
}

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
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.GPUThermalOvertTreshold")
    {
        return "Current GPU temperature above the GPU Max Operating Temperature or Current memory temperature above the Memory Max Operating Temperature";
    }
    if (reason ==
        "xyz.openbmc_project.State.ProcessorPerformance.ThrottleReasons.None")
    {
        return "NA";
    }

    return "";
}

inline std::string toPowerSystemInputType(const std::string& state)
{
    if (state ==
        "xyz.openbmc_project.State.Decorator.PowerSystemInputs.Status.Good")
    {
        return "Normal";
    }
    if (state ==
        "xyz.openbmc_project.State.Decorator.PowerSystemInputs.Status.Fault")
    {
        return "Fault";
    }
    if (state ==
        "xyz.openbmc_project.State.Decorator.PowerSystemInputs.Status.InputOutOfRange")
    {
        return "OutOfRange";
    }
    if (state ==
        "xyz.openbmc_project.State.Decorator.PowerSystemInputs.Status.Unknown")
    {
        return "Unknown";
    }
    return "";
}

inline std::string toPerformanceStateType(const std::string& state)
{
    if (state ==
        "xyz.openbmc_project.State.ProcessorPerformance.PerformanceStates.Normal")
    {
        return "Normal";
    }
    if (state ==
        "xyz.openbmc_project.State.ProcessorPerformance.PerformanceStates.Throttled")
    {
        return "Throttled";
    }
    if (state ==
        "xyz.openbmc_project.State.ProcessorPerformance.PerformanceStates.Degraded")
    {
        return "Degraded";
    }
    if (state ==
        "xyz.openbmc_project.State.ProcessorPerformance.PerformanceStates.Unknown")
    {
        return "Unknown";
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

/**
 * @brief Defer the callback function until the shared_ptr destroys the returned
 * object.
 * @param callback A callback function, [](boost::system::error_code ec)
 */
template <typename Callback>
inline auto deferTask(Callback&& callback)
{
    struct DeferTaskStruct
    {
        DeferTaskStruct() = delete;
        DeferTaskStruct(const DeferTaskStruct&) = delete;
        DeferTaskStruct& operator=(const DeferTaskStruct&) = delete;
        DeferTaskStruct(DeferTaskStruct&&) = delete;
        DeferTaskStruct& operator=(DeferTaskStruct&&) = delete;

        DeferTaskStruct(Callback&& callback) : callback(callback) {}

        ~DeferTaskStruct()
        {
            callback(ec);
        }

        Callback callback;
        boost::system::error_code ec;
    };
    return std::make_shared<DeferTaskStruct>(std::forward<Callback>(callback));
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
inline std::string toSMPBIPrivilegeString(uint8_t privilege)
{
    if (privilege == 0x01)
    {
        return "HMC";
    }
    else if (privilege == 0x02)
    {
        return "HostBMC";
    }
    else
    {
        return "None";
    }
}

inline uint8_t toSMPBIPrivilegeType(const std::string& privilegeType)
{
    if (privilegeType == "HMC")
    {
        return 0x01;
    }
    else if (privilegeType == "HostBMC")
    {
        return 0x02;
    }
    else
    {
        return 0x00;
    }
}
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

} // namespace dbus_utils
} // namespace redfish
