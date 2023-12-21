#include "task_payload.hpp"

#include "bmcweb_config.h"

#include "app.hpp"
#include "dbus_utility.hpp"
#include "event_service_manager.hpp"
#include "health.hpp"
#include "http/parsing.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "task_messages.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/url/format.hpp>
#include <sdbusplus/bus/match.hpp>

#include <chrono>
#include <memory>
#include <ranges>
#include <variant>

namespace redfish
{

namespace task
{

constexpr size_t maxTaskCount = 100; // arbitrary limit

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::deque<std::shared_ptr<struct TaskData>> tasks;

Payload::Payload(const crow::Request& req) :
    targetUri(req.url().encoded_path()), httpOperation(req.methodString()),
    httpHeaders(nlohmann::json::array())
{
    using field_ns = boost::beast::http::field;
    constexpr const std::array<boost::beast::http::field, 7> headerWhitelist = {
        field_ns::accept, field_ns::accept_encoding, field_ns::user_agent,
        field_ns::host,   field_ns::connection,      field_ns::content_length,
        field_ns::upgrade};

    JsonParseResult ret = parseRequestAsJson(req, jsonBody);
    if (ret != JsonParseResult::Success)
    {
        return;
    }

    for (const auto& field : req.fields())
    {
        if (std::ranges::find(headerWhitelist, field.name()) ==
            headerWhitelist.end())
        {
            continue;
        }
        std::string header;
        header.reserve(field.name_string().size() + 2 + field.value().size());
        header += field.name_string();
        header += ": ";
        header += field.value();
        httpHeaders.emplace_back(std::move(header));
    }
}

TaskData::TaskData(
    std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                       const std::shared_ptr<TaskData>&)>&& handler,
    const std::string& matchIn, size_t idx) :
    callback(std::move(handler)),
    matchStr(matchIn), index(idx),
    startTime(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
    status("OK"), state("Running"), messages(nlohmann::json::array()),
    timer(crow::connections::systemBus->get_io_context())

{}

std::shared_ptr<TaskData>& TaskData::createTask(
    std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                       const std::shared_ptr<TaskData>&)>&& handler,
    const std::string& match)
{
    static size_t lastTask = 0;
    struct MakeSharedHelper : public TaskData
    {
        MakeSharedHelper(
            std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                               const std::shared_ptr<TaskData>&)>&& handler,
            const std::string& match2, size_t idx) :
            TaskData(std::move(handler), match2, idx)
        {}
    };

    if (tasks.size() >= maxTaskCount)
    {
        const auto& last = tasks.front();

        // destroy all references
        last->timer.cancel();
        last->match.reset();
        tasks.pop_front();
    }

    return tasks.emplace_back(std::make_shared<MakeSharedHelper>(
        std::move(handler), match, lastTask++));
}

void TaskData::populateResp(crow::Response& res, size_t retryAfterSeconds)
{
    if (!endTime)
    {
        res.result(boost::beast::http::status::accepted);
        std::string strIdx = std::to_string(index);
        std::string uri = "/redfish/v1/TaskService/Tasks/" + strIdx;

        res.jsonValue["@odata.id"] = uri;
        res.jsonValue["@odata.type"] = "#Task.v1_4_3.Task";
        res.jsonValue["Id"] = strIdx;
        res.jsonValue["TaskState"] = state;
        res.jsonValue["TaskStatus"] = status;

        res.addHeader(boost::beast::http::field::location, uri + "/Monitor");
        res.addHeader(boost::beast::http::field::retry_after,
                      std::to_string(retryAfterSeconds));
    }
    else if (!gave204)
    {
        res.result(boost::beast::http::status::no_content);
        gave204 = true;
    }
}

void TaskData::finishTask()
{
    endTime =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void TaskData::extendTimer(const std::chrono::seconds& timeout)
{
    timer.expires_after(timeout);
    timer.async_wait([self = shared_from_this()](boost::system::error_code ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return; // completed successfully
        }
        if (!ec)
        {
            // change ec to error as timer expired
            ec = boost::asio::error::operation_aborted;
        }
        self->match.reset();
        sdbusplus::message_t msg;
        self->finishTask();
        self->state = "Cancelled";
        self->status = "Warning";
        self->messages.emplace_back(
            messages::taskAborted(std::to_string(self->index)));
        // Send event :TaskAborted
        self->sendTaskEvent(self->state, self->index);
        self->callback(ec, msg, self);
    });
}

void TaskData::sendTaskEvent(std::string_view state, size_t index)
{
    std::string origin = "/redfish/v1/TaskService/Tasks/" +
                         std::to_string(index);
    std::string resType = "Task";
    // TaskState enums which should send out an event are:
    // "Starting" = taskResumed
    // "Running" = taskStarted
    // "Suspended" = taskPaused
    // "Interrupted" = taskPaused
    // "Pending" = taskPaused
    // "Stopping" = taskAborted
    // "Completed" = taskCompletedOK
    // "Killed" = taskRemoved
    // "Exception" = taskCompletedWarning
    // "Cancelled" = taskCancelled
    if (state == "Starting")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskResumed(std::to_string(index)), origin,
            resType);
    }
    else if (state == "Running")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskStarted(std::to_string(index)), origin,
            resType);
    }
    else if ((state == "Suspended") || (state == "Interrupted") ||
             (state == "Pending"))
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskPaused(std::to_string(index)), origin,
            resType);
    }
    else if (state == "Stopping")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskAborted(std::to_string(index)), origin,
            resType);
    }
    else if (state == "Completed")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskCompletedOK(std::to_string(index)), origin,
            resType);
    }
    else if (state == "Killed")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskRemoved(std::to_string(index)), origin,
            resType);
    }
    else if (state == "Exception")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskCompletedWarning(std::to_string(index)),
            origin, resType);
    }
    else if (state == "Cancelled")
    {
        redfish::EventServiceManager::getInstance().sendEvent(
            redfish::messages::taskCancelled(std::to_string(index)), origin,
            resType);
    }
    else
    {
        BMCWEB_LOG_INFO("sendTaskEvent: No events to send");
    }
}

void TaskData::startTimer(const std::chrono::seconds& timeout)
{
    if (match)
    {
        return;
    }
    match = std::make_unique<sdbusplus::bus::match_t>(
        static_cast<sdbusplus::bus_t&>(*crow::connections::systemBus), matchStr,
        [self = shared_from_this()](sdbusplus::message_t& message) {
        boost::system::error_code ec;

        // callback to return True if callback is done, callback needs
        // to update status itself if needed
        if (self->callback(ec, message, self) == task::completed)
        {
            self->timer.cancel();
            self->finishTask();

            // Send event
            self->sendTaskEvent(self->state, self->index);

            // reset the match after the callback was successful
            boost::asio::post(crow::connections::systemBus->get_io_context(),
                              [self] { self->match.reset(); });
            return;
        }
    });

    extendTimer(timeout);
    messages.emplace_back(messages::taskStarted(std::to_string(index)));
    // Send event : TaskStarted
    sendTaskEvent(state, index);
}

} // namespace task

} // namespace redfish
