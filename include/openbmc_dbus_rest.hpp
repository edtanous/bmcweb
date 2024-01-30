// Copyright (c) 2018 Intel Corporation
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

#pragma once
#include "app.hpp"
namespace crow
{
namespace openbmc_mapper
{

int convertDBusToJSON(const std::string& returnType, sdbusplus::message_t& m,
                      nlohmann::json& response);

bool validateFilename(const std::string& filename);

std::vector<std::string> dbusArgSplit(const std::string& string);

void requestRoutes(App& app);

} // namespace openbmc_mapper
} // namespace crow
