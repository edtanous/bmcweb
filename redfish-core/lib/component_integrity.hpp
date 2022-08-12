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

static std::map<std::string,
                std::pair<std::shared_ptr<boost::asio::steady_timer>,
                          std::shared_ptr<sdbusplus::bus::match::match>>>
    compIntegrityMatches;

// static std::unique_ptr<sdbusplus::bus::match::match> propMatcher;
static const int timeOut = 5;

struct SPDMMeasurementData
{
    uint8_t slot{};
    SPDMCertificates certs;
    std::string hashAlgo{};
    std::string signAlgo{};
    uint8_t version{};
    std::string measurement;
};

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

/**
 * Function that retrieves all properties for SPDM Measurement object
 */
template <typename CallbackFunc>
inline void getSPDMMeasurementData(const std::string& objectPath,
                                   CallbackFunc&& callback)
{

    crow::connections::systemBus->async_method_call(
        [callback,
         objectPath](const boost::system::error_code ec,
                     const dbus::utility::ManagedObjectType& objects) {
            SPDMMeasurementData config{};
            if (ec)
            {
                BMCWEB_LOG_ERROR << "D-Bus responses error: " << ec;
                callback(std::move(config), false);
                return;
            }
            const auto objIt = std::find_if(
                objects.begin(), objects.end(),
                [objectPath](
                    const std::pair<sdbusplus::message::object_path,
                                    dbus::utility::DBusInteracesMap>& object) {
                    return !objectPath.compare(object.first);
                });

            if (objIt == objects.end())
            {
                BMCWEB_LOG_ERROR << "Dbus Object not found:" << objectPath;
                callback(std::move(config), false);
                return;
            }

            for (const auto& interface : objIt->second)
            {
                if (interface.first == "xyz.openbmc_project.SPDM.Responder")
                {
                    for (const auto& property : interface.second)
                    {
                        if (property.first == "Version")
                        {
                            const uint8_t* version =
                                std::get_if<uint8_t>(&property.second);

                            if (version == nullptr)
                            {
                                continue;
                            }
                            config.version = *version;
                        }
                        else if (property.first == "Slot")
                        {
                            const uint8_t* slot =
                                std::get_if<uint8_t>(&property.second);
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
                                std::get_if<SignedMeasurementData>(
                                    &property.second);
                            if (data == nullptr)
                            {
                                continue;
                            }

                            config.measurement.resize(data->size());
                            std::copy(data->begin(), data->end(),
                                      config.measurement.begin());
                            config.measurement =
                                crow::utility::base64encode(config.measurement);
                        }
                    }
                }
            }
            callback(std::move(config), true);
        },

        spdmBusName, rootSPDMDbusPath, dbus_utils::dbusObjManagerIntf,
        "GetManagedObjects");
}

