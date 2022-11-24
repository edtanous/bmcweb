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

#include <app.hpp>
#include <dbus_utility.hpp>
#include <error_messages.hpp>
#include <openbmc_dbus_rest.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <task.hpp>
#include <utils/json_utils.hpp>
#include <utils/stl_utils.hpp>

namespace redfish
{

constexpr const char* rootSPDMDbusPath = "/xyz/openbmc_project/SPDM";
constexpr const char* spdmResponderIntf = "xyz.openbmc_project.SPDM.Responder";
constexpr const char* spdmInventoryIntf =
    "xyz.openbmc_project.inventory.Item.SPDMResponder";
constexpr const char* spdmBusName = "xyz.openbmc_project.SPDM";

using GetObjectType =
    std::vector<std::pair<std::string, std::vector<std::string>>>;
using SPDMCertificates = std::vector<std::tuple<uint8_t, std::string>>;
using SignedMeasurementData = std::vector<uint8_t>;
using GetManagedPropertyType =
    boost::container::flat_map<std::string, dbus::utility::DbusVariantType>;

struct SPDMMeasurementData
{
    uint8_t slot{};
    SPDMCertificates certs;
    std::string hashAlgo{};
    std::string signAlgo{};
    uint8_t version{};
    std::string measurement;
};

// Measurements for index 11 and 12 take around 17 seconds each,
// for other indices it is around 1 second.
// In a worst case scenario measurements for 254 indices will take
// 286 seconds - rounded up to 300 seconds.
static const int spdmMeasurementTimeout = 300;

static std::map<std::string, SPDMMeasurementData> spdmMeasurementData;

inline std::string getVersionStr(const uint8_t version)
{
    switch (version)
    {
        case 0x11:
            return "1.1.0";
        case 0x10:
            return "1.0.0";
        case 0x12:
            return "1.1.2";
        default:
            return "unknown";
    }
}

inline std::string stripPrefix(const std::string& str,
                               const std::string& prefix)
{
    if (str.starts_with(prefix))
    {
        return str.substr(prefix.size());
    }
    return str;
}
inline bool startsWithPrefix(const std::string& str, const std::string& prefix)
{
    if (str.rfind(prefix, 0) == 0)
    {
        return true;
    }
    return false;
}

inline SPDMMeasurementData
    parseSPDMInterfaceProperties(const GetManagedPropertyType& propMap)
{
    SPDMMeasurementData config{};
    for (const auto& property : propMap)
    {
        if (property.first == "Version")
        {
            const uint8_t* version = std::get_if<uint8_t>(&property.second);

            if (version == nullptr)
            {
                continue;
            }
            config.version = *version;
        }
        else if (property.first == "Slot")
        {
            const uint8_t* slot = std::get_if<uint8_t>(&property.second);
            if (slot == nullptr)
            {
                continue;
            }
            config.slot = *slot;
        }
        else if (property.first == "HashingAlgorithm")
        {
            const std::string* hashAlgo =
                std::get_if<std::string>(&property.second);
            if (hashAlgo == nullptr)
            {
                continue;
            }
            config.hashAlgo = stripPrefix(
                *hashAlgo,
                "xyz.openbmc_project.SPDM.Responder.HashingAlgorithms.");
        }
        else if (property.first == "SigningAlgorithm")
        {
            const std::string* signAlgo =
                std::get_if<std::string>(&property.second);
            if (signAlgo == nullptr)
            {
                continue;
            }
            config.signAlgo = stripPrefix(
                *signAlgo,
                "xyz.openbmc_project.SPDM.Responder.SigningAlgorithms.");
        }
        else if (property.first == "Certificate")
        {
            const SPDMCertificates* certs =
                std::get_if<SPDMCertificates>(&property.second);
            if (certs == nullptr)
            {
                continue;
            }
            config.certs = *certs;
        }
        else if (property.first == "SignedMeasurements")
        {
            const SignedMeasurementData* data =
                std::get_if<SignedMeasurementData>(&property.second);
            if (data == nullptr)
            {
                continue;
            }

            config.measurement.resize(data->size());
            std::copy(data->begin(), data->end(), config.measurement.begin());
            config.measurement =
                crow::utility::base64encode(config.measurement);
        }
    }
    return config;
}

/**
 * Function that retrieves all properties for SPDM Measurement object
 */
template <typename CallbackFunc>
inline void asyncGetSPDMMeasurementData(const std::string& objectPath,
                                        CallbackFunc&& callback)
{
    crow::connections::systemBus->async_method_call(
        [objectPath, callback](const boost::system::error_code ec,
                               GetManagedPropertyType& resp) {
            SPDMMeasurementData config{};
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Get all function failed for object = "
                                 << objectPath;
                callback(std::move(config), true);
                return;
            }
            config = parseSPDMInterfaceProperties(resp);
            callback(std::move(config), true);
        },
        "xyz.openbmc_project.SPDM", objectPath,
        "org.freedesktop.DBus.Properties", "GetAll", "");
}

