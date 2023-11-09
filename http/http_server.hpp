#pragma once

#include "file_watcher.hpp"
#include "http_connection.hpp"
#include "logging.hpp"
#include "lsp.hpp"
#include "ssl_key_handler.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <utility>
#include <vector>

namespace crow
{

template <typename Handler, typename Adaptor = boost::asio::ip::tcp::socket>
class Server
{
  public:
    Server(Handler* handlerIn,
           std::unique_ptr<boost::asio::ip::tcp::acceptor>&& acceptorIn,
           [[maybe_unused]] const std::shared_ptr<boost::asio::ssl::context>&
               adaptorCtxIn,
           std::shared_ptr<boost::asio::io_context> io =
               std::make_shared<boost::asio::io_context>()) :
        ioService(std::move(io)),
        acceptor(std::move(acceptorIn)),
        signals(*ioService, SIGINT, SIGTERM, SIGHUP), timer(*ioService),
        fileWatcher(), handler(handlerIn), adaptorCtx(std::move(adaptorCtx))
    {}

    Server(Handler* handlerIn, const std::string& bindaddr, uint16_t port,
           const std::shared_ptr<boost::asio::ssl::context>& adaptorCtxIn,
           const std::shared_ptr<boost::asio::io_context>& io =
               std::make_shared<boost::asio::io_context>()) :
        Server(handlerIn,
               std::make_unique<boost::asio::ip::tcp::acceptor>(
                   *io, boost::asio::ip::tcp::endpoint(
                            boost::asio::ip::make_address(bindaddr), port)),
               adaptorCtxIn, io)
    {}

    Server(Handler* handlerIn, int existingSocket,
           const std::shared_ptr<boost::asio::ssl::context>& adaptorCtxIn,
           const std::shared_ptr<boost::asio::io_context>& io =
               std::make_shared<boost::asio::io_context>()) :
        Server(handlerIn,
               std::make_unique<boost::asio::ip::tcp::acceptor>(
                   *io, boost::asio::ip::tcp::v6(), existingSocket),
               adaptorCtxIn, io)
    {}

    void updateDateStr()
    {
        time_t lastTimeT = time(nullptr);
        tm myTm{};

        gmtime_r(&lastTimeT, &myTm);

        dateStr.resize(100);
        size_t dateStrSz = strftime(&dateStr[0], 99,
                                    "%a, %d %b %Y %H:%M:%S GMT", &myTm);
        dateStr.resize(dateStrSz);
    }

    void run()
    {
        BMCWEB_LOG_INFO << "Server<Handler,Adaptor>::run()";
        loadCertificate();
        watchCertificateChange();
        updateDateStr();

        getCachedDateStr = [this]() -> std::string {
            static std::chrono::time_point<std::chrono::steady_clock>
                lastDateUpdate = std::chrono::steady_clock::now();
            if (std::chrono::steady_clock::now() - lastDateUpdate >=
                std::chrono::seconds(10))
            {
                lastDateUpdate = std::chrono::steady_clock::now();
                updateDateStr();
            }
            return this->dateStr;
        };

        BMCWEB_LOG_INFO << "bmcweb server is running, local endpoint "
                        << acceptor->local_endpoint().address().to_string();
        startAsyncWaitForSignal();
        doAccept();
    }

    void loadCertificate()
    {
#ifdef BMCWEB_ENABLE_SSL
        if constexpr (std::is_same<Adaptor,
                                   boost::beast::ssl_stream<
                                       boost::asio::ip::tcp::socket>>::value)
        {
            namespace fs = std::filesystem;
            // Cleanup older certificate file existing in the system
            fs::path oldCert = "/home/root/server.pem";
            if (fs::exists(oldCert))
            {
                fs::remove("/home/root/server.pem");
            }
            fs::path certPath = "/etc/ssl/certs/https/";
            // if path does not exist create the path so that
            // self signed certificate can be created in the
            // path
            if (!fs::exists(certPath))
            {
                fs::create_directories(certPath);
            }
            fs::path certFile = certPath / "server.pem";
            BMCWEB_LOG_INFO << "Building SSL Context file=" << certFile;
            std::string sslPemFile(certFile);
            std::vector<char>& pwd = lsp::getLsp();
            ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
                sslPemFile, &pwd, lsp::passwordCallback);
            std::shared_ptr<boost::asio::ssl::context> sslContext =
                ensuressl::getSslContext(sslPemFile);
            adaptorCtx = sslContext;
            handler->ssl(std::move(sslContext));
        }
#endif
    }

