/*
// Copyright (c) 2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once
#include <sys/inotify.h>

#include <async_resp.hpp>
#include "metric_report.hpp"
#include "registries.hpp"
#include "registries/base_message_registry.hpp"
#include "registries/openbmc_message_registry.hpp"
#include "registries/task_event_message_registry.hpp"
#include "utility.hpp"

#include <sys/inotify.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_singleton.hpp>
#include <dbus_utility.hpp>
#include <error_messages.hpp>
#include <event_service_store.hpp>
#include <http_client.hpp>
#include <metric_report.hpp>
#include <persistent_data.hpp>
#include <random.hpp>
#include <sdbusplus/bus/match.hpp>
#include <server_sent_events.hpp>
#include <utils/dbus_log_utils.hpp>
#include <utils/json_utils.hpp>
#include <utils/origin_utils.hpp>
#include <utils/registry_utils.hpp>

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <memory>
#include <span>

namespace redfish
{
const std::regex urlRegex("(http|https)://([^/\\x20\\x3f\\x23\\x3a]+):?([0-9]*)"
                          "((/[^\\x20\\x23\\x3f]*\\x3f?[^\\x20\\x23\\x3f]*)?)");

using ReadingsObjType =
    std::vector<std::tuple<std::string, std::string, double, int32_t>>;

static constexpr const char* eventFormatType = "Event";
static constexpr const char* metricReportFormatType = "MetricReport";

static constexpr const char* eventServiceFile =
    "/var/lib/bmcweb/eventservice_config.json";

namespace registries
{
inline std::span<const MessageEntry>
    getRegistryFromPrefix(const std::string& registryName)
{
    if (task_event::header.registryPrefix == registryName)
    {
        return {task_event::registry};
    }
    if (openbmc::header.registryPrefix == registryName)
    {
        return {openbmc::registry};
    }
    if (base::header.registryPrefix == registryName)
    {
        return {base::registry};
    }
    return {openbmc::registry};
}
} // namespace registries

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES
static std::optional<boost::asio::posix::stream_descriptor> inotifyConn;
static constexpr const char* redfishEventLogDir = "/var/log";
static constexpr const char* redfishEventLogFile = "/var/log/redfish";
static constexpr const size_t iEventSize = sizeof(inotify_event);
static int inotifyFd = -1;
static int dirWatchDesc = -1;
static int fileWatchDesc = -1;

// <ID, timestamp, RedfishLogId, registryPrefix, MessageId, MessageArgs>
using EventLogObjectsType =
    std::tuple<std::string, std::string, std::string, std::string, std::string,
               std::vector<std::string>>;

namespace registries
{
// static const Message*
//     getMsgFromRegistry(const std::string& messageKey,
//                        const std::span<const MessageEntry>& registry)
// {
//     std::span<const MessageEntry>::iterator messageIt =
//         std::find_if(registry.begin(), registry.end(),
//                      [&messageKey](const MessageEntry& messageEntry) {
//         return messageKey == messageEntry.first;
//         });
//     if (messageIt != registry.end())
//     {
//         return &messageIt->second;
//     }

//     return nullptr;
// }

static const Message* formatMessage(const std::string_view& messageID)
{
    // Redfish MessageIds are in the form
    // RegistryName.MajorVersion.MinorVersion.MessageKey, so parse it to find
    // the right Message
    std::vector<std::string> fields;
    fields.reserve(4);
    boost::split(fields, messageID, boost::is_any_of("."));
    if (fields.size() != 4)
    {
        return nullptr;
    }
    const std::string& registryName = fields[0];
    const std::string& messageKey = fields[3];

    // Find the right registry and check it for the MessageKey
    return redfish::message_registries::getMessageFromRegistry(messageKey,
                                  getRegistryFromPrefix(registryName));
}
} // namespace registries

namespace event_log
{
inline bool getUniqueEntryID(const std::string& logEntry, std::string& entryID)
{
    static time_t prevTs = 0;
    static int index = 0;

    // Get the entry timestamp
    std::time_t curTs = 0;
    std::tm timeStruct = {};
    std::istringstream entryStream(logEntry);
    if (entryStream >> std::get_time(&timeStruct, "%Y-%m-%dT%H:%M:%S"))
    {
        curTs = std::mktime(&timeStruct);
        if (curTs == -1)
        {
            return false;
        }
    }
    // If the timestamp isn't unique, increment the index
    index = (curTs == prevTs) ? index + 1 : 0;

    // Save the timestamp
    prevTs = curTs;

    entryID = std::to_string(curTs);
    if (index > 0)
    {
        entryID += "_" + std::to_string(index);
    }
    return true;
}

inline int getEventLogParams(const std::string& logEntry,
                             std::string& timestamp, std::string& messageID,
                             std::vector<std::string>& messageArgs)
{
    // The redfish log format is "<Timestamp> <MessageId>,<MessageArgs>"
    // First get the Timestamp
    size_t space = logEntry.find_first_of(' ');
    if (space == std::string::npos)
    {
        return -EINVAL;
    }
    timestamp = logEntry.substr(0, space);
    // Then get the log contents
    size_t entryStart = logEntry.find_first_not_of(' ', space);
    if (entryStart == std::string::npos)
    {
        return -EINVAL;
    }
    std::string_view entry(logEntry);
    entry.remove_prefix(entryStart);
    // Use split to separate the entry into its fields
    std::vector<std::string> logEntryFields;
    boost::split(logEntryFields, entry, boost::is_any_of(","),
                 boost::token_compress_on);
    // We need at least a MessageId to be valid
    if (logEntryFields.empty())
    {
        return -EINVAL;
    }
    messageID = logEntryFields[0];

    // Get the MessageArgs from the log if there are any
    if (logEntryFields.size() > 1)
    {
        const std::string& messageArgsStart = logEntryFields[1];
        // If the first string is empty, assume there are no MessageArgs
        if (!messageArgsStart.empty())
        {
            messageArgs.assign(logEntryFields.begin() + 1,
                               logEntryFields.end());
        }
    }

    return 0;
}

inline void getRegistryAndMessageKey(const std::string& messageID,
                                     std::string& registryName,
                                     std::string& messageKey)
{
    // Redfish MessageIds are in the form
    // RegistryName.MajorVersion.MinorVersion.MessageKey, so parse it to find
    // the right Message
    std::vector<std::string> fields;
    fields.reserve(4);
    boost::split(fields, messageID, boost::is_any_of("."));
    if (fields.size() == 4)
    {
        registryName = fields[0];
        messageKey = fields[3];
    }
}

inline int formatEventLogEntry(const std::string& logEntryID,
                               const std::string& messageID,
                               const std::span<std::string_view> messageArgs,
                               std::string timestamp,
                               const std::string& customText,
                               nlohmann::json& logEntryJson)
{
    // Get the Message from the MessageRegistry
    const registries::Message* message = registries::formatMessage(messageID);

    if (message == nullptr)
    {
        return -1;
    }

    std::string msg =
        redfish::registries::fillMessageArgs(messageArgs, message->message);
    if (msg.empty())
    {
        return -1;
    }

    // Get the Created time from the timestamp. The log timestamp is in
    // RFC3339 format which matches the Redfish format except for the
    // fractional seconds between the '.' and the '+', so just remove them.
    std::size_t dot = timestamp.find_first_of('.');
    std::size_t plus = timestamp.find_first_of('+', dot);
    if (dot != std::string::npos && plus != std::string::npos)
    {
        timestamp.erase(dot, plus - dot);
    }

    // Fill in the log entry with the gathered data
    logEntryJson["EventId"] = logEntryID;
    logEntryJson["EventType"] = "Event";
    logEntryJson["Severity"] = message->messageSeverity;
    logEntryJson["Message"] = std::move(msg);
    logEntryJson["MessageId"] = messageID;
    logEntryJson["MessageArgs"] = messageArgs;
    logEntryJson["EventTimestamp"] = std::move(timestamp);
    logEntryJson["Context"] = customText;
    return 0;
}

} // namespace event_log
#endif

enum redfish_bool
{
    redfishBoolNa, // NOT APPLICABLE
    redfishBoolTrue,
    redfishBoolFalse
};

// Error constants of class Event
static constexpr int redfishInvalidEvent = -1;
static constexpr int redfishInvalidArgs = -2;

/*
 * Structure for an event which is based on Event v1.7.0 in "Redfish Schema
 * Supplement(DSP0268)".
 */
