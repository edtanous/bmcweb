/*
// Copyright (c) 2022 Nvidia Corporation
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
*/
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using Json = nlohmann::json;

struct CommitImageValueEntry
{
    public:
        std::string inventoryUri;
        uint32_t mctpEndpointId = {};

        friend bool operator== ( const CommitImageValueEntry &c1, const std::string &c2 )
        {
            return c1.inventoryUri == c2;
        }
};


class CommitImageValuesManager
{
    public:

        /**
         * @brief Get all allowable values 
         * The function gets all allowable values from config file 
         * /usr/share/bmcweb/fw_mctp_mapping.json.
         * 
         * @returns Collection of CommitImageValueEntry.
         */
        std::vector<CommitImageValueEntry> getAllowableValues();

    private:
        static std::vector<CommitImageValueEntry> allowableValues;
        std::string configPath = FWMCTPMAPPINGJSON;
        
        /**
         * @brief Read config files with allowable values
         * The function gets all allowable values from config file 
         * /usr/share/bmcweb/fw_mctp_mapping.json.
         * If the file doesn't exist or if the json from the file 
         * is invalid, the function returns empty collection.
         * @returns Collection of CommitImageValueEntry.
         */
        std::vector<CommitImageValueEntry> readConfigFile();
};

std::vector<CommitImageValueEntry> CommitImageValuesManager::allowableValues;

std::vector<CommitImageValueEntry> CommitImageValuesManager::getAllowableValues()
{
    if(allowableValues.empty() == true)
    {
        allowableValues = this->readConfigFile();
    }

    return allowableValues;
}

std::vector<CommitImageValueEntry> CommitImageValuesManager::readConfigFile()
{
    
    std::vector<CommitImageValueEntry> allowablevalues;

    if (!fs::exists(this->configPath))
    {
        BMCWEB_LOG_ERROR << "The file doesn't exist: " << this->configPath;
        return allowablevalues;
    }

    std::ifstream jsonFile(this->configPath);
    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        BMCWEB_LOG_ERROR << "Unable to parse json data " << this->configPath;
        return allowablevalues;
    }

    const Json emptyJson{};

    auto entries = data.value("FwMctpMap", emptyJson);
    for (const auto& element : entries.items())
    {
        try
        {
            CommitImageValueEntry allowableVal;

            allowableVal.inventoryUri = element.key();
            allowableVal.mctpEndpointId = static_cast<uint32_t>(element.value());            

            allowablevalues.push_back(allowableVal);
        }
        catch (const std::exception& e)
        {
            BMCWEB_LOG_ERROR << "FW MCTP EID map format error.";
        }
    }

    return allowablevalues;
}