    bool fileHasCredentials(const std::string& filename)
    {
        FILE* fp = fopen(filename.c_str(), "r");
        if (fp == nullptr)
        {
            BMCWEB_LOG_ERROR << "Cannot open filename for reading: " << filename
                             << "\n";
            return false;
        }
        BMCWEB_LOG_INFO << "Opened " << filename << "\n";
        return PEM_read_PrivateKey(fp, nullptr, lsp::passwordCallback,
                                   nullptr) != nullptr;
    }

    void ensureCredentialsAreEncrypted(const std::string& filename)
    {
        bool isEncrypted = false;
        asn1::pemPkeyIsEncrypted(filename, &isEncrypted);
        if (!isEncrypted)
        {
            BMCWEB_LOG_INFO << "Credentials are not encrypted, encrypting.\n";
            std::vector<char>& pwd = lsp::getLsp();
            ensuressl::encryptCredentials(filename, &pwd);
        }
    }

    void watchCertificateChange()
    {
        fileWatcher.setup(ioService);
        fileWatcher.addPath("/etc/ssl/certs/https/", IN_CLOSE_WRITE);
        fileWatcher.watch([&](const std::vector<FileWatcherEvent>& events) {
            for (const auto& ev : events)
            {
                std::string filename = ev.path + ev.name;
                if (fileHasCredentials(filename))
                {
                    BMCWEB_LOG_INFO << "Written file has credentials.";
                    ensureCredentialsAreEncrypted(filename);
                }
            }
        });
    }

    void startAsyncWaitForSignal()
    {
        signals.async_wait(
            [this](const boost::system::error_code& ec, int signalNo) {
            if (ec)
            {
                BMCWEB_LOG_INFO << "Error in signal handler" << ec.message();
            }
            else
            {
                if (signalNo == SIGHUP)
                {
                    BMCWEB_LOG_INFO << "Receivied reload signal";
                    loadCertificate();
                    boost::system::error_code ec2;
                    acceptor->cancel(ec2);
                    if (ec2)
                    {
                        BMCWEB_LOG_ERROR
                            << "Error while canceling async operations:"
                            << ec2.message();
                    }
                    this->startAsyncWaitForSignal();
                }
                else
                {
                    stop();
                }
            }
        });
    }

    void stop()
    {
        ioService->stop();
    }

    void doAccept()
    {
        boost::asio::steady_timer timer(*ioService);
        std::shared_ptr<Connection<Adaptor, Handler>> connection;
        if constexpr (std::is_same<Adaptor,
                                   boost::beast::ssl_stream<
                                       boost::asio::ip::tcp::socket>>::value)
        {
            connection = std::make_shared<Connection<Adaptor, Handler>>(
                handler, std::move(timer), getCachedDateStr,
                Adaptor(*ioService, *adaptorCtx));
        }
        else
        {
            connection = std::make_shared<Connection<Adaptor, Handler>>(
                handler, std::move(timer), getCachedDateStr,
                Adaptor(*ioService));
        }
        acceptor->async_accept(
            boost::beast::get_lowest_layer(connection->socket()),
            [this, connection](boost::system::error_code ec) {
            if (!ec)
            {
                boost::asio::post(*this->ioService,
                                  [connection] { connection->start(); });
            }
            doAccept();
        });
    }

  private:
    std::shared_ptr<boost::asio::io_context> ioService;
    std::function<std::string()> getCachedDateStr;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    boost::asio::signal_set signals;
    boost::asio::steady_timer timer;
    InotifyFileWatcher fileWatcher;

    std::string dateStr;

    Handler* handler;

    std::shared_ptr<boost::asio::ssl::context> adaptorCtx;
};
} // namespace crow
