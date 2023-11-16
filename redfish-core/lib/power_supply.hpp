#pragma once

<<<<<<< HEAD
#include <app.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/json_utils.hpp>
=======
#include "app.hpp"
#include "dbus_utility.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "utils/chassis_utils.hpp"
#include "utils/dbus_utils.hpp"
#include "utils/json_utils.hpp"

#include <boost/system/error_code.hpp>
#include <boost/url/format.hpp>

#include <memory>
#include <optional>
#include <string>
>>>>>>> origin/master-october-10

namespace redfish
{

<<<<<<< HEAD
inline void
    getPowerSupplyAsset(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& connectionName,
                        const std::string& path)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string, std::variant<std::string>>>&
                        propertiesList) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Can't get PowerSupply asset!");
            messages::internalError(asyncResp->res);
            return;
        }
        for (const std::pair<std::string, std::variant<std::string>>& property :
             propertiesList)
        {
            const std::string& propertyName = property.first;

            if ((propertyName == "PartNumber") ||
                (propertyName == "SerialNumber") || (propertyName == "Model") ||
                (propertyName == "SparePartNumber") ||
                (propertyName == "Manufacturer"))
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue[propertyName] = *value;
            }
        }
    },
        connectionName, path, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.Asset");
}

inline void
    getPowerSupplyLocation(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& connectionName,
                           const std::string& path)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const std::vector<
                        std::pair<std::string, std::variant<std::string>>>&
                        propertiesList) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Can't get PowerSupply location!");
            messages::internalError(asyncResp->res);
            return;
        }
        for (const std::pair<std::string, std::variant<std::string>>& property :
             propertiesList)
        {
            const std::string& propertyName = property.first;

            if (propertyName == "LocationCode")
            {
                const std::string* value =
                    std::get_if<std::string>(&property.second);
                if (value == nullptr)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }
                asyncResp->res.jsonValue["Location"]["PartLocation"]
                                        ["ServiceLabel"] = *value;
            }
        }
    },
        connectionName, path, "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.Decorator.LocationCode");
=======
static constexpr std::array<std::string_view, 1> powerSupplyInterface = {
    "xyz.openbmc_project.Inventory.Item.PowerSupply"};

inline void updatePowerSupplyList(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const dbus::utility::MapperGetSubTreePathsResponse& powerSupplyPaths)
{
    nlohmann::json& powerSupplyList = asyncResp->res.jsonValue["Members"];
    for (const std::string& powerSupplyPath : powerSupplyPaths)
    {
        std::string powerSupplyName =
            sdbusplus::message::object_path(powerSupplyPath).filename();
        if (powerSupplyName.empty())
        {
            continue;
        }

        nlohmann::json item = nlohmann::json::object();
        item["@odata.id"] = boost::urls::format(
            "/redfish/v1/Chassis/{}/PowerSubsystem/PowerSupplies/{}", chassisId,
            powerSupplyName);

        powerSupplyList.emplace_back(std::move(item));
    }
    asyncResp->res.jsonValue["Members@odata.count"] = powerSupplyList.size();
}

inline void
    doPowerSupplyCollection(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                            const std::string& chassisId,
                            const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }

    asyncResp->res.addHeader(
        boost::beast::http::field::link,
        "</redfish/v1/JsonSchemas/PowerSupplyCollection/PowerSupplyCollection.json>; rel=describedby");
    asyncResp->res.jsonValue["@odata.type"] =
        "#PowerSupplyCollection.PowerSupplyCollection";
    asyncResp->res.jsonValue["Name"] = "Power Supply Collection";
    asyncResp->res.jsonValue["@odata.id"] = boost::urls::format(
        "/redfish/v1/Chassis/{}/PowerSubsystem/PowerSupplies", chassisId);
    asyncResp->res.jsonValue["Description"] =
        "The collection of PowerSupply resource instances.";
    asyncResp->res.jsonValue["Members"] = nlohmann::json::array();
    asyncResp->res.jsonValue["Members@odata.count"] = 0;

    std::string powerPath = *validChassisPath + "/powered_by";
    dbus::utility::getAssociatedSubTreePaths(
        powerPath,
        sdbusplus::message::object_path("/xyz/openbmc_project/inventory"), 0,
        powerSupplyInterface,
        [asyncResp, chassisId](
            const boost::system::error_code& ec,
            const dbus::utility::MapperGetSubTreePathsResponse& subtreePaths) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR("DBUS response error{}", ec.value());
                messages::internalError(asyncResp->res);
            }
            return;
        }

        updatePowerSupplyList(asyncResp, chassisId, subtreePaths);
        });
}

