/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
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

#include "component_integrity.hpp"
#include "dbus_utility.hpp"
#include "openbmc_dbus_rest.hpp"
#include "utils/dbus_utils.hpp"
#include "utils/mctp_utils.hpp"

#include <boost/asio.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/process.hpp>
#include <boost/process/async.hpp>
#include <boost/process/child.hpp>

#include <functional>
#include <memory>
#include <variant>
#include <vector>

namespace redfish
{

namespace debug_token
{

constexpr const std::string_view debugTokenIntf = "com.nvidia.DebugToken";
constexpr const std::string_view debugTokenBasePath =
    "/xyz/openbmc_project/debug_token";
constexpr const std::string_view debugTokenOpcodesEnumPrefix =
    "com.nvidia.DebugToken.TokenOpcodes.";
constexpr const std::string_view debugTokenTypesEnumPrefix =
    "com.nvidia.DebugToken.TokenTypes.";

const std::map<std::string, int> debugTokenServiceIndex{
    {"GetDebugTokenRequest", 50},
#ifdef BMCWEB_ENABLE_DOT
    {"GetDOTCAKUnlockTokenRequest", 58}, {"GetDOTEnableTokenRequest", 59},
    {"GetDOTSignTestToken", 60},         {"GetDOTOverrideTokenRequest", 61},
#endif
};

inline int getMeasurementIndex(const std::string& requestType)
{
    auto idx = debugTokenServiceIndex.find(requestType);
    if (idx != debugTokenServiceIndex.end())
    {
        return idx->second;
    }
    return -1;
}

constexpr const int targetedOpTimeoutSeconds = 2;

// mctp-vdm-util's output size per endpoint
constexpr const size_t statusQueryOutputSize = 256;
constexpr const int statusQueryTimeoutSeconds = 60;
constexpr const size_t statusQueryResponseLength = 19;
constexpr const size_t statusQueryDebugTokenStatusOctet = 9;

enum class EndpointState
{
    None,
    StatusAcquired,
    TokenInstalled,
    RequestAcquired,
    Error,
    DebugTokenUnsupported
};

using DebugTokenEndpoint =
    std::tuple<mctp_utils::MctpEndpoint, /* endpoint data */
               std::string /* status data */,
               std::vector<uint8_t> /* request data */, EndpointState>;
using ResultCallback = std::function<void(
    const std::shared_ptr<std::vector<DebugTokenEndpoint>>&)>;
using ErrorCallback = std::function<void(
    bool /* is critical (end of operation) */,
    const std::string& /* resource / procedure associated with the error */,
    const std::string& /* error message */)>;

class OperationHandler
{
  public:
    virtual ~OperationHandler() = default;

    virtual bool update(sdbusplus::message::message&) = 0;

    virtual void getResult(std::string&) const = 0;

    OperationHandler(const OperationHandler&) = delete;
    OperationHandler(OperationHandler&&) = delete;
    OperationHandler& operator=(const OperationHandler&) = delete;
    OperationHandler& operator=(const OperationHandler&&) = delete;

  protected:
    OperationHandler() = default;

    std::shared_ptr<std::vector<DebugTokenEndpoint>> endpoints;

