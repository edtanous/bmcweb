#pragma once

namespace redfish
{
namespace nvidia_pcie_utils
{

static constexpr const char* pciePath = "/xyz/openbmc_project/PCIe";

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
                    {{"@odata.id", "/redfish/v1/Systems/" BMCWEB_REDFISH_SYSTEM_URI_NAME
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

} // namespace nvidia_pcie_utils
} // namespace redfish