inline void handlePowerSupplyCollectionHead(
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
        [asyncResp,
         chassisId](const std::optional<std::string>& validChassisPath) {
        if (!validChassisPath)
        {
            messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
            return;
        }
        asyncResp->res.addHeader(
            boost::beast::http::field::link,
            "</redfish/v1/JsonSchemas/PowerSupplyCollection/PowerSupplyCollection.json>; rel=describedby");
        });
}

inline void handlePowerSupplyCollectionGet(
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
        std::bind_front(doPowerSupplyCollection, asyncResp, chassisId));
}

inline void requestRoutesPowerSupplyCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/PowerSubsystem/PowerSupplies/")
        .privileges(redfish::privileges::headPowerSupplyCollection)
        .methods(boost::beast::http::verb::head)(
            std::bind_front(handlePowerSupplyCollectionHead, std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/PowerSubsystem/PowerSupplies/")
        .privileges(redfish::privileges::getPowerSupplyCollection)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handlePowerSupplyCollectionGet, std::ref(app)));
}

inline bool checkPowerSupplyId(const std::string& powerSupplyPath,
                               const std::string& powerSupplyId)
{
    std::string powerSupplyName =
        sdbusplus::message::object_path(powerSupplyPath).filename();

    return !(powerSupplyName.empty() || powerSupplyName != powerSupplyId);
}

inline void getValidPowerSupplyPath(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& validChassisPath, const std::string& powerSupplyId,
    std::function<void(const std::string& powerSupplyPath)>&& callback)
{
    std::string powerPath = validChassisPath + "/powered_by";
    dbus::utility::getAssociatedSubTreePaths(
        powerPath,
        sdbusplus::message::object_path("/xyz/openbmc_project/inventory"), 0,
        powerSupplyInterface,
        [asyncResp, powerSupplyId, callback{std::move(callback)}](
            const boost::system::error_code& ec,
            const dbus::utility::MapperGetSubTreePathsResponse& subtreePaths) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR( "DBUS response error for getAssociatedSubTreePaths{}", ec.value());
                messages::internalError(asyncResp->res);
                return;
            }
            messages::resourceNotFound(asyncResp->res, "PowerSupplies",
                                       powerSupplyId);
            return;
        }

        for (const std::string& path : subtreePaths)
        {
            if (checkPowerSupplyId(path, powerSupplyId))
            {
                callback(path);
                return;
            }
        }

        if (!subtreePaths.empty())
        {
            BMCWEB_LOG_WARNING("Power supply not found: {}", powerSupplyId);
            messages::resourceNotFound(asyncResp->res, "PowerSupplies",
                                       powerSupplyId);
            return;
        }
        });
>>>>>>> origin/master-october-10
}

inline void
    getPowerSupplyState(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
<<<<<<< HEAD
                        const std::string& connectionName,
                        const std::string& path)
{
    // Set the default state to Absent
    asyncResp->res.jsonValue["Status"]["State"] = "Enabled";

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const std::variant<bool> state) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Can't get PowerSupply state!");
            messages::internalError(asyncResp->res);
            return;
        }

        const bool* value = std::get_if<bool>(&state);
        if (value == nullptr)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        if (*value == false)
        {
            asyncResp->res.jsonValue["Status"]["State"] = "Absent";
        }
    },
        connectionName, path, "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Inventory.Item", "Present");
=======
                        const std::string& service, const std::string& path)
{
    sdbusplus::asio::getProperty<bool>(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Inventory.Item", "Present",
        [asyncResp](const boost::system::error_code& ec, const bool value) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR("DBUS response error for State {}", ec.value());
                messages::internalError(asyncResp->res);
            }
            return;
        }

        if (!value)
        {
            asyncResp->res.jsonValue["Status"]["State"] = "Absent";
        }
        });
>>>>>>> origin/master-october-10
}

