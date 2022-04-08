#pragma once

namespace redfish
{

namespace processor_utils
{
using OperatingConfigProperties =
    std::vector<std::pair<std::string, dbus::utility::DbusVariantType>>;

inline void getPCIeErrorData(std::shared_ptr<bmcweb::AsyncResp> aResp,
                             const std::string& service,
                             const std::string& objPath)
{
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                  const OperatingConfigProperties& properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error";
                messages::internalError(aResp->res);
                return;
            }

            for (const auto& property : properties)
            {
                if (property.first == "ceCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res
                        .jsonValue["PCIeErrors"]["CorrectableErrorCount"] =
                        *value;
                }
                else if (property.first == "nonfeCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["NonFatalErrorCount"] =
                        *value;
                }
                else if (property.first == "feCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["FatalErrorCount"] =
                        *value;
                }
                else if (property.first == "L0ToRecoveryCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["L0ToRecoveryCount"] =
                        *value;
                }
                else if (property.first == "ReplayCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["ReplayCount"] = *value;
                }
                else if (property.first == "ReplayRolloverCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["ReplayRolloverCount"] =
                        *value;
                }
                else if (property.first == "NAKSentCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["NAKSentCount"] = *value;
                }
                else if (property.first == "NAKReceivedCount")
                {
                    const int64_t* value =
                        std::get_if<int64_t>(&property.second);
                    if (value == nullptr)
                    {
                        messages::internalError(aResp->res);
                        return;
                    }
                    aResp->res.jsonValue["PCIeErrors"]["NAKReceivedCount"] =
                        *value;
                }
            }
        },
        service, objPath, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.PCIe.PCIeECC");
}

} // namespace processor_utils
} // namespace redfish