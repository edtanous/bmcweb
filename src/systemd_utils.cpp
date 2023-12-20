#include "app.hpp"
#include "logging.hpp"

#include <sys/socket.h>
#include <systemd/sd-daemon.h>

namespace systemd_utils
{

constexpr int defaultPort = 18080;

void setupSocket(crow::App& app)
{
    int listenFd = sd_listen_fds(0);
    if (1 == listenFd)
    {
        BMCWEB_LOG_INFO("attempting systemd socket activation");
        if (sd_is_socket_inet(SD_LISTEN_FDS_START, AF_UNSPEC, SOCK_STREAM, 1,
                              0) != 0)
        {
            BMCWEB_LOG_INFO("Starting webserver on socket handle {}",
                            SD_LISTEN_FDS_START);
            app.socket(SD_LISTEN_FDS_START);
        }
        else
        {
            BMCWEB_LOG_INFO(
                "bad incoming socket, starting webserver on port {}",
                defaultPort);
            app.port(defaultPort);
        }
    }
    else
    {
        BMCWEB_LOG_INFO("Starting webserver on port {}", defaultPort);
        app.port(defaultPort);
    }
}

} // namespace systemd_utils