class Event
{
  public:
    // required properties
    const std::string messageId;
    // optional properties
    std::vector<std::string> actions = {};
    int64_t eventGroupId = -1;
    std::string eventId = "";
    std::string eventTimestamp = "";
    std::string logEntry = "";
    std::string memberId = "";
    std::vector<std::string> messageArgs = {};
    std::string message = "";
    std::string messageSeverity = "";
    std::string oem = "";
    std::string originOfCondition = "";
    redfish_bool specificEventExistsInGroup = redfishBoolNa;

    // derived properties
    std::string registryPrefix;
    std::string resourceType;

  private:
    const registries::Message* registryMsg;
    bool valid;

  public:
    Event(const std::string& messageId) : messageId(messageId)
    {
        registryPrefix = message_registries::getPrefix(messageId);
        registryMsg = message_registries::getMessage(messageId);
        if (registryMsg == nullptr)
        {
            BMCWEB_LOG_ERROR
                << "Message not found in registry with ID: " + messageId;
            valid = false;
            return;
        }
        valid = true;
        messageSeverity = registryMsg->messageSeverity;
    }

    bool isValid()
    {
        return valid;
    }

    int setRegistryMsg(const std::vector<std::string>& messageArgs =
                           std::vector<std::string>{})
    {
        if (!valid)
        {
            BMCWEB_LOG_ERROR << "Invalid Event instance.";
            return redfishInvalidEvent;
        }
        if (messageArgs.size() != registryMsg->numberOfArgs)
        {
            BMCWEB_LOG_ERROR << "Message argument number mismatched.";
            return redfishInvalidArgs;
        }

        int i = 1;
        std::string argStr;

        message = registryMsg->message;
        // Fill the MessageArgs into the Message
        for (const std::string& messageArg : messageArgs)
        {
            argStr = "%" + std::to_string(i++);
            size_t argPos = message.find(argStr);
            if (argPos != std::string::npos)
            {
                message.replace(argPos, argStr.length(), messageArg);
            }
        }
        this->messageArgs = messageArgs;
        return 0;
    }

    int setCustomMsg(const std::string& message,
                     const std::vector<std::string>& messageArgs =
                         std::vector<std::string>{})
    {
        std::string msg = message;
        int i = 1;
        std::string argStr;

        if (!valid)
        {
            BMCWEB_LOG_ERROR << "Invalid Event instance.";
            return redfishInvalidEvent;
        }
        // Fill the MessageArgs into the Message
        for (const std::string& messageArg : messageArgs)
        {
            argStr = "%" + std::to_string(i++);
            size_t argPos = msg.find(argStr);
            if (argPos != std::string::npos)
            {
                msg.replace(argPos, argStr.length(), messageArg);
            }
            else
            {
                BMCWEB_LOG_ERROR << "Too many MessageArgs.";
                return redfishInvalidArgs;
            }
        }
        argStr = "%" + std::to_string(i);
        if (msg.find(argStr) != std::string::npos)
        {
            BMCWEB_LOG_ERROR << "Too few MessageArgs.";
            return redfishInvalidArgs;
        }

        this->message = std::move(msg);
        this->messageArgs = messageArgs;
        return 0;
    }

    /*!
     * @brief   Construct the json format for the event log entry.
     * @param[out] eventLogEntry    The reference to the json event log entry.
     * @return  Return 0 for success. Otherwise, return error codes.
     */
    int formatEventLogEntry(nlohmann::json& eventLogEntry,
                            bool includeOriginOfCondition = true)
    {
        if (!valid)
        {
            BMCWEB_LOG_ERROR << "Invalid Event instance.";
            return redfishInvalidEvent;
        }

        eventLogEntry["MessageId"] = messageId;
        if (!actions.empty())
        {
            eventLogEntry["Actions"] = actions;
        }
        if (eventGroupId >= 0)
        {
            eventLogEntry["EventGroupId"] = eventGroupId;
        }
        if (!eventId.empty())
        {
            eventLogEntry["EventId"] = eventId;
        }
        if (!eventTimestamp.empty())
        {
            eventLogEntry["EventTimeStamp"] = eventTimestamp;
        }
        if (!logEntry.empty())
        {
            eventLogEntry["logEntry"] = nlohmann::json::object();
            eventLogEntry["logEntry"]["@odata.id"] = logEntry;
        }
        if (!memberId.empty())
        {
            eventLogEntry["MemberId"] = memberId;
        }
        if (!messageArgs.empty())
        {
            eventLogEntry["MessageArgs"] = messageArgs;
        }
        if (!message.empty())
        {
            eventLogEntry["Message"] = message;
        }
        if (!messageSeverity.empty())
        {
            eventLogEntry["MessageSeverity"] = messageSeverity;
        }
        if (!oem.empty())
        {
            eventLogEntry["Oem"] = oem;
        }
        if (!originOfCondition.empty() && includeOriginOfCondition)
        {
            eventLogEntry["OriginOfCondition"] = nlohmann::json::object();
            eventLogEntry["OriginOfCondition"]["@odata.id"] = originOfCondition;
        }
        if (specificEventExistsInGroup != redfishBoolNa)
        {
            eventLogEntry["SpecificEventExistsInGroup"] =
                specificEventExistsInGroup == redfishBoolFalse ? false : true;
        }
        return 0;
    }
};

inline bool isFilterQuerySpecialChar(char c)
{
    switch (c)
    {
        case '(':
        case ')':
        case '\'':
            return true;
        default:
            return false;
    }
}

inline bool
    readSSEQueryParams(std::string sseFilter, std::string& formatType,
                       std::vector<std::string>& messageIds,
                       std::vector<std::string>& registryPrefixes,
                       std::vector<std::string>& metricReportDefinitions)
{
    sseFilter.erase(std::remove_if(sseFilter.begin(), sseFilter.end(),
                                   isFilterQuerySpecialChar),
                    sseFilter.end());

    std::vector<std::string> result;
    boost::split(result, sseFilter, boost::is_any_of(" "),
                 boost::token_compress_on);

    BMCWEB_LOG_DEBUG << "No of tokens in SEE query: " << result.size();

    constexpr uint8_t divisor = 4;
    constexpr uint8_t minTokenSize = 3;
    if (result.size() % divisor != minTokenSize)
    {
        BMCWEB_LOG_ERROR << "Invalid SSE filter specified.";
        return false;
    }

    for (std::size_t i = 0; i < result.size(); i += divisor)
    {
        const std::string& key = result[i];
        const std::string& op = result[i + 1];
        const std::string& value = result[i + 2];

        if ((i + minTokenSize) < result.size())
        {
            const std::string& separator = result[i + minTokenSize];
            // SSE supports only "or" and "and" in query params.
            if ((separator != "or") && (separator != "and"))
            {
                BMCWEB_LOG_ERROR
                    << "Invalid group operator in SSE query parameters";
                return false;
            }
        }

        // SSE supports only "eq" as per spec.
        if (op != "eq")
        {
            BMCWEB_LOG_ERROR
                << "Invalid assignment operator in SSE query parameters";
            return false;
        }

        BMCWEB_LOG_DEBUG << key << " : " << value;
        if (key == "EventFormatType")
        {
            formatType = value;
        }
        else if (key == "MessageId")
        {
            messageIds.push_back(value);
        }
        else if (key == "RegistryPrefix")
        {
            registryPrefixes.push_back(value);
        }
        else if (key == "MetricReportDefinition")
        {
            metricReportDefinitions.push_back(value);
        }
        else
        {
            BMCWEB_LOG_ERROR << "Invalid property(" << key
                             << ")in SSE filter query.";
            return false;
        }
    }
    return true;
}

