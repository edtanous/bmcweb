/*
// Copyright (c) 2018-2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "account_service.hpp"
#include "assembly.hpp"
#include "bios.hpp"
#include "cable.hpp"
#include "certificate_service.hpp"
#include "chassis.hpp"
#include "component_integrity.hpp"
#include "control.hpp"
#include "environment_metrics.hpp"
#include "erot_chassis.hpp"
#include "ethernet.hpp"
#include "event_service.hpp"
#include "fabric.hpp"
#include "host_interface.hpp"
#include "hypervisor_system.hpp"
#include "log_services.hpp"
#include "manager_diagnostic_data.hpp"
#include "managers.hpp"
#include "memory.hpp"
#include "message_registries.hpp"
#include "metric_report.hpp"
#include "metric_report_definition.hpp"
#include "network_protocol.hpp"
#include "pcie.hpp"
#include "pcieslots.hpp"
#include "power.hpp"
#include "power_subsystem.hpp"
#include "power_supply.hpp"
#include "processor.hpp"
#include "redfish_sessions.hpp"
#include "redfish_v1.hpp"
#include "roles.hpp"
#include "sensors.hpp"
#include "service_conditions.hpp"
#include "service_root.hpp"
#include "storage.hpp"
#include "systems.hpp"
#include "task.hpp"
#include "telemetry_service.hpp"
#include "thermal.hpp"
#include "thermal_subsystem.hpp"
#include "trigger.hpp"
#include "trusted_components.hpp"
#include "update_service.hpp"
#include "virtual_media.hpp"

namespace redfish
{
/*
 * @brief Top level class installing and providing Redfish services
 */
