#pragma once

#include "http_request.hpp"

#include <boost/asio/io_context.hpp>

void initOtel(boost::asio::io_context& io);

void sendOtel(crow::Request& request);
