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

#include "background_copy.hpp"
#include "in_band.hpp"

#include <app.hpp>
#include <boost/container/flat_map.hpp>
#include <dbus_utility.hpp>
#include <registries/privilege_registry.hpp>
#include <sdbusplus/asio/property.hpp>
#include <utils/chassis_utils.hpp>
#include <utils/collection.hpp>
#include <utils/conditions_utils.hpp>
#include <utils/dbus_utils.hpp>

namespace redfish
{

namespace erot
{
constexpr char const* spdmObjectPath = "/xyz/openbmc_project/SPDM";
constexpr char const* spdmResponderIntf = "xyz.openbmc_project.SPDM.Responder";
constexpr char const* spdmServiceName = "xyz.openbmc_project.SPDM";
using SPDMCertificates = std::vector<std::tuple<uint8_t, std::string>>;

} // namespace erot

/**
 * @brief Retrieve the certificate and append to the response
 * message
 *
 * @param[in] asyncResp Shared pointer to the response message
 * @param[in] objectPath  Path of the D-Bus service object
 * @return None
 */
static void
    getChassisCertificate(const crow::Request& req,
                          const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                          const std::string& objectPath,
                          const std::string& certificateID)
{

    // 1) Get all the measurement object
    // 2) Measurement object have the association to the inventory object
    // 3) Check this is the inventory object which we are interested
    // 4) If yes, Get the certificate object from the measurement object
    // NOTE: EROT chassis will be having only one certificate at any momment of
    // time.
    crow::connections::systemBus->async_method_call(
        [req, asyncResp, objectPath,
         certificateID](const boost::system::error_code ec,
                        const dbus::utility::ManagedObjectType& objects) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "DBUS response error: " << ec;
                messages::internalError(asyncResp->res);
                return;
            }
            for (const auto& object : objects)
            {
                crow::connections::systemBus->async_method_call(
                    [req, asyncResp, object, objectPath, certificateID](
                        const boost::system::error_code ec,
                        std::variant<std::vector<std::string>>& resp) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR
                                << "Didn't find the inventory object";
                            return; // should have associoated inventory object.
                        }
                        std::vector<std::string>* data =
                            std::get_if<std::vector<std::string>>(&resp);
                        if (data == nullptr)
                        {
                            // Object must have associated inventory object.
                            return;
                        }

                        const std::string& associatedInventoryPath =
                            data->front();
                        if (objectPath == associatedInventoryPath)
                        {
                            // Certificates is of collection of slot and it's
                            // associated certificate.
                            // Slot is the index of the slot which has to be
                            // used by the SPDM.
                            const uint8_t* slot = nullptr;
                            const erot::SPDMCertificates* certs = nullptr;
                            for (const auto& interface : object.second)
                            {
                                if (interface.first == erot::spdmResponderIntf)
                                {
                                    // rest of the properties are string.
                                    for (const auto& property :
                                         interface.second)
                                    {
                                        if (property.first == "Certificate")
                                        {
                                            certs = std::get_if<
                                                erot::SPDMCertificates>(
                                                &property.second);
                                        }
                                        if (property.first == "Slot")
                                        {
                                            slot = std::get_if<uint8_t>(
                                                &property.second);
                                            BMCWEB_LOG_DEBUG << "Slot ID:"
                                                             << *slot;
                                        }
                                    }
                                }
                            }
                            // Get the desired certificated and convert it into
                            // PEM.
                            auto chassisID = std::filesystem::path(objectPath)
                                                 .filename()
                                                 .string();
                            asyncResp->res.jsonValue = {
                                {"@odata.id", req.url},
                                {"@odata.type",
                                 "#Certificate.v1_5_0.Certificate"},
                                {"Id", certificateID},
                                {"Name", chassisID + " Certificate Chain"},
                                {"CertificateType", "PEMchain"},
                                {"CertificateUsageTypes",
                                 nlohmann::json::array({"Device"})},
                                {"SPDM", {{"SlotId", *slot}}},
                            };

                            if (certs && slot && certs->size() > 0)
                            {
                                auto it = std::find_if(
                                    (*certs).begin(), (*certs).end(),
                                    [slot](
                                        const std::tuple<uint8_t, std::string>&
                                            cert) {
                                        return std::get<0>(cert) == (*slot);
                                    });
                                if (it != (*certs).end())
                                {
                                    std::cout << "Found" << std::endl;
                                }
                                std::string certStr = std::get<1>(*it);
                                asyncResp->res.jsonValue["CertificateString"] =
                                    certStr;
                            }
                        }
                    },
                    "xyz.openbmc_project.ObjectMapper",
                    std::string(object.first) + "/inventory_object",
                    "org.freedesktop.DBus.Properties", "Get",
                    "xyz.openbmc_project.Association", "endpoints");
            }
        },
        erot::spdmServiceName, erot::spdmObjectPath,
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
}

/* This function implements the OEM property under
 * chassis schema.
 * It first gets the associated ErotInventoryObject then
 * it gets the inventory backed by the Erot and finally converts
 * the Dbus inventory path to the Redfish URL.
 * path: Dbus object path
 * */

