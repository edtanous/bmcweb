#pragma once

namespace redfish
{

namespace chassis_utils
{

constexpr const char* acceleratorInvIntf =
    "xyz.openbmc_project.Inventory.Item.Accelerator";
constexpr const char* switchInvIntf =
    "xyz.openbmc_project.Inventory.Item.Switch";

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
                BMCWEB_LOG_ERROR << "D-Bus responses error:" << ec;
                callback(false, std::string(""));
                return; // should have associoated inventory object.
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                BMCWEB_LOG_ERROR << "Data is null on object=" << objPath;
                callback(false, std::string(""));
                // Object must have associated inventory object.
                return;
            }

            std::string endpointPath;
            if (data->size() > 0)
            {
                endpointPath = data->front();
            }
            callback(true, endpointPath);
        },
        dbus_utils::mapperBusName, objPath, dbus_utils::propertyInterface,
        "Get", dbus_utils::associationInterface, "endpoints");
}

template <typename CallbackFunc>
inline void getRedfishURL(const std::filesystem::path& invObjPath,
                          CallbackFunc&& callback)
{
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
            std::string service = resp.begin()->first;
            auto interfaces = resp.begin()->second;
            // if accelarator interface then the object would be
            // of type fpga or GPU.
            // If switch interface then it could be Nvswitch or
            // PcieSwitch else it is BMC
            for (const auto& interface : interfaces)
            {
                if (interface == acceleratorInvIntf)
                {
                    url = std::string("/redfish/v1/Processors/") +
                          invObjPath.filename().string();
                    callback(true, url);
                    return;
                }
                else if (interface == switchInvIntf)
                {
                    // This is NVSwitch
                    std::string switchID = invObjPath.filename();
                    // Now get the fabric ID
                    getAssociationEndpoint(
                        invObjPath.string() + "/fabrics",
                        [switchID, callback](const bool& status,
                                             const std::string& ep) {
                            std::string url;
                            if (!status)
                            {
                                BMCWEB_LOG_DEBUG
                                    << "Unable to get the association endpoint";
                                url = "";
                            }
                            else
                            {
                                sdbusplus::message::object_path invObjectPath(
                                    ep);
                                const std::string& fabricID =
                                    invObjectPath.filename();
                                url = std::string("/redfish/v1/Fabrics/") +
                                      fabricID + std::string("/Switches/") +
                                      switchID;
                            }
                            callback(true, url);

                            return;
                        });
                }
            }
        },
        dbus_utils::mapperBusName, dbus_utils::mapperObjectPath,
        dbus_utils::mapperIntf, "GetObject", invObjPath.string(),
        std::array<const char*, 0>());
}

} // namespace chassis_utils
} // namespace redfish
