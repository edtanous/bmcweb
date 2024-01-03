#pragma once
#include "baserule.hpp"
#include "dynamicrule.hpp"
#include <boost/beast/http/verb.hpp>

#include <memory>
#include <string>
#include <vector>

namespace crow
{

class StreamingResponseRule : public BaseRule
{
    using self_t = StreamingResponseRule;

  public:
    StreamingResponseRule(const std::string& ruleIn) : BaseRule(ruleIn)
    {}

    void validate() override
    {}

    void handle(const Request&,
                const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                const RoutingParams&) override
    {
        asyncResp->res.result(boost::beast::http::status::not_found);
    }

    void handleUpgrade(const Request& req, Response&,
                       boost::asio::ip::tcp::socket&& adaptor) override
    {
        std::shared_ptr<crow::streaming_response::ConnectionImpl<
            boost::asio::ip::tcp::socket>>
            myConnection =
                std::make_shared<crow::streaming_response::ConnectionImpl<
                    boost::asio::ip::tcp::socket>>(req, std::move(adaptor),
                                                   openHandler, messageHandler,
                                                   closeHandler, errorHandler);

        myConnection->start();
        myConnection.reset();
        myConnection = nullptr;
    }

    void handleUpgrade(const Request& req, Response&,
                       boost::beast::ssl_stream<boost::asio::ip::tcp::socket>&&
                           adaptor) override
    {
        std::shared_ptr<crow::streaming_response::ConnectionImpl<
            boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>>
            myConnection =
                std::make_shared<crow::streaming_response::ConnectionImpl<
                    boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>>(
                    req, std::move(adaptor), openHandler, messageHandler,
                    closeHandler, errorHandler);
        myConnection->start();
        myConnection.reset();
        myConnection = nullptr;
    }

    template <typename Func>
    self_t& onopen(Func f)
    {
        openHandler = f;
        return *this;
    }

    template <typename Func>
    self_t& onmessage(Func f)
    {
        messageHandler = f;
        return *this;
    }

    template <typename Func>
    self_t& onclose(Func f)
    {
        closeHandler = f;
        return *this;
    }

    template <typename Func>
    self_t& onerror(Func f)
    {
        errorHandler = f;
        return *this;
    }

  protected:
    std::function<void(crow::streaming_response::Connection&)> openHandler;
    std::function<void(crow::streaming_response::Connection&,
                       const std::string&, bool)>
        messageHandler;
    std::function<void(crow::streaming_response::Connection&)> closeHandler;
    std::function<void(crow::streaming_response::Connection&)> errorHandler;
};

}//namespace crow