class Subscription : public persistent_data::UserSubscription
{
  public:
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&&) = delete;
    Subscription& operator=(Subscription&&) = delete;

    Subscription(const std::string& inHost, uint16_t inPort,
                 const std::string& inPath, const std::string& inUriProto) :
        eventSeqNum(1),
        host(inHost), port(inPort), path(inPath), uriProto(inUriProto)
    {
        // Subscription constructor
    }

    explicit Subscription(
        const std::shared_ptr<boost::asio::ip::tcp::socket>& adaptor) :
        eventSeqNum(1),
        sseConn(std::make_shared<crow::ServerSentEvents>(adaptor))
    {}

    ~Subscription() = default;

    bool sendEvent(std::string& msg)
    {
        persistent_data::EventServiceConfig eventServiceConfig =
            persistent_data::EventServiceStore::getInstance()
                .getEventServiceConfig();
        if (!eventServiceConfig.enabled)
        {
            return false;
        }

        bool useSSL = (uriProto == "https");
        // A connection pool will be created if one does not already exist
        crow::HttpClient::getInstance().sendData(
            msg, id, host, port, path, useSSL, httpHeaders,
            boost::beast::http::verb::post, retryPolicyName);
        eventSeqNum++;

        if (sseConn != nullptr)
        {
            sseConn->sendData(eventSeqNum, msg);
        }
        return true;
    }

    bool sendTestEventLog()
    {
        nlohmann::json logEntryArray;
        logEntryArray.push_back({});
        nlohmann::json& logEntryJson = logEntryArray.back();

        logEntryJson["EventId"] = "TestID";
        logEntryJson["EventType"] = "Event";
        logEntryJson["Severity"] = "OK";
        logEntryJson["Message"] = "Generated test event";
        logEntryJson["MessageId"] = "OpenBMC.0.2.TestEventLog";
        logEntryJson["MessageArgs"] = nlohmann::json::array();
        logEntryJson["EventTimestamp"] =
            redfish::time_utils::getDateTimeOffsetNow().first;
        logEntryJson["Context"] = customText;

        nlohmann::json msg;
        msg["@odata.type"] = "#Event.v1_4_0.Event";
        msg["Id"] = std::to_string(eventSeqNum);
        msg["Name"] = "Event Log";
        msg["Events"] = logEntryArray;

        std::string strMsg =
            msg.dump(2, ' ', true, nlohmann::json::error_handler_t::replace);
        return this->sendEvent(strMsg);
    }

    /*!
     * @brief   Send the event if this subscription does not filter it out.
     * @param[in] event   The event to be sent.
     * @return  Void
     */
    void sendEvent(Event& event)
    {
        // check if this event should be filtered out or not
        auto filter = [this](Event& event) {
            if (this->eventFormatType != redfish::eventFormatType)
            {
                return false;
            }
            // If registryPrefixes list is empty, don't filter events
            // send everything.
            if (!this->registryPrefixes.empty())
            {
                auto obj = std::find(this->registryPrefixes.begin(),
                                     this->registryPrefixes.end(),
                                     event.registryPrefix);

                if (obj == this->registryPrefixes.end())
                {
                    return false;
                }
            }

            // If registryMsgIds list is empty, don't filter events
            // send everything.
            if (!this->registryMsgIds.empty())
            {
                auto obj =
                    std::find(this->registryMsgIds.begin(),
                              this->registryMsgIds.end(), event.messageId);

                if (obj == this->registryMsgIds.end())
                {
                    return false;
                }
            }

            if (!this->originResources.empty() &&
                !event.originOfCondition.empty())
            {
                auto obj = std::find(this->originResources.begin(),
                                     this->originResources.end(),
                                     event.originOfCondition);

                if (obj == this->originResources.end())
                {
                    return false;
                }
            }

            if (!this->resourceTypes.empty() && !event.resourceType.empty())
            {
                // TODO ResourceType filtering
            }

            // TODO Other property filtering which current UserSubscription do
            // not include

            return true;
        };

        if (filter(event))
        {
            nlohmann::json logEntry;

            if (event.formatEventLogEntry(logEntry,
                                          this->includeOriginOfCondition) != 0)
            {
                BMCWEB_LOG_ERROR << "Failed to format the event log entry";
            }

            nlohmann::json msg = {{"@odata.type", "#Event.v1_7_0.Event"},
                                  {"Id", std::to_string(eventSeqNum)},
                                  {"Name", "Event Log"},
                                  {"Context", customText},
                                  {"Events", logEntry}};

            std::string strMsg =
            msg.dump(2, ' ', true, nlohmann::json::error_handler_t::replace);
            this->sendEvent(strMsg);
        }
    }

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES
    void filterAndSendEventLogs(
        const std::vector<EventLogObjectsType>& eventRecords)
    {
        nlohmann::json logEntryArray;
        for (const EventLogObjectsType& logEntry : eventRecords)
        {
            const std::string& idStr = std::get<0>(logEntry);
            const std::string& timestamp = std::get<1>(logEntry);
            const std::string& messageID = std::get<2>(logEntry);
            const std::string& registryName = std::get<3>(logEntry);
            const std::string& messageKey = std::get<4>(logEntry);
            const std::vector<std::string>& messageArgs = std::get<5>(logEntry);

            // If registryPrefixes list is empty, don't filter events
            // send everything.
            if (!registryPrefixes.empty())
            {
                auto obj = std::find(registryPrefixes.begin(),
                                     registryPrefixes.end(), registryName);
                if (obj == registryPrefixes.end())
                {
                    continue;
                }
            }

            // If registryMsgIds list is empty, don't filter events
            // send everything.
            if (!registryMsgIds.empty())
            {
                auto obj = std::find(registryMsgIds.begin(),
                                     registryMsgIds.end(), messageKey);
                if (obj == registryMsgIds.end())
                {
                    continue;
                }
            }

            std::vector<std::string_view> messageArgsView(messageArgs.begin(),
                                                          messageArgs.end());

            logEntryArray.push_back({});
            nlohmann::json& bmcLogEntry = logEntryArray.back();
            if (event_log::formatEventLogEntry(idStr, messageID,
                                               messageArgsView, timestamp,
                                               customText, bmcLogEntry) != 0)
            {
                BMCWEB_LOG_DEBUG << "Read eventLog entry failed";
                continue;
            }
        }

        if (logEntryArray.empty())
        {
            BMCWEB_LOG_DEBUG << "No log entries available to be transferred.";
            return;
        }

        nlohmann::json msg;
        msg["@odata.type"] = "#Event.v1_4_0.Event";
        msg["Id"] = std::to_string(eventSeqNum);
        msg["Name"] = "Event Log";
        msg["Events"] = logEntryArray;

        std::string strMsg =
            msg.dump(2, ' ', true, nlohmann::json::error_handler_t::replace);
        this->sendEvent(strMsg);
    }
