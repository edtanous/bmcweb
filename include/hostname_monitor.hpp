#pragma once
#ifdef BMCWEB_ENABLE_SSL

namespace crow
{
namespace hostname_monitor
{
void registerHostnameSignal();

} // namespace hostname_monitor
} // namespace crow
#endif
