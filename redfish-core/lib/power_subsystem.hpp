#pragma once

<<<<<<< HEAD
#include <app.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/json_utils.hpp>
=======
#include "app.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "utils/chassis_utils.hpp"

#include <memory>
#include <optional>
#include <string>
>>>>>>> origin/master

namespace redfish
{

<<<<<<< HEAD
inline void
    getPowerSubsystem(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const std::string& chassisID)
{
    BMCWEB_LOG_DEBUG
        << "Get properties for PowerSubsystem associated to chassis = "
        << chassisID;

    asyncResp->res.jsonValue = {
        {"@odata.type", "#PowerSubsystem.v1_0_0.PowerSubsystem"},
        {"Name", "Power Subsystem for Chassis"}};
    asyncResp->res.jsonValue["Id"] = "1";
    asyncResp->res.jsonValue["@odata.id"] =
        "/redfish/v1/Chassis/" + chassisID + "/PowerSubsystem";
    asyncResp->res.jsonValue["PowerSupplies"]["@odata.id"] =
        "/redfish/v1/Chassis/" + chassisID + "/PowerSubsystem/PowerSupplies";
=======
inline void doPowerSubsystemCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        BMCWEB_LOG_ERROR << "Not a valid chassis ID" << chassisId;
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }

    asyncResp->res.jsonValue["@odata.type"] =
        "#PowerSubsystem.v1_1_0.PowerSubsystem";
    asyncResp->res.jsonValue["Name"] = "Power Subsystem";
    asyncResp->res.jsonValue["Id"] = "PowerSubsystem";
    asyncResp->res.jsonValue["@odata.id"] = crow::utility::urlFromPieces(
        "redfish", "v1", "Chassis", chassisId, "PowerSubsystem");
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
    asyncResp->res.jsonValue["Status"]["Health"] = "OK";

    asyncResp->res.addHeader(
        boost::beast::http::field::link,
        "</redfish/v1/JsonSchemas/PowerSubsystem/PowerSubsystem.json>; rel=describedby");
}

inline void handlePowerSubsystemCollectionGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        std::bind_front(doPowerSubsystemCollection, asyncResp, chassisId));
>>>>>>> origin/master
}

inline void requestRoutesPowerSubsystem(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/PowerSubsystem/")
<<<<<<< HEAD
        .privileges(redfish::privileges::getPower)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID) {
                auto getChassisID =
                    [asyncResp, chassisID](
                        const std::optional<std::string>& validChassisID) {
                        if (!validChassisID)
                        {
                            BMCWEB_LOG_ERROR << "Not a valid chassis ID:"
                                             << chassisID;
                            messages::resourceNotFound(asyncResp->res,
                                                       "Chassis", chassisID);
                            return;
                        }

                        getPowerSubsystem(asyncResp, chassisID);
                    };
                redfish::chassis_utils::getValidChassisID(
                    asyncResp, chassisID, std::move(getChassisID));
            });
=======
        .privileges(redfish::privileges::getPowerSubsystem)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handlePowerSubsystemCollectionGet, std::ref(app)));
>>>>>>> origin/master
}

} // namespace redfish