#endif

    void filterAndSendReports(const std::string& reportId,
                              const telemetry::TimestampReadings& var)
    {
        boost::urls::url mrdUri =
            crow::utility::urlFromPieces("redfish", "v1", "TelemetryService",
                                         "MetricReportDefinitions", reportId);

        // Empty list means no filter. Send everything.
        if (!metricReportDefinitions.empty())
        {
            if (std::find(metricReportDefinitions.begin(),
                          metricReportDefinitions.end(),
                          mrdUri.string()) == metricReportDefinitions.end())
            {
                return;
            }
        }

        nlohmann::json msg;
        if (!telemetry::fillReport(msg, reportId, var))
        {
            BMCWEB_LOG_ERROR << "Failed to fill the MetricReport for DBus "
                                "Report with id "
                             << reportId;
            return;
        }

        // Context is set by user during Event subscription and it must be
        // set for MetricReport response.
        if (!customText.empty())
        {
            msg["Context"] = customText;
        }

        std::string strMsg =
            msg.dump(2, ' ', true, nlohmann::json::error_handler_t::replace);
        this->sendEvent(strMsg);
    }

    void updateRetryConfig(const uint32_t retryAttempts,
                           const uint32_t retryTimeoutInterval)
    {
        crow::HttpClient::getInstance().setRetryConfig(
            retryAttempts, retryTimeoutInterval, retryRespHandler,
            retryPolicyName);
    }

    void updateRetryPolicy()
    {
        crow::HttpClient::getInstance().setRetryPolicy(retryPolicy,
                                                       retryPolicyName);
    }

    uint64_t getEventSeqNum() const
    {
        return eventSeqNum;
    }

  private:
    uint64_t eventSeqNum;
    std::string host;
    uint16_t port = 0;
    std::string path;
    std::string uriProto;
    std::shared_ptr<crow::ServerSentEvents> sseConn = nullptr;
    std::string retryPolicyName = "SubscriptionEvent";

    // Check used to indicate what response codes are valid as part of our retry
    // policy.  2XX is considered acceptable
    static boost::system::error_code retryRespHandler(unsigned int respCode)
    {
        BMCWEB_LOG_DEBUG
            << "Checking response code validity for SubscriptionEvent";
        if ((respCode < 200) || (respCode >= 300))
        {
            return boost::system::errc::make_error_code(
                boost::system::errc::result_out_of_range);
        }

        // Return 0 if the response code is valid
        return boost::system::errc::make_error_code(
            boost::system::errc::success);
    }
};

class EventServiceManager
{
  private:
    bool serviceEnabled = false;
    uint32_t retryAttempts = 0;
    uint32_t retryTimeoutInterval = 0;

    EventServiceManager()
    {
        // Load config from persist store.
        initConfig();
    }

    std::streampos redfishLogFilePosition{0};
    size_t noOfEventLogSubscribers{0};
    size_t noOfMetricReportSubscribers{0};
    std::shared_ptr<sdbusplus::bus::match_t> matchTelemetryMonitor;
    std::shared_ptr<sdbusplus::bus::match_t> matchDbusLogging;
    boost::container::flat_map<std::string, std::shared_ptr<Subscription>>
        subscriptionsMap;

    uint64_t eventId{1};

  public:
    EventServiceManager(const EventServiceManager&) = delete;
    EventServiceManager& operator=(const EventServiceManager&) = delete;
    EventServiceManager(EventServiceManager&&) = delete;
    EventServiceManager& operator=(EventServiceManager&&) = delete;
    ~EventServiceManager() = default;

    static EventServiceManager& getInstance()
    {
        static EventServiceManager handler;
        return handler;
    }

    void initConfig()
    {
        loadOldBehavior();

        persistent_data::EventServiceConfig eventServiceConfig =
            persistent_data::EventServiceStore::getInstance()
                .getEventServiceConfig();

        serviceEnabled = eventServiceConfig.enabled;
        retryAttempts = eventServiceConfig.retryAttempts;
        retryTimeoutInterval = eventServiceConfig.retryTimeoutInterval;

        for (const auto& it : persistent_data::EventServiceStore::getInstance()
                                  .subscriptionsConfigMap)
        {
            std::shared_ptr<persistent_data::UserSubscription> newSub =
                it.second;

            std::string host;
            std::string urlProto;
            uint16_t port = 0;
            std::string path;
            bool status = crow::utility::validateAndSplitUrl(
                newSub->destinationUrl, urlProto, host, port, path);

            if (!status)
            {
                BMCWEB_LOG_ERROR
                    << "Failed to validate and split destination url";
                continue;
            }
            std::shared_ptr<Subscription> subValue =
                std::make_shared<Subscription>(host, port, path, urlProto);

            subValue->id = newSub->id;
            subValue->destinationUrl = newSub->destinationUrl;
            subValue->protocol = newSub->protocol;
            subValue->retryPolicy = newSub->retryPolicy;
            subValue->customText = newSub->customText;
            subValue->eventFormatType = newSub->eventFormatType;
            subValue->subscriptionType = newSub->subscriptionType;
            subValue->registryMsgIds = newSub->registryMsgIds;
            subValue->registryPrefixes = newSub->registryPrefixes;
            subValue->resourceTypes = newSub->resourceTypes;
            subValue->httpHeaders = newSub->httpHeaders;
            subValue->metricReportDefinitions = newSub->metricReportDefinitions;
            subValue->originResources = newSub->originResources;
            subValue->includeOriginOfCondition =
                newSub->includeOriginOfCondition;

            if (subValue->id.empty())
            {
                BMCWEB_LOG_ERROR << "Failed to add subscription";
            }
            subscriptionsMap.insert(std::pair(subValue->id, subValue));

            updateNoOfSubscribersCount();

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES

            cacheRedfishLogFile();

#endif
            // Update retry configuration.
            subValue->updateRetryConfig(retryAttempts, retryTimeoutInterval);
            subValue->updateRetryPolicy();
        }

#ifdef BMCWEB_ENABLE_REDFISH_DBUS_EVENT_PUSH
        registerDbusLoggingSignal();
#endif
    }

    static void loadOldBehavior()
    {
        std::ifstream eventConfigFile(eventServiceFile);
        if (!eventConfigFile.good())
        {
            BMCWEB_LOG_DEBUG << "Old eventService config not exist";
            return;
        }
        auto jsonData = nlohmann::json::parse(eventConfigFile, nullptr, false);
        if (jsonData.is_discarded())
        {
            BMCWEB_LOG_ERROR << "Old eventService config parse error.";
            return;
        }

        for (const auto& item : jsonData.items())
        {
            if (item.key() == "Configuration")
            {
                persistent_data::EventServiceStore::getInstance()
                    .getEventServiceConfig()
                    .fromJson(item.value());
            }
            else if (item.key() == "Subscriptions")
            {
                for (const auto& elem : item.value())
                {
                    std::shared_ptr<persistent_data::UserSubscription>
                        newSubscription =
                            persistent_data::UserSubscription::fromJson(elem,
                                                                        true);
                    if (newSubscription == nullptr)
                    {
                        BMCWEB_LOG_ERROR << "Problem reading subscription "
                                            "from old persistent store";
                        continue;
                    }

                    std::uniform_int_distribution<uint32_t> dist(0);
                    bmcweb::OpenSSLGenerator gen;

                    std::string id;

                    int retry = 3;
                    while (retry != 0)
                    {
                        id = std::to_string(dist(gen));
                        if (gen.error())
                        {
                            retry = 0;
                            break;
                        }
                        newSubscription->id = id;
                        auto inserted =
                            persistent_data::EventServiceStore::getInstance()
                                .subscriptionsConfigMap.insert(
                                    std::pair(id, newSubscription));
                        if (inserted.second)
                        {
                            break;
                        }
                        --retry;
                    }

                    if (retry <= 0)
                    {
                        BMCWEB_LOG_ERROR
                            << "Failed to generate random number from old "
                               "persistent store";
                        continue;
                    }
                }
            }

            persistent_data::getConfig().writeData();
            std::remove(eventServiceFile);
            BMCWEB_LOG_DEBUG << "Remove old eventservice config";
        }
    }

