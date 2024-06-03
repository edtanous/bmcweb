#include "redfish.hpp"

#include "bmcweb_config.h"

#include "assembly.hpp"
#include "account_service.hpp"
#include "aggregation_service.hpp"
#include "app.hpp"
#include "bios.hpp"
#include "cable.hpp"
#include "control.hpp"
#include "certificate_service.hpp"
#include "chassis.hpp"
#include "environment_metrics.hpp"
#include "ethernet.hpp"
#include "event_service.hpp"
#include "eventservice_sse.hpp"
#include "fabric_adapters.hpp"
#include "fan.hpp"
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
#include "thermal_metrics.hpp"
#include "thermal_subsystem.hpp"
#include "trigger.hpp"
#include "update_service.hpp"
#include "virtual_media.hpp"
#include "nvidia_oem_dpu.hpp"
#ifdef BMCWEB_ENABLE_NETWORK_ADAPTERS_GENERIC
#include "network_adapters_generic.hpp"
#endif
#include "system_host_eth.hpp"
#include "network_adapters.hpp"
#include "ports.hpp"
#include "boot_options.hpp"
#include "erot_chassis.hpp"
#include "fabric.hpp"
#include "fabric_adapters.hpp"
#include "host_interface.hpp"
#include "secure_boot_database.hpp"
#include "secure_boot.hpp"
#include "pcieslots.hpp"
#include "pcie_slots.hpp"
#include "trusted_components.hpp"

