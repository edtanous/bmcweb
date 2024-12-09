#include "http/parsing.hpp"
#include "http_client.hpp"
#include "sdbusplus/asio/object_server.hpp"
#include "utils/json_utils.hpp"

namespace redfish
{

struct DbusClient
{
    DbusClient(boost::asio::io_context& io,
               sdbusplus::asio::object_server& objServerIn) :
        client(io, getPolicy()), objServer(objServerIn), pollTimer(io)
    {
        startTimer();
    }
    crow::HttpClient client;

    sdbusplus::asio::object_server& objServer;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    boost::asio::steady_timer pollTimer;

    std::shared_ptr<crow::ConnectionPolicy> getPolicy()
    {
        return std::make_shared<crow::ConnectionPolicy>(crow::ConnectionPolicy{
            .maxRetryAttempts = 0,
            .maxConnections = 4,
            .retryPolicyAction = "TerminateAfterRetries"});
    }

    void startTimer()
    {
        pollTimer.expires_after(std::chrono::seconds(30));

        // TODO, should really use the shared_ptr capture model here
        pollTimer.async_wait(std::bind_front(&DbusClient::startRequest, this));
    }

    void processHttpResponse(crow::Response& res)
    {
        // TODO, this code is lifted from redfihs aggregation.  It should be
        // made common in Response We want to attempt prefix fixing regardless
        // of response code The resp will not have a json component We need to
        // create a json from resp's stringResponse
        if (!isJsonContentType(res.getHeaderValue("Content-Type")))
        {
            // Handle error?
            startTimer();
            return;
        }
        nlohmann::json jsonVal =
            nlohmann::json::parse(*res.body(), nullptr, false);
        if (jsonVal.is_discarded())
        {
            // Handle error?
            startTimer();
            return;
        }

        std::optional<std::string> uuid;
        if (!json_util::readJson(jsonVal, res, "UUID", uuid))
        {
            // Handle error?
            startTimer();
            return;
        }
        if (!uuid)
        {
            // Handle error?
            startTimer();
            return;
        }

        // If this is our first time getting a valid value, expose it to the
        // interface
        if (iface == nullptr)
        {
            // TODO Do we allow naming of this?
            iface =
                objServer.add_interface("/xyz/openbmc_project/inventory/smc",
                                        "xyz.openbmc_project.Common.UUID");
            // TODO, is null string allowed on this DBUS interface???
            iface->register_property<std::string>("UUID", "");

            iface->initialize();
        }

        iface->set_property("UUID", uuid);

        startTimer();
    }

    void startRequest(const boost::system::error_code& ec)
    {
        if (ec == boost::asio::error::operation_aborted)
        {
            // Timer aborted, probably being destroyed.
            return;
        }
        if (ec)
        {
            // TODO handle timer error  Do we rearm?
            return;
        }
        boost::beast::http::fields reqFields;
        reqFields.set(boost::beast::http::field::accept, "application/json");
        boost::urls::url url("https://127.0.0.1/redfish/v1");
        client.sendDataWithCallback(
            "", url,
            // TODO, ignoring certificates isn't really acceptable
            ensuressl::VerifyCertificate::NoVerify, reqFields,
            boost::beast::http::verb::get,
            std::bind_front(&DbusClient::processHttpResponse, this));
    }

    ~DbusClient()
    {
        objServer.remove_interface(iface);
    }
};

} // namespace redfish

int main()
{
    boost::asio::io_context io;

    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    sdbusplus::asio::object_server server(systemBus, true);
    server.add_manager("/xyz/openbmc_project/inventory");

    redfish::DbusClient(io, server);

    systemBus->request_name("xyz.openbmc_project.RedfishClient");
    io.run();
}