    void updateSubscriptionData() const
    {
        persistent_data::EventServiceStore::getInstance()
            .eventServiceConfig.enabled = serviceEnabled;
        persistent_data::EventServiceStore::getInstance()
            .eventServiceConfig.retryAttempts = retryAttempts;
        persistent_data::EventServiceStore::getInstance()
            .eventServiceConfig.retryTimeoutInterval = retryTimeoutInterval;

        persistent_data::getConfig().writeData();
    }

    void setEventServiceConfig(const persistent_data::EventServiceConfig& cfg)
    {
        bool updateConfig = false;
        bool updateRetryCfg = false;

        if (serviceEnabled != cfg.enabled)
        {
            serviceEnabled = cfg.enabled;
            if (serviceEnabled && noOfMetricReportSubscribers != 0U)
            {
                registerMetricReportSignal();
            }
            else
            {
                unregisterMetricReportSignal();
            }
#ifdef BMCWEB_ENABLE_REDFISH_DBUS_EVENT_PUSH
            if (serviceEnabled)
            {
                registerDbusLoggingSignal();
            }
            else
            {
                unregisterDbusLoggingSignal();
            }
#endif
            updateConfig = true;
        }

        if (retryAttempts != cfg.retryAttempts)
        {
            retryAttempts = cfg.retryAttempts;
            updateConfig = true;
            updateRetryCfg = true;
        }

        if (retryTimeoutInterval != cfg.retryTimeoutInterval)
        {
            retryTimeoutInterval = cfg.retryTimeoutInterval;
            updateConfig = true;
            updateRetryCfg = true;
        }

        if (updateConfig)
        {
            updateSubscriptionData();
        }

        if (updateRetryCfg)
        {
            // Update the changed retry config to all subscriptions
            for (const auto& it :
                 EventServiceManager::getInstance().subscriptionsMap)
            {
                std::shared_ptr<Subscription> entry = it.second;
                entry->updateRetryConfig(retryAttempts, retryTimeoutInterval);
            }
        }
    }

    void updateNoOfSubscribersCount()
    {
        size_t eventLogSubCount = 0;
        size_t metricReportSubCount = 0;
        for (const auto& it : subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;
            if (entry->eventFormatType == eventFormatType)
            {
                eventLogSubCount++;
            }
            else if (entry->eventFormatType == metricReportFormatType)
            {
                metricReportSubCount++;
            }
        }

        noOfEventLogSubscribers = eventLogSubCount;
        if (noOfMetricReportSubscribers != metricReportSubCount)
        {
            noOfMetricReportSubscribers = metricReportSubCount;
            if (noOfMetricReportSubscribers != 0U)
            {
                registerMetricReportSignal();
            }
            else
            {
                unregisterMetricReportSignal();
            }
        }
    }

    std::shared_ptr<Subscription> getSubscription(const std::string& id)
    {
        auto obj = subscriptionsMap.find(id);
        if (obj == subscriptionsMap.end())
        {
            BMCWEB_LOG_ERROR << "No subscription exist with ID:" << id;
            return nullptr;
        }
        std::shared_ptr<Subscription> subValue = obj->second;
        return subValue;
    }

    std::string addSubscription(const std::shared_ptr<Subscription>& subValue,
                                const bool updateFile = true)
    {

        std::uniform_int_distribution<uint32_t> dist(0);
        bmcweb::OpenSSLGenerator gen;

        std::string id;

        if (isDestinationExist(subValue->destinationUrl))
        {
            return "";
        }

        int retry = 3;
        while (retry != 0)
        {
            id = std::to_string(dist(gen));
            if (gen.error())
            {
                retry = 0;
                break;
            }
            auto inserted = subscriptionsMap.insert(std::pair(id, subValue));
            if (inserted.second)
            {
                break;
            }
            --retry;
        }

        if (retry <= 0)
        {
            BMCWEB_LOG_ERROR << "Failed to generate random number";
            return "";
        }

        std::shared_ptr<persistent_data::UserSubscription> newSub =
            std::make_shared<persistent_data::UserSubscription>();
        newSub->id = id;
        newSub->destinationUrl = subValue->destinationUrl;
        newSub->protocol = subValue->protocol;
        newSub->retryPolicy = subValue->retryPolicy;
        newSub->customText = subValue->customText;
        newSub->eventFormatType = subValue->eventFormatType;
        newSub->subscriptionType = subValue->subscriptionType;
        newSub->registryMsgIds = subValue->registryMsgIds;
        newSub->registryPrefixes = subValue->registryPrefixes;
        newSub->resourceTypes = subValue->resourceTypes;
        newSub->httpHeaders = subValue->httpHeaders;
        newSub->metricReportDefinitions = subValue->metricReportDefinitions;
        newSub->originResources = subValue->originResources;
        newSub->includeOriginOfCondition = subValue->includeOriginOfCondition;
        persistent_data::EventServiceStore::getInstance()
            .subscriptionsConfigMap.emplace(newSub->id, newSub);
        persistent_data::EventServiceStore::getInstance()
            .subscriptionsConfigMap.emplace(newSub->id, newSub);

        updateNoOfSubscribersCount();

        if (updateFile)
        {
            updateSubscriptionData();
        }

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES
        if (redfishLogFilePosition != 0)
        {
            cacheRedfishLogFile();
        }
#endif
        // Update retry configuration.
        subValue->updateRetryConfig(retryAttempts, retryTimeoutInterval);
        subValue->updateRetryPolicy();

        return id;
    }

    bool isSubscriptionExist(const std::string& id)
    {
        auto obj = subscriptionsMap.find(id);
        return obj != subscriptionsMap.end();
    }

    void deleteSubscription(const std::string& id)
    {
        auto obj = subscriptionsMap.find(id);
        if (obj != subscriptionsMap.end())
        {
            subscriptionsMap.erase(obj);
            auto obj2 = persistent_data::EventServiceStore::getInstance()
                            .subscriptionsConfigMap.find(id);
            persistent_data::EventServiceStore::getInstance()
                .subscriptionsConfigMap.erase(obj2);
            updateNoOfSubscribersCount();
            updateSubscriptionData();
        }
    }

    size_t getNumberOfSubscriptions()
    {
        return subscriptionsMap.size();
    }

    std::vector<std::string> getAllIDs()
    {
        std::vector<std::string> idList;
        for (const auto& it : subscriptionsMap)
        {
            idList.emplace_back(it.first);
        }
        return idList;
    }

    bool isDestinationExist(const std::string& destUrl)
    {
        for (const auto& it : subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;
            if (entry->destinationUrl == destUrl)
            {
                BMCWEB_LOG_ERROR << "Destination exists already " << destUrl;
                return true;
            }
        }
        return false;
    }

    bool sendTestEventLog()
    {
        for (const auto& it : this->subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;
            if (!entry->sendTestEventLog())
            {
                return false;
            }
        }
        return true;
    }

