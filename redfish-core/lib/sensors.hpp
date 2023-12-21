#pragma once
#include "app.hpp"
namespace redfish
{

namespace sensors
{
namespace node
{
static constexpr std::string_view power = "Power";
static constexpr std::string_view sensors = "Sensors";
static constexpr std::string_view thermal = "Thermal";
} // namespace node

// clang-format off
namespace dbus
{
constexpr auto powerPaths = std::to_array<std::string_view>({
    "/xyz/openbmc_project/sensors/voltage",
    "/xyz/openbmc_project/sensors/power"
});

constexpr auto sensorPaths = std::to_array<std::string_view>({
    "/xyz/openbmc_project/sensors/power",
    "/xyz/openbmc_project/sensors/current",
    "/xyz/openbmc_project/sensors/airflow",
    "/xyz/openbmc_project/sensors/humidity",
#ifdef BMCWEB_NEW_POWERSUBSYSTEM_THERMALSUBSYSTEM
    "/xyz/openbmc_project/sensors/voltage",
    "/xyz/openbmc_project/sensors/fan_tach",
    "/xyz/openbmc_project/sensors/temperature",
    "/xyz/openbmc_project/sensors/fan_pwm",
    "/xyz/openbmc_project/sensors/altitude",
    "/xyz/openbmc_project/sensors/energy",
#endif
    "/xyz/openbmc_project/sensors/utilization"
});

constexpr auto thermalPaths = std::to_array<std::string_view>({
    "/xyz/openbmc_project/sensors/fan_tach",
    "/xyz/openbmc_project/sensors/temperature",
    "/xyz/openbmc_project/sensors/fan_pwm"
});

} // namespace dbus
// clang-format on

using sensorPair =
    std::pair<std::string_view, std::span<const std::string_view>>;
static constexpr std::array<sensorPair, 3> paths = {
    {{node::power, dbus::powerPaths},
     {node::sensors, dbus::sensorPaths},
     {node::thermal, dbus::thermalPaths}}};
} // namespace sensors

/**
 * SensorsAsyncResp
 * Gathers data needed for response processing after async calls are done
 */
class SensorsAsyncResp
{
  public:
    using DataCompleteCb = std::function<void(
        const boost::beast::http::status status,
        const std::map<std::string, std::string>& uriToDbus)>;

    struct SensorData
    {
        const std::string name;
        std::string uri;
        const std::string dbusPath;
    };

    SensorsAsyncResp(const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn,
                     const std::string& chassisIdIn,
                     std::span<const std::string_view> typesIn,
                     std::string_view subNode) :
        asyncResp(asyncRespIn),
        chassisId(chassisIdIn), types(typesIn), chassisSubNode(subNode),
        efficientExpand(false)
    {}

    // Store extra data about sensor mapping and return it in callback
    SensorsAsyncResp(const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn,
                     const std::string& chassisIdIn,
                     std::span<const std::string_view> typesIn,
                     std::string_view subNode,
                     DataCompleteCb&& creationComplete) :
        asyncResp(asyncRespIn),
        chassisId(chassisIdIn), types(typesIn), chassisSubNode(subNode),
        efficientExpand(false), metadata{std::vector<SensorData>()},
        dataComplete{std::move(creationComplete)}
    {}

    // sensor collections expand
    SensorsAsyncResp(const std::shared_ptr<bmcweb::AsyncResp>& asyncRespIn,
                     const std::string& chassisIdIn,
                     std::span<const std::string_view> typesIn,
                     const std::string_view& subNode, bool efficientExpandIn) :
        asyncResp(asyncRespIn),
        chassisId(chassisIdIn), types(typesIn), chassisSubNode(subNode),
        efficientExpand(efficientExpandIn)
    {}
    ~SensorsAsyncResp()
    {
        if (asyncResp->res.result() ==
            boost::beast::http::status::internal_server_error)
        {
            // Reset the json object to clear out any data that made it in
            // before the error happened todo(ed) handle error condition with
            // proper code
            asyncResp->res.jsonValue = nlohmann::json::object();
        }

        if (dataComplete && metadata)
        {
            std::map<std::string, std::string> map;
            if (asyncResp->res.result() == boost::beast::http::status::ok)
            {
                for (auto& sensor : *metadata)
                {
                    map.emplace(sensor.uri, sensor.dbusPath);
                }
            }
            dataComplete(asyncResp->res.result(), map);
        }
    }

    SensorsAsyncResp(const SensorsAsyncResp&) = delete;
    SensorsAsyncResp(SensorsAsyncResp&&) = delete;
    SensorsAsyncResp& operator=(const SensorsAsyncResp&) = delete;
    SensorsAsyncResp& operator=(SensorsAsyncResp&&) = delete;

    void addMetadata(const nlohmann::json& sensorObject,
                     const std::string& dbusPath)
    {
        if (metadata)
        {
            metadata->emplace_back(SensorData{
                sensorObject["Name"], sensorObject["@odata.id"], dbusPath});
        }
    }

    void updateUri(const std::string& name, const std::string& uri)
    {
        if (metadata)
        {
            for (auto& sensor : *metadata)
            {
                if (sensor.name == name)
                {
                    sensor.uri = uri;
                }
            }
        }
    }

    const std::shared_ptr<bmcweb::AsyncResp> asyncResp;
    const std::string chassisId;
    const std::span<const std::string_view> types;
    const std::string chassisSubNode;
    const bool efficientExpand;

  private:
    std::optional<std::vector<SensorData>> metadata;
    DataCompleteCb dataComplete;
};

/**
 * @brief Entry point for retrieving sensors data related to requested
 *        chassis.
 * @param SensorsAsyncResp   Pointer to object holding response data
 */
void getChassisData(const std::shared_ptr<SensorsAsyncResp>& sensorsAsyncResp);

/**
 * @brief Entry point for overriding sensor values of given sensor
 *
 * @param sensorAsyncResp   response object
 * @param allCollections   Collections extract from sensors' request patch info
 * @param chassisSubNode   Chassis Node for which the query has to happen
 */
void setSensorsOverride(
    const std::shared_ptr<SensorsAsyncResp>& sensorAsyncResp,
    std::unordered_map<std::string, std::vector<nlohmann::json>>&
        allCollections);

std::pair<std::string, std::string>
    splitSensorNameAndType(std::string_view sensorId);

/**
 * @brief Retrieves mapping of Redfish URIs to sensor value property to D-Bus
 * path of the sensor.
 *
 * Function builds valid Redfish response for sensor query of given chassis and
 * node. It then builds metadata about Redfish<->D-Bus correlations and provides
 * it to caller in a callback.
 *
 * @param chassis   Chassis for which retrieval should be performed
 * @param node  Node (group) of sensors. See sensors::node for supported values
 * @param mapComplete   Callback to be called with retrieval result
 */
void retrieveUriToDbusMap(
    const std::string& chassis, const std::string& node,
    std::function<void(const boost::beast::http::status,
                       const std::map<std::string, std::string>&)>&&
        mapComplete);

void requestRoutesSensorCollection(App&);
void requestRoutesSensor(App&);
} // namespace redfish
