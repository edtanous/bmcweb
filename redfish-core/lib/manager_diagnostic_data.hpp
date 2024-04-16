#pragma once
#include "app.hpp"
namespace redfish
{

void setBytesProperty(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                      const nlohmann::json::json_pointer& jPtr,
                      const boost::system::error_code& ec, double bytes);

void setPercentProperty(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                        const nlohmann::json::json_pointer& jPtr,
                        const boost::system::error_code& ec, double userCPU);
void requestRoutesManagerDiagnosticData(App&);
} // namespace redfish