inline void getChassisOEMComponentProtected(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& path)
{

    std::string objPath = path + "/inventory";
    chassis_utils::getAssociationEndpoint(objPath, [objPath, asyncResp](
                                                       const bool& status,
                                                       const std::string& ep) {
        if (!status)
        {
            BMCWEB_LOG_DEBUG << "Unable to get the association endpoint for "
                             << objPath;
            // inventory association is not created for
            // HMC and PcieSwitch
            // if we don't get the association
            // assumption is, it is hmc.
            asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                "#NvidiaChassis.v1_0_0.NvidiaChassis";
            nlohmann::json& componentsProtectedArray =
                asyncResp->res
                    .jsonValue["Oem"]["Nvidia"]["ComponentsProtected"];
            componentsProtectedArray = nlohmann::json::array();
            componentsProtectedArray.push_back({nlohmann::json::array(
                {"@odata.id", "/redfish/v1/Managers/" PLATFORMBMCID})});

            return;
        }
        chassis_utils::getRedfishURL(
            ep, [ep, asyncResp](const bool& status, const std::string& url) {
                std::string redfishURL = url;
                if (!status)
                {
                    BMCWEB_LOG_DEBUG
                        << "Unable to get the Redfish URL for object=" << ep;
                }
                else
                {
                    if (url.empty())
                    {
                        redfishURL =
                            std::string("/redfish/v1/Managers/" PLATFORMBMCID);
                    }
                }
                asyncResp->res.jsonValue["Oem"]["Nvidia"]["@odata.type"] =
                    "#NvidiaChassis.v1_0_0.NvidiaChassis";
                nlohmann::json& componentsProtectedArray =
                    asyncResp->res
                        .jsonValue["Oem"]["Nvidia"]["ComponentsProtected"];
                componentsProtectedArray = nlohmann::json::array();
                componentsProtectedArray.push_back(
                    {nlohmann::json::array({"@odata.id", redfishURL})});
            });
    });
}

inline void getEROTChassis(const crow::Request&,
                           const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                           const std::string& chassisId)
{
    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.Inventory.Item.SPDMResponder"};

    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisId(std::string(chassisId))](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;

                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }

                if (connectionNames.size() < 1)
                {
                    BMCWEB_LOG_ERROR << "Got 0 Connection names";
                    continue;
                }

#ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                auto health = std::make_shared<HealthRollup>(
                    crow::connections::systemBus, path,
                    [asyncResp](const std::string& rootHealth,
                                const std::string& healthRollup) {
                        asyncResp->res.jsonValue["Status"]["Health"] =
                            rootHealth;
                        asyncResp->res.jsonValue["Status"]["HealthRollup"] =
                            healthRollup;
                    },
                    &health_state::ok);
                health->start();
#else  // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE
                auto health = std::make_shared<HealthPopulate>(asyncResp);

                sdbusplus::asio::getProperty<std::vector<std::string>>(
                    *crow::connections::systemBus,
                    "xyz.openbmc_project.ObjectMapper", path + "/all_sensors",
                    "xyz.openbmc_project.Association", "endpoints",
                    [health](const boost::system::error_code ec2,
                             const std::vector<std::string>& resp) {
                        if (ec2)
                        {
                            return; // no sensors = no failures
                        }
                        health->inventory = resp;
                    });

                health->populate();
#endif // ifdef BMCWEB_ENABLE_HEALTH_ROLLUP_ALTERNATIVE

                asyncResp->res.jsonValue["Status"]["State"] = "Enabled";

                asyncResp->res.jsonValue["@odata.type"] =
                    "#Chassis.v1_16_0.Chassis";
                asyncResp->res.jsonValue["@odata.id"] =
                    "/redfish/v1/Chassis/" + chassisId;
                asyncResp->res.jsonValue["Name"] = chassisId;
                asyncResp->res.jsonValue["Id"] = chassisId;
                auto certsObject = std::string("/redfish/v1/Chassis/") +
                                   chassisId + "/Certificates";

                asyncResp->res.jsonValue["Certificates"]["@odata.id"] =
                    certsObject;

                redfish::chassis_utils::getChassisUUID(
                    asyncResp, connectionNames[0].first, path, true);

                redfish::chassis_utils::getChassisLocationType(
                    asyncResp, connectionNames[0].first, path);

                redfish::chassis_utils::getChassisType(
                    asyncResp, connectionNames[0].first, path);

                redfish::chassis_utils::getChassisManufacturer(
                    asyncResp, connectionNames[0].first, path);

                redfish::chassis_utils::getChassisSerialNumber(
                    asyncResp, connectionNames[0].first, path);

                redfish::chassis_utils::getChassisSKU(
                    asyncResp, connectionNames[0].first, path);

                getChassisOEMComponentProtected(asyncResp, path);

                // Link association to parent chassis
                redfish::chassis_utils::getChassisLinksContainedBy(asyncResp,
                                                                   objPath);

                redfish::conditions_utils::populateServiceConditions(asyncResp,
                                                                     chassisId);
                return;
            }

            // Couldn't find an object with that name.  return an error
            messages::resourceNotFound(asyncResp->res,
                                       "#Chassis.v1_16_0.Chassis", chassisId);
        },

        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);
}

