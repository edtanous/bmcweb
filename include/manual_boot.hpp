#pragma once

#include "mctp_vdm_util_wrapper.hpp"
#include "utils/mctp_utils.hpp"

namespace redfish
{

namespace manual_boot
{

inline void bootModeQuery(const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisId)
{
    mctp_utils::enumerateMctpEndpoints(
        [req, asyncResp, chassisId](
            const std::shared_ptr<std::vector<mctp_utils::MctpEndpoint>>&
                endpoints) {
            if (!endpoints || endpoints->size() == 0)
            {
                BMCWEB_LOG_ERROR << "Endpoint ID for " << chassisId
                                 << " not found";
                messages::resourceNotFound(
                    asyncResp->res, "#Chassis.v1_17_0.Chassis", chassisId);
                return;
            }
            uint32_t eid =
                static_cast<uint32_t>(endpoints->begin()->getMctpEid());
            MctpVdmUtil mctpVdmUtilWrapper(eid);
            MctpVdmUtilCommand cmd = MctpVdmUtilCommand::BOOTMODE_QUERY;
            mctpVdmUtilWrapper.run(
                cmd, req, asyncResp,
                [chassisId](
                    const crow::Request&,
                    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                    uint32_t, const std::string& stdOut, const std::string&,
                    const boost::system::error_code& ec, int errorCode) {
                    if (ec || errorCode)
                    {
                        messages::resourceErrorsDetectedFormatError(
                            asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                            "command failure");
                        return;
                    }
                    nlohmann::json& oem =
                        asyncResp->res.jsonValue["Oem"]["Nvidia"];
                    std::string reEnabled = "(.|\n)*RX:( \\d\\d){9} 01(.|\n)*";
                    std::string reDisabled = "(.|\n)*RX:( \\d\\d){9} 00(.|\n)*";
                    if (std::regex_match(stdOut, std::regex(reEnabled)))
                    {
                        oem["ManualBootModeEnabled"] = true;
                        return;
                    }
                    if (std::regex_match(stdOut, std::regex(reDisabled)))
                    {
                        oem["ManualBootModeEnabled"] = false;
                        return;
                    }
                    BMCWEB_LOG_ERROR << "Invalid query_boot_mode response: "
                                     << stdOut;
                    messages::resourceErrorsDetectedFormatError(
                        asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                        "invalid response");
                });
        },
        [asyncResp, chassisId](bool critical, const std::string& desc,
                               const std::string& msg) {
            if (critical)
            {
                BMCWEB_LOG_ERROR << desc << ": " << msg;
                messages::resourceNotFound(
                    asyncResp->res, "#Chassis.v1_17_0.Chassis", chassisId);
            }
        },
        chassisId);
}

inline void bootModeSet(const crow::Request& req,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& chassisId, bool enabled)
{
    mctp_utils::enumerateMctpEndpoints(
        [req, asyncResp, chassisId,
         enabled](const std::shared_ptr<std::vector<mctp_utils::MctpEndpoint>>&
                      endpoints) {
            if (!endpoints || endpoints->size() == 0)
            {
                BMCWEB_LOG_ERROR << "Endpoint ID for " << chassisId
                                 << " not found";
                messages::resourceNotFound(
                    asyncResp->res, "#Chassis.v1_17_0.Chassis", chassisId);
                return;
            }
            uint32_t eid =
                static_cast<uint32_t>(endpoints->begin()->getMctpEid());
            MctpVdmUtil mctpVdmUtilWrapper(eid);
            MctpVdmUtilCommand cmd = enabled
                                         ? MctpVdmUtilCommand::BOOTMODE_ENABLE
                                         : MctpVdmUtilCommand::BOOTMODE_DISABLE;
            mctpVdmUtilWrapper.run(
                cmd, req, asyncResp,
                [enabled, chassisId](
                    const crow::Request&,
                    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                    uint32_t, const std::string& stdOut, const std::string&,
                    const boost::system::error_code& ec, int errorCode) {
                    if (ec || errorCode)
                    {
                        messages::resourceErrorsDetectedFormatError(
                            asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                            "command failure");
                        return;
                    }
                    std::string reSuccess = "(.|\n)*RX:( \\d\\d){8} 00(.|\n)*";
                    std::string reFailure = "(.|\n)*RX:( \\d\\d){8} 81(.|\n)*";
                    if (std::regex_match(stdOut, std::regex(reSuccess)))
                    {
                        messages::success(asyncResp->res);
                        return;
                    }
                    if (std::regex_match(stdOut, std::regex(reFailure)))
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    BMCWEB_LOG_ERROR << "Invalid boot_ap response: " << stdOut;
                    messages::resourceErrorsDetectedFormatError(
                        asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                        "invalid response");
                });
        },
        [asyncResp, chassisId](bool critical, const std::string& desc,
                               const std::string& msg) {
            if (critical)
            {
                BMCWEB_LOG_ERROR << desc << ": " << msg;
                messages::resourceNotFound(
                    asyncResp->res, "#Chassis.v1_17_0.Chassis", chassisId);
            }
        },
        chassisId);
}

inline void bootAp(const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& chassisId)
{
    mctp_utils::enumerateMctpEndpoints(
        [req, asyncResp, chassisId](
            const std::shared_ptr<std::vector<mctp_utils::MctpEndpoint>>&
                endpoints) {
            if (!endpoints || endpoints->size() == 0)
            {
                BMCWEB_LOG_ERROR << "Endpoint ID for " << chassisId
                                 << " not found";
                messages::resourceNotFound(
                    asyncResp->res, "#Chassis.v1_17_0.Chassis", chassisId);
                return;
            }
            uint32_t eid =
                static_cast<uint32_t>(endpoints->begin()->getMctpEid());
            MctpVdmUtil mctpVdmUtilWrapper(eid);
            mctpVdmUtilWrapper.run(
                MctpVdmUtilCommand::BOOT_AP, req, asyncResp,
                [chassisId](
                    const crow::Request&,
                    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                    uint32_t, const std::string& stdOut, const std::string&,
                    const boost::system::error_code& ec, int errorCode) {
                    if (ec || errorCode)
                    {
                        messages::resourceErrorsDetectedFormatError(
                            asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                            "command failure");
                        return;
                    }
                    std::string reSuccess = "(.|\n)*RX:( \\d\\d){8} 00(.|\n)*";
                    std::string reFailure = "(.|\n)*RX:( \\d\\d){8} 01(.|\n)*";
                    if (std::regex_match(stdOut, std::regex(reSuccess)))
                    {
                        messages::success(asyncResp->res);
                        return;
                    }
                    if (std::regex_match(stdOut, std::regex(reFailure)))
                    {
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    BMCWEB_LOG_ERROR << "Invalid boot_ap response: " << stdOut;
                    messages::resourceErrorsDetectedFormatError(
                        asyncResp->res, "/redfish/v1/Chassis/" + chassisId,
                        "invalid response");
                });
        },
        [asyncResp, chassisId](bool critical, const std::string& desc,
                               const std::string& msg) {
            if (critical)
            {
                BMCWEB_LOG_ERROR << desc << ": " << msg;
                messages::resourceNotFound(
                    asyncResp->res, "#Chassis.v1_17_0.Chassis", chassisId);
            }
        },
        chassisId);
}

} // namespace manual_boot

} // namespace redfish