    void sendEvent(nlohmann::json eventMessage, const std::string& origin,
                   const std::string& resType)
    {
        if (!serviceEnabled || (noOfEventLogSubscribers == 0U))
        {
            BMCWEB_LOG_DEBUG << "EventService disabled or no Subscriptions.";
            return;
        }
        nlohmann::json eventRecord = nlohmann::json::array();

        eventMessage["EventId"] = eventId;
        // MemberId is 0 : since we are sending one event record.
        eventMessage["MemberId"] = 0;
        eventMessage["EventTimestamp"] =
            redfish::time_utils::getDateTimeOffsetNow().first;
        eventMessage["OriginOfCondition"] = origin;

        eventRecord.emplace_back(std::move(eventMessage));

        for (const auto& it : this->subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;
            bool isSubscribed = false;
            // Search the resourceTypes list for the subscription.
            // If resourceTypes list is empty, don't filter events
            // send everything.
            if (!entry->resourceTypes.empty())
            {
                for (const auto& resource : entry->resourceTypes)
                {
                    if (resType == resource)
                    {
                        BMCWEB_LOG_INFO << "ResourceType " << resource
                                        << " found in the subscribed list";
                        isSubscribed = true;
                        break;
                    }
                }
            }
            else // resourceTypes list is empty.
            {
                isSubscribed = true;
            }
            if (isSubscribed)
            {
                nlohmann::json msgJson;

                msgJson["@odata.type"] = "#Event.v1_4_0.Event";
                msgJson["Name"] = "Event Log";
                msgJson["Id"] = eventId;
                msgJson["Events"] = eventRecord;

                std::string strMsg = msgJson.dump(
                    2, ' ', true, nlohmann::json::error_handler_t::replace);
                entry->sendEvent(strMsg);
                eventId++; // increament the eventId
            }
            else
            {
                BMCWEB_LOG_INFO << "Not subscribed to this resource";
            }
        }
    }
    void sendBroadcastMsg(const std::string& broadcastMsg)
    {
        for (const auto& it : this->subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;
            nlohmann::json msgJson;
            msgJson["Timestamp"] =
                redfish::time_utils::getDateTimeOffsetNow().first;
            msgJson["OriginOfCondition"] = "/ibm/v1/HMC/BroadcastService";
            msgJson["Name"] = "Broadcast Message";
            msgJson["Message"] = broadcastMsg;

            std::string strMsg = msgJson.dump(
                2, ' ', true, nlohmann::json::error_handler_t::replace);
            entry->sendEvent(strMsg);
        }
    }

    /*!
     * @brief   Send the event to all subscribers.
     * @param[in] event   The event to be sent.
     * @return  Void
     */
    void sendEvent(Event& event)
    {
        for (const auto& it : this->subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;

            entry->sendEvent(event);
        }
        eventId++; // increament the eventId
    }

#ifndef BMCWEB_ENABLE_REDFISH_DBUS_LOG_ENTRIES

    void resetRedfishFilePosition()
    {
        // Control would be here when Redfish file is created.
        // Reset File Position as new file is created
        redfishLogFilePosition = 0;
    }

    void cacheRedfishLogFile()
    {
        // Open the redfish file and read till the last record.

        std::ifstream logStream(redfishEventLogFile);
        if (!logStream.good())
        {
            BMCWEB_LOG_ERROR << " Redfish log file open failed \n";
            return;
        }
        std::string logEntry;
        while (std::getline(logStream, logEntry))
        {
            redfishLogFilePosition = logStream.tellg();
        }

        BMCWEB_LOG_DEBUG << "Next Log Position : " << redfishLogFilePosition;
    }

    void readEventLogsFromFile()
    {
        std::ifstream logStream(redfishEventLogFile);
        if (!logStream.good())
        {
            BMCWEB_LOG_ERROR << " Redfish log file open failed";
            return;
        }

        std::vector<EventLogObjectsType> eventRecords;

        std::string logEntry;

        // Get the read pointer to the next log to be read.
        logStream.seekg(redfishLogFilePosition);

        while (std::getline(logStream, logEntry))
        {
            // Update Pointer position
            redfishLogFilePosition = logStream.tellg();

            std::string idStr;
            if (!event_log::getUniqueEntryID(logEntry, idStr))
            {
                continue;
            }

            if (!serviceEnabled || noOfEventLogSubscribers == 0)
            {
                // If Service is not enabled, no need to compute
                // the remaining items below.
                // But, Loop must continue to keep track of Timestamp
                continue;
            }

            std::string timestamp;
            std::string messageID;
            std::vector<std::string> messageArgs;
            if (event_log::getEventLogParams(logEntry, timestamp, messageID,
                                             messageArgs) != 0)
            {
                BMCWEB_LOG_DEBUG << "Read eventLog entry params failed";
                continue;
            }

            std::string registryName;
            std::string messageKey;
            event_log::getRegistryAndMessageKey(messageID, registryName,
                                                messageKey);
            if (registryName.empty() || messageKey.empty())
            {
                continue;
            }

            eventRecords.emplace_back(idStr, timestamp, messageID, registryName,
                                      messageKey, messageArgs);
        }

        if (!serviceEnabled || noOfEventLogSubscribers == 0)
        {
            BMCWEB_LOG_DEBUG << "EventService disabled or no Subscriptions.";
            return;
        }

        if (eventRecords.empty())
        {
            // No Records to send
            BMCWEB_LOG_DEBUG << "No log entries available to be transferred.";
            return;
        }

        for (const auto& it : this->subscriptionsMap)
        {
            std::shared_ptr<Subscription> entry = it.second;
            if (entry->eventFormatType == "Event")
            {
                entry->filterAndSendEventLogs(eventRecords);
            }
        }
    }

    static void watchRedfishEventLogFile()
    {
        if (!inotifyConn)
        {
            return;
        }

        static std::array<char, 1024> readBuffer;

        inotifyConn->async_read_some(boost::asio::buffer(readBuffer),
                                     [&](const boost::system::error_code& ec,
                                         const std::size_t& bytesTransferred) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Callback Error: " << ec.message();
                return;
            }
            std::size_t index = 0;
            while ((index + iEventSize) <= bytesTransferred)
            {
                struct inotify_event event
                {};
                std::memcpy(&event, &readBuffer[index], iEventSize);
                if (event.wd == dirWatchDesc)
                {
                    if ((event.len == 0) ||
                        (index + iEventSize + event.len > bytesTransferred))
                    {
                        index += (iEventSize + event.len);
                        continue;
                    }

                    std::string fileName(&readBuffer[index + iEventSize]);
                    if (fileName != "redfish")
                    {
                        index += (iEventSize + event.len);
                        continue;
                    }

                    BMCWEB_LOG_DEBUG
                        << "Redfish log file created/deleted. event.name: "
                        << fileName;
                    if (event.mask == IN_CREATE)
                    {
                        if (fileWatchDesc != -1)
                        {
                            BMCWEB_LOG_DEBUG
                                << "Remove and Add inotify watcher on "
                                   "redfish event log file";
                            // Remove existing inotify watcher and add
                            // with new redfish event log file.
                            inotify_rm_watch(inotifyFd, fileWatchDesc);
                            fileWatchDesc = -1;
                        }

                        fileWatchDesc = inotify_add_watch(
                            inotifyFd, redfishEventLogFile, IN_MODIFY);
                        if (fileWatchDesc == -1)
                        {
                            BMCWEB_LOG_ERROR << "inotify_add_watch failed for "
                                                "redfish log file.";
                            return;
                        }

                        EventServiceManager::getInstance()
                            .resetRedfishFilePosition();
                        EventServiceManager::getInstance()
                            .readEventLogsFromFile();
                    }
                    else if ((event.mask == IN_DELETE) ||
                             (event.mask == IN_MOVED_TO))
                    {
                        if (fileWatchDesc != -1)
                        {
                            inotify_rm_watch(inotifyFd, fileWatchDesc);
                            fileWatchDesc = -1;
                        }
                    }
                }
                else if (event.wd == fileWatchDesc)
                {
                    if (event.mask == IN_MODIFY)
                    {
                        EventServiceManager::getInstance()
                            .readEventLogsFromFile();
                    }
                }
                index += (iEventSize + event.len);
            }

            watchRedfishEventLogFile();
        });
    }

    static int startEventLogMonitor(boost::asio::io_context& ioc)
    {
        inotifyConn.emplace(ioc);
        inotifyFd = inotify_init1(IN_NONBLOCK);
        if (inotifyFd == -1)
        {
            BMCWEB_LOG_ERROR << "inotify_init1 failed.";
            return -1;
        }

        // Add watch on directory to handle redfish event log file
        // create/delete.
        dirWatchDesc = inotify_add_watch(inotifyFd, redfishEventLogDir,
                                         IN_CREATE | IN_MOVED_TO | IN_DELETE);
        if (dirWatchDesc == -1)
        {
            BMCWEB_LOG_ERROR
                << "inotify_add_watch failed for event log directory.";
            return -1;
        }

        // Watch redfish event log file for modifications.
        fileWatchDesc =
            inotify_add_watch(inotifyFd, redfishEventLogFile, IN_MODIFY);
        if (fileWatchDesc == -1)
        {
            BMCWEB_LOG_ERROR
                << "inotify_add_watch failed for redfish log file.";
            // Don't return error if file not exist.
            // Watch on directory will handle create/delete of file.
        }

        // monitor redfish event log file
        inotifyConn->assign(inotifyFd);
        watchRedfishEventLogFile();

        return 0;
    }

