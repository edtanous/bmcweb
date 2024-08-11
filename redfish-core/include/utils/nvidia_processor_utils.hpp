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
                "#NvidiaProcessor.v1_2_0.NvidiaGPU";
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
        json["Oem"]["Nvidia"]["@odata.type"] =
            "#NvidiaProcessor.v1_2_0.NvidiaGPU";
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
                json["Oem"]["Nvidia"]["CCDevModeEnabled"] = *pendingCCDevState;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.CCMode");
}

/**
 * @brief Populate Property SMUtilizationPercent by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getSMUtilizationData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                                 const std::string& service,
                                 const std::string& objPath)
{
    BMCWEB_LOG_DEBUG("Get processor metrics SMUtilizationPercent data.");
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const OperatingConfigProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            if (property.first == "Utilization")
            {
                const double* value = std::get_if<double>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR(
                        "Failed to get value of Property Utilization");
                    messages::internalError(aResp->res);
                    return;
                }

                aResp->res.jsonValue["Oem"]["Nvidia"]["SMUtilizationPercent"] =
                    *value;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig");
}

inline void getNvLinkTotalCount(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& cpuId,
                                const std::string& service,
                                const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, cpuId](const boost::system::error_code ec,
                       const OperatingConfigProperties& properties) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        nlohmann::json& json = aResp->res.jsonValue;
        for (const auto& property : properties)
        {
            if (property.first == "TotalNumberNVLinks")
            {
                const uint64_t* totalNumberNVLinks =
                    std::get_if<uint64_t>(&property.second);
                if (totalNumberNVLinks == nullptr)
                {
                    BMCWEB_LOG_ERROR("Invalid Data Type");
                    messages::internalError(aResp->res);
                    return;
                }
                json["Oem"]["Nvidia"]["TotalNumberNVLinks"] =
                    *totalNumberNVLinks;
            }
        }
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "com.nvidia.NVLink.NvLinkTotalCount");
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
    getPowerSmoothingInfo(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                          const std::string& processorId)
{
    std::string powerSmoothingURI = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                    "/Processors/";
    powerSmoothingURI += processorId;
    powerSmoothingURI += "/Oem/Nvidia/PowerSmoothing";
    aResp->res.jsonValue["Oem"]["Nvidia"]["PowerSmoothing"]["@odata.id"] =
        powerSmoothingURI;
}

inline void getClearablePcieCounters(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& objPath,
    const std::string& interface)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp](
            const boost::system::error_code ec,
            const std::vector<
                std::pair<std::string, std::variant<std::vector<std::string>>>>&
                propertiesList) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Get All call Failed for the interface. ec: {}",
                             ec);
            messages::internalError(asyncResp->res);
            return;
        }

        std::vector<std::string> clearableDataSource;
        for (const std::pair<std::string,
                             std::variant<std::vector<std::string>>>& property :
             propertiesList)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "ClearableCounters")
            {
                const std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&property.second);

                if (data)
                {
                    for (auto counter : *data)
                    {
                        clearableDataSource.push_back(
                            counter.substr(counter.find_last_of('.') + 1));
                    }
                }
            }
        }
        asyncResp->res.jsonValue["Parameters"]["AllowableValues"] =
            clearableDataSource;
    },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        interface.c_str());
}

/**
 * @brief Fill out processor nvidia specific info by
 * requesting data from the given D-Bus object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       processorId       Processor ID.
 * @param[in]       portId       Processor ID.
 */
