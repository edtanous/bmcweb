#pragma once

#include "logging.hpp"

#include <sdbusplus/unpack_properties.hpp>

namespace redfish
{
namespace dbus_utils
{

struct UnpackErrorPrinter
{
    void operator()(const sdbusplus::UnpackErrorReason reason,
                    const std::string& property) const noexcept
    {
        BMCWEB_LOG_ERROR(
            "DBUS property error in property: {}, reason: {}", property,
            static_cast<std::underlying_type_t<sdbusplus::UnpackErrorReason>>(
                reason));
    }
};

} // namespace dbus_utils

namespace
{

void afterSetProperty(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const std::string& redfishPropertyName,
                      const nlohmann::json& propertyValue,
                      const boost::system::error_code& ec,
                      const sdbusplus::message_t& msg)
{
    if (ec)
    {
        const sd_bus_error* dbusError = msg.get_error();
        if (dbusError != nullptr)
        {
            std::string_view errorName(dbusError->name);

            if (errorName == "xyz.openbmc_project.Common.Error.InvalidArgument")
            {
                BMCWEB_LOG_WARNING("DBUS response error: {}", ec);
                messages::propertyValueIncorrect(
                    asyncResp->res, redfishPropertyName, propertyValue);
                return;
            }
            else if (errorName ==
                     "xyz.openbmc_project.State.Chassis.Error.BMCNotReady")
            {
                BMCWEB_LOG_WARNING(
                    "BMC not ready, operation not allowed right now");
                messages::serviceTemporarilyUnavailable(asyncResp->res, "10");
                return;
            }
            else if (errorName ==
                     "xyz.openbmc_project.State.Host.Error.BMCNotReady")
            {
                BMCWEB_LOG_WARNING(
                    "BMC not ready, operation not allowed right now");
                messages::serviceTemporarilyUnavailable(asyncResp->res, "10");
                return;
            }
        }
        messages::internalError(asyncResp->res);
        return;
    }
    asyncResp->res.result(boost::beast::http::status::no_content);
};
} // namespace

template <typename PropertyType>
void setDbusProperty(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     std::string_view processName,
                     const sdbusplus::message::object_path& path,
                     std::string_view interface, std::string_view dbusProperty,
                     std::string_view redfishPropertyName,
                     const PropertyType& prop)
{
    std::string processNameStr(processName);
    std::string interfaceStr(interface);
    std::string dbusPropertyStr(dbusProperty);
    sdbusplus::asio::setProperty(
        *crow::connections::systemBus, processNameStr, path.str, interfaceStr,
        dbusPropertyStr, prop,
        [asyncResp, redfishPropertyNameStr = std::string{redfishPropertyName},
         jsonProp = nlohmann::json(prop)](

            const boost::system::error_code& ec,
            const sdbusplus::message_t& msg) {
        afterSetProperty(asyncResp, redfishPropertyNameStr, jsonProp, ec, msg);
    });
}

} // namespace redfish
