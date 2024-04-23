/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
                const std::vector<std::string>&) override
    {
        asyncResp->res.result(boost::beast::http::status::not_found);
    }

    void handleUpgrade(const Request& req,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       boost::asio::ip::tcp::socket&& adaptor) override
    {
        std::shared_ptr<crow::streaming_response::ConnectionImpl<
            boost::asio::ip::tcp::socket>>
            myConnection =
                std::make_shared<crow::streaming_response::ConnectionImpl<
                    boost::asio::ip::tcp::socket>>(req, std::move(adaptor),
                                                   openHandler, messageHandler,
                                                   closeHandler, errorHandler);   
        asyncResp->res.setCompleteRequestHandler(nullptr);                                     
        myConnection->start();
    }

    void handleUpgrade(const Request& req,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
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
        asyncResp->res.setCompleteRequestHandler(nullptr);
        myConnection->start();
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