inline void getClearPCIeCountersActionInfo(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& processorId, const std::string& portId)
{
    BMCWEB_LOG_DEBUG("Get available system processor resource");
    crow::connections::systemBus->async_method_call(
        [processorId, portId, asyncResp](
            const boost::system::error_code ec,
            const boost::container::flat_map<
                std::string, boost::container::flat_map<
                                 std::string, std::vector<std::string>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(asyncResp->res);

            return;
        }
        for (const auto& [path, object] : subtree)
        {
            if (!boost::ends_with(path, processorId))
            {
                continue;
            }
            crow::connections::systemBus->async_method_call(
                [asyncResp, processorId,
                 portId](const boost::system::error_code& e,
                         std::variant<std::vector<std::string>>& resp) {
                if (e)
                {
                    // no state sensors attached.
                    BMCWEB_LOG_ERROR(
                        "Object Mapper call failed while finding all_states association, with error {}",
                        e);
                    messages::internalError(asyncResp->res);
                    return;
                }

                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    BMCWEB_LOG_ERROR("No Association for all_states found");
                    messages::internalError(asyncResp->res);
                    return;
                }

                for (const std::string& sensorpath : *data)
                {
                    // Check Interface in Object or not
                    BMCWEB_LOG_DEBUG("processor state sensor object path {}",
                                     sensorpath);
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, sensorpath, processorId,
                         portId](const boost::system::error_code ec,
                                 const std::vector<std::pair<
                                     std::string, std::vector<std::string>>>&
                                     object) {
                        if (ec)
                        {
                            // the path does not implement port
                            // interfaces
                            BMCWEB_LOG_DEBUG(
                                "no port interface on object path {}",
                                sensorpath);
                            return;
                        }

                        sdbusplus::message::object_path path(sensorpath);
                        if (path.filename() != portId)
                        {
                            return;
                        }

                        std::string clearPcieCountersActionInfoUri =
                            "/redfish/v1/Systems/" PLATFORMSYSTEMID
                            "/Processors/";
                        clearPcieCountersActionInfoUri += processorId;
                        clearPcieCountersActionInfoUri += "/Ports/";
                        clearPcieCountersActionInfoUri +=
                            portId +
                            "/Metrics/Oem/Nvidia/ClearPCIeCountersActionInfo";
                        asyncResp->res.jsonValue["@odata.id"] =
                            clearPcieCountersActionInfoUri;
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#ActionInfo.v1_2_0.ActionInfo";
                        asyncResp->res.jsonValue["Name"] =
                            "ClearPCIeCounters Action Info";
                        asyncResp->res.jsonValue["Id"] =
                            "ClearPCIeCountersActionInfo";

                        for (const auto& [service, interfaces] : object)
                        {
                            for (auto interface : interfaces)
                            {
                                if (interface ==
                                    "xyz.openbmc_project.PCIe.ClearPCIeCounters")
                                {
                                    asyncResp->res
                                        .jsonValue["Parameters"]["Name"] =
                                        "CounterType";
                                    asyncResp->res.jsonValue["Parameters"]
                                                            ["Required"] = true;
                                    asyncResp->res
                                        .jsonValue["Parameters"]["DataType"] =
                                        "String";
                                    getClearablePcieCounters(asyncResp, service,
                                                             sensorpath,
                                                             interface);
                                    return;
                                }
                            }
                        }
                    },
                        "xyz.openbmc_project.ObjectMapper",
                        "/xyz/openbmc_project/object_mapper",
                        "xyz.openbmc_project.ObjectMapper", "GetObject",
                        sensorpath,
                        std::array<std::string, 1>(
                            {"xyz.openbmc_project.Inventory.Item.Port"}));
                }
            },
                "xyz.openbmc_project.ObjectMapper", path + "/all_states",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
            return;
        }
        // Object not found
        messages::resourceNotFound(asyncResp->res,
                                   "#Processor.v1_20_0.Processor", processorId);
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 2>{
            "xyz.openbmc_project.Inventory.Item.Cpu",
            "xyz.openbmc_project.Inventory.Item.Accelerator"});
}

inline void
    getPortLinkStatusSetting(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             const std::string& portPath,
                             const std::string& service,
                             const std::vector<uint8_t>& portsToDisable)
{
    using PropertyType =
        std::variant<std::string, bool, size_t, std::vector<uint8_t>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;

    crow::connections::systemBus->async_method_call(
        [aResp, portsToDisable](const boost::system::error_code ec,
                                const PropertiesMap& properties) {
        if (ec)
        {
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "PortNumber")
            {
                const size_t* value = std::get_if<size_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for port number");
                    messages::internalError(aResp->res);
                    return;
                }

                // check port number if present in vector
                auto it = std::find(portsToDisable.begin(),
                                    portsToDisable.end(), *value);
                if (it != portsToDisable.end())
                {
                    aResp->res.jsonValue["LinkState"] = "Disabled";
                }
                else
                {
                    aResp->res.jsonValue["LinkState"] = "Enabled";
                }
            }
        }
    },
        service, portPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Port");
}