    ResultCallback resCallback;
    ErrorCallback errCallback;
};

class StatusQueryHandler : public OperationHandler
{
  public:
    StatusQueryHandler(ResultCallback&& resultCallback,
                       ErrorCallback&& errorCallback)
    {
        BMCWEB_LOG_DEBUG("StatusQueryHandler constructor");
        resCallback = resultCallback;
        errCallback = errorCallback;
        mctp_utils::enumerateMctpEndpoints(
            [this](const std::shared_ptr<std::vector<mctp_utils::MctpEndpoint>>&
                       mctpEndpoints) {
            if (endpoints)
            {
                return;
            }
            if (mctpEndpoints && mctpEndpoints->size() != 0)
            {
                endpoints = std::make_shared<std::vector<DebugTokenEndpoint>>();
                endpoints->reserve(mctpEndpoints->size());
                for (auto& ep : *mctpEndpoints)
                {
                    const auto& msgTypes = ep.getMctpMessageTypes();
                    if (std::find(msgTypes.begin(), msgTypes.end(),
                                  mctp_utils::mctpMessageTypeVdm) !=
                        msgTypes.end())
                    {
                        endpoints->emplace_back(std::make_tuple(
                            std::move(ep), std::string(),
                            std::vector<uint8_t>(), EndpointState::None));
                    }
                }
                endpoints->shrink_to_fit();
                getStatus();
                return;
            }
            errCallback(true, "Endpoint enumeration", "no endpoints found");
        },
            [errorCallback](bool critical, const std::string& desc,
                            const std::string& error) {
            errorCallback(critical, desc, error);
        },
            "", static_cast<uint64_t>(statusQueryTimeoutSeconds) * 1000000u);
    }

    StatusQueryHandler() = delete;
    StatusQueryHandler(const StatusQueryHandler&) = delete;
    StatusQueryHandler(StatusQueryHandler&&) = delete;
    StatusQueryHandler& operator=(const StatusQueryHandler&) = delete;
    StatusQueryHandler& operator=(const StatusQueryHandler&&) = delete;

    ~StatusQueryHandler() override = default;

    bool update(sdbusplus::message::message&) override
    {
        return false;
    }

    void getResult(std::string& result) const override
    {
        if (endpoints)
        {
            std::ostringstream output;
            for (const auto& ep : *endpoints)
            {
                const auto& status = std::get<1>(ep);
                const auto& state = std::get<3>(ep);
                if (state == EndpointState::StatusAcquired)
                {
                    output << status << std::endl;
                }
            }
            result = output.str();
        }
    }

  private:
    std::unique_ptr<boost::process::child> subprocess;
    std::unique_ptr<boost::asio::steady_timer> subprocessTimer;
    std::vector<char> subprocessOutput;

    void subprocessExitCallback(int exitCode, const std::error_code& ec)
    {
        const std::string desc = "mctp-vdm-util execution exit callback";
        BMCWEB_LOG_DEBUG("{}", desc);
        subprocessTimer.reset();
        if (ec)
        {
            BMCWEB_LOG_ERROR("{}: {}", desc, ec.message());
            errCallback(true, desc, ec.message());
            return;
        }
        if (exitCode != 0)
        {
            // If error is encountered mctp message will not have the proper
            // response for debug token, TX/RX parsing below will handle the
            // error messages
            BMCWEB_LOG_ERROR("{}: {}", desc, exitCode);
        }
        boost::interprocess::bufferstream outputStream(subprocessOutput.data(),
                                                       subprocessOutput.size());
        std::string line, rxLine, txLine;
        int currentEid = -1;
        while (std::getline(outputStream, line))
        {
            if (line.rfind("teid = ", 0) == 0)
            {
                if (currentEid != -1)
                {
                    errCallback(false, desc,
                                "No RX data for EID " +
                                    std::to_string(currentEid));
                }
                currentEid = std::stoi(line.substr(7));
                rxLine.clear();
                txLine.clear();
            }
            if (currentEid != -1 && line.rfind("RX: ", 0) == 0)
            {
                rxLine = line.substr(4);
            }
            if (currentEid != -1 && line.rfind("TX: ", 0) == 0)
            {
                txLine = line.substr(4);
            }
            if (currentEid != -1 && !rxLine.empty() && !txLine.empty())
            {
                BMCWEB_LOG_DEBUG("{} RX: {}", currentEid, rxLine);
                BMCWEB_LOG_DEBUG("{} TX: {}", currentEid, txLine);
                if (rxLine.size() > txLine.size())
                {
                    auto ep = std::find_if(endpoints->begin(), endpoints->end(),
                                           [currentEid](const auto& ep) {
                        const auto& mctpEp = std::get<0>(ep);
                        return mctpEp.getMctpEid() == currentEid;
                    });
                    if (ep != endpoints->end())
                    {
                        auto& status = std::get<1>(*ep);
                        auto& state = std::get<3>(*ep);
                        status = rxLine;
                        state = EndpointState::StatusAcquired;
                    }
                }
                else
                {
                    errCallback(false, desc,
                                "RX data too short for EID " +
                                    std::to_string(currentEid));
                }
                currentEid = -1;
            }
        }
        resCallback(endpoints);
    }

