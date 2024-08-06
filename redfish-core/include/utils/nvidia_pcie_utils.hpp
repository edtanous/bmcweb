#pragma once

namespace redfish
{
namespace nvidia_pcie_utils
{

static constexpr const char* pciePath = "/xyz/openbmc_project/PCIe";
static constexpr const char* pcieAerErrorStatusIntf =
    "com.nvidia.PCIe.AERErrorStatus";

inline void getPCIeDeviceList(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
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
                    {{"@odata.id", "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                   "/PCIeDevices/" +
                                       devName}});
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

} // namespace nvidia_pcie_utils
} // namespace redfish