inline void
    getPowerSupplyHealth(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
<<<<<<< HEAD
                         const std::string& connectionName,
                         const std::string& path)
{
    // Set the default Health to OK
    asyncResp->res.jsonValue["Status"]["Health"] = "OK";

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const std::variant<bool> health) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Can't get PowerSupply health!");
            messages::internalError(asyncResp->res);
            return;
        }

        const bool* value = std::get_if<bool>(&health);
        if (value == nullptr)
        {
            messages::internalError(asyncResp->res);
            return;
        }
        if (*value == false)
        {
            asyncResp->res.jsonValue["Status"]["Health"] = "Critical";
        }
    },
        connectionName, path, "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.State.Decorator.OperationalStatus", "Functional");
}

inline void
    getEfficiencyRatings(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    // Gets the Power Supply Attributes such as EfficiencyPercent.
    // Currently we only support one power supply EfficiencyPercent, use
    // this for all the power supplies.
    crow::connections::systemBus->async_method_call(
        [asyncResp](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("Get PowerSupply attributes respHandler DBus error {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }
        if (subtree.size() == 0)
        {
            BMCWEB_LOG_DEBUG("Can't find Power Supply efficiency ratings!");
            return;
        }
        if (subtree[0].second.empty())
        {
            BMCWEB_LOG_ERROR("Get Power Supply efficiency ratings error!");
            messages::internalError(asyncResp->res);
            return;
        }

        const std::string& psAttributesPath = subtree[0].first;
        const std::string& connection = subtree[0].second[0].first;

        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code ec,
                        const std::variant<uint32_t>& deratingFactor) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("Get PowerSupply DeratingFactor " "respHandler DBus error {}", ec);
                messages::internalError(asyncResp->res);
                return;
            }

            const uint32_t* value = std::get_if<uint32_t>(&deratingFactor);
            if (value == nullptr)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            nlohmann::json& tempArray =
                asyncResp->res.jsonValue["EfficiencyRatings"];
            tempArray.push_back({});
            nlohmann::json& propertyData = tempArray.back();

            propertyData["EfficiencyPercent"] = *value;
        },
            connection, psAttributesPath, "org.freedesktop.DBus.Properties",
            "Get", "xyz.openbmc_project.Control.PowerSupplyAttributes",
            "DeratingFactor");
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Control.PowerSupplyAttributes"});
}