class RedfishService
{
  public:
    /*
     * @brief Redfish service constructor
     *
     * Loads Redfish configuration and installs schema resources
     *
     * @param[in] app   Crow app on which Redfish will initialize
     */
    explicit RedfishService(App& app)
    {
        if (persistent_data::getConfig().isTLSAuthEnabled())
        {
            requestAccountServiceRoutes(app);
        }
        requestAssemblyRoutes(app);
        requestPcieSlotsRoutes(app);
        if (persistent_data::getConfig().isTLSAuthEnabled())
        {
            requestRoutesRoles(app);
            requestRoutesRoleCollection(app);
        }
        requestRoutesServiceRoot(app);
        requestRoutesNetworkProtocol(app);
        if (persistent_data::getConfig().isTLSAuthEnabled())
        {
            requestRoutesSession(app);
        }
        requestEthernetInterfacesRoutes(app);
#ifdef BMCWEB_ALLOW_DEPRECATED_POWER_THERMAL
#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
        requestRoutesThermal(app);
        requestRoutesPower(app);
#endif
#endif
        requestRoutesThermalSubsystem(app);
        requestRoutesThermalMetrics(app);
#ifdef BMCWEB_NEW_POWERSUBSYSTEM_THERMALSUBSYSTEM
        requestRoutesPowerSubsystem(app);
        requestRoutesPowerSupplyCollection(app);
        requestRoutesPowerSupply(app);
#endif
        requestRoutesManagerCollection(app);
        requestRoutesManager(app);
        requestRoutesManagerResetAction(app);
        requestRoutesManagerResetActionInfo(app);
        requestRoutesManagerResetToDefaultsAction(app);

        // requestRoutesManagerDiagnosticData(app);
        requestRoutesChassisCollection(app);
        requestRoutesChassis(app);
#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
        requestRoutesChassisResetAction(app);
        requestRoutesChassisResetActionInfo(app);
#endif
        requestRoutesEnvironmentMetrics(app);
        requestRoutesProcessorEnvironmentMetrics(app);
        requestRoutesMemoryEnvironmentMetrics(app);
        requestRoutesUpdateService(app);
        requestRoutesChassisDrive(app);
        requestRoutesChassisDriveName(app);

        requestRoutesStorageCollection(app);
        requestRoutesStorage(app);
        requestRoutesDrive(app);
#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
        requestRoutesCable(app);
        requestRoutesCableCollection(app);
#endif
#ifdef BMCWEB_INSECURE_ENABLE_REDFISH_FW_TFTP_UPDATE
        requestRoutesUpdateServiceActionsSimpleUpdate(app);
#endif
        requestRoutesSoftwareInventoryCollection(app);
        requestRoutesSoftwareInventory(app);
        requestRoutesInventorySoftwareCollection(app);
        requestRoutesInventorySoftware(app);
        requestRoutesSystemLogServiceCollection(app);
#ifdef BMCWEB_ENABLE_MFG_TEST_API
        requestRoutesEventLogDiagnosticDataCollect(app);
        requestRoutesEventLogDiagnosticDataEntry(app);
#endif
        requestRoutesEventLogService(app);
        requestRoutesSELLogService(app);
        requestRoutesChassisLogServiceCollection(app);
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_LOGSERVICES
        requestRoutesChassisXIDLogService(app);
        requestRoutesChassisXIDLogEntryCollection(app);
#endif // BMCWEB_ENABLE_NVIDIA_OEM_LOGSERVICES

        requestRoutesPostCodesEntryAdditionalData(app);

        requestRoutesPostCodesLogService(app);
        requestRoutesPostCodesClear(app);
        requestRoutesPostCodesEntry(app);
        requestRoutesPostCodesEntryCollection(app);
        requestRoutesDebugToken(app);
        requestRoutesDebugTokenServiceDiagnosticDataCollect(app);
        requestRoutesDebugTokenServiceDiagnosticDataEntryDownload(app);

#ifdef BMCWEB_ENABLE_REDFISH_DUMP_LOG
        requestRoutesSystemDumpService(app);
        requestRoutesSystemDumpEntryCollection(app);
        requestRoutesSystemDumpEntry(app);
        requestRoutesSystemDumpCreate(app);
        requestRoutesSystemDumpClear(app);

        requestRoutesBMCDumpService(app);
        requestRoutesBMCDumpEntryCollection(app);
        requestRoutesBMCDumpEntry(app);
        requestRoutesBMCDumpCreate(app);
        requestRoutesBMCDumpClear(app);
#endif

#ifdef BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG
        requestRoutesSystemFaultLogService(app);
        requestRoutesSystemFaultLogEntryCollection(app);
        requestRoutesSystemFaultLogEntry(app);
        requestRoutesSystemFaultLogClear(app);
#endif // BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES
        requestRoutesJournalEventLogEntryCollection(app);
        requestRoutesJournalEventLogEntry(app);
        requestRoutesJournalEventLogClear(app);
#endif

        requestRoutesBMCLogServiceCollection(app);
#ifdef BMCWEB_ENABLE_REDFISH_BMC_JOURNAL
        requestRoutesBMCJournalLogService(app);
        requestRoutesBMCJournalLogEntryCollection(app);
        requestRoutesBMCJournalLogEntry(app);
#endif

#ifdef BMCWEB_ENABLE_REDFISH_CPU_LOG
        requestRoutesCrashdumpService(app);
        requestRoutesCrashdumpEntryCollection(app);
        requestRoutesCrashdumpEntry(app);
        requestRoutesCrashdumpFile(app);
        requestRoutesCrashdumpClear(app);
        requestRoutesCrashdumpCollect(app);
#endif // BMCWEB_ENABLE_REDFISH_CPU_LOG

        requestRoutesProcessorCollection(app);
        requestRoutesProcessor(app);
        requestRoutesOperatingConfigCollection(app);
        requestRoutesOperatingConfig(app);
        requestRoutesProcessorMetrics(app);
        requestRoutesProcessorMemoryMetrics(app);
        requestRoutesProcessorSettings(app);
        requestRoutesProcessorReset(app);
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
        requestRoutesEdppReset(app);
        requestRoutesNvidiaManagerResetToDefaultsAction(app);

        requestRouteSyncRawOobCommand(app);
        requestRouteAsyncRawOobCommand(app);
        requestRoutesNvidiaAsyncOOBRawCommandActionInfo(app);
        requestRoutesNvidiaSyncOOBRawCommandActionInfo(app);
#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
        requestRoutesProcessorPortCollection(app);
        requestRoutesProcessorPort(app);
        requestRoutesProcessorPortMetrics(app);
        requestRoutesMemoryCollection(app);
        requestRoutesMemory(app);
        requestRoutesMemoryMetrics(app);

        requestRoutesSystemsCollection(app);
        requestRoutesSystems(app);
#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
        requestRoutesSystemActionsReset(app);
        requestRoutesSystemResetActionInfo(app);
#endif
#ifdef BMCWEB_ENABLE_BIOS
        requestRoutesBiosService(app);
        requestRoutesBiosSettings(app);
        requestRoutesBiosReset(app);
        requestRoutesBiosAttrRegistryService(app);
#endif

#ifdef BMCWEB_ENABLE_HOST_IFACE
        requestHostInterfacesRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_RMEDIA
        requestNBDVirtualMediaRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES
        requestRoutesDBusLogServiceActionsClear(app);
        requestRoutesDBusEventLogEntryCollection(app);
        requestRoutesDBusEventLogEntry(app);
        requestRoutesDBusEventLogEntryDownload(app);
        requestRoutesDBusSELLogEntryCollection(app);
        requestRoutesDBusSELLogEntry(app);
        requestRoutesDBusSELLogServiceActionsClear(app);

#endif

#ifdef BMCWEB_ENABLE_REDFISH_HOST_LOGGER
        requestRoutesSystemHostLogger(app);
        requestRoutesSystemHostLoggerCollection(app);
        requestRoutesSystemHostLoggerLogEntry(app);
#endif

        requestRoutesMessageRegistryFileCollection(app);
        requestRoutesMessageRegistryFile(app);
        requestRoutesMessageRegistry(app);
        if (persistent_data::getConfig().isTLSAuthEnabled())
        {
            requestRoutesCertificateService(app);
            requestRoutesHTTPSCertificate(app);
            requestRoutesLDAPCertificate(app);
            requestRoutesTrustStoreCertificate(app);
        }
        requestRoutesSystemPCIeFunctionCollection(app);
        requestRoutesSystemPCIeFunction(app);
        requestRoutesSystemPCIeDeviceCollection(app);
        requestRoutesSystemPCIeDevice(app);
        requestRoutesChassisPCIeFunctionCollection(app);
        requestRoutesChassisPCIeFunction(app);
        requestRoutesChassisPCIeDeviceCollection(app);
        requestRoutesChassisPCIeDevice(app);

        requestRoutesSensorCollection(app);
        requestRoutesSensor(app);

        requestRoutesTaskMonitor(app);
        requestRoutesTaskService(app);
        requestRoutesTaskCollection(app);
        requestRoutesTask(app);
        requestRoutesEventService(app);
        requestRoutesEventDestinationCollection(app);
        requestRoutesEventDestination(app);
        requestRoutesSubmitTestEvent(app);

        hypervisor::requestRoutesHypervisorSystems(app);

        requestRoutesTelemetryService(app);
        requestRoutesMetricReportDefinitionCollection(app);
        requestRoutesMetricReportCollection(app);
#ifdef BMCWEB_ENABLE_PLATFORM_METRICS
        requestRoutesPlatformMetricReportDefinition(app);
        requestRoutesPlatformMetricReport(app);
#else
        requestRoutesMetricReportDefinition(app);
        requestRoutesMetricReport(app);
#endif
        requestRoutesFabricCollection(app);
        requestRoutesFabric(app);
        requestRoutesSwitchCollection(app);
        requestRoutesSwitch(app);
        requestRoutesNVSwitchReset(app);
        requestRoutesSwitchMetrics(app);
        requestRoutesPortCollection(app);
        requestRoutesPort(app);
        requestRoutesPortMetrics(app);
        requestRoutesEndpointCollection(app);
        requestRoutesEndpoint(app);
        requestRoutesZoneCollection(app);
        requestRoutesZone(app);

#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
        requestRoutesTriggerCollection(app);
        requestRoutesTrigger(app);
#endif
        requestRoutesEROTChassisCertificate(app);
        requestRoutesComponentIntegrity(app);
        requestRoutesServiceConditions(app);
        requestRoutesChassisControls(app);
        requestRoutesChassisControlsCollection(app);
        requestRoutesUpdateServiceCommitImage(app);
        requestRoutesTrustedComponents(app);
        // Note, this must be the last route registered
        requestRoutesRedfish(app);
    }
};

} // namespace redfish
