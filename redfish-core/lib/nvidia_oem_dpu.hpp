#pragma once

#include "nlohmann/json.hpp"

#include <app.hpp>
#include <dbus_utility.hpp>
#include <query.hpp>
#include <registries/privilege_registry.hpp>
#include <utils/privilege_utils.hpp>

namespace redfish
{
namespace bluefield
{
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_BF3_PROPERTIES
struct PropertyInfo
{
    const std::string intf;
    const std::string prop;
    const std::unordered_map<std::string, std::string> dbusToRedfish = {};
    const std::unordered_map<std::string, std::string> redfishToDbus = {};
};

struct ObjectInfo
{
    // ObjectInfo() = delete;
    const std::string service;
    const std::string obj;
    const PropertyInfo propertyInfo;
    const bool required = false;
};
class DpuCommonProperties
{
  public:
    DpuCommonProperties(
        const std::unordered_map<std::string, ObjectInfo>& objects) :
        objects(objects)
    {}
    // function assumes input is valid
    inline std::string toRedfish(const std::string& str,
                                 const std::string& name) const
    {
        auto it = objects.find(name);

        if (it == objects.end())
            return "";

        auto it2 = it->second.propertyInfo.dbusToRedfish.find(str);
        if (it2 == it->second.propertyInfo.dbusToRedfish.end())
            return "";
        return it2->second;
    }
    inline std::string toDbus(const std::string& str,
                              const std::string& name) const
    {
        auto it = objects.find(name);

        if (it == objects.end())
            return "";

        auto it2 = it->second.propertyInfo.redfishToDbus.find(str);
        if (it2 == it->second.propertyInfo.redfishToDbus.end())
            return "";
        return it2->second;
    }

    inline bool isValueAllowed(const std::string& str,
                               const std::string& name) const
    {
        bool ret = false;
        auto it = objects.find(name);

        if (it != objects.end())
        {
            ret = it->second.propertyInfo.redfishToDbus.count(str) != 0;
        }

        return ret;
    }

    inline std::vector<std::string> getAllowableValues(const std::string& name)
    {
        std::vector<std::string> ret;
        auto it = objects.find(name);

        if (it != objects.end())
        {
            for (auto pair : it->second.propertyInfo.redfishToDbus)
            {
                ret.push_back(pair.first);
            }
        }
        return ret;
    }

    const std::unordered_map<std::string, ObjectInfo> objects;
};

class DpuGetProperties : virtual public DpuCommonProperties
{
  public:
    DpuGetProperties(
        const std::unordered_map<std::string, ObjectInfo>& objects) :
        DpuCommonProperties(objects)
    {}

    int getObject(nlohmann::json* const json,
                  const std::shared_ptr<bmcweb::AsyncResp> asyncResp,
                  std::string name, const ObjectInfo& objectInfo) const
    {

        crow::connections::systemBus->async_method_call(
            [&, json, asyncResp,
             name](const boost::system::error_code ec,
                   const std::variant<std::string>& variant) {
                if (ec)
                {
                    BMCWEB_LOG_DEBUG << "DBUS response error for " << name;
                    return;
                }

                (*json)[name] =
                    toRedfish(*std::get_if<std::string>(&variant), name);
            },
            objectInfo.service, objectInfo.obj,
            "org.freedesktop.DBus.Properties", "Get",
            objectInfo.propertyInfo.intf, objectInfo.propertyInfo.prop);

        return 0;
    }
    int getProperty(nlohmann::json* const json,
                    const std::shared_ptr<bmcweb::AsyncResp> asyncResp)
    {
        for (auto pair : objects)
        {
            getObject(json, asyncResp, pair.first, pair.second);
        }
        return 0;
    }
};

class DpuActionSetProperties : virtual public DpuCommonProperties
{
  public:
    DpuActionSetProperties(
        const std::unordered_map<std::string, ObjectInfo>& objects,
        const std::string target) :
        DpuCommonProperties(objects),
        target(target)
    {}
    std::string getActionTarget()
    {
        return target;
    }
    void getActionInfo(nlohmann::json* const json)
    {
        nlohmann::json actionInfo;
        nlohmann::json::array_t parameters;
        for (auto& pair : objects)
        {
            auto& name = pair.first;
            auto& objectInfo = pair.second;
            nlohmann::json::object_t parameter;
            parameter["Name"] = name;
            parameter["Required"] = objectInfo.required;
            parameter["DataType"] = "String";
            parameter["AllowableValues"] = getAllowableValues(name);

            parameters.push_back(std::move(parameter));
        }

        (*json)["target"] = target;
        (*json)["Parameters"] = std::move(parameters);
    }