inline void
    getPowerSupplies(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& chassisID)
{
    BMCWEB_LOG_DEBUG("Get power supply list associated to chassis = {}", chassisID);

    asyncResp->res.jsonValue = {
        {"@odata.type", "#PowerSupplyCollection.PowerSupplyCollection"},
        {"@odata.id",
         "/redfish/v1/Chassis/" + chassisID + "/PowerSubsystem/PowerSupplies"},
        {"Name", "Power Supply Collection"},
        {"Description",
         "The collection of PowerSupply resource instances " + chassisID}};

    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisID](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
        if (ec)
        {
            BMCWEB_LOG_ERROR("D-Bus response error on GetSubTree {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }

        nlohmann::json& powerSupplyList = asyncResp->res.jsonValue["Members"];
        powerSupplyList = nlohmann::json::array();

        std::string powerSuppliesPath = "/redfish/v1/Chassis/" + chassisID +
                                        "/PowerSubsystem/PowerSupplies/";
        for (const auto& object : subtree)
        {
            // The association of this PowerSupply is used to determine
            // whether it belongs to this ChassisID
            crow::connections::systemBus->async_method_call(
                [asyncResp, chassisID, &powerSupplyList, powerSuppliesPath,
                 object](
                    const boost::system::error_code ec,
                    const std::variant<std::vector<std::string>>& endpoints) {
                if (ec)
                {
                    if (ec.value() == EBADR)
                    {
                        // This PowerSupply have no chassis association.
                        return;
                    }

                    BMCWEB_LOG_ERROR("DBUS response error");
                    messages::internalError(asyncResp->res);
                    return;
                }

                const std::vector<std::string>* powerSupplyChassis =
                    std::get_if<std::vector<std::string>>(&(endpoints));

                if (powerSupplyChassis != nullptr)
                {
                    if ((*powerSupplyChassis).size() != 1)
                    {
                        BMCWEB_LOG_ERROR("PowerSupply association error! ");
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    std::vector<std::string> chassisPath = *powerSupplyChassis;
                    sdbusplus::message::object_path path(chassisPath[0]);
                    std::string chassisName = path.filename();
                    if (chassisName != chassisID)
                    {
                        // The PowerSupply does't belong to the
                        // chassisID
                        return;
                    }

                    sdbusplus::message::object_path pathPS(object.first);
                    const std::string objectPowerSupplyID = pathPS.filename();

                    powerSupplyList.push_back(
                        {{"@odata.id",
                          powerSuppliesPath + objectPowerSupplyID}});
                    asyncResp->res.jsonValue["Members@odata.count"] =
                        powerSupplyList.size();
                }
            },
                "xyz.openbmc_project.ObjectMapper", object.first + "/chassis",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        }
    },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", int32_t(0),
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.PowerSupply"});
}

template <typename Callback>
inline void
    getValidPowerSupplyID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisID,
                          const std::string& powerSupplyID, Callback&& callback)
{
    BMCWEB_LOG_DEBUG("getValidPowerSupplyID enter");

    auto respHandler =
        [callback{std::forward<Callback>(callback)}, asyncResp, chassisID,
         powerSupplyID](
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string,
                std::vector<std::pair<std::string, std::vector<std::string>>>>>&
                subtree) {
        BMCWEB_LOG_DEBUG("getValidPowerSupplyID respHandler enter");

        if (ec)
        {
            BMCWEB_LOG_ERROR("getValidPowerSupplyID respHandler DBUS error: {}", ec);
            messages::internalError(asyncResp->res);
            return;
        }

        // Set the default value to resourceNotFound, and if we confirm that
        // powerSupplyID is correct, the error response will be cleared.
        messages::resourceNotFound(asyncResp->res, "PowerSupply",
                                   powerSupplyID);

        for (const auto& object : subtree)
        {
            // The association of this PowerSupply is used to determine
            // whether it belongs to this ChassisID
            crow::connections::systemBus->async_method_call(
                [callback{std::move(callback)}, asyncResp, chassisID,
                 powerSupplyID, object](
                    const boost::system::error_code ec,
                    const std::variant<std::vector<std::string>>& endpoints) {
                if (ec)
                {
                    if (ec.value() == EBADR)
                    {
                        // This PowerSupply have no chassis association.
                        return;
                    }

                    BMCWEB_LOG_ERROR("DBUS response error");
                    messages::internalError(asyncResp->res);
                    return;
                }

                const std::vector<std::string>* powerSupplyChassis =
                    std::get_if<std::vector<std::string>>(&(endpoints));

                if (powerSupplyChassis != nullptr)
                {
                    if ((*powerSupplyChassis).size() != 1)
                    {
                        BMCWEB_LOG_ERROR("PowerSupply association error! ");
                        messages::internalError(asyncResp->res);
                        return;
                    }
                    std::vector<std::string> chassisPath = *powerSupplyChassis;
                    sdbusplus::message::object_path path(chassisPath[0]);
                    std::string chassisName = path.filename();
                    if (chassisName != chassisID)
                    {
                        // The PowerSupply does't belong to the
                        // chassisID
                        return;
                    }

                    sdbusplus::message::object_path pathPS(object.first);
                    const std::string powerSupplyName = pathPS.filename();
                    if (powerSupplyName.empty())
                    {
                        BMCWEB_LOG_ERROR("Failed to find powerSupplyName in {}", object.first);
                        return;
                    }

                    std::string validPowerSupplyPath;

                    if (powerSupplyName == powerSupplyID)
                    {
                        // Clear resourceNotFound response
                        asyncResp->res.clear();

                        if (object.second.size() != 1)
                        {
                            BMCWEB_LOG_ERROR("Error getting PowerSupply " "D-Bus object!");
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        const std::string& path = object.first;
                        const std::string& connectionName =
                            object.second[0].first;

                        callback(path, connectionName);
                    }
                }
            },
                "xyz.openbmc_project.ObjectMapper", object.first + "/chassis",
                "org.freedesktop.DBus.Properties", "Get",
                "xyz.openbmc_project.Association", "endpoints");
        }
    };

    // Get the PowerSupply Collection
    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.PowerSupply"});
    BMCWEB_LOG_DEBUG("getValidPowerSupplyID exit");
}

inline void requestRoutesPowerSupplyCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/PowerSubsystem/PowerSupplies/")
        .privileges(redfish::privileges::getPower)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID) {
        auto getChassisID =
            [asyncResp,
             chassisID](const std::optional<std::string>& validChassisID) {
            if (!validChassisID)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }

            getPowerSupplies(asyncResp, chassisID);
        };
        redfish::chassis_utils::getValidChassisID(asyncResp, chassisID,
                                                  std::move(getChassisID));
    });
}

