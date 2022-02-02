#pragma once

#include "sensors.hpp"

namespace redfish
{

inline void
    processSensorsValue(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::vector<std::string>& sensorPaths,
                        const std::string& chassisId,
                        const ManagedObjectsVectorType& managedObjectsResp,
                        const std::string& metricsType)
{
    // Get sensor reading from managed object
    for (const std::string& sensorPath : sensorPaths)
    {
        auto sensorElem = std::find_if(
            managedObjectsResp.begin(), managedObjectsResp.end(),
            [sensorPath](const std::pair<
                         sdbusplus::message::object_path,
                         boost::container::flat_map<
                             std::string,
                             boost::container::flat_map<
                                 std::string, dbus::utility::DbusVariantType>>>&
                             element) { return element.first == sensorPath; });
        if (sensorElem == std::end(managedObjectsResp))
        {
            // Sensor not found continue
            BMCWEB_LOG_DEBUG << "Sensor not found sensorPath" << sensorPath
                             << "\n";
            continue;
        }
        std::vector<std::string> split;
        // Reserve space for
        // /xyz/openbmc_project/sensors/<name>/<subname>
        split.reserve(6);
        boost::algorithm::split(split, sensorPath, boost::is_any_of("/"));
        if (split.size() < 6)
        {
            BMCWEB_LOG_ERROR << "Got path that isn't long enough "
                             << sensorPath;
            continue;
        }
        // These indexes aren't intuitive, as boost::split puts an empty
        // string at the beginning
        const std::string& sensorType = split[4];
        const std::string& sensorName = split[5];
        BMCWEB_LOG_DEBUG << "sensorName " << sensorName << " sensorType "
                         << sensorType;

        if ((metricsType == "thermal") && (sensorType != "temperature"))
        {
            BMCWEB_LOG_DEBUG << "Skip non thermal sensor type:" << sensorType;
            continue;
        }

        // Process sensor reading
        auto interfacesDict = sensorElem->second;
        auto interfaceProperties =
            interfacesDict.find("xyz.openbmc_project.Sensor.Value");
        if (interfaceProperties != interfacesDict.end())
        {
            auto thisValueIt = interfaceProperties->second.find("Value");
            if (thisValueIt != interfaceProperties->second.end())
            {
                const dbus::utility::DbusVariantType& valueVariant =
                    thisValueIt->second;
                const double* doubleValue = std::get_if<double>(&valueVariant);
                double reading;
                if (doubleValue != nullptr)
                {
                    reading = *doubleValue;
                }
                if (metricsType == "thermal")
                {
                    nlohmann::json& resArray =
                        asyncResp->res.jsonValue["TemperatureReadingsCelsius"];
                    resArray.push_back(
                        {{"@odata.id", "/redfish/v1/Chassis/" + chassisId +
                                           "/Sensors/" + sensorName},
                         {"DataSourceUri", "/redfish/v1/Chassis/" + chassisId +
                                               "/Sensors/" + sensorName},
                         {"DeviceName", sensorName},
                         {"Reading", std::to_string(reading)}});
                }
                else
                {
                    nlohmann::json& resArray =
                        asyncResp->res.jsonValue["MetricValues"];
                    resArray.push_back(
                        {{"MetricProperty", "/redfish/v1/Chassis/" + chassisId +
                                                "/Sensors/" + sensorName},
                         {"MetricValue", std::to_string(reading)}});
                }
            }
        }
    }
}

inline void
    processChassisSensors(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const ManagedObjectsVectorType& managedObjectsResp,
                          const std::string& chassisPath,
                          const std::string& metricsType)
{
    auto getAllChassisHandler =
        [asyncResp, chassisPath, managedObjectsResp,
         metricsType](const boost::system::error_code ec,
                      std::variant<std::vector<std::string>>& chassisLinks) {
            std::vector<std::string> chassisPaths;
            if (ec)
            {
                // no chassis links = no failures
            }
            // Add parent chassis to the list
            chassisPaths.emplace_back(chassisPath);
            // Add underneath chassis paths
            std::vector<std::string>* chassisData =
                std::get_if<std::vector<std::string>>(&chassisLinks);
            if (chassisData != nullptr)
            {
                for (const std::string& path : *chassisData)
                {
                    chassisPaths.emplace_back(path);
                }
            }
            // Sort the chassis for sensors paths
            std::sort(chassisPaths.begin(), chassisPaths.end());

            // Process all sensors to all chassis
            for (const std::string& objectPath : chassisPaths)
            {
                // Get the chassis path for respective sensors
                sdbusplus::message::object_path path(objectPath);
                const std::string& chassisId = path.filename();

                auto getAllChassisSensors =
                    [asyncResp, chassisId, managedObjectsResp,
                     metricsType](const boost::system::error_code ec,
                                  const std::variant<std::vector<std::string>>&
                                      variantEndpoints) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR
                                << "getAllChassisSensors DBUS error: " << ec;
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        const std::vector<std::string>* sensorPaths =
                            std::get_if<std::vector<std::string>>(
                                &(variantEndpoints));
                        if (sensorPaths == nullptr)
                        {
                            BMCWEB_LOG_ERROR
                                << "getAllChassisSensors empty sensors list"
                                << "\n";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        // Process sensor readings
                        processSensorsValue(asyncResp, *sensorPaths, chassisId,
                                            managedObjectsResp, metricsType);
                    };
                crow::connections::systemBus->async_method_call(
                    getAllChassisSensors, "xyz.openbmc_project.ObjectMapper",
                    objectPath + "/all_sensors",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");
            }
        };
    // Get all chassis
    crow::connections::systemBus->async_method_call(
        getAllChassisHandler, "xyz.openbmc_project.ObjectMapper",
        chassisPath + "/all_chassis", "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void getServiceRootManagedObjects(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& connection, const std::string& chassisPath,
    const std::string& metricsType)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connection, chassisPath,
         metricsType](const boost::system::error_code ec,
                      ManagedObjectsVectorType& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "getServiceRootManagedObjects for connection:"
                    << connection << " error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            std::sort(resp.begin(), resp.end());
            processChassisSensors(asyncResp, resp, chassisPath, metricsType);
        },
        connection, "/", "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects");
}