    void setAction(crow::App& app, const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
    {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }

        std::list<std::tuple<ObjectInfo, std::string, std::string>> updates;
        nlohmann::json jsonRequest;
        if (!json_util::processJsonFromRequest(asyncResp->res, req,
                                               jsonRequest))
        {

            return;
        }

        for (auto pair : objects)
        {
            auto& name = pair.first;
            auto& objectInfo = pair.second;
            std::optional<std::string> value;
            if (objectInfo.required && !jsonRequest.contains(name.c_str()))
            {

                BMCWEB_LOG_DEBUG << "Missing required param: " << name;
                messages::actionParameterMissing(asyncResp->res, name, target);
                return;
            }
        }
        for (auto item : jsonRequest.items())
        {
            auto name = item.key();
            auto it = objects.find(name);
            if (it == objects.end())
            {
                messages::actionParameterNotSupported(asyncResp->res, name,
                                                      target);
                return;
            }
            auto value = item.value().get_ptr<const std::string*>();
            if (!value)
            {
                messages::actionParameterValueError(asyncResp->res, name,
                                                    target);
                return;
            }
            if (!isValueAllowed(*value, name))
            {
                messages::actionParameterValueFormatError(asyncResp->res,
                                                          *value, name, target);
                return;
            }
        }

        for (auto item : jsonRequest.items())
        {
            auto name = item.key();
            auto value = item.value().get<std::string>();
            auto objectInfo = objects.find(name)->second;
            crow::connections::systemBus->async_method_call(
                [asyncResp](const boost::system::error_code ec) {
                    if (ec)
                    {
                        BMCWEB_LOG_ERROR << "Set failed " << ec;
                        messages::internalError(asyncResp->res);
                        return;
                    }

                    messages::success(asyncResp->res);
                },
                objectInfo.service, objectInfo.obj,
                "org.freedesktop.DBus.Properties", "Set",
                objectInfo.propertyInfo.intf, objectInfo.propertyInfo.prop,
                std::variant<std::string>(toDbus(value, name)));
        }
    }

    const std::string target;
};

class DpuActionSetAndGetProp :
    public DpuActionSetProperties,
    public DpuGetProperties
{
  public:
    DpuActionSetAndGetProp(
        const std::unordered_map<std::string, ObjectInfo>& objects,
        const std::string target) :
        DpuCommonProperties(objects),
        DpuActionSetProperties(objects, target), DpuGetProperties(objects)
    {}
};

const PropertyInfo modeInfo = {
    .intf = "xyz.openbmc_project.Control.NicAttribute",
    .prop = "NicAttribute",
    .dbusToRedfish =
        {{"xyz.openbmc_project.Control.NicAttribute.Modes.Enabled", "DpuMode"},
         {"xyz.openbmc_project.Control.NicAttribute.Modes.Disabled", "NicMode"},
         {"xyz.openbmc_project.Control.NicAttribute.Modes.Invaild", "Invaild"}},
    .redfishToDbus = {
        {"DpuMode", "xyz.openbmc_project.Control.NicAttribute.Modes.Enabled"},
        {"NicMode",
         "xyz.openbmc_project.Control.NicAttribute.Modes.Disabled"}}};

constexpr char oemNvidiaGet[] =
    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Oem/Nvidia";

constexpr char modeTarget[] =
    "/redfish/v1/Systems/" PLATFORMSYSTEMID "/Oem/Nvidia/Actions/Mode.Set";

DpuActionSetAndGetProp mode(
    {{"Mode",
      {.service = "xyz.openbmc_project.Settings.connectx",
       .obj =
           "/xyz/openbmc_project/network/connectx/smartnic_mode/smartnic_mode/INTERNAL_CPU_OFFLOAD_ENGINE",
       .propertyInfo = bluefield::modeInfo,
       .required = true}}},
    modeTarget);

#endif //BMCWEB_ENABLE_NVIDIA_OEM_BF3_PROPERTIES
inline void getIsOemNvidiaRshimEnable(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    const char* systemdServiceBf = "org.freedesktop.systemd1";
    const char* systemdUnitIntfBf = "org.freedesktop.systemd1.Unit";
    const char* rshimSystemdObjBf =
        "/org/freedesktop/systemd1/unit/rshim_2eservice";
    std::filesystem::path rshimDir = "/dev/rshim0";

    if (!std::filesystem::exists(rshimDir))
    {
        BMCWEB_LOG_DEBUG << "No /dev/rshim0. Interface not started";
        asyncResp->res.jsonValue["BmcRShim"]["BmcRShimEnabled"] = false;
        return;
    }

    sdbusplus::asio::getProperty<std::string>(
        *crow::connections::systemBus, systemdServiceBf, rshimSystemdObjBf,
        systemdUnitIntfBf, "ActiveState",
        [asyncResp](const boost::system::error_code ec,
                    const std::string& rshimActiveState) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "DBUS response error for getIsOemNvidiaRshimEnable";
                messages::internalError(asyncResp->res);
                return;
            }
            asyncResp->res.jsonValue["BmcRShim"]["BmcRShimEnabled"] =
                (rshimActiveState == "active");
        });
}