/**
 * Certificate resource for a chassis
 */
inline void requestRoutesEROTChassisCertificate(App& app)
{
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Certificates/<str>/")
        .privileges(redfish::privileges::getCertificate)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request& req,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID,
               const std::string& certificateID) -> void {
                redfish::chassis_utils::isEROTChassis(
                    chassisID,
                    [req, asyncResp, chassisID, certificateID](bool isEROT) {
                        if (!isEROT)
                        {
                            BMCWEB_LOG_DEBUG << "Not a EROT chassis";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        if (certificateID != "CertChain")
                        {
                            BMCWEB_LOG_DEBUG << "Not a valid Certificate ID";
                            messages::internalError(asyncResp->res);
                            return;
                        }
                        BMCWEB_LOG_DEBUG << "URL=" << req.url;

                        std::string objectPath =
                            "/xyz/openbmc_project/inventory/system/chassis/" +
                            chassisID;
                        getChassisCertificate(req, asyncResp, objectPath,
                                              certificateID);
                    });
            });

    /**
     * Collection of Chassis(EROT) certificates
     */
    BMCWEB_ROUTE(app, "/redfish/v1/Chassis/<str>/Certificates/")
        .privileges(redfish::privileges::getCertificateCollection)
        .methods(boost::beast::http::verb::get)(
            [](const crow::Request&,
               const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
               const std::string& chassisID) {
                std::string url =
                    "/redfish/v1/Chassis/" + chassisID + "/Certificates/";
                asyncResp->res.jsonValue = {
                    {"@odata.id", url},
                    {"@odata.type",
                     "#CertificateCollection.CertificateCollection"},
                    {"Name", "Certificates Collection"}};

                nlohmann::json& members = asyncResp->res.jsonValue["Members"];
                members = nlohmann::json::array();
                members.push_back({{"@odata.id", url + "CertChain"}});

                asyncResp->res.jsonValue["Members@odata.count"] =
                    members.size();
            });
}

/**
 * @brief Handles request PATCH 
 * The function set all delivered properties 
 * in request body on chassis defined in chassisId 
 * The function is designed only for chassis
 * which is ERoot
 * 
 * @param resp Async HTTP response.
 * @param asyncResp Pointer to object holding response data
 * @param chassisId  Chassis ID
 */
inline void handleEROTChassisPatch( const crow::Request& req,
                        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const std::string& chassisId)
{
    std::optional<bool> backgroundCopyEnabled;
    std::optional<bool> inBandEnabled;

    if (chassisId.empty())
    {
        return;
    }

    if (!json_util::readJsonAction(
            req, asyncResp->res, 
            "AutomaticBackgroundCopyEnabled", backgroundCopyEnabled, 
            "InbandUpdatePolicyEnabled", inBandEnabled))
    {
        return;
    }

    const std::array<const char*, 1> interfaces = {
        "xyz.openbmc_project.Inventory.Item.SPDMResponder"};

    crow::connections::systemBus->async_method_call(
        [asyncResp, chassisId(std::string(chassisId)), backgroundCopyEnabled, inBandEnabled](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            if (ec)
            {
                messages::internalError(asyncResp->res);
                return;
            }
            // Iterate over all retrieved ObjectPaths.
            for (const std::pair<std::string,
                                 std::vector<std::pair<
                                     std::string, std::vector<std::string>>>>&
                     object : subtree)
            {
                const std::string& path = object.first;
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>&
                    connectionNames = object.second;

                sdbusplus::message::object_path objPath(path);
                if (objPath.filename() != chassisId)
                {
                    continue;
                }

                const std::string& connectionName = connectionNames[0].first;

                sdbusplus::asio::getProperty<std::string>(
                        *crow::connections::systemBus, connectionName, path,
                        "xyz.openbmc_project.Common.UUID", "UUID",
                        [asyncResp, backgroundCopyEnabled, inBandEnabled](const boost::system::error_code ec,
                                    const std::string& chassisUUID) {
                            if (ec)
                            {
                                BMCWEB_LOG_DEBUG << "DBUS response error for UUID";
                                messages::internalError(asyncResp->res);
                                return;
                            }
                            
                            if(backgroundCopyEnabled.has_value())
                            {
                                redfish::chassis_utils::setBackgroundCopyEnabled(asyncResp, chassisUUID, backgroundCopyEnabled.value());
                            }

                            if(inBandEnabled.has_value())
                            {
                                redfish::chassis_utils::setInBandEnabled(asyncResp, chassisUUID, inBandEnabled.value());
                            }
                        });
                return;
            }

            // Couldn't find an object with that name.  return an error
            messages::resourceNotFound(asyncResp->res,
                                       "#Chassis.v1_16_0.Chassis", chassisId);
        },

        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory", 0, interfaces);

}


} // namespace redfish