=======
                         const std::string& service, const std::string& path)
{
    sdbusplus::asio::getProperty<bool>(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.State.Decorator.OperationalStatus", "Functional",
        [asyncResp](const boost::system::error_code& ec, const bool value) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR("DBUS response error for Health {}", ec.value());
                messages::internalError(asyncResp->res);
            }
            return;
        }

        if (!value)
        {
            asyncResp->res.jsonValue["Status"]["Health"] = "Critical";
        }
        });
}

inline void
    getPowerSupplyAsset(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& service, const std::string& path)
{
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Inventory.Decorator.Asset",
        [asyncResp](const boost::system::error_code& ec,
                    const dbus::utility::DBusPropertiesMap& propertiesList) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR("DBUS response error for Asset {}", ec.value());
                messages::internalError(asyncResp->res);
            }
            return;
        }

        const std::string* partNumber = nullptr;
        const std::string* serialNumber = nullptr;
        const std::string* manufacturer = nullptr;
        const std::string* model = nullptr;
        const std::string* sparePartNumber = nullptr;

        const bool success = sdbusplus::unpackPropertiesNoThrow(
            dbus_utils::UnpackErrorPrinter(), propertiesList, "PartNumber",
            partNumber, "SerialNumber", serialNumber, "Manufacturer",
            manufacturer, "Model", model, "SparePartNumber", sparePartNumber);

        if (!success)
        {
            messages::internalError(asyncResp->res);
            return;
        }

        if (partNumber != nullptr)
        {
            asyncResp->res.jsonValue["PartNumber"] = *partNumber;
        }

        if (serialNumber != nullptr)
        {
            asyncResp->res.jsonValue["SerialNumber"] = *serialNumber;
        }

        if (manufacturer != nullptr)
        {
            asyncResp->res.jsonValue["Manufacturer"] = *manufacturer;
        }

        if (model != nullptr)
        {
            asyncResp->res.jsonValue["Model"] = *model;
        }

        // SparePartNumber is optional on D-Bus so skip if it is empty
        if (sparePartNumber != nullptr && !sparePartNumber->empty())
        {
            asyncResp->res.jsonValue["SparePartNumber"] = *sparePartNumber;
        }
        });
}

inline void getPowerSupplyFirmwareVersion(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& service, const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Software.Version", "Version",
        [asyncResp](const boost::system::error_code& ec,
                    const std::string& value) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR("DBUS response error for FirmwareVersion {}", ec.value());
                messages::internalError(asyncResp->res);
            }
            return;
        }
        asyncResp->res.jsonValue["FirmwareVersion"] = value;
        });
}

inline void
    getPowerSupplyLocation(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& service, const std::string& path)
{
    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Inventory.Decorator.LocationCode", "LocationCode",
        [asyncResp](const boost::system::error_code& ec,
                    const std::string& value) {
        if (ec)
        {
            if (ec.value() != EBADR)
            {
                BMCWEB_LOG_ERROR("DBUS response error for Location {}", ec.value());
                messages::internalError(asyncResp->res);
            }
            return;
        }
        asyncResp->res.jsonValue["Location"]["PartLocation"]["ServiceLabel"] =
            value;
        });
}

