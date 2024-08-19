#pragma once

namespace redfish
{
namespace nvidia_pcie_utils
{

static constexpr const char* pciePath = "/xyz/openbmc_project/PCIe";
static constexpr const char* pcieAerErrorStatusIntf =
    "com.nvidia.PCIe.AERErrorStatus";

inline void
    getPCIeDeviceList(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const std::string& name,
                      const std::string& path = pciePath,
                      const std::string& chassisId = std::string())
{
    auto getPCIeMapCallback = [asyncResp{asyncResp}, name, chassisId](
                                  const boost::system::error_code ec,
                                  std::vector<std::string>& pcieDevicePaths) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG("no PCIe device paths found ec: ", ec.message());
            // Not an error, system just doesn't have PCIe info
            return;
        }
        nlohmann::json& pcieDeviceList = asyncResp->res.jsonValue[name];
        pcieDeviceList = nlohmann::json::array();
        for (const std::string& pcieDevicePath : pcieDevicePaths)
        {
            size_t devStart = pcieDevicePath.rfind('/');
            if (devStart == std::string::npos)
            {
                continue;
            }

            std::string devName = pcieDevicePath.substr(devStart + 1);
            if (devName.empty())
            {
                continue;
            }
            if (!chassisId.empty())
            {
                pcieDeviceList.push_back(
                    {{"@odata.id", std::string("/redfish/v1/Chassis/")
                                       .append(chassisId)
                                       .append("/PCIeDevices/")
                                       .append(devName)}});
            }
            else
            {
                pcieDeviceList.push_back(
                    {{"@odata.id",
                      "/redfish/v1/Systems/" +
                          std::string(BMCWEB_REDFISH_SYSTEM_URI_NAME) +
                          "/PCIeDevices/" + devName}});
            }
        }
        asyncResp->res.jsonValue[name + "@odata.count"] = pcieDeviceList.size();
    };
    crow::connections::systemBus->async_method_call(
        std::move(getPCIeMapCallback), "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        std::string(path) + "/", 1, std::array<std::string, 0>());
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

static inline void
    getAerErrorStatusOem(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& device, const std::string& path,
                           const std::string& service)
{
    auto getAerErrorStatusOemCallback =
        [asyncResp{asyncResp}](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::variant<std::string, std::vector<uint32_t>>>>&
                propertiesList) {
        if (ec)
        {
            BMCWEB_LOG_DEBUG(
                "DBUS response error on getting AER Error Status reference OEM properties");
            messages::internalError(asyncResp->res);
            return;
        }

        for (const std::pair<std::string, std::variant<std::string,
                             std::vector<uint32_t> > >& property : propertiesList)
        {
            const std::string& propertyName = property.first;
            if (propertyName == "AERUncorrectableErrorStatus")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"][propertyName] =
                        *value;
                }
            }
            else if (propertyName == "AERCorrectableErrorStatus")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value != nullptr)
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"][propertyName] =
                        *value;
                }
            }
            else if (propertyName == "RXErrorsPerLane")
            {
                const std::vector<uint32_t>* value =
                    std::get_if<std::vector<uint32_t>>(&property.second);
                if (value != nullptr)
                {
                    asyncResp->res.jsonValue["Oem"]["Nvidia"][propertyName] =
                        *value;
                }
            }
        }
    };
    std::string escapedPath = std::string(path) + "/" + device;
    dbus::utility::escapePathForDbus(escapedPath);
    crow::connections::systemBus->async_method_call(
        std::move(getAerErrorStatusOemCallback), service, escapedPath,
        "org.freedesktop.DBus.Properties", "GetAll", pcieAerErrorStatusIntf);
}

#endif // BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getFabricSwitchLink(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp, objPath](const boost::system::error_code ec,
                         std::variant<std::vector<std::string>>& resp) {
        if (ec)
        {
            return; // no fabric = no failures
        }
        std::vector<std::string>* dataFabric =
            std::get_if<std::vector<std::string>>(&resp);
        if (dataFabric == nullptr || dataFabric->size() > 1)
        {
            // There must be single fabric
            return;
        }
        const std::string& fabricPath = dataFabric->front();
        sdbusplus::message::object_path objectPath(fabricPath);
        std::string fabricId = objectPath.filename();
        if (fabricId.empty())
        {
            BMCWEB_LOG_ERROR("Fabric name empty");
            messages::internalError(aResp->res);
            return;
        }

        crow::connections::systemBus->async_method_call(
            [aResp, fabricId](const boost::system::error_code ec,
                              std::variant<std::vector<std::string>>& resp1) {
            if (ec)
            {
                return; // no switches = no failures
            }
            std::vector<std::string>* dataSwitch =
                std::get_if<std::vector<std::string>>(&resp1);
            if (dataSwitch == nullptr || dataSwitch->size() > 1)
            {
                // There must be single fabric
                return;
            }

            const std::string& switchPath = dataSwitch->front();
            sdbusplus::message::object_path objectPath1(switchPath);
            std::string switchId = objectPath1.filename();
            if (switchId.empty())
            {
                BMCWEB_LOG_ERROR("Switch name empty");
                messages::internalError(aResp->res);
                return;
            }
            std::string switchLink =
                (boost::format("/redfish/v1/Fabrics/%s/Switches/%s") %
                 fabricId % switchId)
                    .str();
            aResp->res.jsonValue["Links"]["Switch"]["@odata.id"] = switchLink;
            return;
        },
            "xyz.openbmc_project.ObjectMapper", objPath + "/all_switches",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    },
        "xyz.openbmc_project.ObjectMapper", objPath + "/fabrics",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

} // namespace nvidia_pcie_utils
} // namespace redfish
