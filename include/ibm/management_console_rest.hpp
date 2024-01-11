#pragma once

#include "app.hpp"
#include "http_response.hpp"

#include <string>

namespace crow
{
namespace ibm_mc
{

void requestRoutes(App& app);
bool isValidConfigFileName(const std::string& fileName, crow::Response& res);

} // namespace ibm_mc
} // namespace crow