namespace redfish
{

RedfishService::RedfishService(App& app)
{
    requestAssemblyRoutes(app);
    if (persistent_data::getConfig().isTLSAuthEnabled())
    {
        requestAccountServiceRoutes(app);
    }
    if constexpr (BMCWEB_REDFISH_AGGREGATION)
    {
        requestRoutesAggregationService(app);
        requestRoutesAggregationSourceCollection(app);
        requestRoutesAggregationSource(app);
    }
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
#ifdef BMCWEB_ENABLE_LLDP_DEDICATED_PORTS
    requestDedicatedPortsInterfacesRoutes(app);
#endif

 #ifdef BMCWEB_ENABLE_NETWORK_ADAPTERS
    requestRoutesNetworkAdapters(app);
    requestRoutesNetworkDeviceFunctions(app);
    requestRoutesACDPort(app);
#endif

#ifdef BMCWEB_ENABLE_HOST_ETH_IFACE
    requestHostEthernetInterfacesRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_NETWORK_ADAPTERS_GENERIC
    requestRoutesNetworkAdapters(app);
#endif

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
    requestRoutesChassisEnvironmentMetricsClearOOBSetPoint(app);
    requestRoutesProcessorEnvironmentMetricsClearOOBSetPoint(app);
#endif
    if constexpr (BMCWEB_REDFISH_ALLOW_DEPRECATED_POWER_THERMAL)
    {
#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
        requestRoutesThermal(app);
        requestRoutesPower(app);
#endif
    }
    if constexpr (BMCWEB_REDFISH_NEW_POWERSUBSYSTEM_THERMALSUBSYSTEM)
    {
        requestRoutesEnvironmentMetrics(app);
        requestRoutesPowerSubsystem(app);
        requestRoutesPowerSupply(app);
        requestRoutesPowerSupplyCollection(app);
        requestRoutesThermalMetrics(app);
        requestRoutesThermalSubsystem(app);
        requestRoutesFan(app);
        requestRoutesFanCollection(app);
    }
    requestRoutesManagerCollection(app);
    requestRoutesManager(app);
    requestRoutesManagerResetAction(app);
    requestRoutesManagerResetActionInfo(app);
    requestRoutesManagerResetToDefaultsAction(app);
    requestRoutesManagerDiagnosticData(app);
    requestRoutesChassisCollection(app);
    requestRoutesChassis(app);
#ifdef BMCWEB_ENABLE_HOST_OS_FEATURE
    requestRoutesThermal(app);
    requestRoutesPower(app);
#endif

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
    requestRoutesSplitUpdateService(app);
#endif
    #if defined(BMCWEB_INSECURE_ENABLE_REDFISH_FW_TFTP_UPDATE) ||                  \
    defined(BMCWEB_ENABLE_REDFISH_FW_SCP_UPDATE)
        requestRoutesUpdateServiceActionsSimpleUpdate(app);
    #endif

    requestRoutesChassisDrive(app);
    requestRoutesChassisDriveName(app);
    requestRoutesUpdateService(app);
    requestRoutesStorageCollection(app);
    requestRoutesStorage(app);
    requestRoutesStorageControllerCollection(app);
    requestRoutesStorageController(app);
    requestRoutesDrive(app);
    requestRoutesCable(app);
    requestRoutesCableCollection(app);
    requestRoutesInventorySoftwareCollection(app);
    requestRoutesInventorySoftware(app);
    requestRoutesSystemLogServiceCollection(app);
    requestRoutesEventLogService(app);
    requestRoutesPostCodesEntryAdditionalData(app);
    requestRoutesSELLogService(app);
    requestRoutesChassisLogServiceCollection(app);
    requestRoutesPostCodesLogService(app);
    requestRoutesPostCodesClear(app);
    requestRoutesPostCodesEntry(app);
    requestRoutesPostCodesEntryCollection(app);

#ifdef BMCWEB_ENABLE_MFG_TEST_API
    requestRoutesEventLogDiagnosticDataCollect(app);
    requestRoutesEventLogDiagnosticDataEntry(app);
#endif

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_LOGSERVICES
    requestRoutesChassisXIDLogService(app);
    requestRoutesChassisXIDLogEntryCollection(app);
#endif

    requestRoutesDebugToken(app);
    requestRoutesDebugTokenServiceEntry(app);
    requestRoutesDebugTokenServiceEntryCollection(app);
    requestRoutesDebugTokenServiceDiagnosticDataCollect(app);
    requestRoutesDebugTokenServiceDiagnosticDataEntryDownload(app);

    if constexpr (BMCWEB_REDFISH_DUMP_LOG)
    {
        requestRoutesSystemDumpService(app);
        requestRoutesSystemDumpEntryCollection(app);
        requestRoutesSystemDumpEntry(app);
        requestRoutesSystemDumpCreate(app);
        requestRoutesSystemDumpClear(app);

        requestRoutesBMCDumpService(app);
        requestRoutesBMCDumpEntryCollection(app);
        requestRoutesBMCDumpEntry(app);
        requestRoutesBMCDumpEntryDownload(app);
        requestRoutesBMCDumpCreate(app);
        requestRoutesBMCDumpClear(app);

#ifdef BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG
        // Nvidia has faultlog support under system
        requestRoutesFaultLogDumpService(app);
        requestRoutesFaultLogDumpEntryCollection(app);
        requestRoutesFaultLogDumpEntry(app);
        requestRoutesFaultLogDumpClear(app);
#endif // BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG

    }

#ifdef BMCWEB_ENABLE_REDFISH_FDR_DUMP_LOG
    requestRoutesSystemFDRService(app);
    requestRoutesSystemFDREntryCollection(app);
    requestRoutesSystemFDREntry(app);
    requestRoutesSystemFDRCreate(app);
    requestRoutesSystemFDRClear(app);
#endif

#ifdef BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG
        requestRoutesSystemFaultLogService(app);
        requestRoutesSystemFaultLogEntryCollection(app);
        requestRoutesSystemFaultLogEntry(app);
        requestRoutesSystemFaultLogClear(app);
#endif
    if constexpr (!BMCWEB_REDFISH_DBUS_LOG)
    {
        requestRoutesJournalEventLogEntryCollection(app);
        requestRoutesJournalEventLogEntry(app);
        requestRoutesJournalEventLogClear(app);
    }

    requestRoutesBMCLogServiceCollection(app);
    if constexpr (BMCWEB_REDFISH_BMC_JOURNAL)
    {
        requestRoutesBMCJournalLogService(app);
        requestRoutesBMCJournalLogEntryCollection(app);
        requestRoutesBMCJournalLogEntry(app);
    }

    if constexpr (BMCWEB_REDFISH_CPU_LOG)
    {
        requestRoutesCrashdumpService(app);
        requestRoutesCrashdumpEntryCollection(app);
        requestRoutesCrashdumpEntry(app);
        requestRoutesCrashdumpFile(app);
        requestRoutesCrashdumpClear(app);
        requestRoutesCrashdumpCollect(app);
    }

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
    requestRoutesNvidiaManagerEmmcSecureErase(app);
    requestRoutesManagerEmmcSecureEraseActionInfo(app);

    requestRouteSyncRawOobCommand(app);
    requestRouteAsyncRawOobCommand(app);
    requestRoutesNvidiaAsyncOOBRawCommandActionInfo(app);
    requestRoutesNvidiaSyncOOBRawCommandActionInfo(app);
#endif

    requestRoutesProcessorPortCollection(app);
    requestRoutesProcessorPort(app);
    requestRoutesProcessorPortMetrics(app);
    requestRoutesMemoryCollection(app);
    requestRoutesMemory(app);
    requestRoutesMemoryMetrics(app);

    requestRoutesSystems(app);

#ifdef BMCWEB_ENABLE_BIOS
    requestRoutesBiosService(app);
    requestRoutesBiosSettings(app);
    // requestRoutesBiosReset(app);
    requestRoutesBiosChangePassword(app);
    requestRoutesBiosAttrRegistryService(app);
    requestRoutesBootOptions(app);
    requestRoutesSecureBoot(app);
    requestRoutesSecureBootDatabase(app);
#endif


#ifdef BMCWEB_ENABLE_HOST_IFACE
    requestHostInterfacesRoutes(app);
#endif


    if constexpr (BMCWEB_VM_NBDPROXY)
    {
        requestNBDVirtualMediaRoutes(app);
    }

    if constexpr (BMCWEB_REDFISH_DBUS_LOG)
    {
        requestRoutesDBusLogServiceActionsClear(app);
        requestRoutesDBusEventLogEntryCollection(app);
        requestRoutesDBusEventLogEntry(app);
        requestRoutesDBusEventLogEntryDownload(app);
        requestRoutesDBusSELLogEntryCollection(app);
        requestRoutesDBusSELLogEntry(app);
        requestRoutesDBusSELLogServiceActionsClear(app);
    }

    if constexpr (BMCWEB_REDFISH_HOST_LOGGER)
    {
        requestRoutesSystemHostLogger(app);
        requestRoutesSystemHostLoggerCollection(app);
        requestRoutesSystemHostLoggerLogEntry(app);
    }

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
#ifdef BMCWEB_ENABLE_SSE
    requestRoutesEventServiceSse(app);
#endif
    

    requestRoutesEventDestinationCollection(app);
    requestRoutesEventDestination(app);
    requestRoutesFabricAdapters(app);
    requestRoutesFabricAdapterCollection(app);
    requestRoutesSubmitTestEvent(app);

    requestRoutesHypervisorSystems(app);

    requestRoutesTelemetryService(app);
    requestRoutesMetricReportDefinitionCollection(app);
    requestRoutesMetricReportDefinition(app);
    requestRoutesMetricReportCollection(app);
    requestRoutesMetricReport(app);

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
#ifdef BMCWEB_ENABLE_DOT
    requestRoutesEROTChassisDOT(app);
#endif


#ifdef BMCWEB_ENABLE_MANUAL_BOOT_MODE
    requestRoutesEROTChassisManualBootMode(app);
#endif
    requestRoutesComponentIntegrity(app);
    requestRoutesServiceConditions(app);
    requestRoutesChassisControls(app);
    requestRoutesChassisControlsCollection(app);
    requestRoutesUpdateServiceCommitImage(app);

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_BF_PROPERTIES
    requestRoutesNvidiaOemBf(app);
    requestRoutesNvidiaManagerSetSelCapacityAction(app);
    requestRoutesNvidiaManagerGetSelCapacity(app);
#endif
    requestRoutesTrustedComponents(app);
#ifdef BMCWEB_ENABLE_REDFISH_FW_SCP_UPDATE
    requestRoutesUpdateServicePublicKeyExchange(app);
    requestRoutesUpdateServiceRevokeAllRemoteServerPublicKeys(app);
#endif

    // Note, this must be the last route registered
    requestRoutesRedfish(app);
}

} // namespace redfish