inline void handleGetEfficiencyResponse(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const boost::system::error_code& ec, uint32_t value)
{
    if (ec)
    {
        if (ec.value() != EBADR)
        {
            BMCWEB_LOG_ERROR("DBUS response error for DeratingFactor {}", ec.value());
            messages::internalError(asyncResp->res);
        }
        return;
    }
    // The PDI default value is 0, if it hasn't been set leave off
    if (value == 0)
    {
        return;
    }

    nlohmann::json::array_t efficiencyRatings;
    nlohmann::json::object_t efficiencyPercent;
    efficiencyPercent["EfficiencyPercent"] = value;
    efficiencyRatings.emplace_back(std::move(efficiencyPercent));
    asyncResp->res.jsonValue["EfficiencyRatings"] =
        std::move(efficiencyRatings);
}

inline void handlePowerSupplyAttributesSubTreeResponse(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const boost::system::error_code& ec,
    const dbus::utility::MapperGetSubTreeResponse& subtree)
{
    if (ec)
    {
        if (ec.value() != EBADR)
        {
            BMCWEB_LOG_ERROR("DBUS response error for EfficiencyPercent {}", ec.value());
            messages::internalError(asyncResp->res);
        }
        return;
    }

    if (subtree.empty())
    {
        BMCWEB_LOG_DEBUG("Can't find Power Supply Attributes!");
        return;
    }

    if (subtree.size() != 1)
    {
        BMCWEB_LOG_ERROR( "Unexpected number of paths returned by getSubTree: {}", subtree.size());
        messages::internalError(asyncResp->res);
        return;
    }

    const auto& [path, serviceMap] = *subtree.begin();
    const auto& [service, interfaces] = *serviceMap.begin();
    sdbusplus::asio::getProperty<uint32_t>(
        *crow::connections::systemBus, service, path,
        "xyz.openbmc_project.Control.PowerSupplyAttributes", "DeratingFactor",
        [asyncResp](const boost::system::error_code& ec1, uint32_t value) {
        handleGetEfficiencyResponse(asyncResp, ec1, value);
        });
}

inline void
    getEfficiencyPercent(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    constexpr std::array<std::string_view, 1> efficiencyIntf = {
        "xyz.openbmc_project.Control.PowerSupplyAttributes"};

    dbus::utility::getSubTree(
        "/xyz/openbmc_project", 0, efficiencyIntf,
        [asyncResp](const boost::system::error_code& ec,
                    const dbus::utility::MapperGetSubTreeResponse& subtree) {
        handlePowerSupplyAttributesSubTreeResponse(asyncResp, ec, subtree);
        });
}

inline void
    doPowerSupplyGet(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const std::string& chassisId,
                     const std::string& powerSupplyId,
                     const std::optional<std::string>& validChassisPath)
{
    if (!validChassisPath)
    {
        messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
        return;
    }

    // Get the correct Path and Service that match the input parameters
    getValidPowerSupplyPath(asyncResp, *validChassisPath, powerSupplyId,
                            [asyncResp, chassisId, powerSupplyId](
                                const std::string& powerSupplyPath) {
        asyncResp->res.addHeader(
            boost::beast::http::field::link,
            "</redfish/v1/JsonSchemas/PowerSupply/PowerSupply.json>; rel=describedby");
        asyncResp->res.jsonValue["@odata.type"] =
            "#PowerSupply.v1_5_0.PowerSupply";
        asyncResp->res.jsonValue["Name"] = "Power Supply";
        asyncResp->res.jsonValue["Id"] = powerSupplyId;
        asyncResp->res.jsonValue["@odata.id"] = boost::urls::format(
            "/redfish/v1/Chassis/{}/PowerSubsystem/PowerSupplies/{}", chassisId,
            powerSupplyId);

        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
        asyncResp->res.jsonValue["Status"]["Health"] = "OK";

        dbus::utility::getDbusObject(
            powerSupplyPath, powerSupplyInterface,
            [asyncResp,
             powerSupplyPath](const boost::system::error_code& ec,
                              const dbus::utility::MapperGetObject& object) {
            if (ec || object.empty())
            {
                messages::internalError(asyncResp->res);
                return;
            }

            getPowerSupplyState(asyncResp, object.begin()->first,
                                powerSupplyPath);
            getPowerSupplyHealth(asyncResp, object.begin()->first,
                                 powerSupplyPath);
            getPowerSupplyAsset(asyncResp, object.begin()->first,
                                powerSupplyPath);
            getPowerSupplyFirmwareVersion(asyncResp, object.begin()->first,
                                          powerSupplyPath);
            getPowerSupplyLocation(asyncResp, object.begin()->first,
                                   powerSupplyPath);
            });

        getEfficiencyPercent(asyncResp);
    });
}

