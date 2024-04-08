#pragma once
#include <systemd/sd-daemon.h>

#include <boost/asio/steady_timer.hpp>
#include <common.hpp>

namespace crow
{

namespace watchdog
{

class ServiceWD
{
  public:
    ServiceWD(const int expiryTimeInS,
              std::shared_ptr<boost::asio::io_context>& io) :
        timer(*io), expiryTimeInS(expiryTimeInS)
    {
        timer.expires_after(std::chrono::seconds(expiryTimeInS));
        handler = [this](const boost::system::error_code& error) {
            if (error)
            {
                BMCWEB_LOG_ERROR("ServiceWD async_wait failed: {}",
                                 error.message());
            }
            sd_notify(0, "WATCHDOG=1");
            timer.expires_after(std::chrono::seconds(this->expiryTimeInS));
            timer.async_wait(handler);
        };
        timer.async_wait(handler);
    }

  private:
    boost::asio::steady_timer timer;
    const int expiryTimeInS;
    std::function<void(const boost::system::error_code& error)> handler;
};

} // namespace watchdog
} // namespace crow