    void getStatus()
    {
        const std::string desc = "mctp-vdm-util execution";
        BMCWEB_LOG_DEBUG("{}", desc);
        subprocessTimer = std::make_unique<boost::asio::steady_timer>(
            crow::connections::systemBus->get_io_context());
        subprocessTimer->expires_after(
            std::chrono::seconds(statusQueryTimeoutSeconds));
        subprocessTimer->async_wait(
            [this, desc](const boost::system::error_code ec) {
            if (ec && ec != boost::asio::error::operation_aborted)
            {
                if (subprocess)
                {
                    subprocess.reset();
                    errCallback(true, desc, "Timeout");
                }
            }
        });

        std::ostringstream vdmCalls;
        for (const auto& endpoint : *endpoints)
        {
            const auto& mctpEp = std::get<0>(endpoint);
            const auto& mctpEid = mctpEp.getMctpEid();
            if (mctpEid != -1)
            {
                vdmCalls << "mctp-vdm-util -c debug_token_query -t "
                         << std::to_string(mctpEid) << ";";
            }
        }

        BMCWEB_LOG_DEBUG("/bin/sh -c '{}'", vdmCalls.str());
        std::vector<std::string> args = {"-c", vdmCalls.str()};
        try
        {
            subprocessOutput.resize(statusQueryOutputSize * endpoints->size());
            auto callback = [this](int exitCode, const std::error_code& ec) {
                subprocessExitCallback(exitCode, ec);
            };
            subprocess = std::make_unique<boost::process::child>(
                "/bin/sh", args, boost::process::std_err > boost::process::null,
                boost::process::std_out > boost::asio::buffer(subprocessOutput),
                crow::connections::systemBus->get_io_context(),
                boost::process::on_exit = std::move(callback));
        }
        catch (const std::runtime_error& e)
        {
            errCallback(true, desc, e.what());
        }
    }
};

#pragma pack(1)
struct ServerRequestHeader
{
    /* Versioning for token request structure (0x0001) */
    uint16_t version;
    /* Size of the token request structure */
    uint16_t size;
};
#pragma pack()

#pragma pack(1)
struct FileHeader
{
    static constexpr uint8_t typeTokenRequest = 1;
    static constexpr uint8_t typeDebugToken = 2;