inline void getPortDisableFutureStatus(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const std::string& processorId, const std::string& objectPath,
    const dbus::utility::MapperServiceMap& serviceMap,
    const std::string& portId)
{
    using PropertyType =
        std::variant<std::string, bool, size_t, std::vector<uint8_t>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;

    crow::connections::systemBus->async_method_call(
        [aResp, processorId, portId,
         objectPath](const boost::system::error_code ec,
                     const PropertiesMap& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Dbus response error"
                             "while getting port disable status");
            messages::internalError(aResp->res);
            return;
        }
        std::vector<uint8_t> portsToDisable;

        for (const auto& property : properties)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "PortDisableFuture")
            {
                auto* value =
                    std::get_if<std::vector<uint8_t>>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned "
                                     "for Port Disable Future mask");
                    messages::internalError(aResp->res);
                    return;
                }
                portsToDisable = *value;
            }
        }

        crow::connections::systemBus->async_method_call(
            [aResp, processorId, portId,
             portsToDisable](const boost::system::error_code ec,
                             std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBUS response error");
                messages::internalError(aResp->res);
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_ERROR("DBUS response error while getting ports");
                messages::internalError(aResp->res);
                return;
            }
            for (const std::string& portPath : *data)
            {
                sdbusplus::message::object_path pPath(portPath);
                if (pPath.filename() != portId)
                {
                    continue;
                }

                std::string processorPortSettingsURI =
                    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/";
                processorPortSettingsURI += processorId;
                processorPortSettingsURI += "/Ports/";
                processorPortSettingsURI += portId;
                processorPortSettingsURI += "/Settings";

                aResp->res.jsonValue["@odata.type"] = "#Port.v1_4_0.Port";
                aResp->res.jsonValue["@odata.id"] = processorPortSettingsURI;
                aResp->res.jsonValue["Id"] = "Settings";
                aResp->res.jsonValue["Name"] = processorId + " " + portId +
                                               " Pending Settings";

                crow::connections::systemBus->async_method_call(
                    [aResp, processorId, portId, portPath, portsToDisable](
                        const boost::system::error_code ec,
                        const std::vector<std::pair<
                            std::string, std::vector<std::string>>>& object) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG("No port interface on {}", portPath);
                        return;
                    }
                    getPortLinkStatusSetting(
                        aResp, portPath, object.front().first, portsToDisable);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject", portPath,
                    std::array<std::string, 1>(
                        {"xyz.openbmc_project.Inventory.Item.Port"}));
            }
        },
            "xyz.openbmc_project.ObjectMapper", objectPath + "/all_states",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    },
        serviceMap.front().first, objectPath, "org.freedesktop.DBus.Properties",
        "GetAll", "com.nvidia.NVLink.NVLinkDisableFuture");
}

inline void getPortNumberAndCallSetAsync(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const std::string& processorId, const std::string& portId,
    const std::string& propertyValue, const std::string& propertyName,
    const std::string& processorPath, const std::string& processorService,
    const std::string& portService, const std::string& portPath,
    const std::vector<uint8_t>& portsToDisable)
{
    using PropertyType =
        std::variant<std::string, bool, size_t, std::vector<uint8_t>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;

    crow::connections::systemBus->async_method_call(
        [aResp, processorId, portId, propertyValue, propertyName, processorPath,
         processorService, portsToDisable](const boost::system::error_code ec,
                                           const PropertiesMap& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }

        for (const auto& property : properties)
        {
            const std::string& propName = property.first;
            if (propName == "PortNumber")
            {
                const size_t* value = std::get_if<size_t>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_DEBUG("Null value returned "
                                     "for port number");
                    messages::internalError(aResp->res);
                    return;
                }
                uint32_t portNumber = static_cast<uint32_t>(*value);

                dbus::utility::getDbusObject(
                    objectPath,
                    std::array<std::string_view, 1>{
                        nvidia_async_operation_utils::setAsyncInterfaceName},
                    [aResp, propertyValue, propertyName, portNumber,
                     processorId, processorPath, processorService,
                     portsToDisable](
                        const boost::system::error_code& ec,
                        const dbus::utility::MapperGetObject& object) {
                    if (!ec)
                    {
                        for (const auto& [serv, _] : object)
                        {
                            if (serv != processorService)
                            {
                                continue;
                            }

                            std::vector<uint8_t> portListToDisable =
                                portsToDisable;
                            auto it = std::find(portListToDisable.begin(),
                                                portListToDisable.end(),
                                                portNumber);
                            if (propertyValue == "Disabled")
                            {
                                if (it == portListToDisable.end())
                                    portListToDisable.push_back(
                                        static_cast<uint8_t>(portNumber));
                            }
                            else if (propertyValue == "Enabled")
                            {
                                if (it != portListToDisable.end())
                                    portListToDisable.erase(it);
                            }
                            else
                            {
                                BMCWEB_LOG_ERROR(
                                    "Invalid value for patch on property {}",
                                    propertyName);
                                messages::internalError(aResp->res);
                                return;
                            }

                            BMCWEB_LOG_DEBUG(
                                "Performing Patch using Set Async Method Call for {}",
                                propertyName);

                            nvidia_async_operation_utils::
                                doGenericSetAsyncAndGatherResult(
                                    aResp, std::chrono::seconds(60),
                                    processorService, processorPath,
                                    "com.nvidia.NVLink.NVLinkDisableFuture",
                                    propertyName,
                                    std::variant<std::vector<uint8_t>>(
                                        portListToDisable),
                                    nvidia_async_operation_utils::PatchPortDisableCallback{
                                        aResp});
                            return;
                        }
                    }
                });
            }
        }
    },
        portService, portPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Item.Port");
}