inline void handleSPDMGETSignedMeasurement(
    const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp, const std::string& id)
{
    std::optional<std::string> nonce;
    std::vector<uint8_t> nonceVec;
    std::optional<uint8_t> slotID;
    std::optional<std::vector<uint8_t>> indices;

    // Not reading the JSON directly as if user gives the empty JSON
    // readJSOn function returns the error, in this API it is expected
    // that user may give empty JSON.

    nlohmann::json reqJson = nlohmann::json::parse(req.body, nullptr, false);
    if (!reqJson.empty())
    {
        json_util::readJson(req, asyncResp->res, "Nonce", nonce, "SlotId",
                            slotID, "MeasurementIndices", indices);
    }

    // If nonce not provided by the client, the SPDM Requester shall
    // generate the nonce. if slotid not provided by the client, the value
    // shall be assumed to be `0` if "MeasurementIndices" not provided by
    // the client, the value shall be assumed to be an array containing a
    // single value of `255`
    if (!nonce)
    {
        BMCWEB_LOG_DEBUG << "Nonce is not given, setting it to default value";
        nonce = "";
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
    nonceVec = redfish::stl_utils::getBytes(*nonce);

    const std::string objPath = std::string(rootSPDMDbusPath) + "/" + id;
    if (compIntegrityMatches.find(objPath) != compIntegrityMatches.end())
    {
        messages::serviceTemporarilyUnavailable(asyncResp->res,
                                                std::to_string(timeOut));
        BMCWEB_LOG_DEBUG
            << "Already measurement collection is going on this object"
            << objPath;
        return;
    }

    // TODO convert nonce from string into array of bytes
    // slot id from int 64 into uint8
    // array of int64 into array of uint8_t

    // The below code is creating the timer and creating the match for the
    // property changed signal for  the refesh async Dbus call.
    // Need to handle the two cases
    // - Either get the property changed signal in timeout sec with the value
    //   of the status property which we are interested in or
    // - Timer will time out and send the internal error.

    auto timer = std::make_shared<boost::asio::steady_timer>(*(req.ioService));
    timer->expires_after(std::chrono::seconds(timeOut));
    timer->async_wait(
        [asyncResp, objPath](const boost::system::error_code& ec) {
            if (ec)
            {
                // operation_aborted is expected if timer is canceled
                // before completion.
                if (ec != boost::asio::error::operation_aborted)
                {
                    BMCWEB_LOG_ERROR << "Async_wait failed " << ec;
                }
                return;
            }
            BMCWEB_LOG_ERROR
                << "Timed out waiting for Getting the SPDM Measurement Data";
            messages::operationTimeout(asyncResp->res);
            compIntegrityMatches.erase(objPath);
        });

    // create a matcher to wait for the status property changed signal
    BMCWEB_LOG_DEBUG << "create matcher with path " << objPath;
    std::string match = "type='signal',member='PropertiesChanged',"
                        "interface='org.freedesktop.DBus.Properties',"
                        "path='" +
                        objPath +
                        "',"
                        "arg0=xyz.openbmc_project.SPDM.Responder";

    auto propMatcher = std::make_shared<sdbusplus::bus::match::match>(
        *crow::connections::systemBus, match,
        [asyncResp, objPath, id](sdbusplus::message::message& msg) {
            if (msg.is_method_error())
            {
                BMCWEB_LOG_ERROR << "Dbus method error!!!";
                messages::internalError(asyncResp->res);
                return;
            }
            std::string interface;
            std::map<std::string, dbus::utility::DbusVariantType> props;

            msg.read(interface, props);
            auto it = props.find("Status");
            if (it == props.end())
            {
                BMCWEB_LOG_ERROR << "Did not receive an SPDM Status value";
                return;
            }
            auto value = std::get_if<std::string>(&(it->second));
            if (!value)
            {
                BMCWEB_LOG_ERROR << "Received SPDM Status is not a string";
                return;
            }

            if (*value ==
                "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Success")
            {
                getSPDMMeasurementData(
                    objPath,
                    [asyncResp, id, objPath](const SPDMMeasurementData& data,
                                             bool gotData) {
                        if (!gotData)
                        {
                            BMCWEB_LOG_ERROR
                                << "Did not receive SPDM Measurement data";
                            messages::internalError(asyncResp->res);
                            compIntegrityMatches.erase(objPath);
                            return;
                        }
                        asyncResp->res.jsonValue["SignedMeasurements"] =
                            data.measurement;
                        asyncResp->res.jsonValue["Version"] =
                            getVersionStr(data.version);
                        asyncResp->res.jsonValue["HashingAlgorithm"] =
                            data.hashAlgo;
                        asyncResp->res.jsonValue["SigningAlgorithm"] =
                            data.signAlgo;
                        compIntegrityMatches.erase(objPath);
                    });
            }
            else if (
                startsWithPrefix(
                    *value,
                    "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Error_"))
            {
                BMCWEB_LOG_ERROR << "Received SPDM Error: " << *value;
                if (*value ==
                    "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Error_ConnectionTimeout")
                {
                    messages::operationTimeout(asyncResp->res);
                }
                else
                {
                    messages::resourceErrorsDetectedFormatError(
                        asyncResp->res, "Status", *value);
                }
                compIntegrityMatches.erase(objPath);
            }
            else
            {
                //other intermediate states like GettingCertificates, GettingMeasurements, are ignored
                BMCWEB_LOG_DEBUG << "Ignoring SPDM Status update: " << *value;
            }
        });

    compIntegrityMatches.emplace(
        std::make_pair(objPath, std::make_pair(timer, propMatcher)));

    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Failed to refresh the SPDM measurement "
                                 << ec;
                messages::internalError(asyncResp->res);
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
            getSPDMMeasurementData(objectPath, [asyncResp, id, objectPath](
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
                                    // inventory association is not created for
                                    // HMC and PcieSwitch
                                    // if we don't get the association
                                    // assumption is, it is hmc.
                                    nlohmann::json& componentsProtectedArray =
                                        asyncResp->res
                                            .jsonValue["Links"]
                                                      ["ComponentsProtected"];
                                    componentsProtectedArray =
                                        nlohmann::json::array();
                                    componentsProtectedArray.push_back(
                                        {nlohmann::json::array(
                                            {"@odata.id",
                                             "/redfish/v1/managers/" PLATFORMBMCID})});
                                    return;
                                }

                                chassis_utils::getRedfishURL(ep, [ep,
                                                                  asyncResp](
                                                                     const bool&
                                                                         status,
                                                                     const std::
                                                                         string&
                                                                             url) {
                                    std::string redfishURL = url;
                                    if (!status)
                                    {
                                        BMCWEB_LOG_DEBUG
                                            << "Unable to get the Redfish URL for object="
                                            << ep;
                                    }
                                    else
                                    {
                                        if (url.empty())
                                        {
                                            redfishURL = std::string(
                                                "/redfish/v1/managers/" PLATFORMBMCID);
                                        }
                                    }

                                    nlohmann::json& componentsProtectedArray =
                                        asyncResp->res
                                            .jsonValue["Links"]
                                                      ["ComponentsProtected"];
                                    componentsProtectedArray =
                                        nlohmann::json::array();
                                    componentsProtectedArray.push_back(
                                        {nlohmann::json::array(
                                            {"@odata.id", redfishURL})});
                                });
                            });
                    });
            });
        });

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/ComponentIntegrity/<str>/Actions/SPDMGetSignedMeasurements")
        .privileges(redfish::privileges::getManagerAccount)
        .methods(boost::beast::http::verb::post)(
            handleSPDMGETSignedMeasurement);

    BMCWEB_ROUTE(
        app,
        "/redfish/v1/ComponentIntegrity/<str>/SPDMGetSignedMeasurementsActionInfo")
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