inline void
    handlePowerSupplyHead(App& app, const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& chassisId,
                          const std::string& powerSupplyId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        [asyncResp, chassisId,
         powerSupplyId](const std::optional<std::string>& validChassisPath) {
        if (!validChassisPath)
        {
            messages::resourceNotFound(asyncResp->res, "Chassis", chassisId);
            return;
        }

        // Get the correct Path and Service that match the input parameters
        getValidPowerSupplyPath(asyncResp, *validChassisPath, powerSupplyId,
                                [asyncResp](const std::string&) {
            asyncResp->res.addHeader(
                boost::beast::http::field::link,
                "</redfish/v1/JsonSchemas/PowerSupply/PowerSupply.json>; rel=describedby");
        });
        });
}

inline void
    handlePowerSupplyGet(App& app, const crow::Request& req,
                         const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                         const std::string& chassisId,
                         const std::string& powerSupplyId)
{
    if (!redfish::setUpRedfishRoute(app, req, asyncResp))
    {
        return;
    }

    redfish::chassis_utils::getValidChassisPath(
        asyncResp, chassisId,
        std::bind_front(doPowerSupplyGet, asyncResp, chassisId, powerSupplyId));
}

>>>>>>> origin/master-october-10
inline void requestRoutesPowerSupply(App& app)
{
    BMCWEB_ROUTE(
        app, "/redfish/v1/Chassis/<str>/PowerSubsystem/PowerSupplies/<str>/")
<<<<<<< HEAD
        .privileges(redfish::privileges::getPower)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID, const std::string& powerSupplyID) {
        auto getChassisID =
            [asyncResp, chassisID,
             powerSupplyID](const std::optional<std::string>& validChassisID) {
            if (!validChassisID)
            {
                BMCWEB_LOG_ERROR("Not a valid chassis ID:{}", chassisID);
                messages::resourceNotFound(asyncResp->res, "Chassis",
                                           chassisID);
                return;
            }

            // Get the PowerSupply information using the path and
            // service obtained by getValidPowerSupplyID function
            auto getPowerSupplyID =
                [asyncResp, chassisID,
                 powerSupplyID](const std::string& validPowerSupplyPath,
                                const std::string& validPowerSupplyService) {
                asyncResp->res.jsonValue["@odata.type"] =
                    "#PowerSupply.v1_0_0.PowerSupply";
                asyncResp->res.jsonValue["Name"] = powerSupplyID;
                asyncResp->res.jsonValue["Id"] = powerSupplyID;
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Chassis/" + chassisID +
                    "/PowerSubsystem/PowerSupplies/" + powerSupplyID;

                // Get power supply asset
                getPowerSupplyAsset(asyncResp, validPowerSupplyService,
                                    validPowerSupplyPath);

                // Get power supply Location
                getPowerSupplyLocation(asyncResp, validPowerSupplyService,
                                       validPowerSupplyPath);

                // Get power supply state
                getPowerSupplyState(asyncResp, validPowerSupplyService,
                                    validPowerSupplyPath);

                // Get power supply health
                getPowerSupplyHealth(asyncResp, validPowerSupplyService,
                                     validPowerSupplyPath);

                // Get power supply efficiency ratings
                getEfficiencyRatings(asyncResp);
            };
            // Get the correct Path and Service that match the input
            // parameters
            getValidPowerSupplyID(asyncResp, chassisID, powerSupplyID,
                                  std::move(getPowerSupplyID));
        };
        redfish::chassis_utils::getValidChassisID(asyncResp, chassisID,
                                                  std::move(getChassisID));
    });
=======
        .privileges(redfish::privileges::headPowerSupply)
        .methods(boost::beast::http::verb::head)(
            std::bind_front(handlePowerSupplyHead, std::ref(app)));

    BMCWEB_ROUTE(
        app, "/redfish/v1/Chassis/<str>/PowerSubsystem/PowerSupplies/<str>/")
        .privileges(redfish::privileges::getPowerSupply)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(handlePowerSupplyGet, std::ref(app)));
>>>>>>> origin/master-october-10
}

} // namespace redfish
