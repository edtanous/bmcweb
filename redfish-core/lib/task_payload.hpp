#pragma once
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

constexpr bool completed = true;
extern std::deque<std::shared_ptr<struct TaskData>> tasks;

struct Payload
{
    explicit Payload(const crow::Request& req);
    Payload() = delete;

    std::string targetUri;
    std::string httpOperation;
    nlohmann::json httpHeaders;
    nlohmann::json jsonBody;
};

struct TaskData : std::enable_shared_from_this<TaskData>
{
  private:
    TaskData(
        std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                           const std::shared_ptr<TaskData>&)>&& handler,
        const std::string& matchIn, size_t idx);

  public:
    TaskData() = delete;

    static std::shared_ptr<TaskData>& createTask(
        std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                           const std::shared_ptr<TaskData>&)>&& handler,
        const std::string& match);

    void populateResp(crow::Response& res, size_t retryAfterSeconds = 30);

    void finishTask();

    void extendTimer(const std::chrono::seconds& timeout);

    static void sendTaskEvent(std::string_view state, size_t index);

    void startTimer(const std::chrono::seconds& timeout);

    std::function<bool(boost::system::error_code, sdbusplus::message_t&,
                       const std::shared_ptr<TaskData>&)>
        callback;
    std::string matchStr;
    size_t index;
    time_t startTime;
    std::string status;
    std::string state;
    nlohmann::json messages;
    boost::asio::steady_timer timer;
    std::unique_ptr<sdbusplus::bus::match_t> match;
    std::optional<time_t> endTime;
    std::optional<Payload> payload;
    bool gave204 = false;
    int percentComplete = 0;
};

} // namespace task

} // namespace redfish
