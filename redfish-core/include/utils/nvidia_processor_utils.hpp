#pragma once

namespace redfish
{
namespace nvidia_processor_utils
{

using OperatingConfigProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

/**
 * Handle the PATCH operation of the CC Mode Property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       ccMode         New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchCCMode(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                        const std::string& processorId, const bool ccMode,
                        const std::string& cpuObjectPath,
                        const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.CCMode") != interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        BMCWEB_LOG_ERROR(" CCDevMode interface not found ");
        messages::internalError(resp->res);
        return;
    }

    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, ccMode](boost::system::error_code ec,
                                    sdbusplus::message::message& msg) {
        if (!ec)
        {
            BMCWEB_LOG_DEBUG("Set CC Mode property succeeded");
            return;
        }
        BMCWEB_LOG_DEBUG("CPU:{} set CC Mode  property failed: {}", processorId,
                         ec);

        // Read and convert dbus error message to redfish error
        const sd_bus_error* dbusError = msg.get_error();
        if (dbusError == nullptr)
        {
            messages::internalError(resp->res);
            return;
        }

        if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                    "Device.Error.WriteFailure") == 0)
        {
            // Service failed to change the config
            messages::operationFailed(resp->res);
        }
        else
        {
            messages::internalError(resp->res);
        }
    },
        *inventoryService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "com.nvidia.CCMode", "CCModeEnabled",
        std::variant<bool>(ccMode));
}

/**
 * Handle the PATCH operation of the MIG Mode Property. Do basic
 * validation of the input data, and then set the D-Bus property.
 *
 * @param[in,out]   resp            Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       ccDevMode         New property value to apply.
 * @param[in]       cpuObjectPath   Path of CPU object to modify.
 * @param[in]       serviceMap      Service map for CPU object.
 */
inline void patchCCDevMode(const std::shared_ptr<bmcweb::AsyncResp>& resp,
                           const std::string& processorId, const bool ccDevMode,
                           const std::string& cpuObjectPath,
                           const MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.CCMode") != interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        BMCWEB_LOG_ERROR(" CCMode interface not found ");
        messages::internalError(resp->res);
        return;
    }

    // Set the property, with handler to check error responses
    crow::connections::systemBus->async_method_call(
        [resp, processorId, ccDevMode](boost::system::error_code ec,
                                       sdbusplus::message::message& msg) {
        if (!ec)
        {
            BMCWEB_LOG_DEBUG("Set CC Dev Mode property succeeded");
            return;
        }

        BMCWEB_LOG_DEBUG("CPU:{} set CC Dev Mode  property failed: {}",
                         processorId, ec);
        // Read and convert dbus error message to redfish error
        const sd_bus_error* dbusError = msg.get_error();
        if (dbusError == nullptr)
        {
            messages::internalError(resp->res);
            return;
        }

        if (strcmp(dbusError->name, "xyz.openbmc_project.Common."
                                    "Device.Error.WriteFailure") == 0)
        {
            // Service failed to change the config
            messages::operationFailed(resp->res);
        }
        else
        {
            messages::internalError(resp->res);
        }
    },
        *inventoryService, cpuObjectPath, "org.freedesktop.DBus.Properties",
        "Set", "com.nvidia.CCMode", "CCDevModeEnabled",
        std::variant<bool>(ccDevMode));
}

/**
 * @brief Fill out processor nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       cpuId       Processor ID.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */

inline void getCCModeData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                          const std::string& cpuId, const std::string& service,
                          const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId](const boost::system::error_code ec,
                       const OperatingConfigProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        nlohmann::json& json = aResp->res.jsonValue;
        for (const auto& property : properties)
        {
            json["Oem"]["Nvidia"]["@odata.type"] =
                "#NvidiaProcessor.v1_0_0.NvidiaProcessor";
            if (property.first == "CCModeEnabled")
            {
                const bool* ccModeEnabled = std::get_if<bool>(&property.second);
                if (ccModeEnabled == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                json["Oem"]["Nvidia"]["CCModeEnabled"] = *ccModeEnabled;
            }
            else if (property.first == "CCDevModeEnabled")
            {
                const bool* ccDevModeEnabled =
                    std::get_if<bool>(&property.second);
                if (ccDevModeEnabled == nullptr)
                {
                    messages::internalError(aResp->res);
                    return;
                }
                json["Oem"]["Nvidia"]["CCDevModeEnabled"] = *ccDevModeEnabled;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.CCMode");
}

/**
 * @brief Fill out processor nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       cpuId       Processor ID.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */

inline void
    getCCModePendingData(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                         const std::string& cpuId, const std::string& service,
                         const std::string& objPath)

{
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId](const boost::system::error_code ec,
                       const OperatingConfigProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        nlohmann::json& json = aResp->res.jsonValue;
        for (const auto& property : properties)
        {
            if (property.first == "PendingCCModeState")
            {
                const bool* pendingCCState =
                    std::get_if<bool>(&property.second);
                if (pendingCCState == nullptr)
                {
                    BMCWEB_LOG_ERROR("Get PendingCCModeState property failed");
                    messages::internalError(aResp->res);
                    return;
                }
                json["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaProcessor.v1_0_0.NvidiaProcessor";
                json["Oem"]["Nvidia"]["CCModeEnabled"] = *pendingCCState;
            }
            else if (property.first == "PendingCCDevModeState")
            {
                const bool* pendingCCDevState =
                    std::get_if<bool>(&property.second);
                if (pendingCCDevState == nullptr)
                {
                    BMCWEB_LOG_ERROR(
                        "Get PendingCCDevModeState property failed");
                    messages::internalError(aResp->res);
                    return;
                }
                json["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaProcessor.v1_0_0.NvidiaProcessor";
                json["Oem"]["Nvidia"]["CCDevModeEnabled"] = *pendingCCDevState;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.CCMode");
}

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

} // namespace nvidia_processor_utils
} // namespace redfish
