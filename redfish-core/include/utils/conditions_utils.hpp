/*!
 * @file    origin_utils.cpp
 * @brief   Source code for utility functions of handling service conditions.
 */

#pragma once

#include <utils/dbus_log_utils.hpp>
#include <utils/origin_utils.hpp>
#include <utils/time_utils.hpp>

namespace redfish
{
namespace conditions_utils
{

inline void handleDeviceServiceConditions(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}, chassisId(std::string(chassisId))](
            const boost::system::error_code ec,
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
            const std::string prefix =
                "xyz.openbmc_project.Logging.Entry.Level.";
            const std::string criticalSev = prefix + "Critical";
            const std::string warningSev = prefix + "Warning";
            std::time_t timestamp{};

            for (auto& objectPath : resp)
            {
                additionalDataRaw = nullptr;
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
                            else if (propertyMap.first == "Timestamp")
                            {
                                const uint64_t* millisTimeStamp =
                                    std::get_if<uint64_t>(&propertyMap.second);
                                if (millisTimeStamp != nullptr)
                                {
                                    timestamp =
                                        redfish::time_utils::getTimestamp(
                                            *millisTimeStamp);
                                }
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
                std::string messageId;
                std::string deviceName;

                if (additionalDataRaw != nullptr)
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
                    if (additional.count("REDFISH_MESSAGE_ID") > 0)
                    {
                        messageId = additional["REDFISH_MESSAGE_ID"];
                    }
                    if (additional.count("DEVICE_NAME") > 0)
                    {
                        deviceName = additional["DEVICE_NAME"];
                    }
                    if ((*severity == criticalSev || *severity == warningSev) &&
                        messageArgs.find(chassisId) != std::string::npos)
                    {
                        origin_utils::convertDbusObjectToOriginOfCondition(
                            originOfCondition, std::to_string(*id), asyncResp,
                            asyncResp->res.jsonValue, deviceName,
                            (*severity).substr(prefix.length()), messageArgs,
                            redfish::time_utils::getDateTimeStdtime(timestamp),
                            messageId);
                    }
                }
            }
        },
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Namespace", "GetAll", chassisId,
        "xyz.openbmc_project.Logging.Namespace.ResolvedFilterType.Unresolved");
}


inline void handleServiceConditionsURI(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp)
{
    crow::connections::systemBus->async_method_call(
        [asyncResp{asyncResp}](const boost::system::error_code ec,
                               const dbus::utility::ManagedObjectType& resp) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                BMCWEB_LOG_ERROR
                    << "getLogEntriesIfaceData resp_handler got error " << ec;
                return;
            }
            const std::map<std::string, int> severityMap = {
                {"OK", 1}, {"Warning", 2}, {"Critical", 3}};
            const uint32_t* id = nullptr;
            const std::string* message = nullptr;
            const std::string* severity = nullptr;
            const std::vector<std::string>* additionalDataRaw = nullptr;
            const std::string prefix =
                "xyz.openbmc_project.Logging.Entry.Level.";
            const std::string criticalSev = prefix + "Critical";
            const std::string warningSev = prefix + "Warning";
            std::time_t timestamp{};
            for (auto& objectPath : resp)
            {
                additionalDataRaw = nullptr;
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
                            else if (propertyMap.first == "Timestamp")
                            {
                                const uint64_t* millisTimeStamp =
                                    std::get_if<uint64_t>(&propertyMap.second);
                                if (millisTimeStamp != nullptr)
                                {
                                    timestamp =
                                        redfish::time_utils::getTimestamp(
                                            *millisTimeStamp);
                                }
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
                std::string messageId;
                std::string deviceName;

                if (additionalDataRaw != nullptr)
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
                    if (additional.count("REDFISH_MESSAGE_ID") > 0)
                    {
                        messageId = additional["REDFISH_MESSAGE_ID"];
                    }
                    if (additional.count("DEVICE_NAME") > 0)
                    {
                        deviceName = additional["DEVICE_NAME"];
                    }
                    if (*severity == criticalSev || *severity == warningSev)
                    {
                        origin_utils::convertDbusObjectToOriginOfCondition(
                            originOfCondition, std::to_string(*id), asyncResp,
                            asyncResp->res.jsonValue, deviceName,
                            (*severity).substr(prefix.length()), messageArgs,
                            redfish::time_utils::getDateTimeStdtime(timestamp),
                            messageId);

                        std::string sev = (*severity).substr(prefix.length());
                        std::string currSev =
                            asyncResp->res.jsonValue["HealthRollup"];

                        if (severityMap.at(sev) > severityMap.at(currSev))
                        {
                            asyncResp->res.jsonValue["HealthRollup"] = sev;
                        }
                    }
                }
            }
            if (asyncResp->res.jsonValue["Conditions"].size() == 0)
            {
                asyncResp->res.jsonValue["HealthRollup"] = "OK";
            }
        },
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Namespace", "GetAll", "Namespace.All",
        "xyz.openbmc_project.Logging.Namespace.ResolvedFilterType.Unresolved");
}


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
    BMCWEB_LOG_DEBUG << "Populating service conditions for device " << chassisId
                     << "\n";
    BMCWEB_LOG_DEBUG << "ON REDFISH URI "
                     << asyncResp->res.jsonValue["@odata.id"] << "\n";
    BMCWEB_LOG_DEBUG << "PLATFORM DEVICE PREFIX IS " << PLATFORMDEVICEPREFIX
                     << "\n";

    std::string chasId = chassisId;
    if (strlen(PLATFORMDEVICEPREFIX) > 0)
    {
        if (boost::starts_with(chassisId, PLATFORMDEVICEPREFIX))
        {
            chasId = chassisId.substr(strlen(PLATFORMDEVICEPREFIX));
        }
    }
    bool isDevice = !chasId.empty();

    if (isDevice)
    {
        if (!asyncResp->res.jsonValue["Status"].contains("Conditions"))
        {
            asyncResp->res.jsonValue["Status"]["Conditions"] =
                nlohmann::json::array();
        }
        handleDeviceServiceConditions(asyncResp, chasId);
    }
    else
    {
        if (!asyncResp->res.jsonValue.contains("Conditions"))
        {
            asyncResp->res.jsonValue["Conditions"] = nlohmann::json::array();
        }
        handleServiceConditionsURI(asyncResp);
    }
}

} // namespace conditions_utils
} // namespace redfish