    /* Set to 0x01 */
    uint8_t version;
    /* Either 1 for token request or 2 for token data */
    uint8_t type;
    /* Count of stored debug tokens / requests */
    uint16_t numberOfRecords;
    /* Equal to sizeof(struct FileHeader) for version 0x01. */
    uint16_t offsetToListOfStructs;
    /* Equal to sum of sizes of a given structure type +
     * sizeof(struct FileHeader) */
    uint32_t fileSize;
    /* Padding */
    std::array<uint8_t, 6> reserved;
};
#pragma pack()

class RequestHandler : public OperationHandler
{
  public:
    RequestHandler(ResultCallback&& resultCallback,
                   ErrorCallback&& errorCallback, int index) :
        spdmMeasurementIndex{static_cast<uint8_t>(index)}
    {
        BMCWEB_LOG_DEBUG("RequestHandler constructor");
        resCallback = resultCallback;
        errCallback = errorCallback;
        statusHandler = std::make_unique<StatusQueryHandler>(
            [this](const std::shared_ptr<std::vector<DebugTokenEndpoint>>&
                       endpoints) {
            if (!endpoints || endpoints->size() == 0)
            {
                errCallback(true, "Debug token status check",
                            "No valid endpoints");
                return;
            }
            this->endpoints = endpoints;
            std::vector<uint8_t> indices{spdmMeasurementIndex};
            bool refreshIssued = false;
            for (auto& endpoint : *endpoints)
            {
                const auto& mctpEp = std::get<0>(endpoint);
                const auto& status = std::get<1>(endpoint);
                const auto& state = std::get<3>(endpoint);
                const auto& spdmObject = mctpEp.getSpdmObject();
                const std::string desc = "SPDM refresh call for " + spdmObject;
                BMCWEB_LOG_DEBUG("{}", desc);
                if (spdmObject.empty() ||
                    state != EndpointState::StatusAcquired || status.empty())
                {
                    errCallback(false, desc, "invalid endpoint state");
                    continue;
                }
                std::istringstream iss(status);
                std::vector<std::string> tokens{
                    std::istream_iterator<std::string>{iss},
                    std::istream_iterator<std::string>{}};
                if (tokens.size() == statusQueryResponseLength &&
                    tokens[statusQueryDebugTokenStatusOctet] ==
                        std::string("00"))
                {
                    crow::connections::systemBus->async_method_call(
                        [this, desc,
                         &endpoint](const boost::system::error_code ec) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR("{}: {}", desc, ec.message());
                            errCallback(false, desc, ec.message());
                            auto& state = std::get<3>(endpoint);
                            state = EndpointState::Error;
                            finalize();
                        }
                    },
                        spdmBusName, spdmObject, spdmResponderIntf, "Refresh",
                        static_cast<uint8_t>(0), std::vector<uint8_t>(),
                        indices, static_cast<uint32_t>(0));
                    refreshIssued = true;
                }
                else
                {
                    auto& state = std::get<3>(endpoint);
                    state = EndpointState::TokenInstalled;
                }
            }
            if (!refreshIssued)
            {
                // debug token is already installed for all endpoints
                resCallback(endpoints);
            }
        },
            [errorCallback](bool critical, const std::string& desc,
                            const std::string& error) {
            errorCallback(critical, desc, error);
        });
    }

    RequestHandler() = delete;
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler(RequestHandler&&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&&) = delete;

    ~RequestHandler() override = default;