#endif
    static void getReadingsForReport(sdbusplus::message_t& msg)
    {
        if (msg.is_method_error())
        {
            BMCWEB_LOG_ERROR << "TelemetryMonitor Signal error";
            return;
        }

        sdbusplus::message::object_path path(msg.get_path());
        std::string id = path.filename();
        if (id.empty())
        {
            BMCWEB_LOG_ERROR << "Failed to get Id from path";
            return;
        }

        std::string interface;
        dbus::utility::DBusPropertiesMap props;
        std::vector<std::string> invalidProps;
        msg.read(interface, props, invalidProps);

        auto found =
            std::find_if(props.begin(), props.end(),
                         [](const auto& x) { return x.first == "Readings"; });
        if (found == props.end())
        {
            BMCWEB_LOG_INFO << "Failed to get Readings from Report properties";
            return;
        }

        const telemetry::TimestampReadings* readings =
            std::get_if<telemetry::TimestampReadings>(&found->second);
        if (readings == nullptr)
        {
            BMCWEB_LOG_INFO << "Failed to get Readings from Report properties";
            return;
        }

        for (const auto& it :
             EventServiceManager::getInstance().subscriptionsMap)
        {
            Subscription& entry = *it.second;
            if (entry.eventFormatType == metricReportFormatType)
            {
                entry.filterAndSendReports(id, *readings);
            }
        }
    }

    void unregisterMetricReportSignal()
    {
        if (matchTelemetryMonitor)
        {
            BMCWEB_LOG_DEBUG << "Metrics report signal - Unregister";
            matchTelemetryMonitor.reset();
            matchTelemetryMonitor = nullptr;
        }
    }

    void registerMetricReportSignal()
    {
        if (!serviceEnabled || matchTelemetryMonitor)
        {
            BMCWEB_LOG_DEBUG << "Not registering metric report signal.";
            return;
        }

        BMCWEB_LOG_DEBUG << "Metrics report signal - Register";
        std::string matchStr = "type='signal',member='PropertiesChanged',"
                               "interface='org.freedesktop.DBus.Properties',"
                               "arg0=xyz.openbmc_project.Telemetry.Report";

        matchTelemetryMonitor = std::make_shared<sdbusplus::bus::match_t>(
            *crow::connections::systemBus, matchStr,
            [this](sdbusplus::message::message& msg) {
                if (msg.is_method_error())
                {
                    BMCWEB_LOG_ERROR << "TelemetryMonitor Signal error";
                    return;
                }

                getReadingsForReport(msg);
            });
    }

