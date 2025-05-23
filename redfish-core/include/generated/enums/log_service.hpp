// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#pragma once
#include <nlohmann/json.hpp>

namespace log_service
{
// clang-format off

enum class OverWritePolicy{
    Invalid,
    Unknown,
    WrapsWhenFull,
    NeverOverWrites,
};

enum class LogEntryTypes{
    Invalid,
    Event,
    SEL,
    Multiple,
    OEM,
    CXL,
};

enum class SyslogSeverity{
    Invalid,
    Emergency,
    Alert,
    Critical,
    Error,
    Warning,
    Notice,
    Informational,
    Debug,
    All,
};

enum class SyslogFacility{
    Invalid,
    Kern,
    User,
    Mail,
    Daemon,
    Auth,
    Syslog,
    LPR,
    News,
    UUCP,
    Cron,
    Authpriv,
    FTP,
    NTP,
    Security,
    Console,
    SolarisCron,
    Local0,
    Local1,
    Local2,
    Local3,
    Local4,
    Local5,
    Local6,
    Local7,
};

enum class LogDiagnosticDataTypes{
    Invalid,
    Manager,
    PreOS,
    OS,
    OEM,
    Device,
};

enum class LogPurpose{
    Invalid,
    Diagnostic,
    Operations,
    Security,
    Telemetry,
    ExternalEntity,
    OEM,
};

enum class TransferProtocolType{
    Invalid,
    CIFS,
    FTP,
    SFTP,
    HTTP,
    HTTPS,
    NFS,
    SCP,
    TFTP,
    OEM,
};

enum class AutoClearResolvedEntries{
    Invalid,
    ClearEventGroup,
    RetainCauseResolutionEntries,
    UpdateCauseEntry,
    None,
};

NLOHMANN_JSON_SERIALIZE_ENUM(OverWritePolicy, {
    {OverWritePolicy::Invalid, "Invalid"},
    {OverWritePolicy::Unknown, "Unknown"},
    {OverWritePolicy::WrapsWhenFull, "WrapsWhenFull"},
    {OverWritePolicy::NeverOverWrites, "NeverOverWrites"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(LogEntryTypes, {
    {LogEntryTypes::Invalid, "Invalid"},
    {LogEntryTypes::Event, "Event"},
    {LogEntryTypes::SEL, "SEL"},
    {LogEntryTypes::Multiple, "Multiple"},
    {LogEntryTypes::OEM, "OEM"},
    {LogEntryTypes::CXL, "CXL"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(SyslogSeverity, {
    {SyslogSeverity::Invalid, "Invalid"},
    {SyslogSeverity::Emergency, "Emergency"},
    {SyslogSeverity::Alert, "Alert"},
    {SyslogSeverity::Critical, "Critical"},
    {SyslogSeverity::Error, "Error"},
    {SyslogSeverity::Warning, "Warning"},
    {SyslogSeverity::Notice, "Notice"},
    {SyslogSeverity::Informational, "Informational"},
    {SyslogSeverity::Debug, "Debug"},
    {SyslogSeverity::All, "All"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(SyslogFacility, {
    {SyslogFacility::Invalid, "Invalid"},
    {SyslogFacility::Kern, "Kern"},
    {SyslogFacility::User, "User"},
    {SyslogFacility::Mail, "Mail"},
    {SyslogFacility::Daemon, "Daemon"},
    {SyslogFacility::Auth, "Auth"},
    {SyslogFacility::Syslog, "Syslog"},
    {SyslogFacility::LPR, "LPR"},
    {SyslogFacility::News, "News"},
    {SyslogFacility::UUCP, "UUCP"},
    {SyslogFacility::Cron, "Cron"},
    {SyslogFacility::Authpriv, "Authpriv"},
    {SyslogFacility::FTP, "FTP"},
    {SyslogFacility::NTP, "NTP"},
    {SyslogFacility::Security, "Security"},
    {SyslogFacility::Console, "Console"},
    {SyslogFacility::SolarisCron, "SolarisCron"},
    {SyslogFacility::Local0, "Local0"},
    {SyslogFacility::Local1, "Local1"},
    {SyslogFacility::Local2, "Local2"},
    {SyslogFacility::Local3, "Local3"},
    {SyslogFacility::Local4, "Local4"},
    {SyslogFacility::Local5, "Local5"},
    {SyslogFacility::Local6, "Local6"},
    {SyslogFacility::Local7, "Local7"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(LogDiagnosticDataTypes, {
    {LogDiagnosticDataTypes::Invalid, "Invalid"},
    {LogDiagnosticDataTypes::Manager, "Manager"},
    {LogDiagnosticDataTypes::PreOS, "PreOS"},
    {LogDiagnosticDataTypes::OS, "OS"},
    {LogDiagnosticDataTypes::OEM, "OEM"},
    {LogDiagnosticDataTypes::Device, "Device"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(LogPurpose, {
    {LogPurpose::Invalid, "Invalid"},
    {LogPurpose::Diagnostic, "Diagnostic"},
    {LogPurpose::Operations, "Operations"},
    {LogPurpose::Security, "Security"},
    {LogPurpose::Telemetry, "Telemetry"},
    {LogPurpose::ExternalEntity, "ExternalEntity"},
    {LogPurpose::OEM, "OEM"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(TransferProtocolType, {
    {TransferProtocolType::Invalid, "Invalid"},
    {TransferProtocolType::CIFS, "CIFS"},
    {TransferProtocolType::FTP, "FTP"},
    {TransferProtocolType::SFTP, "SFTP"},
    {TransferProtocolType::HTTP, "HTTP"},
    {TransferProtocolType::HTTPS, "HTTPS"},
    {TransferProtocolType::NFS, "NFS"},
    {TransferProtocolType::SCP, "SCP"},
    {TransferProtocolType::TFTP, "TFTP"},
    {TransferProtocolType::OEM, "OEM"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(AutoClearResolvedEntries, {
    {AutoClearResolvedEntries::Invalid, "Invalid"},
    {AutoClearResolvedEntries::ClearEventGroup, "ClearEventGroup"},
    {AutoClearResolvedEntries::RetainCauseResolutionEntries, "RetainCauseResolutionEntries"},
    {AutoClearResolvedEntries::UpdateCauseEntry, "UpdateCauseEntry"},
    {AutoClearResolvedEntries::None, "None"},
});

}
// clang-format on