    bool update(sdbusplus::message::message& msg) override
    {
        if (endpoints)
        {
            std::string interface;
            std::map<std::string, dbus::utility::DbusVariantType> props;
            msg.read(interface, props);
            if (interface == spdmResponderIntf)
            {
                auto it = props.find("Status");
                if (it != props.end())
                {
                    auto status = std::get_if<std::string>(&(it->second));
                    if (status)
                    {
                        statusUpdate(msg.get_path(), *status);
                    }
                }
            }
            for (const auto& ep : *endpoints)
            {
                const auto& state = std::get<3>(ep);
                if (state == EndpointState::StatusAcquired)
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    void getResult(std::string& result) const override
    {
        size_t size = 0;
        uint16_t recordCount = 0;
        if (endpoints)
        {
            for (const auto& ep : *endpoints)
            {
                const auto& state = std::get<3>(ep);
                if (state == EndpointState::RequestAcquired)
                {
                    const auto& request = std::get<2>(ep);
                    size += request.size();
                    ++recordCount;
                }
            }
        }

        if (size != 0)
        {
            size += sizeof(FileHeader);
            auto header = std::make_unique<FileHeader>();
            header->version = 0x01;
            header->type = FileHeader::typeTokenRequest;
            header->numberOfRecords = recordCount;
            header->offsetToListOfStructs = sizeof(FileHeader);
            header->fileSize = static_cast<uint32_t>(size);

            std::vector<uint8_t> output;
            output.reserve(size);
            output.resize(sizeof(FileHeader));
            std::memcpy(output.data(), header.get(), sizeof(FileHeader));
            for (const auto& ep : *endpoints)
            {
                const auto& state = std::get<3>(ep);
                if (state == EndpointState::RequestAcquired)
                {
                    const auto& request = std::get<2>(ep);
                    output.insert(output.end(), request.begin(), request.end());
                }
            }
            result = std::string(output.begin(), output.end());
        }
    }

  private:
    std::unique_ptr<StatusQueryHandler> statusHandler;

    uint8_t spdmMeasurementIndex;

    void statusUpdate(const std::string& object, const std::string& status)
    {
        const std::string desc = "Update of " + object +
                                 " object with status " + status;
        BMCWEB_LOG_DEBUG("{}", desc);
        auto endpoint = std::find_if(endpoints->begin(), endpoints->end(),
                                     [object](const auto& ep) {
            const auto& mctpEp = std::get<0>(ep);
            return mctpEp.getSpdmObject() == object;
        });
        if (endpoint == endpoints->end())
        {
            errCallback(false, desc, "unknown object");
            return;
        }
        auto& state = std::get<3>(*endpoint);
        if (state == EndpointState::Error)
        {
            errCallback(false, desc, "invalid object");
            return;
        }
        if (state == EndpointState::RequestAcquired)
        {
            BMCWEB_LOG_INFO("{} data already present", desc);
        }
        else if (status ==
                 "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Success")
        {
            crow::connections::systemBus->async_method_call(
                [this, endpoint](
                    const boost::system::error_code ec,
                    const boost::container::flat_map<
                        std::string, dbus::utility::DbusVariantType>& props) {
                const auto& mctpEp = std::get<0>(*endpoint);
                const auto& spdmObject = mctpEp.getSpdmObject();
                auto& request = std::get<2>(*endpoint);
                auto& state = std::get<3>(*endpoint);
                const std::string desc = "Reading properties of " + spdmObject +
                                         " object";
                BMCWEB_LOG_DEBUG("{}", desc);
                if (ec)
                {
                    BMCWEB_LOG_ERROR("{}: {}", desc, ec.message());
                    errCallback(false, desc, ec.message());
                    state = EndpointState::Error;
                    finalize();
                    return;
                }
                auto itSign = props.find("SignedMeasurements");
                if (itSign == props.end())
                {
                    errCallback(false, desc,
                                "cannot find SignedMeasurements property");
                    state = EndpointState::Error;
                    finalize();
                    return;
                }
                auto sign = std::get_if<std::vector<uint8_t>>(&itSign->second);
                if (!sign)
                {
                    errCallback(false, desc,
                                "cannot decode SignedMeasurements property");
                    state = EndpointState::Error;
                    finalize();
                    return;
                }
                auto itCaps = props.find("Capabilities");
                if (itCaps == props.end())
                {
                    errCallback(false, desc,
                                "cannot find Capabilities property");
                    state = EndpointState::Error;
                    finalize();
                    return;
                }
                auto caps = std::get_if<uint32_t>(&itCaps->second);
                if (!caps)
                {
                    errCallback(false, desc,
                                "cannot decode Capabilities property");
                    state = EndpointState::Error;
                    finalize();
                    return;
                }
                std::string pem;
                if (*caps & spdmCertCapability)
                {
                    auto itCert = props.find("Certificate");
                    if (itCert == props.end())
                    {
                        errCallback(false, desc,
                                    "cannot find Certificate property");
                        state = EndpointState::Error;
                        finalize();
                        return;
                    }
                    auto cert = std::get_if<
                        std::vector<std::tuple<uint8_t, std::string>>>(
                        &itCert->second);
                    if (!cert)
                    {
                        errCallback(false, desc,
                                    "cannot decode Certificate property");
                        state = EndpointState::Error;
                        finalize();
                        return;
                    }
                    auto certSlot = std::find_if(
                        cert->begin(), cert->end(),
                        [](const auto& e) { return std::get<0>(e) == 0; });
                    if (certSlot == cert->end())
                    {
                        errCallback(false, desc,
                                    "cannot find certificate for slot 0");
                        state = EndpointState::Error;
                        finalize();
                        return;
                    }
                    pem = std::get<1>(*certSlot);
                }
                size_t size = sizeof(ServerRequestHeader) + sign->size() +
                              pem.size();
                auto header = std::make_unique<ServerRequestHeader>();
                header->version = 0x0001;
                header->size = static_cast<uint16_t>(size);
                request.reserve(size);
                request.resize(sizeof(ServerRequestHeader));
                std::memcpy(request.data(), header.get(),
                            sizeof(ServerRequestHeader));
                request.insert(request.end(), sign->begin(), sign->end());
                request.insert(request.end(), pem.begin(), pem.end());
                state = EndpointState::RequestAcquired;
                finalize();
                return;
            },
                spdmBusName, object, "org.freedesktop.DBus.Properties",
                "GetAll", spdmResponderIntf);
        }
        else if (startsWithPrefix(
                     status,
                     "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Error_"))
        {
            errCallback(false, desc, status);
            state = EndpointState::Error;
        }
        finalize();
    }

    void finalize()
    {
        const std::string desc = "Debug token request data processing";
        BMCWEB_LOG_DEBUG("{}", desc);
        int completedRequestsCount = 0;
        for (const auto& ep : *endpoints)
        {
            const auto& state = std::get<3>(ep);
            if (state == EndpointState::StatusAcquired)
            {
                return;
            }
            if (state == EndpointState::RequestAcquired ||
                state == EndpointState::TokenInstalled)
            {
                ++completedRequestsCount;
            }
        }
        if (completedRequestsCount > 0)
        {
            resCallback(endpoints);
        }
        else
        {
            errCallback(true, desc, "No valid debug token request responses");
        }
    }
};

enum class TargetedOperation
{
    DisableTokens,
    GenerateTokenRequest,
    GetTokenStatus,
    InstallToken
};

using TargetedOperationArgument =
    std::variant<std::monostate, std::string, std::vector<uint8_t>>;

class TargetedOperationHandler
{
  public:
    TargetedOperationHandler(const std::string& chassisId,
                             const std::shared_ptr<bmcweb::AsyncResp>& aResp,
                             TargetedOperation op,
                             TargetedOperationArgument arg = std::monostate()) :
        operation(op),
        argument(arg), asyncResp(aResp)
    {
        constexpr std::array<std::string_view, 1> interfaces = {debugTokenIntf};
        dbus::utility::getSubTree(
            std::string(debugTokenBasePath), 0, interfaces,
            [this,
             chassisId](const boost::system::error_code& ec,
                        const dbus::utility::MapperGetSubTreeResponse& resp) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("GetSubTreePaths error: {}", ec);
                errorHandler();
                return;
            }
            if (resp.size() == 0)
            {
                BMCWEB_LOG_ERROR("No objects with DebugToken interface found");
                errorHandler();
                return;
            }
            std::string objectPath;
            std::string service;
            for (const auto& [path, serviceMap] : resp)
            {
                if (path.find(chassisId) != std::string::npos)
                {
                    objectPath = path;
                    service = serviceMap[0].first;
                    break;
                }
            }
            if (objectPath.empty())
            {
                BMCWEB_LOG_ERROR("DebugToken interface not implemented for {}",
                                 chassisId);
                asyncResp->res.result(boost::beast::http::status::not_found);
                cleanup();
                return;
            }
            tokenOperationTimer = std::make_unique<boost::asio::steady_timer>(
                crow::connections::systemBus->get_io_context());
            tokenOperationTimer->expires_after(
                std::chrono::seconds(targetedOpTimeoutSeconds));
            tokenOperationTimer->async_wait(
                [this](const boost::system::error_code& ec) {
                match.reset(nullptr);
                if (!ec)
                {
                    BMCWEB_LOG_ERROR("Debug token operation timeout");
                    errorHandler();
                    return;
                }
                if (ec != boost::asio::error::operation_aborted)
                {
                    BMCWEB_LOG_ERROR("async_wait error {}", ec);
                    errorHandler();
                }
            });
            std::string matchRule("type='signal',"
                                  "interface='org.freedesktop.DBus.Properties',"
                                  "path='" +
                                  objectPath +
                                  "',"
                                  "member='PropertiesChanged'");
            match = std::make_unique<sdbusplus::bus::match_t>(
                *crow::connections::systemBus, matchRule,
                [this, objectPath, service](sdbusplus::message_t& msg) {
                std::string interface;
                std::map<std::string, dbus::utility::DbusVariantType> props;
                msg.read(interface, props);
                std::string opStatus;
                if (interface == "xyz.openbmc_project.Common.Progress")
                {
                    auto it = props.find("Status");
                    if (it != props.end())
                    {
                        auto status = std::get_if<std::string>(&(it->second));
                        if (status)
                        {
                            opStatus = *status;
                        }
                    }
                }
                if (opStatus.empty())
                {
                    return;
                }
                tokenOperationTimer.reset(nullptr);
                if (opStatus !=
                    "xyz.openbmc_project.Common.Progress.OperationStatus.Completed")
                {
                    BMCWEB_LOG_ERROR("End status: {}", opStatus);
                    errorHandler();
                    return;
                }
                switch (operation)
                {
                    case TargetedOperation::GenerateTokenRequest:
                        requestHandler(objectPath, service);
                        return;

                    case TargetedOperation::GetTokenStatus:
                        statusHandler(objectPath, service);
                        return;

                    default:
                        genericHandler();
                        return;
                }
            });
            auto dbusErrorHandler =
                [this](const boost::system::error_code& ec) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR("DBus error: {}", ec.message());
                    errorHandler();
                }
            };
            switch (operation)
            {
                case TargetedOperation::DisableTokens:
                {
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "DisableTokens");
                    break;
                }

                case TargetedOperation::GenerateTokenRequest:
                {
                    std::string* tokenOpcode =
                        std::get_if<std::string>(&argument);
                    if (!tokenOpcode)
                    {
                        BMCWEB_LOG_ERROR("Invalid argument");
                        errorHandler();
                        return;
                    }
                    std::string arg = std::string(debugTokenOpcodesEnumPrefix) +
                                      *tokenOpcode;
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "GetRequest", arg);
                    break;
                }

                case TargetedOperation::GetTokenStatus:
                {
                    std::string* tokenType =
                        std::get_if<std::string>(&argument);
                    if (!tokenType)
                    {
                        BMCWEB_LOG_ERROR("Invalid argument");
                        errorHandler();
                        return;
                    }
                    std::string arg = std::string(debugTokenTypesEnumPrefix) +
                                      *tokenType;
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "GetStatus", arg);
                    break;
                }