inline void handleSPDMGETSignedMeasurement(
    const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id)
{
    std::optional<std::string> nonce;
    std::vector<uint8_t> nonceVec;
    std::optional<uint8_t> slotID;
    std::optional<std::vector<uint8_t>> indices;

    // the request body should either be empty (or pure whitespace),
    // contain an empty json, or be fully valid
    std::string body = req.body;
    body.erase(std::remove_if(body.begin(), body.end(), isspace), body.end());
    if (!body.empty() && body != "{}" &&
        !json_util::readJson(req, asyncResp->res, "Nonce", nonce, "SlotId",
                             slotID, "MeasurementIndices", indices))
    {
        messages::unrecognizedRequestBody(asyncResp->res);
        return;
    }

    // If nonce not provided by the client, the SPDM Requester shall
    // generate the nonce. if slotid not provided by the client, the value
    // shall be assumed to be `0` if "MeasurementIndices" not provided by
    // the client, the value shall be assumed to be an array containing a
    // single value of `255`
    if (nonce)
    {
        if (nonce->size() != 64)
        {
            BMCWEB_LOG_ERROR << "Invalid length for nonce hex string "
                                "(should be 64)";
            messages::actionParameterValueError(asyncResp->res, "Nonce",
                                                "SPDMGetSignedMeasurements");
            return;
        }

        try
        {
            nonceVec = redfish::stl_utils::hexStringToVector(*nonce);
        }
        catch (const std::invalid_argument& e)
        {
            BMCWEB_LOG_ERROR << "Invalid character for nonce hex string in '"
                             << *nonce << "'";
            messages::actionParameterValueError(asyncResp->res, "Nonce",
                                                "SPDMGetSignedMeasurements");
            return;
        }
    }
    if (!slotID)
    {
        BMCWEB_LOG_DEBUG << "SlotID is not given, setting it to default value";
        slotID = 0;
    }
    if (!indices)
    {
        BMCWEB_LOG_DEBUG
            << "MeasurementIndices is not given, setting it to default value";
        indices = std::vector<uint8_t>{};
        indices.value().emplace_back(255);
    }
    else if (indices->empty())
    {
        BMCWEB_LOG_ERROR << "Invalid measurement indices vector";
        messages::actionParameterValueError(
            asyncResp->res, "MeasurementIndices", "SPDMGetSignedMeasurements");
        return;
    }

    const std::string objPath = std::string(rootSPDMDbusPath) + "/" + id;
    const auto meas = spdmMeasurementData.find(objPath);
    if (meas != spdmMeasurementData.end() &&
        meas->second.measurement.size() == 0)
    {
        messages::serviceTemporarilyUnavailable(
            asyncResp->res, std::to_string(spdmMeasurementTimeout));
        BMCWEB_LOG_DEBUG
            << "Already measurement collection is going on this object"
            << objPath;
        return;
    }
    spdmMeasurementData[objPath] = SPDMMeasurementData{};

    // create a task to wait for the status property changed signal
    std::shared_ptr<task::TaskData> task = task::TaskData::createTask(
        [id, objPath](boost::system::error_code ec,
                      sdbusplus::message::message& msg,
                      const std::shared_ptr<task::TaskData>& taskData) {
            if (ec)
            {
                if (ec != boost::asio::error::operation_aborted)
                {
                    taskData->state = "Aborted";
                    taskData->messages.emplace_back(
                        messages::resourceErrorsDetectedFormatError(
                            "GetSignedMeasurement task", ec.message()));
                    taskData->finishTask();
                }
                spdmMeasurementData.erase(objPath);
                return task::completed;
            }

            std::string interface;
            std::map<std::string, dbus::utility::DbusVariantType> props;

            msg.read(interface, props);
            auto it = props.find("Status");
            if (it == props.end())
            {
                BMCWEB_LOG_DEBUG << "Did not receive an SPDM Status value";
                return !task::completed;
            }
            auto value = std::get_if<std::string>(&(it->second));
            if (!value)
            {
                BMCWEB_LOG_ERROR << "Received SPDM Status is not a string";
                return !task::completed;
            }

            if (*value ==
                "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Success")
            {
                std::string location =
                    "Location: /redfish/v1/"
                    "ComponentIntegrity/" +
                    id + "/Actions/SPDMGetSignedMeasurements/data";
                taskData->payload->httpHeaders.emplace_back(
                    std::move(location));
                taskData->state = "Completed";
                taskData->percentComplete = 100;
                taskData->messages.emplace_back(
                    messages::taskCompletedOK(std::to_string(taskData->index)));
                taskData->finishTask();
                spdmMeasurementData.erase(objPath);
                return task::completed;
            }
            if (startsWithPrefix(
                    *value,
                    "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Error_"))
            {
                BMCWEB_LOG_ERROR << "Received SPDM Error: " << *value;
                taskData->state = "Aborted";
                taskData->messages.emplace_back(
                    messages::resourceErrorsDetectedFormatError("Status",
                                                                *value));
                taskData->finishTask();
                spdmMeasurementData.erase(objPath);
                return task::completed;
            }
            // other intermediate states are ignored
            BMCWEB_LOG_DEBUG << "Ignoring SPDM Status update: " << *value;
            return !task::completed;
        },
        "type='signal',member='PropertiesChanged',"
        "interface='org.freedesktop.DBus.Properties',"
        "path='" +
            objPath +
            "',"
            "arg0=xyz.openbmc_project.SPDM.Responder");
    task->startTimer(std::chrono::seconds(spdmMeasurementTimeout));
    task->populateResp(asyncResp->res);
    task->payload.emplace(req);

    crow::connections::systemBus->async_method_call(
        [objPath, task](const boost::system::error_code ec) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Failed to refresh the SPDM measurement "
                                 << ec;
                task->state = "Aborted";
                task->messages.emplace_back(
                    messages::resourceErrorsDetectedFormatError("SPDM refresh",
                                                                ec.message()));
                task->finishTask();
                spdmMeasurementData.erase(objPath);
                return;
            }
        },
        spdmBusName, objPath, spdmResponderIntf, "Refresh", *slotID, nonceVec,
        *indices, static_cast<uint32_t>(0));
} // namespace redfish

