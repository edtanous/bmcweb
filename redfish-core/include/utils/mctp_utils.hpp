#pragma once

#include <boost/asio.hpp>
#include <openbmc_dbus_rest.hpp>
#include <utils/dbus_utils.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace redfish
{

namespace mctp_utils
{

constexpr const char* spdmResponderIntf = "xyz.openbmc_project.SPDM.Responder";
constexpr const std::string_view mctpObjectPrefix =
    "/xyz/openbmc_project/mctp/0/";

using AssociationCallback = std::function<void(
    bool /* success */,
    const std::string& /* MCTP object name OR error message */)>;
class MctpEndpoint
{
  public:
    MctpEndpoint(const std::string& spdmObject,
                 AssociationCallback&& callback) :
        spdmObj(spdmObject)
    {
        dbus::utility::findAssociations(
            spdmObject + "/transport_object",
            [this, callback{std::forward<AssociationCallback>(callback)}](
                const boost::system::error_code ec,
                std::variant<std::vector<std::string>>& association) {
                if (ec)
                {
                    callback(false, ec.message());
                    return;
                }
                std::vector<std::string>* data =
                    std::get_if<std::vector<std::string>>(&association);
                if (data == nullptr || data->empty())
                {
                    callback(false,
                             spdmObj + ": no SPDM / MCTP association found");
                    return;
                }
                mctpObj = data->front();
                if (mctpObj.rfind(mctpObjectPrefix, 0) == 0)
                {
                    try
                    {
                        mctpEid = std::stoi(
                            mctpObj.substr(mctpObjectPrefix.length()));
                        callback(true, mctpObj);
                    }
                    catch (std::invalid_argument const&)
                    {
                        callback(false, "invalid MCTP object path: " + mctpObj);
                    }
                }
                else
                {
                    callback(false, "invalid MCTP object path: " + mctpObj);
                }
            });
    }

    int getMctpEid() const
    {
        return mctpEid;
    }

    const std::string& getMctpObject() const
    {
        return mctpObj;
    }

    const std::string& getSpdmObject() const
    {
        return spdmObj;
    }

  protected:
    std::string mctpObj;
    std::string spdmObj;
    int mctpEid{-1};
};

using Endpoints = std::vector<MctpEndpoint>;
using EndpointCallback = std::function<void(const std::shared_ptr<Endpoints>&)>;
using ErrorCallback = std::function<void(
    bool /* is critical (end of operation) */,
    const std::string& /* resource / procedure associated with the error */,
    const std::string& /* error message*/)>;
void enumerateMctpEndpoints(EndpointCallback&& endpointCallback,
                            ErrorCallback&& errorCallback,
                            const std::string& spdmObjectFilter = "")
{
    std::shared_ptr<size_t> enumeratedEndpoints = std::make_shared<size_t>(0);
    crow::connections::systemBus->async_method_call(
        [enumeratedEndpoints,
         endpointCallback{std::forward<EndpointCallback>(endpointCallback)},
         errorCallback{std::forward<ErrorCallback>(errorCallback)},
         spdmObjectFilter](
            const boost::system::error_code ec,
            const crow::openbmc_mapper::GetSubTreeType& subtree) {
            const std::string desc = "SPDM / MCTP endpoint enumeration";
            BMCWEB_LOG_DEBUG << desc;
            if (ec)
            {
                errorCallback(true, desc, ec.message());
                return;
            }
            if (subtree.size() == 0)
            {
                errorCallback(true, desc, "no SPDM objects found");
                return;
            }
            if (!spdmObjectFilter.empty())
            {
                auto obj = std::find_if(
                    subtree.begin(), subtree.end(),
                    [spdmObjectFilter](const auto& object) {
                        const auto& name =
                            sdbusplus::message::object_path(object.first)
                                .filename();
                        return spdmObjectFilter.compare(name) == 0;
                    });
                if (obj != subtree.end())
                {
                    auto endpoints = std::make_shared<Endpoints>();
                    endpoints->reserve(1);
                    endpoints->emplace_back(
                        obj->first,
                        [desc, endpoints, enumeratedEndpoints, endpointCallback,
                         errorCallback](bool success, const std::string& msg) {
                            if (!success)
                            {
                                errorCallback(true, desc, msg);
                            }
                            else
                            {
                                endpointCallback(endpoints);
                            }
                        });
                }
                else
                {
                    errorCallback(true, desc,
                                  "no SPDM objects matching " +
                                      spdmObjectFilter + " found");
                }
            }
            else
            {
                auto endpoints = std::make_shared<Endpoints>();
                endpoints->reserve(subtree.size());
                for (const auto& object : subtree)
                {
                    endpoints->emplace_back(
                        object.first,
                        [desc, endpoints, enumeratedEndpoints, endpointCallback,
                         errorCallback](bool success, const std::string& msg) {
                            if (!success)
                            {
                                errorCallback(false, desc, msg);
                            }
                            *enumeratedEndpoints += 1;
                            if (*enumeratedEndpoints == endpoints->capacity())
                            {
                                std::sort(endpoints->begin(), endpoints->end(),
                                          [](const MctpEndpoint& a,
                                             const MctpEndpoint& b) {
                                              return a.getMctpEid() <
                                                     b.getMctpEid();
                                          });
                                endpointCallback(endpoints);
                            }
                        });
                }
            }
        },
        dbus_utils::mapperBusName, dbus_utils::mapperObjectPath,
        dbus_utils::mapperIntf, "GetSubTree", "/xyz/openbmc_project/SPDM", 0,
        std::array{spdmResponderIntf});
}

} // namespace mctp_utils

} // namespace redfish