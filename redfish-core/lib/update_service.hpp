#pragma once
#include "app.hpp"
namespace redfish
{

struct TftpUrl
{
    std::string fwFile;
    std::string tftpServer;
};

std::optional<TftpUrl> parseTftpUrl(std::string imageURI,
                                    std::optional<std::string> transferProtocol,
                                    crow::Response& res);

void requestRoutesUpdateService(App&);
} // namespace redfish
