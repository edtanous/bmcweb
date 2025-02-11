// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#pragma once
#include "app.hpp"
#include "dbus_singleton.hpp"
#include "logging.hpp"
#include "openbmc_dbus_rest.hpp"
#include "websocket.hpp"

#include <systemd/sd-bus.h>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace crow
{
namespace dbus_monitor
{

struct DbusWebsocketSession
{
    std::vector<std::unique_ptr<sdbusplus::bus::match_t>> matches;
    boost::container::flat_set<std::string, std::less<>,
                               std::vector<std::string>>
        interfaces;
};

using SessionMap = boost::container::flat_map<crow::websocket::Connection*,
                                              DbusWebsocketSession>;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static SessionMap sessions;

inline int onPropertyUpdate(sd_bus_message* m, void* userdata,
                            sd_bus_error* retError)
{
    if (retError == nullptr || (sd_bus_error_is_set(retError) != 0))
    {
        BMCWEB_LOG_ERROR("Got sdbus error on match");
        return 0;
    }
    crow::websocket::Connection* connection =
        static_cast<crow::websocket::Connection*>(userdata);
    auto thisSession = sessions.find(connection);
    if (thisSession == sessions.end())
    {
        BMCWEB_LOG_ERROR("Couldn't find dbus connection {}",
                         logPtr(connection));
        return 0;
    }
    sdbusplus::message_t message(m);
    nlohmann::json json;
    json["event"] = message.get_member();
    json["path"] = message.get_path();
    if (strcmp(message.get_member(), "PropertiesChanged") == 0)
    {
        nlohmann::json data;
        int r = openbmc_mapper::convertDBusToJSON("sa{sv}as", message, data);
        if (r < 0)
        {
            BMCWEB_LOG_ERROR("convertDBusToJSON failed with {}", r);
            return 0;
        }
        if (!data.is_array())
        {
            BMCWEB_LOG_ERROR("No data in PropertiesChanged signal");
            return 0;
        }

        // data is type sa{sv}as and is an array[3] of string, object, array
        json["interface"] = data[0];
        json["properties"] = data[1];
    }
    else if (strcmp(message.get_member(), "InterfacesAdded") == 0)
    {
        nlohmann::json data;
        int r = openbmc_mapper::convertDBusToJSON("oa{sa{sv}}", message, data);
        if (r < 0)
        {
            BMCWEB_LOG_ERROR("convertDBusToJSON failed with {}", r);
            return 0;
        }
        nlohmann::json::array_t* arr = data.get_ptr<nlohmann::json::array_t*>();
        if (arr == nullptr)
        {
            BMCWEB_LOG_ERROR("No data in InterfacesAdded signal");
            return 0;
        }
        if (arr->size() < 2)
        {
            BMCWEB_LOG_ERROR("No data in InterfacesAdded signal");
            return 0;
        }

        nlohmann::json::object_t* obj =
            (*arr)[1].get_ptr<nlohmann::json::object_t*>();
        if (obj == nullptr)
        {
            BMCWEB_LOG_ERROR("No data in InterfacesAdded signal");
            return 0;
        }
        // data is type oa{sa{sv}} which is an array[2] of string, object
        for (const auto& entry : *obj)
        {
            auto it = thisSession->second.interfaces.find(entry.first);
            if (it != thisSession->second.interfaces.end())
            {
                json["interfaces"][entry.first] = entry.second;
            }
        }
    }
    else
    {
        BMCWEB_LOG_CRITICAL("message {} was unexpected", message.get_member());
        return 0;
    }

    connection->sendText(
        json.dump(2, ' ', true, nlohmann::json::error_handler_t::replace));
    return 0;
}

inline void requestRoutes(App& app)
{
    BMCWEB_ROUTE(app, "/subscribe")
        .privileges({{"Login"}})
        .websocket()
        .onopen([](crow::websocket::Connection& conn) {
            BMCWEB_LOG_DEBUG("Connection {} opened", logPtr(&conn));
            sessions.try_emplace(&conn);
        })
        .onclose([](crow::websocket::Connection& conn, const std::string&) {
            sessions.erase(&conn);
        })
        .onmessage([](crow::websocket::Connection& conn,
                      const std::string& data, bool) {
            const auto sessionPair = sessions.find(&conn);
            if (sessionPair == sessions.end())
            {
                conn.close("Internal error");
            }
            DbusWebsocketSession& thisSession = sessionPair->second;
            BMCWEB_LOG_DEBUG("Connection {} received {}", logPtr(&conn), data);
            nlohmann::json j = nlohmann::json::parse(data, nullptr, false);
            if (j.is_discarded())
            {
                BMCWEB_LOG_ERROR("Unable to parse json data for monitor");
                conn.close("Unable to parse json request");
                return;
            }
            nlohmann::json::iterator interfaces = j.find("interfaces");
            if (interfaces != j.end())
            {
                thisSession.interfaces.reserve(interfaces->size());
                for (auto& interface : *interfaces)
                {
                    const std::string* str =
                        interface.get_ptr<const std::string*>();
                    if (str != nullptr)
                    {
                        thisSession.interfaces.insert(*str);
                    }
                }
            }

            nlohmann::json::iterator paths = j.find("paths");
            if (paths == j.end())
            {
                BMCWEB_LOG_ERROR("Unable to find paths in json data");
                conn.close("Unable to find paths in json data");
                return;
            }

            size_t interfaceCount = thisSession.interfaces.size();
            if (interfaceCount == 0)
            {
                interfaceCount = 1;
            }

            // These regexes derived on the rules here:
            // https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names
            static std::regex validPath("^/([A-Za-z0-9_]+/?)*$");
            static std::regex validInterface(
                "^[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_][A-Za-z0-9_]*)+$");

            for (const auto& thisPath : *paths)
            {
                const std::string* thisPathString =
                    thisPath.get_ptr<const std::string*>();
                if (thisPathString == nullptr)
                {
                    BMCWEB_LOG_ERROR("subscribe path isn't a string?");
                    conn.close();
                    return;
                }
                if (!std::regex_match(*thisPathString, validPath))
                {
                    BMCWEB_LOG_ERROR("Invalid path name {}", *thisPathString);
                    conn.close();
                    return;
                }
                std::string propertiesMatchString =
                    ("type='signal',"
                     "interface='org.freedesktop.DBus.Properties',"
                     "path_namespace='" +
                     *thisPathString +
                     "',"
                     "member='PropertiesChanged'");
                // If interfaces weren't specified, add a single match for all
                // interfaces
                if (thisSession.interfaces.empty())
                {
                    BMCWEB_LOG_DEBUG("Creating match {}",
                                     propertiesMatchString);

                    thisSession.matches.emplace_back(
                        std::make_unique<sdbusplus::bus::match_t>(
                            *crow::connections::systemBus,
                            propertiesMatchString, onPropertyUpdate, &conn));
                }
                else
                {
                    // If interfaces were specified, add a match for each
                    // interface
                    for (const std::string& interface : thisSession.interfaces)
                    {
                        if (!std::regex_match(interface, validInterface))
                        {
                            BMCWEB_LOG_ERROR("Invalid interface name {}",
                                             interface);
                            conn.close();
                            return;
                        }
                        std::string ifaceMatchString = propertiesMatchString;
                        ifaceMatchString += ",arg0='";
                        ifaceMatchString += interface;
                        ifaceMatchString += "'";
                        BMCWEB_LOG_DEBUG("Creating match {}", ifaceMatchString);
                        thisSession.matches.emplace_back(
                            std::make_unique<sdbusplus::bus::match_t>(
                                *crow::connections::systemBus, ifaceMatchString,
                                onPropertyUpdate, &conn));
                    }
                }
                std::string objectManagerMatchString =
                    ("type='signal',"
                     "interface='org.freedesktop.DBus.ObjectManager',"
                     "path_namespace='" +
                     *thisPathString +
                     "',"
                     "member='InterfacesAdded'");
                BMCWEB_LOG_DEBUG("Creating match {}", objectManagerMatchString);
                thisSession.matches.emplace_back(
                    std::make_unique<sdbusplus::bus::match_t>(
                        *crow::connections::systemBus, objectManagerMatchString,
                        onPropertyUpdate, &conn));
            }
        });
}
} // namespace dbus_monitor
} // namespace crow