inline void requestRoutesComponentIntegrity(App& app)
{

    BMCWEB_ROUTE(app, "/redfish/v1/ComponentIntegrity/")
        .privileges(redfish::privileges::getManagerAccountCollection)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) -> void {
                asyncResp->res.jsonValue = {
                    {"@odata.id", "/redfish/v1/ComponentIntegrity"},
                    {"@odata.type", "#ComponentIntegrityCollection."
                                    "ComponentIntegrityCollection"},
                    {"Name", "ComponentIntegrity Collection"}};

                const std::array<const char*, 1> interface = {
                    "xyz.openbmc_project.SPDM.Responder"};

                crow::connections::systemBus->async_method_call(
                    [asyncResp](
                        const boost::system::error_code ec,
                        const crow::openbmc_mapper::GetSubTreeType& subtree) {
                        if (ec)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        nlohmann::json& memberArray =
                            asyncResp->res.jsonValue["Members"];
                        memberArray = nlohmann::json::array();

                        for (const std::pair<
                                 std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                                 object : subtree)
                        {
                            sdbusplus::message::object_path objPath(
                                object.first);
                            memberArray.push_back(
                                {{"@odata.id",
                                  "/redfish/v1/ComponentIntegrity/" +
                                      objPath.filename()}});
                        }

                        asyncResp->res.jsonValue["Members@odata.count"] =
                            memberArray.size();
                    },
                    dbus_utils::mapperBusName, dbus_utils::mapperObjectPath,
                    dbus_utils::mapperIntf, "GetSubTree", rootSPDMDbusPath, 0,
                    interface);
            });

    BMCWEB_ROUTE(app, "/redfish/v1/ComponentIntegrity/<str>")
        .privileges(redfish::privileges::getManagerAccount)
        .methods(
            boost::beast::http::verb::get)([](const crow::Request&,
                                              const std::shared_ptr<
                                                  bmcweb::AsyncResp>& asyncResp,
                                              const std::string& id) -> void {
            std::string objectPath = std::string(rootSPDMDbusPath) + "/" + id;
            asyncGetSPDMMeasurementData(objectPath, [asyncResp, id, objectPath](
                                                        const SPDMMeasurementData&
                                                            data,
                                                        bool gotData) {
                if (!gotData)
                {
                    messages::internalError(asyncResp->res);
                    return;
                }

                asyncResp->res.jsonValue = {
                    {"@odata.type",
                     "#ComponentIntegrity.v1_0_0.ComponentIntegrity"},
                    {"@odata.id", "/redfish/v1/ComponentIntegrity/" + id},
                    {"Id", id},
                    {"Name", "SPDM Integrity for " + id},
                    {"ComponentIntegrityType", "SPDM"},
                    {"ComponentIntegrityEnabled", true},
                    {"SPDM",
                     {{"Requester",
                       {{"@odata.id",
                         "/redfish/v1/Managers/" PLATFORMBMCID}}}}},
                    {"Actions",
                     {{"#ComponentIntegrity.SPDMGetSignedMeasurements",
                       {{"target", "/redfish/v1/ComponentIntegrity/" + id +
                                       "/Actions/SPDMGetSignedMeasurements"},
                        {"@Redfish.ActionInfo",
                         "/redfish/v1/ComponentIntegrity/" + id +
                             "/SPDMGetSignedMeasurementsActionInfo"}}}}},
                    {"ComponentIntegrityTypeVersion",
                     getVersionStr(data.version)},
                };

                chassis_utils::getAssociationEndpoint(
                    objectPath + "/inventory_object",
                    [asyncResp](const bool& status,
                                const std::string& endpoint) {
                        if (!status)
                        {
                            BMCWEB_LOG_DEBUG
                                << "Unable to get the association endpoint";
                        }
                        sdbusplus::message::object_path erotInvObjectPath(
                            endpoint);
                        const std::string& objName =
                            erotInvObjectPath.filename();
                        std::string chassisURI =
                            (boost::format("/redfish/v1/Chassis/%s") % objName)
                                .str();
                        std::string certificateURI =
                            chassisURI + "/Certificates/CertChain";
                        asyncResp->res.jsonValue["TargetComponentURI"] =
                            chassisURI;
                        asyncResp->res
                            .jsonValue["SPDM"]["IdentityAuthentication"] = {
                            {"ResponderAuthentication",
                             {{"ComponentCertificate",
                               {{"@odata.id", certificateURI}}}}}};
                        std::string objPath = endpoint + "/inventory";
                        chassis_utils::getAssociationEndpoint(
                            objPath,
                            [objPath, asyncResp](const bool& status,
                                                 const std::string& ep) {
                                if (!status)
                                {
                                    BMCWEB_LOG_DEBUG
                                        << "Unable to get the association endpoint for "
                                        << objPath;

                                    nlohmann::json& componentsProtectedArray =
                                        asyncResp->res
                                            .jsonValue["Links"]
                                                      ["ComponentsProtected"];
                                    componentsProtectedArray =
                                        nlohmann::json::array();

                                    return;
                                }

                                chassis_utils::getRedfishURL(
                                    ep,
                                    [ep, asyncResp](const bool& status,
                                                    const std::string& url) {
                                        nlohmann::json&
                                            componentsProtectedArray =
                                                asyncResp->res.jsonValue
                                                    ["Links"]
                                                    ["ComponentsProtected"];
                                        componentsProtectedArray =
                                            nlohmann::json::array();

                                        // if (!status || url.empty()) In curent
                                        // implementation of getRedfishURL
                                        // function never returns empty URL with
                                        // status = true
                                        if (!status)
                                        {
                                            BMCWEB_LOG_DEBUG
                                                << "Unable to get the Redfish URL for "
                                                << ep;
                                            return;
                                        }

                                        componentsProtectedArray.push_back(
                                            {nlohmann::json::array(
                                                {"@odata.id", url})});
                                    });
                            });
                    });
            });
        });

    BMCWEB_ROUTE(app, "/redfish/v1/ComponentIntegrity/<str>/"
                      "Actions/SPDMGetSignedMeasurements")
        .privileges(redfish::privileges::getManagerAccount)
        .methods(boost::beast::http::verb::post)(
            handleSPDMGETSignedMeasurement);

    BMCWEB_ROUTE(app, "/redfish/v1/ComponentIntegrity/<str>/"
                      "Actions/SPDMGetSignedMeasurements/data")
        .privileges(redfish::privileges::getManagerAccount)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& id) {
                const std::string objPath =
                    std::string(rootSPDMDbusPath) + "/" + id;
                asyncGetSPDMMeasurementData(
                    objPath,
                    [asyncResp, id, objPath](const SPDMMeasurementData& config,
                                             bool gotData) {
                        if (!gotData)
                        {
                            messages::internalError(asyncResp->res);
                            return;
                        }

                        asyncResp->res.jsonValue["SignedMeasurements"] =
                            config.measurement;
                        asyncResp->res.jsonValue["Version"] =
                            getVersionStr(config.version);
                        asyncResp->res.jsonValue["HashingAlgorithm"] =
                            config.hashAlgo;
                        asyncResp->res.jsonValue["SigningAlgorithm"] =
                            config.signAlgo;
                    });
            });

    BMCWEB_ROUTE(app, "/redfish/v1/ComponentIntegrity/<str>/"
                      "SPDMGetSignedMeasurementsActionInfo")
        .privileges(redfish::privileges::getActionInfo)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& compIntegrityID) {
                asyncResp->res.jsonValue = {
                    {"@odata.type", "#ActionInfo.v1_1_2.ActionInfo"},
                    {"@odata.id", "/redfish/v1/ComponentIntegrity/" +
                                      compIntegrityID +
                                      "/SPDMGetSignedMeasurementsActionInfo"},
                    {"Name", "SPDMGetSignedMeasurementsActionInfo"},
                    {"Id", "SPDMGetSignedMeasurementsActionInfo"},
                    {"Parameters",
                     {{{"Name", "MeasurementIndices"},
                       {"Required", false},
                       {"DataType", "NumberArray"},
                       {"MinimumValue", 0},
                       {"MaximumValue", 255}},
                      {{"Name", "Nonce"},
                       {"Required", false},
                       {"DataType", "String"}},
                      {{"Name", "SlotId"},
                       {"Required", false},
                       {"DataType", "Number"},
                       {"MinimumValue", 0},
                       {"MaximumValue", 7}}}}};
            });

} // routes component integrity

} // namespace redfish