inline void patchPortDisableFuture(
    const std::shared_ptr<bmcweb::AsyncResp>& aResp,
    const std::string& processorId, const std::string& portId,
    const std::string& propertyValue, const std::string& propertyName,
    const std::string& objectPath,
    const dbus::utility::MapperServiceMap& serviceMap)
{
    // Check that the property even exists by checking for the interface
    const std::string* inventoryService = nullptr;
    for (const auto& [serviceName, interfaceList] : serviceMap)
    {
        if (std::find(interfaceList.begin(), interfaceList.end(),
                      "com.nvidia.NVLink.NVLinkDisableFuture") !=
            interfaceList.end())
        {
            inventoryService = &serviceName;
            break;
        }
    }
    if (inventoryService == nullptr)
    {
        BMCWEB_LOG_ERROR(
            "NVLinkDisableFuture interface not found while {} patch",
            propertyName);
        messages::internalError(aResp->res);
        return;
    }

    using PropertyType =
        std::variant<std::string, bool, size_t, std::vector<uint8_t>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;

    crow::connections::systemBus->async_method_call(
        [aResp, processorId, portId, propertyValue, propertyName, objectPath,
         service = *inventoryService](const boost::system::error_code ec,
                                      const PropertiesMap& properties) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error");
            messages::internalError(aResp->res);
            return;
        }
        std::vector<uint8_t> portsToDisable;

        for (const auto& property : properties)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "PortDisableFuture")
            {
                auto* value =
                    std::get_if<std::vector<uint8_t>>(&property.second);
                if (value == nullptr)
                {
                    BMCWEB_LOG_ERROR("Null value returned "
                                     "for Port Disable Future mask");
                    messages::internalError(aResp->res);
                    return;
                }
                portsToDisable = *value;
            }
        }
        crow::connections::systemBus->async_method_call(
            [aResp, processorId, portId, propertyValue, propertyName,
             objectPath, service,
             portsToDisable](const boost::system::error_code ec,
                             std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBUS response error");
                messages::internalError(aResp->res);
                return;
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_ERROR("DBUS response error while getting ports");
                messages::internalError(aResp->res);
                return;
            }
            for (const std::string& portPath : *data)
            {
                // Get the portId object
                sdbusplus::message::object_path pPath(portPath);
                if (pPath.filename() != portId)
                {
                    continue;
                }

                crow::connections::systemBus->async_method_call(
                    [aResp, processorId, portId, portPath, propertyValue,
                     propertyName, objectPath, service, portsToDisable](
                        const boost::system::error_code ec,
                        const std::vector<std::pair<
                            std::string, std::vector<std::string>>>& object) {
                    if (ec)
                    {
                        BMCWEB_LOG_DEBUG("No port interface on {}", portPath);
                        return;
                    }

                    getPortNumberAndCallSetAsync(
                        aResp, processorId, portId, propertyValue, propertyName,
                        objectPath, service, object.front().first, portPath,
                        portsToDisable);
                },
                    "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetObject", portPath,
                    std::array<std::string, 1>(
                        {"xyz.openbmc_project.Inventory.Item.Port"}));
            }
        },
            "xyz.openbmc_project.ObjectMapper", objectPath + "/all_states",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    },
        serviceMap.front().first, objectPath, "org.freedesktop.DBus.Properties",
        "GetAll", "com.nvidia.NVLink.NVLinkDisableFuture");
}

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void
    setOperatingSpeedRange(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const uint32_t& value, const std::string& patchProp,
                           const std::string& path)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, path, value, patchProp](
            const boost::system::error_code errorno,
            const std::vector<std::pair<std::string, std::vector<std::string>>>&
                objInfo) {
        if (errorno)
        {
            BMCWEB_LOG_ERROR("ObjectMapper::GetObject call failed: {}",
                             errorno);
            messages::internalError(asyncResp->res);
            return;
        }

        for (auto& [service, interfaces] : objInfo)
        {
            if (std::find(
                    interfaces.begin(), interfaces.end(),
                    nvidia_async_operation_utils::setAsyncInterfaceName) ==
                interfaces.end())
            {
                continue;
            }

            if (patchProp == "SettingMin")

            {
                nvidia_async_operation_utils::doGenericSetAsyncAndGatherResult(
                    asyncResp, std::chrono::seconds(60), service, path,
                    "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
                    "RequestedSpeedLimitMin", std::variant<uint32_t>(value),
                    nvidia_async_operation_utils::
                        PatchClockLimitControlCallback{asyncResp});
            }
            else if (patchProp == "SettingMax")

            {
                nvidia_async_operation_utils::doGenericSetAsyncAndGatherResult(
                    asyncResp, std::chrono::seconds(60), service, path,
                    "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
                    "RequestedSpeedLimitMax", std::variant<uint32_t>(value),
                    nvidia_async_operation_utils::
                        PatchClockLimitControlCallback{asyncResp});
            }

            else
            {
                BMCWEB_LOG_ERROR("Invalid patch properrty name: {}", patchProp);
            }

            return;
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetObject", path,
        std::array<const char*, 1>(
            {"xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig"}));
}

