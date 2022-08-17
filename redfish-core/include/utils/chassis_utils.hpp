#pragma once

namespace redfish
{

namespace chassis_utils
{

constexpr const char* acceleratorInvIntf =
    "xyz.openbmc_project.Inventory.Item.Accelerator";

constexpr const char* switchInvIntf =
    "xyz.openbmc_project.Inventory.Item.Switch";

constexpr const char* bmcInvInterf =
    "xyz.openbmc_project.Inventory.Item.BMC";

/**
 * @brief Retrieves valid chassis ID
 * @param asyncResp   Pointer to object holding response data
 * @param callback  Callback for next step to get valid chassis ID
 */
template <typename Callback>
void getValidChassisID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& chassisID, Callback&& callback)
{
    BMCWEB_LOG_DEBUG << "checkChassisId enter";
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};

    auto respHandler = [callback{std::forward<Callback>(callback)}, asyncResp,
                        chassisID](
                           const boost::system::error_code ec,
                           const std::vector<std::string>& chassisPaths) {
        BMCWEB_LOG_DEBUG << "getValidChassisID respHandler enter";
        if (ec)
        {
            BMCWEB_LOG_ERROR << "getValidChassisID respHandler DBUS error: "
                             << ec;
            messages::internalError(asyncResp->res);
            return;
        }

        std::optional<std::string> validChassisID;
        std::string chassisName;
        for (const std::string& chassis : chassisPaths)
        {
            sdbusplus::message::object_path path(chassis);
            chassisName = path.filename();
            if (chassisName.empty())
            {
                BMCWEB_LOG_ERROR << "Failed to find chassisName in " << chassis;
                continue;
            }
            if (chassisName == chassisID)
            {
                validChassisID = chassisID;
                break;
            }
        }
        callback(validChassisID);
    };

    // Get the Chassis Collection
    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0, interfaces);
    BMCWEB_LOG_DEBUG << "checkChassisId exit";
}
template <typename Callback>
void getValidChassisPath(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& chassisID, Callback&& callback)
{
    BMCWEB_LOG_DEBUG << "checkChassisId enter";
    const std::array<const char*, 2> interfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis"};

    auto respHandler =
        [callback{std::forward<Callback>(callback)}, asyncResp,
         chassisID](const boost::system::error_code ec,
                    const std::vector<std::string>& chassisPaths) {
            BMCWEB_LOG_DEBUG << "getValidChassisPath respHandler enter";
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "getValidChassisPath respHandler DBUS error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }

            std::optional<std::string> chassisPath;
            std::string chassisName;
            for (const std::string& chassis : chassisPaths)
            {
                sdbusplus::message::object_path path(chassis);
                chassisName = path.filename();
                if (chassisName.empty())
                {
                    BMCWEB_LOG_ERROR << "Empty chassis Path";
                    continue;
                }
                if (chassisName == chassisID)
                {
                    chassisPath = chassis;
                    break;
                }
            }
            callback(chassisPath);
        };

    // Get the Chassis Collection
    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

inline std::string getChassisType(const std::string& chassisType)
{
    if (chassisType ==
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component")
    {
        return "Component";
    }
    if (chassisType ==
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Enclosure")
    {
        return "Enclosure";
    }
    if (chassisType ==
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module")
    {
        return "Module";
    }
    if (chassisType ==
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.RackMount")
    {
        return "RackMount";
    }
    if (chassisType ==
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.StandAlone")
    {
        return "StandAlone";
    }
    if (chassisType ==
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Zone")
    {
        return "Zone";
    }
    // Unknown or others
    return "";
}

/**
 * @brief Fill out links association to parent chassis by
 * requesting data from the given D-Bus association object.
 *
 * @param[in,out]   aResp       Async HTTP response.
 * @param[in]       objPath     D-Bus object to query.
 */
inline void
    getChassisLinksContainedBy(const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                               const std::string& objPath)
{
    BMCWEB_LOG_DEBUG << "Get parent chassis link";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec2,
                std::variant<std::vector<std::string>>& resp) {
            if (ec2)
            {
                return; // no chassis = no failures
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr || data->size() > 1)
            {
                // There must be single parent chassis
                return;
            }
            const std::string& chassisPath = data->front();
            sdbusplus::message::object_path objectPath(chassisPath);
            std::string chassisName = objectPath.filename();
            if (chassisName.empty())
            {
                messages::internalError(aResp->res);
                return;
            }
            aResp->res.jsonValue["Links"]["ContainedBy"] = {
                {"@odata.id", "/redfish/v1/Chassis/" + chassisName}};
        },
        "xyz.openbmc_project.ObjectMapper", objPath + "/parent_chassis",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Association", "endpoints");
}

inline void
    getChassisLocationType(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.Location", "LocationType",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& property) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for Location";
                messages::internalError(asyncResp->res);
                return;
            }

            asyncResp->res
                .jsonValue["Location"]["PartLocation"]["LocationType"] =
                redfish::dbus_utils::toLocationType(property);
        });
}

#ifdef BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES
/**
 * @brief Translates PowerMode DBUS property value to redfish.
 *
 * @param[in] dbusAction    The powerMode action in D-BUS.
 *
 * @return Returns as a string, the powermode in Redfish terms. If
 * translation cannot be done, returns an empty string.
 */
inline std::string getPowerModeType(const std::string& dbusAction)
{
    if (dbusAction == "xyz.openbmc_project.Control.Power.Mode.PowerMode.MaximumPerformance")
    {
        return "MaxP";
    }
    if (dbusAction == "xyz.openbmc_project.Control.Power.Mode.PowerMode.PowerSaving")
    {
        return "MaxQ";
    }
    if (dbusAction == "xyz.openbmc_project.Control.Power.Mode.PowerMode.OEM")
    {
        return "Custom";
    }
    
    return "";
}

/**
 *@brief Translates PowerMode from Redfish to DBUS property value.
 *
 *@param[in] powerMode The powerMode value(MaxP, MaxQ, Custom) in Redfish.
 *
 *@return Returns as a string as expected by dbus.
 *If translation cannot be done, returns an empty string.
 */

inline std::string convertToPowerModeType(const std::string& powerMode)
{
    if (powerMode == "MaxP")
    {
        return "xyz.openbmc_project.Control.Power.Mode.PowerMode.MaximumPerformance";
    }
    if (powerMode == "MaxQ")
    {
        return "xyz.openbmc_project.Control.Power.Mode.PowerMode.PowerSaving";
    }
    if (powerMode == "Custom")
    {
        return "xyz.openbmc_project.Control.Power.Mode.PowerMode.OEM";
    }

    return "";
}
#endif  //BMCWEB_ENABLE_NVIDIA_OEM_PROPERTIES

inline void getChassisUUID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Common.UUID", "UUID",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& chassisUUID) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for UUID";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["UUID"] = chassisUUID;
        });
}