#ifdef BMCWEB_ENABLE_REDFISH_DBUS_EVENT_PUSH
    void unregisterDbusLoggingSignal()
    {
        if (matchDbusLogging)
        {
            BMCWEB_LOG_DEBUG << "Dbus logging signal - Unregister.";
            matchDbusLogging.reset();
        }
    }

    /**
     * Populates event with origin of condition
     * then sends the event for Redfish Event Listener
     * to pick up
     */
    void sendEventWithOOC(const std::string& ooc, Event& event)
    {
        if (ooc.empty())
        {
            BMCWEB_LOG_ERROR << "Invalid OriginOfCondition "
                             << "in AdditionalData property\n";
        }
        event.originOfCondition = ooc;
        sendEvent(event);
    }

    /**
     * Looks up the chassis required in redfish URI
     * for a particular dbus path so OOC can be
     * populated correctly for an event
     */
    void chassisLookupOOC(const std::string& path,
                          const std::string& redfishPrefix,
                          const std::string& endpointType, Event& event)
    {

        sdbusplus::message::object_path objPath(path);
        std::string name = objPath.filename();
        if (name.empty())
        {
            BMCWEB_LOG_ERROR << "File name empty for dbus path: " << path
                             << "\n";
            return;
        }

        const std::string& deviceName = name;
        crow::connections::systemBus->async_method_call(
            [this, redfishPrefix, deviceName, path, endpointType,
             event](const boost::system::error_code ec,
                    std::variant<std::vector<std::string>>& resp) mutable {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << "Failed: " << ec << "\n";
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&resp);
                if (data == nullptr)
                {
                    return;
                }
                std::vector<std::string> paths = *data;
                std::string chassisPath;
                if (paths.size() > 0)
                {
                    chassisPath = paths[0];
                }
                else
                {
                    BMCWEB_LOG_ERROR << "Empty chassis paths for " << path
                                     << "\n";
                    return;
                }
                sdbusplus::message::object_path objPath(chassisPath);
                std::string chassisName = objPath.filename();
                std::string oocDevice = redfishPrefix;
                oocDevice.append(chassisName);
                oocDevice.append(endpointType);
                oocDevice.append(deviceName);
                sendEventWithOOC(oocDevice, event);
            },
            "xyz.openbmc_project.ObjectMapper", path + "/chassis",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Association", "endpoints");
    }

    /**
     * Calculates origin of condition redfish URI
     * for a dbus path and populates an event of the
     * event service manager with it
     */
    void eventServiceOOC(const std::string& path, Event& event)
    {
        if (path.empty())
        {
            sendEventWithOOC(path, event);
        }
        sdbusplus::message::object_path objPath(path);
        std::string name = objPath.filename();
        if (name.empty())
        {
            BMCWEB_LOG_ERROR << "File name empty for dbus path: " << path
                             << "\n";
            return;
        }
        if (boost::starts_with(path, "/xyz/openbmc_project/sensors"))
        {
            chassisLookupOOC(path, "/redfish/v1/Chassis/", "/Sensors/", event);
            return;
        }
        const std::string& deviceName = name;
        crow::connections::systemBus->async_method_call(
            [this, deviceName, path, event](
                const boost::system::error_code ec,
                const std::vector<std::pair<
                    std::string, std::vector<std::string>>>& objects) mutable {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << "Failed: " << ec << "\n";
                    return;
                }
                if (objects.empty())
                {
                    BMCWEB_LOG_ERROR << "Objects empty!\n";
                    return;
                }

                for (const auto& object : objects)
                {
                    for (const auto& interfaces : object.second)
                    {
                        if (interfaces ==
                            "xyz.openbmc_project.Inventory.Item.PCIeDevice")
                        {
                            chassisLookupOOC(path, "/redfish/v1/Chassis/",
                                             "/PCIeDevices/", event);
                            return;
                        }
                        if (interfaces ==
                                "xyz.openbmc_project.Inventory.Item.Accelerator" ||
                            interfaces ==
                                "xyz.openbmc_project.Inventory.Item.Cpu")
                        {
                            sendEventWithOOC(
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Processors/" +
                                    deviceName,
                                event);
                            return;
                        }
                        if (interfaces ==
                            "xyz.openbmc_project.Inventory.Item.Dimm")
                        {
                            sendEventWithOOC(
                                "/redfish/v1/Systems/" PLATFORMSYSTEMID
                                "/Memory/" +
                                    deviceName,
                                event);
                            return;
                        }
                        sendEventWithOOC("/redfish/v1/Chassis/" + deviceName,
                                         event);
                    }
                }
            },
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetObject", path,
            std::array<const char*, 6>{
                "xyz.openbmc_project.Inventory.Item.Accelerator",
                "xyz.openbmc_project.Inventory.Item.PCIeDevice",
                "xyz.openbmc_project.Inventory.Item.Cpu",
                "xyz.openbmc_project.Inventory.Item.Board",
                "xyz.openbmc_project.Inventory.Item.Chassis",
                "xyz.openbmc_project.Inventory.Item.Dimm"});
    }

    void registerDbusLoggingSignal()
    {
        if (!serviceEnabled || matchDbusLogging)
        {
            BMCWEB_LOG_DEBUG << "Not registering dbus logging signal.";
            return;
        }

        BMCWEB_LOG_DEBUG << "Dbus logging signal - Register.";
        std::string matchStr("type='signal', "
                             "member='InterfacesAdded', "
                             "path_namespace='/xyz/openbmc_project/logging'");

        auto signalHandler = [this](sdbusplus::message::message& msg) {
            if (msg.get_type() != SD_BUS_MESSAGE_SIGNAL)
            {
                BMCWEB_LOG_ERROR << "Dbus logging signal error.";
                return;
            }

            sdbusplus::message::object_path objPath;
            std::map<std::string,
                     std::map<std::string,
                              std::variant<std::string, uint32_t, uint64_t,
                                           bool, std::vector<std::string>>>>
                properties;

            std::string messageId = "";
            std::string eventId = "";
            std::string severity = "";
            std::string timestamp = "";
            std::string originOfCondition = "";
            std::string message;
            std::vector<std::string> messageArgs = {};
            const std::vector<std::string>* additionalDataPtr;

            msg.read(objPath, properties);
            for (auto const& [key, val] :
                 properties["xyz.openbmc_project.Logging.Entry"])
            {
                if (key == "AdditionalData")
                {
                    additionalDataPtr =
                        std::get_if<std::vector<std::string>>(&val);
                    if (additionalDataPtr != nullptr)
                    {
                        AdditionalData additional(*additionalDataPtr);
                        if (additional.count("REDFISH_MESSAGE_ID") == 1)
                        {
                            messageId = additional["REDFISH_MESSAGE_ID"];
                            if (additional.count("REDFISH_MESSAGE_ARGS") == 1)
                            {
                                std::string argDelimiter = ", ";
                                size_t cur = 0, delimPos;
                                std::string arg;
                                std::string args =
                                    additional["REDFISH_MESSAGE_ARGS"];

                                while (
                                    (delimPos = args.substr(cur, args.length())
                                                    .find(argDelimiter)) !=
                                    std::string::npos)
                                {
                                    arg = args.substr(cur, cur + delimPos);
                                    messageArgs.push_back(arg);
                                    cur += delimPos + argDelimiter.length();
                                }
                                messageArgs.push_back(
                                    args.substr(cur, args.length()));
                            }
                            else if (additional.count("REDFISH_MESSAGE_ARGS") >
                                     0)
                            {
                                BMCWEB_LOG_ERROR
                                    << "Multiple "
                                       "REDFISH_MESSAGE_ARGS in the Dbus "
                                       "signal message.";
                                return;
                            }
                        }
                        else
                        {
                            BMCWEB_LOG_ERROR
                                << "There should be exactly one "
                                   "MessageId in the Dbus signal message. Found " +
                                       std::to_string(additional.count(
                                           "REDFISH_MESSAGE_ID")) +
                                       ".";
                            return;
                        }
                    }
                    else
                    {
                        BMCWEB_LOG_ERROR << "Invalid type of AdditionalData "
                                            "property.";
                        return;
                    }
                }
                else if (key == "EventId")
                {
                    const std::string* eventIdPtr;

                    eventIdPtr = std::get_if<std::string>(&val);
                    if (eventIdPtr != nullptr)
                    {
                        eventId = *eventIdPtr;
                    }
                    else
                    {
                        BMCWEB_LOG_ERROR << "Invalid type of EventId property.";
                        return;
                    }
                }
                else if (key == "Severity")
                {
                    const std::string* severityPtr;

                    severityPtr = std::get_if<std::string>(&val);
                    if (severityPtr != nullptr)
                    {
                        severity = std::move(*severityPtr);
                    }
                    else
                    {
                        BMCWEB_LOG_ERROR << "Invalid type of Severity "
                                            "property.";
                        return;
                    }
                }
                else if (key == "Timestamp")
                {
                    const uint64_t* timestampPtr;

                    timestampPtr = std::get_if<uint64_t>(&val);
                    if (timestampPtr != nullptr)
                    {
                        timestamp = std::move(redfish::time_utils::getDateTimeStdtime(
                            redfish::time_utils::getTimestamp(*timestampPtr)));
                    }
                    else
                    {
                        BMCWEB_LOG_ERROR << "Invalid type of Timestamp "
                                            "property.";
                        return;
                    }
                }
                else
                {
                    continue;
                }
            }

            if (messageId == "")
            {
                BMCWEB_LOG_ERROR << "Invalid Dbus log entry.";
                return;
            }
            else
            {
                Event event(messageId);
                if (!event.isValid())
                {
                    return;
                }
                event.eventId = eventId;
                event.messageSeverity =
                    translateSeverityDbusToRedfish(severity);
                event.eventTimestamp = timestamp;
                event.setRegistryMsg(messageArgs);
                event.messageArgs = messageArgs;
                AdditionalData additional(*additionalDataPtr);
                if (additional.count("REDFISH_ORIGIN_OF_CONDITION") == 1)
                {
                    eventServiceOOC(additional["REDFISH_ORIGIN_OF_CONDITION"],
                                    event);
                }
                else
                {
                    BMCWEB_LOG_ERROR << "No OriginOfCondition provided"
                                     << "in AdditionalData property\n";
                    eventServiceOOC(std::string(""), event);
                }
            }
        };

        matchDbusLogging = std::make_shared<sdbusplus::bus::match_t>(
            *crow::connections::systemBus, matchStr, signalHandler);
    }
#endif

    bool validateAndSplitUrl(const std::string& destUrl, std::string& urlProto,
                             std::string& host, std::string& port,
                             std::string& path)
    {
        // Validate URL using regex expression
        // Format: <protocol>://<host>:<port>/<path>
        // protocol: http/https
        std::cmatch match;
        if (!std::regex_match(destUrl.c_str(), match, urlRegex))
        {
            BMCWEB_LOG_INFO << "Dest. url did not match ";
            return false;
        }

        urlProto = std::string(match[1].first, match[1].second);
        if (urlProto == "http")
        {
#ifndef BMCWEB_INSECURE_ENABLE_HTTP_PUSH_STYLE_EVENTING
            return false;
#endif
        }

        host = std::string(match[2].first, match[2].second);
        port = std::string(match[3].first, match[3].second);
        path = std::string(match[4].first, match[4].second);
        if (port.empty())
        {
            if (urlProto == "http")
            {
                port = "80";
            }
            else
            {
                port = "443";
            }
        }
        if (path.empty())
        {
            path = "/";
        }
        return true;
    }
};

} // namespace redfish