                case TargetedOperation::InstallToken:
                {
                    std::vector<uint8_t>* token =
                        std::get_if<std::vector<uint8_t>>(&argument);
                    if (!token)
                    {
                        BMCWEB_LOG_ERROR("Invalid argument");
                        errorHandler();
                        return;
                    }
                    crow::connections::systemBus->async_method_call(
                        dbusErrorHandler, service, objectPath,
                        std::string(debugTokenIntf), "InstallToken", *token);
                    break;
                }

                default:
                {
                    BMCWEB_LOG_ERROR("Invalid token operation");
                    errorHandler();
                    break;
                }
            }
        });
    }

    bool finished(int& timeout) const
    {
        BMCWEB_LOG_DEBUG("asyncResp: {}, match: {}, timeout: {}",
                         static_cast<bool>(asyncResp), static_cast<bool>(match),
                         static_cast<bool>(tokenOperationTimer));
        bool isFinished = !asyncResp && !match && !tokenOperationTimer;
        timeout = isFinished ? 0 : targetedOpTimeoutSeconds;
        return isFinished;
    }

  private:
    TargetedOperation operation;
    TargetedOperationArgument argument;
    std::shared_ptr<bmcweb::AsyncResp> asyncResp;

    std::unique_ptr<boost::asio::steady_timer> tokenOperationTimer;
    std::unique_ptr<sdbusplus::bus::match_t> match;

    void requestHandler(const std::string& objectPath,
                        const std::string& service)
    {
        sdbusplus::asio::getProperty<sdbusplus::message::unix_fd>(
            *crow::connections::systemBus, service, objectPath,
            std::string(debugTokenIntf), "RequestFd",
            [this](const boost::system::error_code ec,
                   const sdbusplus::message::unix_fd& unixfd) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBus Get error {}", ec);
                errorHandler();
                return;
            }
            BMCWEB_LOG_DEBUG("Received fd: {}", unixfd.fd);
            std::unique_ptr<FILE, int (*)(FILE*)> file(fdopen(unixfd.fd, "rb"),
                                                       &fclose);
            if (!file)
            {
                BMCWEB_LOG_ERROR("fdopen error");
                errorHandler();
                return;
            }
            int rc = fseek(file.get(), 0, SEEK_END);
            if (rc < 0)
            {
                BMCWEB_LOG_ERROR("fseek error: {}", rc);
                errorHandler();
                return;
            }
            auto filesize = ftell(file.get());
            if (filesize <= 0)
            {
                BMCWEB_LOG_ERROR("ftell error or size is zero: {}", filesize);
                errorHandler();
                return;
            }
            rewind(file.get());
            size_t size = static_cast<size_t>(filesize);
            std::unique_ptr<char[]> buffer(new char[size]);
            auto len = fread(buffer.get(), 1, size, file.get());
            if (len != size)
            {
                BMCWEB_LOG_ERROR("fread error or length is invalid: {}", len);
                errorHandler();
                return;
            }
            std::string_view binaryData(buffer.get(), len);
            asyncResp->res.jsonValue["Token"] =
                crow::utility::base64encode(binaryData);
            cleanup();
        });
    }

    void statusHandler(const std::string& objectPath,
                       const std::string& service)
    {
        sdbusplus::asio::getProperty<
            std::tuple<std::string, std::string, std::string, uint32_t>>(
            *crow::connections::systemBus, service, objectPath,
            std::string(debugTokenIntf), "TokenStatus",
            [this](const boost::system::error_code ec,
                   const std::tuple<std::string, std::string, std::string,
                                    uint32_t>& status) {
            if (ec)
            {
                BMCWEB_LOG_ERROR("DBus Get error {}", ec);
                errorHandler();
                return;
            }
            auto& dbusTokenType = std::get<0>(status);
            auto& dbusStatus = std::get<1>(status);
            auto& dbusInfo = std::get<2>(status);
            try
            {
                asyncResp->res.jsonValue["TokenType"] =
                    dbusTokenType.substr(dbusTokenType.find_last_of('.') + 1);
                asyncResp->res.jsonValue["Status"] =
                    dbusStatus.substr(dbusStatus.find_last_of('.') + 1);
                asyncResp->res.jsonValue["AdditionalInfo"] =
                    dbusInfo.substr(dbusInfo.find_last_of('.') + 1);
                asyncResp->res.jsonValue["TimeLeft"] = std::get<3>(status);
            }
            catch (...)
            {
                BMCWEB_LOG_ERROR("Invalid token status: {} {} {}",
                                 std::get<1>(status), std::get<2>(status),
                                 std::get<3>(status));
                errorHandler();
                return;
            }
            cleanup();
        });
    }

    void genericHandler()
    {
        messages::success(asyncResp->res);
        cleanup();
    }

    void errorHandler()
    {
        messages::internalError(asyncResp->res);
        cleanup();
    }

    void cleanup()
    {
        asyncResp = nullptr;
        boost::asio::post(crow::connections::systemBus->get_io_context(),
                          [this] {
            match.reset(nullptr);
            tokenOperationTimer.reset(nullptr);
        });
    }
};

} // namespace debug_token

} // namespace redfish