inline void getChassisName(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Item", "PrettyName",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& chassisName) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for chassis name";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["Name"] = chassisName;
        });
}

inline void getChassisType(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Item.Chassis", "Type",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& chassisType) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for UUID";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["ChassisType"] =
                getChassisType(chassisType);
        });
}

inline void
    getChassisManufacturer(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.Asset", "Manufacturer",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& manufacturer) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for Manufacturer";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["Manufacturer"] = manufacturer;
        });
}

inline void
    getChassisSKU(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.Asset", "SKU",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& chassisSKU) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for chassisSKU";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["SKU"] = chassisSKU;
        });
}

inline void
    getChassisSerialNumber(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, connectionName, path,
        "xyz.openbmc_project.Inventory.Decorator.Asset", "SerialNumber",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& serialNumber) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error for SerialNumber";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["SerialNumber"] = serialNumber;
        });
}

template <typename CallbackFunc>
inline void isEROTChassis(const std::string& chassisID, CallbackFunc&& callback)
{
    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.Inventory.Item.SPDMResponder"};

    crow::connections::systemBus->async_method_call(
        [chassisID,
         callback](const boost::system::error_code ec,
                   const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                callback(false);
                return;
            }
            const auto objIt = std::find_if(
                subtree.begin(), subtree.end(),
                [chassisID](
                    const std::pair<
                        std::string,
                        std::vector<std::pair<
                            std::string, std::vector<std::string>>>>& object) {
                    return !chassisID.compare(
                        sdbusplus::message::object_path(object.first)
                            .filename());
                });

            if (objIt == subtree.end())
            {
                BMCWEB_LOG_DEBUG << "Dbus Object not found:" << chassisID;
                callback(false);
                return;
            }
            callback(true);
            return;
        },

        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

template <typename CallbackFunc>
inline void getAssociationEndpoint(const std::string& objPath,
                                   CallbackFunc&& callback)
{
    crow::connections::systemBus->async_method_call(
        [callback, objPath](const boost::system::error_code ec,
                            std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "D-Bus responses error: " << ec
                    << " (busctl call " << dbus_utils::mapperBusName
                    << " " << objPath << " " << dbus_utils::propertyInterface
                    << " Get ss " << dbus_utils::associationInterface
                    << " endpoints)";
                callback(false, std::string(""));
                return; // should have associated inventory object.
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr || data->size() == 0)
            {
                BMCWEB_LOG_ERROR << ((data == nullptr) ?
                        "Data is null. " : "Data is empty")
                    << "(busctl call " << dbus_utils::mapperBusName
                    << " " << objPath << " " << dbus_utils::propertyInterface
                    << " Get ss " <<dbus_utils::associationInterface
                    << " endpoints)";
/*
                Object must have associated inventory object.
                Exemplary test on hardware:
                    busctl call xyz.openbmc_project.ObjectMapper \
                    /xyz/openbmc_project/inventory/system/chassis/HGX_ERoT_FPGA_0/inventory \
                    org.freedesktop.DBus.Properties \
                    Get ss xyz.openbmc_project.Association endpoints
                Response: v as 1 "/xyz/openbmc_project/inventory/system/processors/FPGA_0"
*/
                callback(false, std::string(""));
                return;
            }
            // Getting only the first endpoint as we have 1*1 relationship
            // with ERoT and the inventory backed by it.
            std::string endpointPath = data->front();
            callback(true, endpointPath);
        },
        dbus_utils::mapperBusName, objPath, dbus_utils::propertyInterface,
        "Get", dbus_utils::associationInterface, "endpoints");
}

