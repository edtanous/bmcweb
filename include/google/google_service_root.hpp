#pragma once

#include "app.hpp"
#include "async_resp.hpp"
#include "http_request.hpp"

#include <memory>

namespace crow
{
namespace google_api
{

void requestRoutes(App& app);

void handleGoogleV1Get(const crow::Request& /*req*/,
                       const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);

} // namespace google_api
} // namespace crow
