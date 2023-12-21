#!/usr/bin/python3
import os
import re

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
REDFISH_FILES_DIR = os.path.realpath(
    os.path.join(SCRIPT_DIR, "..", "redfish-core", "lib")
)


def main():
    cpp_files = []
    for file in os.listdir(REDFISH_FILES_DIR):
        filepath = os.path.join(REDFISH_FILES_DIR, file)
        if not file.endswith(".hpp"):
            continue
        with open(filepath, "r") as myfile:
            contents = myfile.read()

        path, ext = os.path.splitext(filepath)

        contents = contents.replace("inline ", "")
        contents = contents.replace(
            "#pragma once\n",
            '#include "{}"\n'.format(os.path.basename(filepath)),
        )
        contents = re.sub(
            R"\/\*\n\/\/ Copy[\w\(\s\)\/,\.\";\:\-\n]+\*\/", "", contents
        )
        m = re.finditer(R"(request\w*Routes\w*)\(", contents)

        m = [x for x in m]
        if len(m) == 0:
            continue
        cpp_file = path + ".cpp"
        cpp_files.append(cpp_file)
        with open(cpp_file, "w") as cpp_file:
            cpp_file.write(contents)

        with open(filepath, "w") as hpp_file:
            hpp_file.write("#pragma once\n" '#include "app.hpp"\n')
            if filepath.endswith("task.hpp"):
                hpp_file.write("#include <sdbusplus/bus/match.hpp>\n")

            hpp_file.write("namespace redfish\n" "{\n")
            if filepath.endswith("sensors.hpp"):
                hpp_file.write(sensors_string)
            if filepath.endswith("metric_report.hpp"):
                hpp_file.write(metric_report_string)
            if filepath.endswith("hypervisor_system.hpp"):
                hpp_file.write(hypervisor_system_string)
            if filepath.endswith("ethernet.hpp"):
                hpp_file.write(ethernet_string)
            if filepath.endswith("task.hpp"):
                hpp_file.write(task_string)
            if filepath.endswith("thermal_subystem.hpp"):
                hpp_file.write(thermal_subsystem_string)
            if filepath.endswith("service_root.hpp"):
                hpp_file.write(service_root_string)
            for function in m:
                hpp_file.write("    void " + function.group(1) + "(App&);\n")
            hpp_file.write("}\n")

    print("add this to meson")
    for cpp_file in cpp_files:
        cpp_file = "redfish-core/lib/" + os.path.basename(cpp_file)
        print("  '{}',".format(cpp_file))


# everything below this line is modules that didn't conform to the mechanism of putting common dependencies in a shared file
# in theory each one needs broken out into its own header, but in terms of automation, doing this makes things easier to accomplish that.


service_root_string = """
void handleServiceRootGetImpl(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);
    
"""

sensors_string = """
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
}

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
 void
    getChassisData(const std::shared_ptr<SensorsAsyncResp>& sensorsAsyncResp);

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
void retrieveUriToDbusMap(const std::string& chassis,
                                 const std::string& node,
                                 std::function<void(const boost::beast::http::status,
                     const std::map<std::string, std::string>&)>&& mapComplete);

"""

metric_report_string = """
namespace telemetry
{

using Readings = std::vector<std::tuple<std::string, double, uint64_t>>;
using TimestampReadings = std::tuple<uint64_t, Readings>;

bool fillReport(nlohmann::json& json, const std::string& id,
                       const TimestampReadings& timestampReadings);

}
"""


hypervisor_system_string = """
   void handleHypervisorSystemGet(
      const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);


 void handleHypervisorResetActionGet(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);
 """


ethernet_string = """
enum class LinkType
{
    Local,
    Global
};

/**
 * Structure for keeping IPv4 data required by Redfish
 */
struct IPv4AddressData
{
    std::string id;
    std::string address;
    std::string domain;
    std::string gateway;
    std::string netmask;
    std::string origin;
    LinkType linktype{};
    bool isActive{};
};

/**
 * Structure for keeping IPv6 data required by Redfish
 */
struct IPv6AddressData
{
    std::string id;
    std::string address;
    std::string origin;
    uint8_t prefixLength = 0;
};

/**
 * Structure for keeping basic single Ethernet Interface information
 * available from DBus
 */
struct EthernetInterfaceData
{
    uint32_t speed;
    size_t mtuSize;
    bool autoNeg;
    bool dnsEnabled;
    bool ntpEnabled;
    bool hostNameEnabled;
    bool linkUp;
    bool nicEnabled;
    bool ipv6AcceptRa;
    std::string dhcpEnabled;
    std::string operatingMode;
    std::string hostName;
    std::string defaultGateway;
    std::string ipv6DefaultGateway;
    std::string macAddress;
    std::optional<uint32_t> vlanId;
    std::vector<std::string> nameServers;
    std::vector<std::string> staticNameServers;
    std::vector<std::string> domainnames;
};

// Helper function that changes bits netmask notation (i.e. /24)
// into full dot notation
std::string getNetmask(unsigned int bits);



bool translateDhcpEnabledToBool(const std::string& inputDHCP,
                                       bool isIPv4);

std::string
    translateAddressOriginDbusToRedfish(const std::string& inputOrigin,
                                        bool isIPv4);

std::string getDhcpEnabledEnumeration(bool isIPv4, bool isIPv6);
bool isHostnameValid(const std::string& hostname);

"""

task_string = """
namespace task {

constexpr bool completed = true;

struct Payload
{
    Payload(const crow::Request& req);

    std::string targetUri;
    std::string httpOperation;
    nlohmann::json httpHeaders;
    nlohmann::json jsonBody;
};


struct TaskData : std::enable_shared_from_this<TaskData>
{
  private:
    explicit TaskData(
        std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                           const std::shared_ptr<TaskData>&)>&& handler,
        const std::string& matchIn, size_t idx);

  public:
    TaskData() = delete;

    static std::shared_ptr<TaskData>& createTask(
        std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                           const std::shared_ptr<TaskData>&)>&& handler,
        const std::string& match);

    void populateResp(crow::Response& res, size_t retryAfterSeconds = 30);

    void finishTask();

    void extendTimer(const std::chrono::seconds& timeout);

    static void sendTaskEvent(std::string_view state, size_t index);

    void startTimer(const std::chrono::seconds& timeout);

    std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                       const std::shared_ptr<TaskData>&)>
        callback;
    std::string matchStr;
    size_t index;
    time_t startTime;
    std::string status;
    std::string state;
    nlohmann::json messages;
    boost::asio::steady_timer timer;
    std::unique_ptr<sdbusplus::bus::match_t> match;
    std::optional<time_t> endTime;
    std::optional<Payload> payload;
    bool gave204 = false;
    int percentComplete = 0;
};
}
"""

thermal_subsystem_string = """
void doThermalSubsystemCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath);

"""

if __name__ == "__main__":
    main()