/**
 * Handle the PATCH operation of the OperatingSpeedRangeMHz Property SettingMin/SettingMax.
 *
 * @param[in,out]   asyncResp          Async HTTP response.
 * @param[in]       processorId     Processor's Id.
 * @param[in]       value           value of the property to be patched.
 * @param[in]       patchProp       string representing property name SettingMin/SettingMax
 * @param[in]       processorObjPath   Path of processor object used to get clockLimit control path.
 */

inline void patchOperatingSpeedRangeMHz(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& processorId, const uint32_t& value,
    const std::string& patchProp, const std::string processorObjPath)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, value, patchProp, processorId,
         processorObjPath](const boost::system::error_code ec,
                           std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("ObjectMapper call failed with error {ec}", ec);
            messages::internalError(asyncResp->res);
            return; 
        }
        std::vector<std::string>* data =
            std::get_if<std::vector<std::string>>(&resp);

        if (data)
        {
            for (auto chassisPath : *data)
            {
                crow::connections::systemBus->async_method_call(
                    [asyncResp, value, patchProp, processorId, chassisPath](
                        const boost::system::error_code ec,
                        std::variant<std::vector<std::string>>& resp) {
                    if (ec)
                    {
                        return; // no clock Limit Path for the chassis path
                    }

                    std::vector<std::string>* data =
                        std::get_if<std::vector<std::string>>(&resp);

                    for (auto clockLimitPath : *data)
                    {
                        setOperatingSpeedRange(asyncResp, value, patchProp,
                                               clockLimitPath);
                    }
                },

                    "xyz.openbmc_project.ObjectMapper",
                    chassisPath + "/clock_controls",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");
            }
        }
        else
        {
            BMCWEB_LOG_ERROR(
                "Chassis Path not found for processorId {processorId}",
                processorId);
            messages::internalError(asyncResp->res);
            return;
        }
    },

        "xyz.openbmc_project.ObjectMapper",
        processorObjPath + "/parent_chassis", "org.freedesktop.DBus.Properties",
        "Get", "xyz.openbmc_project.Association", "endpoints");
}

} // namespace nvidia_processor_utils
} // namespace redfish