inline void
    requestOemNvidiaRshim(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const bool& bmcRshimEnabled)
{
    const char* systemdServiceBf = "org.freedesktop.systemd1";
    const char* systemdUnitIntfBf = "org.freedesktop.systemd1.Unit";
    const char* rshimSystemdObjBf =
        "/org/freedesktop/systemd1/unit/rshim_2eservice";
    std::string method = bmcRshimEnabled ? "Start" : "Stop";

    BMCWEB_LOG_DEBUG << "requestOemNvidiaRshim: " << method.c_str()
                     << " rshim interface";

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec) {
            if (ec)
            {
                BMCWEB_LOG_ERROR
                    << "DBUS response error for rshim enable/disable";
                messages::internalError(asyncResp->res);
                return;
            }
        },
        systemdServiceBf, rshimSystemdObjBf, systemdUnitIntfBf, method.c_str(),
        "replace");

    messages::success(asyncResp->res);
}

} // namespace bluefield

inline void requestRoutesNvidiaOemBf(App& app)
{

    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia")
        .privileges(redfish::privileges::getManager)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                bluefield::getIsOemNvidiaRshimEnable(asyncResp);
            });
    BMCWEB_ROUTE(app, "/redfish/v1/Managers/" PLATFORMBMCID "/Oem/Nvidia/")
        .privileges(redfish::privileges::patchManager)
        .methods(boost::beast::http::verb::patch)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                std::optional<nlohmann::json> bmcRshim;
                if (!redfish::json_util::readJsonPatch(req, asyncResp->res,
                                                       "BmcRShim", bmcRshim))
                {
                    BMCWEB_LOG_ERROR
                        << "Illegal Property "
                        << asyncResp->res.jsonValue.dump(
                               2, ' ', true,
                               nlohmann::json::error_handler_t::replace);
                    return;
                }
                if (bmcRshim)
                {
                    std::optional<bool> bmcRshimEnabled;
                    if (!redfish::json_util::readJson(*bmcRshim, asyncResp->res,
                                                      "BmcRShimEnabled",
                                                      bmcRshimEnabled))
                    {
                        BMCWEB_LOG_ERROR
                            << "Illegal Property "
                            << asyncResp->res.jsonValue.dump(
                                   2, ' ', true,
                                   nlohmann::json::error_handler_t::replace);
                        return;
                    }

                    bluefield::requestOemNvidiaRshim(asyncResp,
                                                     *bmcRshimEnabled);
                }
            });
#ifdef BMCWEB_ENABLE_NVIDIA_OEM_BF3_PROPERTIES
    BMCWEB_ROUTE(app, bluefield::oemNvidiaGet)
        .privileges(redfish::privileges::getComputerSystem)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
                if (!redfish::setUpRedfishRoute(app, req, asyncResp))
                {
                    return;
                }
                auto& nvidia = asyncResp->res.jsonValue;
                auto& actions = nvidia["Actions"];
                auto& modeAction = actions["#Mode.Set"];

                bluefield::mode.getProperty(&nvidia, asyncResp);
                bluefield::mode.getActionInfo(&modeAction);
            });
    BMCWEB_ROUTE(app, bluefield::modeTarget)
        .privileges(redfish::privileges::postComputerSystem)
        .methods(boost::beast::http::verb::post)(
            std::bind_front(&bluefield::DpuActionSetAndGetProp::setAction,
                            &bluefield::mode, std::ref(app)));
#endif //BMCWEB_ENABLE_NVIDIA_OEM_BF3_PROPERTIES
}

} // namespace redfish
