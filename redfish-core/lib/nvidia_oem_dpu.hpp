#pragma once

#include "nlohmann/json.hpp"

#include <app.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/privilege_utils.hpp>

namespace redfish
{
namespace bluefield
{
inline void getIsOemNvidiaRshimEnable(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const char* systemdServiceBf = "org.freedesktop.systemd1";
    const char* systemdUnitIntfBf = "org.freedesktop.systemd1.Unit";
    const char* rshimSystemdObjBf =
        "/org/freedesktop/systemd1/unit/rshim_2eservice";
    std::filesystem::path rshimDir = "/dev/rshim0";

    if (!std::filesystem::exists(rshimDir))
    {
        BMCWEB_LOG_DEBUG("No /dev/rshim0. Interface not started");
        asyncResp->res.jsonValue["BmcRShim"]["BmcRShimEnabled"] = false;
        return;
    }

    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, systemdServiceBf, rshimSystemdObjBf,
        systemdUnitIntfBf, "ActiveState",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& rshimActiveState) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error for getIsOemNvidiaRshimEnable");
            messages::internalError(asyncResp->res);
            return;
        }
        asyncResp->res.jsonValue["BmcRShim"]["BmcRShimEnabled"] =
            (rshimActiveState == "active");
    });
}

inline void
    requestOemNvidiaRshim(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const bool& bmcRshimEnabled)
{
    const char* systemdServiceBf = "org.freedesktop.systemd1";
    const char* systemdUnitIntfBf = "org.freedesktop.systemd1.Unit";
    const char* rshimSystemdObjBf =
        "/org/freedesktop/systemd1/unit/rshim_2eservice";
    std::string method = bmcRshimEnabled ? "Start" : "Stop";

    BMCWEB_LOG_DEBUG("requestOemNvidiaRshim: {} rshim interface", method.c_str());

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("DBUS response error for rshim enable/disable");
            messages::internalError(asyncResp->res);
            return;
        }
    },
        systemdServiceBf, rshimSystemdObjBf, systemdUnitIntfBf, method.c_str(),
        "replace");

    messages::success(asyncResp->res);
}
} // namespace bluefield

inline void requestRoutesNvidiaOemBf(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia")
        .privileges(redfish::privileges::getManager)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        bluefield::getIsOemNvidiaRshimEnable(asyncResp);
    });
    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia/")
        .privileges(redfish::privileges::patchManager)
        .methods(boost::beast::http::verb::patch)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        std::optional<nlohmann::json> bmcRshim;
        if (!redfish::json_util::readJsonPatch(req, asyncResp->res, "BmcRShim",
                                               bmcRshim))
        {
            BMCWEB_LOG_ERROR("Illegal Property {}", asyncResp->res.jsonValue.dump( 2, ' ', true, nlohmann::json::error_handler_t::replace));
            return;
        }
        if (bmcRshim)
        {
            std::optional<bool> bmcRshimEnabled;
            if (!redfish::json_util::readJson(*bmcRshim, asyncResp->res,
                                              "BmcRShimEnabled",
                                              bmcRshimEnabled))
            {
                BMCWEB_LOG_ERROR("Illegal Property {}", asyncResp->res.jsonValue.dump( 2, ' ', true, nlohmann::json::error_handler_t::replace));
                return;
            }

            bluefield::requestOemNvidiaRshim(asyncResp, *bmcRshimEnabled);
        }
    });
}

} // namespace redfish
