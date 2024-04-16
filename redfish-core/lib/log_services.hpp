#pragma once
#include "app.hpp"
namespace redfish
{

bool getTimestampFromID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        std::string_view entryIDStrView, sd_id128_t& bootID,
                        uint64_t& timestamp, uint64_t& index);
void getDumpServiceInfo(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& dumpType);
void requestRoutesSystemLogServiceCollection(App&);
void requestRoutesEventLogService(App&);
void requestRoutesJournalEventLogClear(App&);
void requestRoutesJournalEventLogEntryCollection(App&);
void requestRoutesJournalEventLogEntry(App&);
void requestRoutesDBusEventLogEntryCollection(App&);
void requestRoutesDBusEventLogEntry(App&);
void requestRoutesSystemHostLogger(App&);
void requestRoutesSystemHostLoggerCollection(App&);
void requestRoutesSystemHostLoggerLogEntry(App&);
void requestRoutesBMCLogServiceCollection(App&);
void requestRoutesBMCJournalLogService(App&);
void requestRoutesBMCJournalLogEntryCollection(App&);
void requestRoutesBMCJournalLogEntry(App&);
void requestRoutesBMCDumpService(App&);
void requestRoutesBMCDumpEntryCollection(App&);
void requestRoutesBMCDumpEntry(App&);
void requestRoutesBMCDumpEntryDownload(App&);
void requestRoutesBMCDumpCreate(App&);
void requestRoutesBMCDumpClear(App&);
void requestRoutesDBusEventLogEntryDownload(App&);
void requestRoutesFaultLogDumpService(App&);
void requestRoutesFaultLogDumpEntryCollection(App&);
void requestRoutesFaultLogDumpEntry(App&);
void requestRoutesFaultLogDumpClear(App&);
void requestRoutesSystemDumpService(App&);
void requestRoutesSystemDumpEntryCollection(App&);
void requestRoutesSystemDumpEntry(App&);
void requestRoutesSystemDumpCreate(App&);
void requestRoutesSystemDumpClear(App&);
void requestRoutesCrashdumpService(App&);
void requestRoutesCrashdumpClear(App&);
void requestRoutesCrashdumpEntryCollection(App&);
void requestRoutesCrashdumpEntry(App&);
void requestRoutesCrashdumpFile(App&);
void requestRoutesCrashdumpCollect(App&);
void requestRoutesDBusLogServiceActionsClear(App&);
void requestRoutesPostCodesLogService(App&);
void requestRoutesPostCodesClear(App&);
void requestRoutesPostCodesEntryCollection(App&);
void requestRoutesPostCodesEntryAdditionalData(App&);
void requestRoutesPostCodesEntry(App&);
} // namespace redfish
