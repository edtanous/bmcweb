/*!
 * @file    origin_utils.cpp
 * @brief   Source code for utility functions of handling service conditions.
 */

#pragma once

#include <utils/dbus_log_utils.hpp>
#include <utils/origin_utils.hpp>

namespace redfish
{
namespace conditions_utils
{

/**
 * Utility function for populating Conditions
 * array of the ServiceConditions uri
 * at /redfish/v1/ServiceConditions or the Conditions
 * array of each device depending on the chassisId
 */

inline void populateServiceConditions(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    bool isDevice = !chassisId.empty();
    if (isDevice)
    {
        if (!asyncResp->res.jsonValue["Status"].contains("Conditions"))
        {
            asyncResp->res.jsonValue["Status"]["Conditions"] =
                nlohmann::json::array();
        }
    }
    else
    {
        if (!asyncResp->res.jsonValue.contains("Conditions"))
        {
            asyncResp->res.jsonValue["Conditions"] = nlohmann::json::array();
        }
    }
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, chassisId(std::string(chassisId)),
         isDevice](const boost::system::error_code ec,
                   const dbus::utility::ManagedObjectType& resp) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                BMCWEB_LOG_ERROR
                    << "getLogEntriesIfaceData resp_handler got error " << ec;
                return;
            }

            const uint32_t* id = nullptr;
            const std::string* message = nullptr;
            const std::string* severity = nullptr;
            const std::vector<std::string>* additionalDataRaw = nullptr;
            std::vector<std::string> oocDashboard;
            const std::string prefix =
                "xyz.openbmc_project.Logging.Entry.Level.";
            const std::string criticalSev = prefix + "Critical";
            const std::string warningSev = prefix + "Warning";
            bool resolved = false;

            for (auto& objectPath : resp)
            {
                for (auto& interfaceMap : objectPath.second)
                {
                    if (interfaceMap.first ==
                        "xyz.openbmc_project.Logging.Entry")
                    {
                        for (auto& propertyMap : interfaceMap.second)
                        {
                            if (propertyMap.first == "Id")
                            {
                                id = std::get_if<uint32_t>(&propertyMap.second);
                            }
                            else if (propertyMap.first == "Severity")
                            {
                                severity = std::get_if<std::string>(
                                    &propertyMap.second);
                            }
                            else if (propertyMap.first == "Message")
                            {
                                message = std::get_if<std::string>(
                                    &propertyMap.second);
                            }
                            else if (propertyMap.first == "AdditionalData")
                            {
                                additionalDataRaw =
                                    std::get_if<std::vector<std::string>>(
                                        &propertyMap.second);
                            }
                            else if (propertyMap.first == "Resolved")
                            {
                                const bool* resolveptr =
                                    std::get_if<bool>(&propertyMap.second);
                                if (resolveptr == nullptr)
                                {
                                    messages::internalError(asyncResp->res);
                                    return;
                                }
                                resolved = *resolveptr;
                            }
                        }
                        if (id == nullptr || message == nullptr ||
                            severity == nullptr)
                        {
                            BMCWEB_LOG_ERROR
                                << "id, message, severity of log entry is null\n";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        break;
                    }
                }
                std::string originOfCondition;
                std::string messageArgs;

                if (!resolved && additionalDataRaw != nullptr)
                {
                    AdditionalData additional(*additionalDataRaw);
                    if (additional.count("REDFISH_ORIGIN_OF_CONDITION") > 0)
                    {
                        originOfCondition =
                            additional["REDFISH_ORIGIN_OF_CONDITION"];
                    }
                    if (additional.count("REDFISH_MESSAGE_ARGS") > 0)
                    {
                        messageArgs = additional["REDFISH_MESSAGE_ARGS"];
                    }

                    if (isDevice)
                    {
                        if ((*severity == criticalSev ||
                             *severity == warningSev) &&
                            messageArgs.find(chassisId) != std::string::npos)
                        {

                            origin_utils::convertDbusObjectToOriginOfCondition(
                                originOfCondition, asyncResp,
                                std::string("conditions"),
                                (*severity).substr(prefix.length()));

                            return;
                        }
                    }
                    else
                    {
                        if (*severity == criticalSev || *severity == warningSev)
                        {
                            if (std::find(
                                    oocDashboard.begin(), oocDashboard.end(),
                                    originOfCondition) == oocDashboard.end())
                            {
                                oocDashboard.push_back(originOfCondition);

                                origin_utils::
                                    convertDbusObjectToOriginOfCondition(
                                        originOfCondition, asyncResp,
                                        std::string("conditions"),
                                        (*severity).substr(prefix.length()));
                            }
                        }
                    }
                }
            }
        },
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
}

} // namespace conditions_utils
} // namespace redfish