inline void getServiceManagedObjects(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& connection, const std::string& chassisPath,
    const std::string& metricsType)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp, connection, chassisPath,
         metricsType](const boost::system::error_code ec,
                      ManagedObjectsVectorType& resp) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG
                    << "GetManagedObjects is not at sensor path for connection:"
                    << connection << "\n";
                // Check managed objects on service root
                getServiceRootManagedObjects(asyncResp, connection, chassisPath,
                                             metricsType);
                return;
            }
            std::sort(resp.begin(), resp.end());
            processChassisSensors(asyncResp, resp, chassisPath, metricsType);
        },
        connection, "/xyz/openbmc_project/sensors",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
}

inline void
    processSensorServices(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisPath,
                          const std::string& metricsType)
{
    // Sensor interface implemented by sensor services
    const std::array<const char*, 1> sensorInterface = {
        "xyz.openbmc_project.Sensor.Value"};

    // Get all sensors on the system
    auto getAllSensors =
        [asyncResp, chassisPath,
         metricsType](const boost::system::error_code ec,
                      const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "processSensorServices: Error in getting DBUS sensors: "
                    << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            if (subtree.empty())
            {
                BMCWEB_LOG_ERROR
                    << "processSensorServices: Empty sensors subtree";
                messages::internalError(asyncResp->res);
                return;
            }
            else
            {
                // Identify unique services for GetManagedObjects
                std::set<std::string> sensorServices;
                for (const auto& [objectPath, serviceMap] : subtree)
                {
                    if (serviceMap.size() < 1)
                    {
                        BMCWEB_LOG_DEBUG
                            << "Got 0 service names for sensorpath:"
                            << objectPath;
                        continue;
                    }
                    sensorServices.insert(serviceMap[0].first);
                }
                // Collect all GetManagedObjects for services
                for (const std::string& connection : sensorServices)
                {
                    getServiceManagedObjects(asyncResp, connection, chassisPath,
                                             metricsType);
                }
            }
        };
    crow::connections::systemBus->async_method_call(
        getAllSensors, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/sensors", 2, sensorInterface);
}

inline void requestRoutesThermalMetrics(App& app)
{
    BMCWEB_ROUTE(app,
                 "/redfish/v1/Chassis/<str>/ThermalSubsystem/ThermalMetrics/")
        .privileges({{"Login"}})
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& param) {
                const std::string& chassisId = param;
                // Identify chassis
                const std::array<const char*, 1> interface = {
                    "xyz.openbmc_project.Inventory.Item.Chassis"};

                auto respHandler = [asyncResp, chassisId](
                                       const boost::system::error_code ec,
                                       const std::vector<std::string>&
                                           chassisPaths) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR
                            << "thermal metrics respHandler DBUS error: " << ec;
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    // Identify the chassis
                    for (const std::string& chassisPath : chassisPaths)
                    {
                        sdbusplus::message::object_path path(chassisPath);
                        const std::string& chassisName = path.filename();
                        if (chassisName.empty())
                        {
                            BMCWEB_LOG_ERROR << "Failed to find '/' in "
                                             << chassisPath << "\n";
                            continue;
                        }
                        if (chassisName != chassisId)
                        {
                            continue;
                        }
                        // Process response
                        asyncResp->res.jsonValue["@odata.type"] =
                            "#ThermalMetrics.v1_0_0.ThermalMetrics";
                        asyncResp->res.jsonValue["@odata.id"] =
                            "/redfish/v1/Chassis/" + chassisId +
                            "/ThermalSubsystem/ThermalMetrics";
                        asyncResp->res.jsonValue["Id"] = "ThermalMetrics";
                        asyncResp->res.jsonValue["Name"] =
                            "Chassis Thermal Metrics";
                        asyncResp->res.jsonValue["TemperatureReadingsCelsius"] =
                            nlohmann::json::array();

                        // Identify sensor services for sensor readings
                        processSensorServices(asyncResp, chassisPath,
                                              "thermal");
                        return;
                    }
                    messages::resourceNotFound(asyncResp->res, "Chassis",
                                               chassisId);
                };
                // Get the Chassis Collection
                crow::connections::systemBus->async_method_call(
                    respHandler, "xyz.openbmc_project.ObjectMapper",
                    "/xyz/openbmc_project/object_mapper",
                    "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
                    "/xyz/openbmc_project/inventory", 0, interface);
            });
}
} // namespace redfish
