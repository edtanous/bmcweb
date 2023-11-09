#pragma once

#include "nlohmann/json.hpp"

#include <app.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/privilege_utils.hpp>

#include <fstream>
#include <iostream>

namespace redfish
{
namespace boot_options
{

/**
 * @brief Create a new BootOption
 * @param id ID of the new BootOption
 * @param callback A callback function, [](boost::system::error_code ec).
 */
template <typename Callback>
void createBootOption(const std::string& id, Callback&& callback)
{
    crow::connections::systemBus->async_method_call(
        callback, "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.BootOrder", "CreateBootOption", id);
}

/**
 * @brief Delete a BootOption
 * @param id ID of the BootOption
 * @param callback A callback function, [](boost::system::error_code ec).
 */
template <typename Callback>
void deleteBootOption(const std::string& id, Callback&& callback)
{
    std::string path = "/xyz/openbmc_project/bios_config/bootOptions/" + id;
    crow::connections::systemBus->async_method_call(
        callback, "xyz.openbmc_project.BIOSConfigManager", path,
        "xyz.openbmc_project.Object.Delete", "Delete");
}

/**
 * @brief Get all property of a BootOption
 * @param id ID of the BootOption
 * @param callback A callback function, [](boost::system::error_code,
 * dbus::utility::DBusPropertiesMap).
 */
template <typename Callback>
void getBootOption(const std::string& id, Callback&& callback)
{
    std::string path = "/xyz/openbmc_project/bios_config/bootOptions/" + id;
    sdbusplus::asio::getAllProperties(
        *crow::connections::systemBus, "xyz.openbmc_project.BIOSConfigManager",
        path, "xyz.openbmc_project.BIOSConfig.BootOption", callback);
}

/**
 * @brief Set properties to a BootOption
 * @param id ID of the BootOption
 * @param properties vector of pair of property name and property value
 * @param callback A callback function, [](boost::system::error_code ec).
 */
template <typename Callback>
void setBootOption(const std::string& id,
                   const dbus::utility::DBusPropertiesMap& properties,
                   Callback&& callback)
{
    if (properties.empty())
    {
        callback(boost::system::errc::make_error_code(
            boost::system::errc::invalid_argument));
        return;
    }

    std::string path = "/xyz/openbmc_project/bios_config/bootOptions/" + id;

    // holds the callback until all properties were set
    auto holdTask = dbus_utils::deferTask(std::forward<Callback>(callback));
    for (const auto& [propertyName, propertyVariant] : properties)
    {
        crow::connections::systemBus->async_method_call(
            [holdTask](const boost::system::error_code ec) {
            if (ec)
            {
                holdTask->ec = ec;
                BMCWEB_LOG_DEBUG << " setBootOption D-BUS error";
            }
        },
            "xyz.openbmc_project.BIOSConfigManager", path,
            "org.freedesktop.DBus.Properties", "Set",
            "xyz.openbmc_project.BIOSConfig.BootOption", propertyName,
            propertyVariant);
    }
}

inline void handleBootOptionCollectionGet(
    crow::App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& aResp)
{
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }

    aResp->res.jsonValue["@odata.type"] =
        "#BootOptionCollection.BootOptionCollection";
    aResp->res.jsonValue["@odata.id"] = "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                        "/BootOptions";
    aResp->res.jsonValue["Name"] = "Boot Option Collection";

    collection_util::getCollectionMembers(
        aResp, "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions",
        {"xyz.openbmc_project.BIOSConfig.BootOption"}, "/xyz/openbmc_project/");
}

inline void handleBootOptionCollectionPost(
    crow::App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& aResp)
{
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }

    privilege_utils::isBiosPrivilege(
        req,
        [req, aResp](const boost::system::error_code ec, const bool isBios) {
        if (ec || isBios == false)
        {
            messages::insufficientPrivilege(aResp->res);
            return;
        }
        std::string newBootOptionReference;
        bool newBootOptionEnabled = true;
        std::optional<std::string> optBootOptionDescription;
        std::optional<std::string> optBootOptionDisplayName;
        std::optional<std::string> optBootOptionUefiDevicePath;
        if (!json_util::readJsonPatch(
                req, aResp->res, "BootOptionReference", newBootOptionReference,
                "BootOptionEnabled", newBootOptionEnabled, "Description",
                optBootOptionDescription, "DisplayName",
                optBootOptionDisplayName, "UefiDevicePath",
                optBootOptionUefiDevicePath))
        {
            return;
        }

        if (newBootOptionReference.empty())
        {
            messages::propertyValueIncorrect(aResp->res, "BootOptionReference",
                                             newBootOptionReference);
            return;
        }
        std::string id = newBootOptionReference;

        dbus::utility::escapePathForDbus(id);
        dbus::utility::DBusPropertiesMap properties;
        properties.push_back({"Enabled", newBootOptionEnabled});
        if (optBootOptionDescription)
        {
            properties.push_back({"Description", *optBootOptionDescription});
        }
        if (optBootOptionDisplayName)
        {
            properties.push_back({"DisplayName", *optBootOptionDisplayName});
        }
        if (optBootOptionUefiDevicePath)
        {
            properties.push_back(
                {"UefiDevicePath", *optBootOptionUefiDevicePath});
        }

        createBootOption(
            id, [aResp, id, properties](const boost::system::error_code ec2) {
            if (ec2)
            {
                messages::resourceAlreadyExists(aResp->res, "BootOption",
                                                "BootOptionReference", id);
                return;
            }

            setBootOption(id, properties,
                          [aResp](const boost::system::error_code ec3) {
                if (ec3)
                {
                    messages::internalError(aResp->res);
                    return;
                }
            });

            messages::created(aResp->res);
            aResp->res.addHeader(
                boost::beast::http::field::location,
                "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/" + id);
        });
    });
}

