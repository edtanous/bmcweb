
#include "task.hpp"

#include "app.hpp"
#include "dbus_utility.hpp"
#include "event_service_manager.hpp"
#include "http/parsing.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "task_payload.hpp"

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

void requestRoutesTaskMonitor(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TaskService/Tasks/<str>/Monitor/")
        .privileges(redfish::privileges::getTask)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& strParam) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        auto find = std::ranges::find_if(
            task::tasks,
            [&strParam](const std::shared_ptr<task::TaskData>& task) {
            if (!task)
            {
                return false;
            }

            // we compare against the string version as on failure
            // strtoul returns 0
            return std::to_string(task->index) == strParam;
        });

        if (find == task::tasks.end())
        {
            messages::resourceNotFound(asyncResp->res, "Task", strParam);
            return;
        }
        std::shared_ptr<task::TaskData>& ptr = *find;
        // monitor expires after 204
        if (ptr->gave204)
        {
            messages::resourceNotFound(asyncResp->res, "Task", strParam);
            return;
        }
        ptr->populateResp(asyncResp->res);
    });
}

void requestRoutesTask(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TaskService/Tasks/<str>/")
        .privileges(redfish::privileges::getTask)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                   const std::string& strParam) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        auto find = std::ranges::find_if(
            task::tasks,
            [&strParam](const std::shared_ptr<task::TaskData>& task) {
            if (!task)
            {
                return false;
            }

            // we compare against the string version as on failure
            // strtoul returns 0
            return std::to_string(task->index) == strParam;
        });

        if (find == task::tasks.end())
        {
            messages::resourceNotFound(asyncResp->res, "Task", strParam);
            return;
        }

        const std::shared_ptr<task::TaskData>& ptr = *find;

        asyncResp->res.jsonValue["@odata.type"] = "#Task.v1_4_3.Task";
        asyncResp->res.jsonValue["Id"] = strParam;
        asyncResp->res.jsonValue["Name"] = "Task " + strParam;
        asyncResp->res.jsonValue["TaskState"] = ptr->state;
        asyncResp->res.jsonValue["StartTime"] =
            redfish::time_utils::getDateTimeStdtime(ptr->startTime);
        if (ptr->endTime)
        {
            asyncResp->res.jsonValue["EndTime"] =
                redfish::time_utils::getDateTimeStdtime(*(ptr->endTime));
        }
        asyncResp->res.jsonValue["TaskStatus"] = ptr->status;
        asyncResp->res.jsonValue["Messages"] = ptr->messages;
        asyncResp->res.jsonValue["@odata.id"] =
            boost::urls::format("/redfish/v1/TaskService/Tasks/{}", strParam);
        if (!ptr->gave204)
        {
            asyncResp->res.jsonValue["TaskMonitor"] =
                "/redfish/v1/TaskService/Tasks/" + strParam + "/Monitor";
        }

        asyncResp->res.jsonValue["HidePayload"] = !ptr->payload;

        if (ptr->payload)
        {
            const task::Payload& p = *(ptr->payload);
            asyncResp->res.jsonValue["Payload"]["TargetUri"] = p.targetUri;
            asyncResp->res.jsonValue["Payload"]["HttpOperation"] =
                p.httpOperation;
            asyncResp->res.jsonValue["Payload"]["HttpHeaders"] = p.httpHeaders;
            asyncResp->res.jsonValue["Payload"]["JsonBody"] = p.jsonBody.dump(
                2, ' ', true, nlohmann::json::error_handler_t::replace);
        }
        asyncResp->res.jsonValue["PercentComplete"] = ptr->percentComplete;
    });
}

void requestRoutesTaskCollection(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TaskService/Tasks/")
        .privileges(redfish::privileges::getTaskCollection)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#TaskCollection.TaskCollection";
        asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/TaskService/Tasks";
        asyncResp->res.jsonValue["Name"] = "Task Collection";
        asyncResp->res.jsonValue["Members@odata.count"] = task::tasks.size();
        nlohmann::json& members = asyncResp->res.jsonValue["Members"];
        members = nlohmann::json::array();

        for (const std::shared_ptr<task::TaskData>& task : task::tasks)
        {
            if (task == nullptr)
            {
                continue; // shouldn't be possible
            }
            nlohmann::json::object_t member;
            member["@odata.id"] =
                boost::urls::format("/redfish/v1/TaskService/Tasks/{}",
                                    std::to_string(task->index));
            members.emplace_back(std::move(member));
        }
    });
}

void requestRoutesTaskService(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/TaskService/")
        .privileges(redfish::privileges::getTaskService)
        .methods(boost::beast::http::verb::get)(
            [&app](const crow::Request& req,
                   const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) {
        if (!redfish::setUpRedfishRoute(app, req, asyncResp))
        {
            return;
        }
        asyncResp->res.jsonValue["@odata.type"] =
            "#TaskService.v1_1_4.TaskService";
        asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/TaskService";
        asyncResp->res.jsonValue["Name"] = "Task Service";
        asyncResp->res.jsonValue["Id"] = "TaskService";
        asyncResp->res.jsonValue["DateTime"] =
            redfish::time_utils::getDateTimeOffsetNow().first;
        asyncResp->res.jsonValue["CompletedTaskOverWritePolicy"] = "Oldest";

        asyncResp->res.jsonValue["LifeCycleEventOnTaskStateChange"] = true;

        asyncResp->res.jsonValue["Status"]["State"] = "Enabled";
        asyncResp->res.jsonValue["ServiceEnabled"] = true;
        asyncResp->res.jsonValue["Tasks"]["@odata.id"] =
            "/redfish/v1/TaskService/Tasks";
    });
}

} // namespace redfish
