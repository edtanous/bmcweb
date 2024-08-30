#pragma once
#include <nlohmann/json.hpp>

namespace log_entry
{
// clang-format off

enum class EventSeverity{
    Invalid,
    OK,
    Warning,
    Critical,
};

enum class LogEntryType{
    Invalid,
    Event,
    SEL,
    Oem,
    CXL,
};

enum class LogDiagnosticDataTypes{
    Invalid,
    Manager,
    PreOS,
    OS,
    OEM,
    CPER,
    CPERSection,
};

enum class OriginatorTypes{
    Invalid,
    Client,
    Internal,
    SupportingService,
};

enum class CXLEntryType{
    Invalid,
    DynamicCapacity,
    Informational,
    Warning,
    Failure,
    Fatal,
};

enum class LogEntryCode{
    Invalid,
    Assert,
    Deassert,
};

enum class SensorType{
    Invalid,
    Reserved,
    Temperature,
    Voltage,
    Current,
    Fan,
    PhysicalChassisSecurity,
    PlatformSecurityViolationAttempt,
    Processor,
    PowerSupplyConverter,
    PowerUnit,
    CoolingDevice,
    OtherUnitsbasedSensor,
    Memory,
    DriveSlotBay,
    POSTMemoryResize,
    SystemFirmwareProgress,
    EventLoggingDisabled,
    Watchdog,
    SystemEvent,
    CriticalInterrupt,
    ButtonSwitch,
    ModuleBoard,
    MicrocontrollerCoprocessor,
    AddinCard,
    Chassis,
    ChipSet,
    OtherFRU,
    CableInterconnect,
    Terminator,
    SystemBootRestart,
    BootError,
    BaseOSBootInstallationStatus,
    OSStopShutdown,
    SlotConnector,
    SystemACPIPowerState,
    PlatformAlert,
    EntityPresence,
    MonitorASICIC,
    LAN,
    ManagementSubsystemHealth,
    Battery,
    SessionAudit,
    VersionChange,
    FRUState,
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventSeverity, {
    {EventSeverity::Invalid, "Invalid"},
    {EventSeverity::OK, "OK"},
    {EventSeverity::Warning, "Warning"},
    {EventSeverity::Critical, "Critical"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(LogEntryType, {
    {LogEntryType::Invalid, "Invalid"},
    {LogEntryType::Event, "Event"},
    {LogEntryType::SEL, "SEL"},
    {LogEntryType::Oem, "Oem"},
    {LogEntryType::CXL, "CXL"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(LogDiagnosticDataTypes, {
    {LogDiagnosticDataTypes::Invalid, "Invalid"},
    {LogDiagnosticDataTypes::Manager, "Manager"},
    {LogDiagnosticDataTypes::PreOS, "PreOS"},
    {LogDiagnosticDataTypes::OS, "OS"},
    {LogDiagnosticDataTypes::OEM, "OEM"},
    {LogDiagnosticDataTypes::CPER, "CPER"},
    {LogDiagnosticDataTypes::CPERSection, "CPERSection"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(OriginatorTypes, {
    {OriginatorTypes::Invalid, "Invalid"},
    {OriginatorTypes::Client, "Client"},
    {OriginatorTypes::Internal, "Internal"},
    {OriginatorTypes::SupportingService, "SupportingService"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(CXLEntryType, {
    {CXLEntryType::Invalid, "Invalid"},
    {CXLEntryType::DynamicCapacity, "DynamicCapacity"},
    {CXLEntryType::Informational, "Informational"},
    {CXLEntryType::Warning, "Warning"},
    {CXLEntryType::Failure, "Failure"},
    {CXLEntryType::Fatal, "Fatal"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(LogEntryCode, {
    {LogEntryCode::Invalid, "Invalid"},
    {LogEntryCode::Assert, "Assert"},
    {LogEntryCode::Deassert, "Deassert"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(SensorType, {
    {SensorType::Invalid, "Invalid"},
    {SensorType::Temperature, "Temperature"},
    {SensorType::Voltage, "Voltage"},
    {SensorType::Current, "Current"},
    {SensorType::Fan, "Fan"},
    {SensorType::PhysicalChassisSecurity, "Physical Chassis Security"},
    {SensorType::PlatformSecurityViolationAttempt, "Platform Security Violation Attempt"},
    {SensorType::Processor, "Processor"},
    {SensorType::PowerSupplyConverter, "Power Supply / Converter"},
    {SensorType::PowerUnit, "PowerUnit"},
    {SensorType::CoolingDevice, "Cooling Device"},
    {SensorType::OtherUnitsbasedSensor, "Other Units-based Sensor"},
    {SensorType::Memory, "Memory"},
    {SensorType::DriveSlotBay, "Drive Slot/Bay"},
    {SensorType::POSTMemoryResize, "POST Memory Resize"},
    {SensorType::SystemFirmwareProgress, "System Firmware Progress"},
    {SensorType::EventLoggingDisabled, "Event Logging Disabled"},
    {SensorType::Watchdog, "Watchdog"},
    {SensorType::SystemEvent, "System Event"},
    {SensorType::CriticalInterrupt, "Critical Interrupt"},
    {SensorType::ButtonSwitch, "Button/Switch"},
    {SensorType::ModuleBoard, "Module/Board"},
    {SensorType::MicrocontrollerCoprocessor, "Microcontroller/Coprocessor"},
    {SensorType::AddinCard, "Add-in Card"},
    {SensorType::Chassis, "Chassis"},
    {SensorType::ChipSet, "ChipSet"},
    {SensorType::OtherFRU, "Other FRU"},
    {SensorType::CableInterconnect, "Cable/Interconnect"},
    {SensorType::Terminator, "Terminator"},
    {SensorType::SystemBootRestart, "SystemBoot/Restart"},
    {SensorType::BootError, "Boot Error"},
    {SensorType::BaseOSBootInstallationStatus, "BaseOSBoot/InstallationStatus"},
    {SensorType::OSStopShutdown, "OS Stop/Shutdown"},
    {SensorType::SlotConnector, "Slot/Connector"},
    {SensorType::SystemACPIPowerState, "System ACPI PowerState"},
    {SensorType::PlatformAlert, "Platform Alert"},
    {SensorType::EntityPresence, "Entity Presence"},
    {SensorType::MonitorASICIC, "Monitor ASIC/IC"},
    {SensorType::LAN, "LAN"},
    {SensorType::ManagementSubsystemHealth, "Management Subsystem Health"},
    {SensorType::Battery, "Battery"},
    {SensorType::SessionAudit, "Session Audit"},
    {SensorType::VersionChange, "Version Change"},
    {SensorType::FRUState, "FRU State"},
});
}
// clang-format on
