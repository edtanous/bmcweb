/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "app.hpp"
#include "dbus_singleton.hpp"
#include "dbus_utility.hpp"
#include "error_messages.hpp"
#include "generated/enums/resource.hpp"
#include "generated/enums/sensor.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "sensors_common.hpp"
#include "str_utility.hpp"
#include "utils/dbus_utils.hpp"
#include "utils/json_utils.hpp"
#include "utils/query_param.hpp"

#include <boost/system/error_code.hpp>
#include <boost/url/format.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/unpack_properties.hpp>

#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace redfish
{

namespace sensors
{

inline void getChassisCallback(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    std::string_view chassisId, std::string_view chassisSubNode,
    const std::shared_ptr<std::set<std::string>>& sensorNames)
{
    BMCWEB_LOG_DEBUG("getChassisCallback enter ");

    nlohmann::json& entriesArray = asyncResp->res.jsonValue["Members"];
    for (const std::string& sensor : *sensorNames)
    {
        BMCWEB_LOG_DEBUG("Adding sensor: {}", sensor);

        sdbusplus::message::object_path path(sensor);
        std::string sensorName = path.filename();
        if (sensorName.empty())
        {
            BMCWEB_LOG_ERROR("Invalid sensor path: {}", sensor);
            messages::internalError(asyncResp->res);
            return;
        }
        std::string type = path.parent_path().filename();
        // fan_tach has an underscore in it, so remove it to "normalize" the
        // type in the URI
        auto remove = std::ranges::remove(type, '_');
        type.erase(std::ranges::begin(remove), type.end());

        nlohmann::json::object_t member;
        std::string id = type;
        id += "_";
        id += sensorName;
        member["@odata.id"] = boost::urls::format(
            "/redfish/v1/Chassis/{}/{}/{}", chassisId, chassisSubNode, id);

        entriesArray.emplace_back(std::move(member));
    }

    asyncResp->res.jsonValue["Members@odata.count"] = entriesArray.size();
    BMCWEB_LOG_DEBUG("getChassisCallback exit");
}

inline void handleSensorCollectionGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    query_param::QueryCapabilities capabilities = {
        .canDelegateExpandLevel = 1,
    };
    query_param::Query delegatedQuery;
    if (!redfish::setUpRedfishRouteWithDelegation(app, req, asyncResp,
                                                  delegatedQuery, capabilities))
    {
        return;
    }

    if (delegatedQuery.expandType != query_param::ExpandType::None)
    {
        // we perform efficient expand.
        auto sensorsAsyncResp = std::make_shared<SensorsAsyncResp>(
            asyncResp, chassisId, sensors::dbus::sensorPaths,
            sensors::node::sensors,
            /*efficientExpand=*/true);
        getChassisData(sensorsAsyncResp);

        BMCWEB_LOG_DEBUG(
            "SensorCollection doGet exit via efficient expand handler");
        return;
    }

    // We get all sensors as hyperlinkes in the chassis (this
    // implies we reply on the default query parameters handler)
    getChassis(asyncResp, chassisId, sensors::node::sensors, dbus::sensorPaths,
               std::bind_front(sensors::getChassisCallback, asyncResp,
                               chassisId, sensors::node::sensors));
}

inline void
    getSensorFromDbus(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const std::string& sensorPath,
                      const ::dbus::utility::MapperGetObject& mapperResponse)
{
    if (mapperResponse.size() != 1)
    {
        messages::internalError(asyncResp->res);
        return;
    }
    const auto& valueIface = *mapperResponse.begin();
    const std::string& connectionName = valueIface.first;
    BMCWEB_LOG_DEBUG("Looking up {}", connectionName);
    BMCWEB_LOG_DEBUG("Path {}", sensorPath);

    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, connectionName, sensorPath, "",
        [asyncResp,
         sensorPath](const boost::system::error_code& ec,
                     const ::dbus::utility::DBusPropertiesMap& valuesDict) {
        if (ec)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        sdbusplus::message::object_path path(sensorPath);
        std::string name = path.filename();
        path = path.parent_path();
        std::string type = path.filename();
        objectPropertiesToJson(name, type, sensors::node::sensors, valuesDict,
                               asyncResp->res.jsonValue, nullptr);
    });
}

inline void handleSensorGet(App& app, const crow::Request& req,
                            const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& chassisId,
                            const std::string& sensorId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }
    std::pair<std::string, std::string> nameType =
        splitSensorNameAndType(sensorId);
    if (nameType.first.empty() || nameType.second.empty())
    {
        messages::resourceNotFound(asyncResp->res, sensorId, "Sensor");
        return;
    }

    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format(
        "/redfish/v1/Chassis/{}/Sensors/{}", chassisId, sensorId);

    BMCWEB_LOG_DEBUG("Sensor doGet enter");

    constexpr std::array<std::string_view, 1> interfaces = {
        "xyz.openbmc_project.Sensor.Value"};
    std::string sensorPath = "/xyz/openbmc_project/sensors/" + nameType.first +
                             '/' + nameType.second;
    // Get a list of all of the sensors that implement Sensor.Value
    // and get the path and service name associated with the sensor
    ::dbus::utility::getDbusObject(
        sensorPath, interfaces,
        [asyncResp, sensorId,
         sensorPath](const boost::system::error_code& ec,
                     const ::dbus::utility::MapperGetObject& subtree) {
        BMCWEB_LOG_DEBUG("respHandler1 enter");
        if (ec == boost::system::errc::io_error)
        {
            BMCWEB_LOG_WARNING("Sensor not found from getSensorPaths");
            messages::resourceNotFound(asyncResp->res, sensorId, "Sensor");
            return;
        }
        if (ec)
        {
            messages::internalError(asyncResp->res);
            BMCWEB_LOG_ERROR(
                "Sensor getSensorPaths resp_handler: Dbus error {}", ec);
            return;
        }
        getSensorFromDbus(asyncResp, sensorPath, subtree);
        BMCWEB_LOG_DEBUG("respHandler1 exit");
    });
}

} // namespace sensors

inline void requestRoutesSensorCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Sensors/")
        .privileges(redfish::privileges::getSensorCollection)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(sensors::handleSensorCollectionGet, std::ref(app)));
}

inline void requestRoutesSensor(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Sensors/<str>/")
        .privileges(redfish::privileges::getSensor)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(sensors::handleSensorGet, std::ref(app)));
}

} // namespace redfish