inline void handleBootOptionGet(crow::App& app, const crow::Request& req,
                                const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                                const std::string& bootOptionName)
{
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }
    getBootOption(
        bootOptionName,
        [aResp, bootOptionName](
            const boost::system::error_code ec,
            const dbus::utility::DBusPropertiesMap& bootOptonProperties) {
        if (ec)
        {
            messages::resourceNotFound(aResp->res, "BootOption",
                                       bootOptionName);
            return;
        }
        aResp->res.jsonValue["@odata.type"] = "#BootOption.v1_0_4.BootOption";
        aResp->res.jsonValue["@odata.id"] =
            "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/" +
            bootOptionName;
        aResp->res.jsonValue["Name"] = bootOptionName;
        aResp->res.jsonValue["Id"] = bootOptionName;
        aResp->res.jsonValue["BootOptionReference"] = bootOptionName;
        for (const auto& [propertyName, propertyVariant] : bootOptonProperties)
        {
            if (propertyName == "Enabled" &&
                std::holds_alternative<bool>(propertyVariant))
            {
                aResp->res.jsonValue["BootOptionEnabled"] =
                    std::get<bool>(propertyVariant);
            }
            else if (propertyName == "Description" &&
                     std::holds_alternative<std::string>(propertyVariant))
            {
                aResp->res.jsonValue["Description"] =
                    std::get<std::string>(propertyVariant);
            }
            else if (propertyName == "DisplayName" &&
                     std::holds_alternative<std::string>(propertyVariant))
            {
                aResp->res.jsonValue["DisplayName"] =
                    std::get<std::string>(propertyVariant);
            }
            else if (propertyName == "UefiDevicePath" &&
                     std::holds_alternative<std::string>(propertyVariant))
            {
                aResp->res.jsonValue["UefiDevicePath"] =
                    std::get<std::string>(propertyVariant);
            }
        }
    });
}

inline void
    handleBootOptionPatch(crow::App& app, const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                          const std::string& bootOptionName)
{
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }

    bool newBootOptionEnabled = true;
    if (!json_util::readJsonPatch(req, aResp->res, "BootOptionEnabled",
                                  newBootOptionEnabled))
    {
        return;
    }

    dbus::utility::DBusPropertiesMap properties;
    properties.push_back({"Enabled", newBootOptionEnabled});
    setBootOption(bootOptionName, properties,
                  [aResp, bootOptionName](const boost::system::error_code ec) {
        if (ec == boost::system::errc::no_such_device_or_address)
        {
            messages::resourceNotFound(aResp->res, "BootOption",
                                       bootOptionName);
            return;
        }
        if (ec)
        {
            messages::internalError(aResp->res);
            return;
        }
        aResp->res.result(boost::beast::http::status::no_content);
    });
}

inline void
    handleBootOptionDelete(crow::App& app, const crow::Request& req,
                           const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                           const std::string& bootOptionName)
{
    if (!redfish::setUpRedfishRoute(app, req, aResp))
    {
        return;
    }

    privilege_utils::isBiosPrivilege(
        req, [aResp, bootOptionName](const boost::system::error_code ec,
                                     const bool isBios) {
        if (ec || isBios == false)
        {
            messages::insufficientPrivilege(aResp->res);
            return;
        }
        deleteBootOption(
            bootOptionName,
            [aResp, bootOptionName](const boost::system::error_code ec2) {
            if (ec2)
            {
                messages::resourceNotFound(aResp->res, "BootOption",
                                           bootOptionName);
                return;
            }
            aResp->res.result(boost::beast::http::status::no_content);
        });
    });
}

} // namespace boot_options

inline void requestRoutesBootOptions(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/")
        .privileges(redfish::privileges::getBootOptionCollection)
        .methods(boost::beast::http::verb::get)(std::bind_front(
            boot_options::handleBootOptionCollectionGet, std::ref(app)));

    BMCWEB_ROUTE(app, "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/")
        .privileges(redfish::privileges::postBootOptionCollection)
        .methods(boost::beast::http::verb::post)(std::bind_front(
            boot_options::handleBootOptionCollectionPost, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/<str>/")
        .privileges(redfish::privileges::getBootOption)
        .methods(boost::beast::http::verb::get)(
            std::bind_front(boot_options::handleBootOptionGet, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/<str>/")
        .privileges(redfish::privileges::patchBootOption)
        .methods(boost::beast::http::verb::patch)(std::bind_front(
            boot_options::handleBootOptionPatch, std::ref(app)));

    BMCWEB_ROUTE(app,
                 "/redfish/v1/Systems/" PLATFORMSYSTEMID "/BootOptions/<str>/")
        .privileges(redfish::privileges::deleteBootOption)
        .methods(boost::beast::http::verb::delete_)(std::bind_front(
            boot_options::handleBootOptionDelete, std::ref(app)));
}

} // namespace redfish