template <typename CallbackFunc>
inline void getRedfishURL(const std::filesystem::path& invObjPath,
                          CallbackFunc&& callback)
{
    BMCWEB_LOG_DEBUG << "getRedfishURL(" << invObjPath <<")";
    crow::connections::systemBus->async_method_call(
        [invObjPath, callback](const boost::system::error_code ec,
                               const GetObjectType& resp) {
            std::string url;
            if (ec || resp.empty())
            {
                BMCWEB_LOG_ERROR
                    << "DBUS response error during getting of service name: "
                    << ec;
                callback(false, url);
                return;
            }
            // if accelerator interface then the object would be
            // of type fpga or GPU.
            // If switch interface then it could be Nvswitch or
            // PcieSwitch else it is BMC
            for (const auto& serObj: resp)
            {
                std::string service = serObj.first;
                auto interfaces = serObj.second;

                for (const auto& interface : interfaces)
                {
                    if (interface == acceleratorInvIntf)
                    {
/*
busctl call xyz.openbmc_project.ObjectMapper /xyz/openbmc_project/object_mapper xyz.openbmc_project.ObjectMapper GetObject sas /xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1 0
 a{sas} 2
 - "xyz.openbmc_project.GpuMgr" ...
 - "xyz.openbmc_project.ObjectMapper" ...
busctl call xyz.openbmc_project.ObjectMapper /xyz/openbmc_project/object_mapper xyz.openbmc_project.ObjectMapper GetObject sas /xyz/openbmc_project/inventory/system/chassis/HGX_FPGA_0 0
  a{sas} 2
   - "xyz.openbmc_project.GpuMgr" ...
   - "xyz.openbmc_project.ObjectMapper" ...
*/
                        url = std::string("/redfish/v1/Systems/" PLATFORMSYSTEMID "/Processors/")
                            + invObjPath.filename().string();
                        BMCWEB_LOG_DEBUG << service << " " << interface
                            << " => URL: " << url;
                        callback(true, url);
                        return;
                    }

                    if (interface == switchInvIntf)
                    {
/* busctl call xyz.openbmc_project.ObjectMapper /xyz/openbmc_project/object_mapper xyz.openbmc_project.ObjectMapper GetObject sas /xyz/openbmc_project/inventory/system/chassis/HGX_NVSwitch_0 0
a{sas} 2
 - "xyz.openbmc_project.GpuMgr" ...
 - "xyz.openbmc_project.ObjectMapper" ...
*/
                        // This is NVSwitch or PCIeSwitch
                        std::string switchID = invObjPath.filename();
                        // Now get the fabric ID
                        BMCWEB_LOG_DEBUG << "DBUS resp: " << service << " "
                            << interface << " => getAssociationEndpoint("
                            << invObjPath.string() << "/fabrics, CALLBACK)";
                        getAssociationEndpoint(
                            invObjPath.string() + "/fabrics",
                            [switchID, callback](const bool& status,
                                            const std::string& ep) {
                                std::string url;
                                if (!status)
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "Unable to get the association endpoint";

                                    callback(false, url);
                                    return;
                                }
                                sdbusplus::message::object_path invObjectPath(ep);
                                const std::string& fabricID = invObjectPath.filename();

                                url = std::string("/redfish/v1/Fabrics/");
                                url += fabricID;
                                url += "/Switches/";
                                url += switchID;

                                callback(true, url);
                                return;
                            });
                        return;
                    }
                    if (interface == bmcInvInterf)
                    {
                        url = std::string("/redfish/v1/Managers/" PLATFORMBMCID);
                        BMCWEB_LOG_DEBUG << service << " " << interface
                            << " => URL: " << url;
                        callback(true, url);
                        return;
                    }
                }
                BMCWEB_LOG_DEBUG << "Not found proper interface for service " 
                                 << service;
            }
            BMCWEB_LOG_ERROR << "Failed to find proper URL";
            callback(false, url);
        },
        dbus_utils::mapperBusName, dbus_utils::mapperObjectPath,
        dbus_utils::mapperIntf, "GetObject", invObjPath.string(),
        std::array<const char*, 0>());
}

} // namespace chassis_utils
} // namespace redfish
