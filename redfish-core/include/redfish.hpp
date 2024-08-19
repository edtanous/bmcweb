#pragma once

#include "app.hpp"

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
#include "nvidia_debug_token.hpp"
#endif

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
    explicit RedfishService(App& app);
};

} // namespace redfish
