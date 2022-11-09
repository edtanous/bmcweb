#pragma once

namespace redfish
{
namespace port_utils
{

// Get PCIe device link speed generation
inline std::string getLinkSpeedGeneration(size_t speed)
{
    if (speed == 1)
    {
        return "Gen1";
    }
    if (speed == 2)
    {
        return "Gen2";
    }
    if (speed == 3)
    {
        return "Gen3";
    }
    if (speed == 4)
    {
        return "Gen4";
    }
    if (speed == 5)
    {
        return "Gen5";
    }
    // Unknown or others
    return "";
}

// PCIe device link width
inline size_t getLinkWidth(size_t width)
{
    if (width == 1)
    {
        return 1;
    }
    if (width == 2)
    {
        return 2;
    }
    if (width == 3)
    {
        return 4;
    }
    if (width == 4)
    {
        return 8;
    }
    if (width == 5)
    {
        return 16;
    }
    // Unknown or others
    return 0;
}

inline std::string getLinkStatusType(const std::string& linkStatusType)
{
    if (linkStatusType ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.LinkDown")
    {
        return "LinkDown";
    }
    if (linkStatusType ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.LinkUp")
    {
        return "LinkUp";
    }
    if (linkStatusType ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.NoLink")
    {
        return "NoLink";
    }
    if (linkStatusType ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.Starting")
    {
        return "Starting";
    }
    if (linkStatusType ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStatusType.Training")
    {
        return "Training";
    }
    // Unknown or others
    return "";
}

inline std::string getPortProtocol(const std::string& portProtocol)
{
    if (portProtocol ==
        "xyz.openbmc_project.Inventory.Item.Port.PortProtocol.Ethernet")
    {
        return "Ethernet";
    }
    if (portProtocol ==
        "xyz.openbmc_project.Inventory.Item.Port.PortProtocol.FC")
    {
        return "FC";
    }
    if (portProtocol ==
        "xyz.openbmc_project.Inventory.Item.Port.PortProtocol.NVLink")
    {
        return "NVLink";
    }
    if (portProtocol ==
        "xyz.openbmc_project.Inventory.Item.Port.PortProtocol.PCIe")
    {
        return "PCIe";
    }
    if (portProtocol ==
        "xyz.openbmc_project.Inventory.Item.Port.PortProtocol.OEM")
    {
        return "OEM";
    }
    if (portProtocol ==
        "xyz.openbmc_project.Inventory.Item.Port.PortProtocol.PCIe")
    {
        return "PCIe";
    }

    // Unknown or others
    return "";
}

inline std::string getLinkStates(const std::string& linkState)
{
    if (linkState ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStates.Enabled")
    {
        return "Enabled";
    }
    if (linkState ==
        "xyz.openbmc_project.Inventory.Item.Port.LinkStates.Disabled")
    {
        return "Disabled";
    }
    // Unknown or others
    return "";
}

inline std::string getPortType(const std::string& portType)
{
    if (portType ==
        "xyz.openbmc_project.Inventory.Item.Port.PortType.BidirectionalPort")
    {
        return "BidirectionalPort";
    }
    if (portType ==
        "xyz.openbmc_project.Inventory.Item.Port.PortType.DownstreamPort")
    {
        return "DownstreamPort";
    }
    if (portType ==
        "xyz.openbmc_project.Inventory.Item.Port.PortType.InterswitchPort")
    {
        return "InterswitchPort";
    }
    if (portType ==
        "xyz.openbmc_project.Inventory.Item.Port.PortType.ManagementPort")
    {
        return "ManagementPort";
    }
    if (portType ==
        "xyz.openbmc_project.Inventory.Item.Port.PortType.UnconfiguredPort")
    {
        return "UnconfiguredPort";
    }
    if (portType ==
        "xyz.openbmc_project.Inventory.Item.Port.PortType.UpstreamPort")
    {
        return "UpstreamPort";
    }
    // Unknown or others
    return "";
}

/**
 * @brief Get all switch info by requesting data
 * from the given D-Bus object.
 *
 * @param[in,out]   asyncResp   Async HTTP response.
 * @param[in]       service     D-Bus service to query.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void getPortData(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& service, const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get Port Data";
    using PropertyType =
        std::variant<std::string, bool, uint8_t, uint16_t, size_t, std::vector<std::string>>;
    using PropertiesMap = boost::container::flat_map<std::string, PropertyType>;
    // Get interface properties
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}](const boost::system::error_code ec,
                               const PropertiesMap& properties) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                                        "#NvidiaPort.v1_0_0.NvidiaPort";
#endif  //BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
            for (const auto& property : properties)
            {
                const std::string& propertyName = property.first;
                if (propertyName == "Type")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for port type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["PortType"] = getPortType(*value);
                }

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
                else if (propertyName == "TXWidth")
                {
                    const uint16_t* value = std::get_if<uint16_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for TXWidth";
                        messages::internalError(asyncResp->res);
                        return;
                    }


                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["TXWidth"] = *value;
                }
                else if (propertyName == "RXWidth")
                {
                    const uint16_t* value = std::get_if<uint16_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for RXWidth";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["Oem"]["Nvidia"]["RXWidth"] = *value;
                }
#endif  //BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES               
                else if (propertyName == "Protocol")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for protocol type";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["PortProtocol"] =
                        getPortProtocol(*value);
                }
                else if (propertyName == "LinkStatus")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for link status";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["LinkStatus"] =
                        getLinkStatusType(*value);
                }
                else if (propertyName == "LinkState")
                {
                    const std::string* value =
                        std::get_if<std::string>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for link state";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["LinkState"] =
                        getLinkStates(*value);
                }
                else if (propertyName == "CurrentSpeed")
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for CurrentSpeed";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["CurrentSpeedGbps"] = *value;
                }
                else if (propertyName == "MaxSpeed")
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for MaxSpeed";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue["MaxSpeedGbps"] = *value;
                }
                else if ((propertyName == "Width") || (propertyName == "ActiveWidth"))
                {
                    const size_t* value = std::get_if<size_t>(&property.second);
                    if (value == nullptr)
                    {
                        BMCWEB_LOG_DEBUG << "Null value returned "
                                            "for Width or ActiveWidth";
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    asyncResp->res.jsonValue[propertyName] = *value;
                }
                else if (propertyName == "CurrentPowerState")
                {
                    const std::string* state =
                        std::get_if<std::string>(&property.second);
                    if (*state ==
                        "xyz.openbmc_project.State.Chassis.PowerState.On")
                    {
                        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
                    }
                    else if (*state ==
                             "xyz.openbmc_project.State.Chassis.PowerState.Off")
                    {
                        asyncResp->res.jsonValue["Status"]["State"] =
                            "StandbyOffline";
                    }
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll", "");

        asyncResp->res.jsonValue["Status"]["Health"] = "OK";
        asyncResp->res.jsonValue["Status"]["HealthRollup"] = "OK";

        // update port health
#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                        std::shared_ptr<HealthRollup> health =
                            std::make_shared<HealthRollup>(
                                crow::connections::systemBus, objPath,
                                [asyncResp](const std::string& rootHealth,
                                            const std::string& healthRollup) {
                                    asyncResp->res
                                        .jsonValue["Status"]["Health"] =
                                        rootHealth;
                                    asyncResp->res
                                        .jsonValue["Status"]["HealthRollup"] =
                                        healthRollup;
                                });
                        health->start();

#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

}

} // namespace port_utils
} // namespace redfish
