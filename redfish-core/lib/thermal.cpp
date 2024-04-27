
#include "thermal.hpp"

#include "app.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "sensors_common.hpp"
#include "utils/json_utils.hpp"

namespace redfish
{

void requestRoutesThermal(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Thermal/")
        .privileges(redfish::privileges::getThermal)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& chassisName) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }

        auto sensorAsyncResp = std::make_shared<SensorsAsyncResp>(
            asyncResp, chassisName, sensors::dbus::thermalPaths,
            sensors::node::thermal);

        // TODO Need to get Chassis Redundancy information.
        getChassisData(sensorAsyncResp);
    });

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Thermal/")
        .privileges(redfish::privileges::patchThermal)
        .methods(boost::beast::http::verb::patch)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& chassisName) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }

        std::optional<std::vector<nlohmann::json::object_t>>
            temperatureCollections;
        std::optional<std::vector<nlohmann::json::object_t>> fanCollections;
        std::unordered_map<std::string, std::vector<nlohmann::json::object_t>>
            allCollections;

        auto sensorsAsyncResp = std::make_shared<SensorsAsyncResp>(
            asyncResp, chassisName, sensors::dbus::thermalPaths,
            sensors::node::thermal);

        if (!json_util::readJsonPatch(req, sensorsAsyncResp->asyncResp->res,
                                      "Temperatures", temperatureCollections,
                                      "Fans", fanCollections))
        {
            return;
        }
        if (!temperatureCollections && !fanCollections)
        {
            messages::resourceNotFound(sensorsAsyncResp->asyncResp->res,
                                       "Thermal", "Temperatures / Voltages");
            return;
        }
        if (temperatureCollections)
        {
            allCollections.emplace("Temperatures",
                                   *std::move(temperatureCollections));
        }
        if (fanCollections)
        {
            allCollections.emplace("Fans", *std::move(fanCollections));
        }
        setSensorsOverride(sensorsAsyncResp, allCollections);
    });
}

} // namespace redfish
