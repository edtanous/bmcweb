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

static std::unique_ptr<sdbusplus::bus::match::match> propMatcher;
static const int timeOut = 10;

struct SPDMMeasurementData
{
    uint8_t slot{};
    SPDMCertificates certs;
    uint8_t hashAlgo{};
    uint8_t signAlgo{};
    uint8_t version{};
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

enum class HashingAlgorithms
{
    SHA_256,
    SHA_384,
    SHA_512,
    SHA3_256,
    SHA3_384,
    SHA3_512,
    OEM,
    None,
};
enum class SigningAlgorithms
{
    ffdhe2048,
    ffdhe3072,
    ffdhe4096,
    secp256r1,
    secp384r1,
    secp521r1,
    AES_128_GCM,
    AES_256_GCM,
    CHACHA20_POLY1305,
    RSASSA_2048,
    RSAPSS_2048,
    RSASSA_3072,
    RSAPSS_3072,
    ECDSA_ECC_NIST_P256,
    RSASSA_4096,
    RSAPSS_4096,
    ECDSA_ECC_NIST_P384,
    ECDSA_ECC_NIST_P521,
    OEM,
    None,
};

inline std::string getHashAlgoStr(const HashingAlgorithms& algo)
{
    switch (algo)
    {
        case HashingAlgorithms::SHA_256:
            return "SHA_256";
        case HashingAlgorithms::SHA_384:
            return "SHA_384";
        case HashingAlgorithms::SHA_512:
            return "SHA_512";
        case HashingAlgorithms::SHA3_256:
            return "SHA3_256";
        case HashingAlgorithms::SHA3_384:
            return "SHA3_384";
        case HashingAlgorithms::SHA3_512:
            return "SHA3_512";
        case HashingAlgorithms::OEM:
            return "OEM";
        default:
            return "";
    }
}

inline std::string getSignAlgoStr(const SigningAlgorithms& algo)
{
    switch (algo)
    {
        case SigningAlgorithms::ffdhe2048:
            return "ffdhe2048";
        case SigningAlgorithms::ffdhe3072:
            return "ffdhe3072";
        case SigningAlgorithms::ffdhe4096:
            return "ffdhe4096";
        case SigningAlgorithms::secp256r1:
            return "secp256r1";
        case SigningAlgorithms::secp384r1:
            return "secp384r1";
        case SigningAlgorithms::secp521r1:
            return "secp521r1";
        case SigningAlgorithms::AES_128_GCM:
            return "AES_128_GCM";
        case SigningAlgorithms::AES_256_GCM:
            return "AES_256_GCM";
        case SigningAlgorithms::CHACHA20_POLY1305:
            return "CHACHA20_POLY1305";
        case SigningAlgorithms::RSASSA_2048:
            return "RSASSA_2048";
        case SigningAlgorithms::RSAPSS_2048:
            return "RSAPSS_2048";
        case SigningAlgorithms::RSASSA_3072:
            return "RSASSA_3072";
        case SigningAlgorithms::RSAPSS_3072:
            return "RSAPSS_3072";
        case SigningAlgorithms::ECDSA_ECC_NIST_P256:
            return "ECDSA_ECC_NIST_P256";
        case SigningAlgorithms::RSASSA_4096:
            return "RSASSA_4096";
        case SigningAlgorithms::RSAPSS_4096:
            return "RSAPSS_4096";
        case SigningAlgorithms::ECDSA_ECC_NIST_P384:
            return "ECDSA_ECC_NIST_P384";
        case SigningAlgorithms::ECDSA_ECC_NIST_P521:
            return "ECDSA_ECC_NIST_P521";
        case SigningAlgorithms::OEM:
            return "OEM";
        default:
            return "";
    }
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
                callback(std::move(config));
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
                callback(std::move(config));
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
                            const uint8_t* hashAlgo =
                                std::get_if<uint8_t>(&property.second);
                            if (hashAlgo == nullptr)
                            {
                                continue;
                            }
                            config.hashAlgo = *hashAlgo;
                        }
                        else if (property.first == "SigningAlgorithm")
                        {
                            const uint8_t* signAlgo =
                                std::get_if<uint8_t>(&property.second);
                            if (signAlgo == nullptr)
                            {
                                continue;
                            }
                            config.signAlgo = *signAlgo;
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
                    }
                }
            }
            callback(std::move(config));
        },

        spdmBusName, rootSPDMDbusPath, dbus_utils::dbusObjManagerIntf,
        "GetManagedObjects");
}

template <typename CallbackFunc>
inline void getAssociationEndpoint(const std::string& objPath,
                                   CallbackFunc&& callback)
{
    crow::connections::systemBus->async_method_call(
        [callback, objPath](const boost::system::error_code ec,
                            std::variant<std::vector<std::string>>& resp) {
            if (ec)
            {
                return; // should have associoated inventory object.
            }
            std::vector<std::string>* data =
                std::get_if<std::vector<std::string>>(&resp);
            if (data == nullptr)
            {
                // Object must have associated inventory object.
                return;
            }
            std::string endpointPath(data->front());
            callback(endpointPath);
        },
        dbus_utils::mapperBusName, objPath, dbus_utils::propertyInterface,
        "Get", dbus_utils::associationInterface, "endpoints");
}

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
                    {"Name", "ComponentIntegrity Collection"},
                    {"Description", "ComponentIntegrity Collection."}};

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
            getSPDMMeasurementData(
                objectPath,
                [asyncResp, id, objectPath](const SPDMMeasurementData& data) {
                    asyncResp->res.jsonValue = {
                        {"@odata.type",
                         "#ComponentIntegrity.v1_0_0.ComponentIntegrity"},
                        {"@odata.id", "/redfish/v1/ComponentIntegrity/" + id},
                        {"Id", id},
                        {"Name", "ComponentIntegrity"},
                        {"Description", "Measurement data for device"},
                        {"ComponentIntegrityType", "SPDM"},
                        {"ComponentIntegrityEnabled", true},
                        {"SPDM",
                         {{"Requester",
                           {{"@odata.id", "/redfish/v1/Managers/bmc"}}}}},
                        {"Actions",
                         {{"#SPDMGetSignedMeasurements",
                           {{"target",
                             {{"target",
                               "/redfish/v1/ComponentIntegrity/" + id +
                                   "/Actions/SPDMGetSignedMeasurements"}}}}},
                          {"ComponentIntegrityTypeVersion",
                           getVersionStr(data.version)}}}};

                    getAssociationEndpoint(
                        objectPath + "/inventory_object",
                        [asyncResp](const std::string endpoint) {
                            sdbusplus::message::object_path erotInvObjectPath(
                                endpoint);
                            const std::string& objName =
                                erotInvObjectPath.filename();
                            std::string chassisURI =
                                (boost::format("/redfish/v1/Chassis/%s") %
                                 objName)
                                    .str();
                            std::string certificateURI =
                                chassisURI + "Certificates/CertChain";
                            asyncResp->res.jsonValue["TargetComponentURI"] =
                                chassisURI;
                            asyncResp->res
                                .jsonValue["SPDM"]["IdentityAuthentication"] = {
                                {"ResponderAuthentication",
                                 {{"ComponentCertificate",
                                   {{"@odata.id", certificateURI}}}}}};
                        });
                });
        });

} // routes component integrity

} // namespace redfish
