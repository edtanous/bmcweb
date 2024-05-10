#pragma once
#include "bmcweb_config.h"

#include "async_resp.hpp"
#include "authentication.hpp"
#include "complete_response_fields.hpp"
#include "http2_connection.hpp"
#include "http_body.hpp"
#include "http_response.hpp"
#include "http_utility.hpp"
#include "logging.hpp"
#include "mutual_tls.hpp"
#include "persistent_data.hpp"
#include "ssl_key_handler.hpp"
#include "str_utility.hpp"
#include "utility.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url/url_view.hpp>
#include <json_html_serializer.hpp>
#include <redfish_util.hpp>
#include <security_headers.hpp>
#include <ssl_key_handler.hpp>

#include <atomic>
#include <chrono>
#include <vector>

namespace crow
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static int connectionCount = 0;

// request body limit size set by the bmcwebHttpReqBodyLimitMb option
constexpr uint64_t httpReqBodyLimit = 1024UL * 1024UL *
                                      bmcwebHttpReqBodyLimitMb;

constexpr uint64_t loggedOutPostBodyLimit = 4096;

constexpr uint32_t httpHeaderLimit = 8192;

template <typename>
struct IsTls : std::false_type
{};

template <typename T>
struct IsTls<boost::beast::ssl_stream<T>> : std::true_type
{};

template <typename Adaptor, typename Handler>
class Connection :
    public std::enable_shared_from_this<Connection<Adaptor, Handler>>
{
    using self_type = Connection<Adaptor, Handler>;

  public:
    Connection(Handler* handlerIn, boost::asio::steady_timer&& timerIn,
               std::function<std::string()>& getCachedDateStrF,
               Adaptor adaptorIn) :
        adaptor(std::move(adaptorIn)),
        handler(handlerIn), timer(std::move(timerIn)),
        getCachedDateStr(getCachedDateStrF)
    {
        parser.emplace(std::piecewise_construct, std::make_tuple());
        parser->body_limit(httpReqBodyLimit);
        parser->header_limit(httpHeaderLimit);

#ifdef BMCWEB_ENABLE_MUTUAL_TLS_AUTHENTICATION
        if constexpr (std::is_same<Adaptor,
                                   boost::beast::ssl_stream<
                                       boost::asio::ip::tcp::socket>>::value)
        {
            prepareMutualTls();
        }
#endif // BMCWEB_ENABLE_MUTUAL_TLS_AUTHENTICATION

        connectionCount++;

        BMCWEB_LOG_DEBUG("{} Connection open, total {}", logPtr(this),
                         connectionCount);
    }

    ~Connection()
    {
        res.setCompleteRequestHandler(nullptr);
        cancelDeadlineTimer();

        connectionCount--;
        BMCWEB_LOG_DEBUG("{} Connection closed, total {}", logPtr(this),
                         connectionCount);
    }

    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&&) = delete;

    bool tlsVerifyCallback(bool preverified,
                           boost::asio::ssl::verify_context& ctx)
    {
        // We always return true to allow full auth flow for resources that
        // don't require auth
        if (preverified)
        {
            boost::asio::ip::address ipAddress;
            if (getClientIp(ipAddress))
            {
                return true;
            }

            mtlsSession = verifyMtlsUser(ipAddress, ctx);
            if (mtlsSession)
            {
                BMCWEB_LOG_DEBUG("{} Generating TLS session: {}", logPtr(this),
                                 mtlsSession->uniqueId);
            }
        }
        return true;
    }

    void prepareMutualTls()
    {
        if constexpr (IsTls<Adaptor>::value)
        {
            std::error_code error;
            std::filesystem::path caPath(ensuressl::trustStorePath);
            auto caAvailable = !std::filesystem::is_empty(caPath, error);
            caAvailable = caAvailable && !error;
            if (caAvailable && persistent_data::SessionStore::getInstance()
                                   .getAuthMethodsConfig()
                                   .tls)
            {
                adaptor.set_verify_mode(boost::asio::ssl::verify_peer);
                std::string id = "bmcweb";

                const char* cStr = id.c_str();
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const auto* idC = reinterpret_cast<const unsigned char*>(cStr);
                int ret = SSL_set_session_id_context(
                    adaptor.native_handle(), idC,
                    static_cast<unsigned int>(id.length()));
                if (ret == 0)
                {
                    BMCWEB_LOG_ERROR("{} failed to set SSL id", logPtr(this));
                }
            }

            adaptor.set_verify_callback(
                std::bind_front(&self_type::tlsVerifyCallback, this));
        }
    }

    Adaptor& socket()
    {
        return adaptor;
    }

    void start()
    {
        if (connectionCount >= 200)
        {
            BMCWEB_LOG_CRITICAL("{}Max connection count exceeded.",
                                logPtr(this));
            return;
        }

        startDeadline();

        // TODO(ed) Abstract this to a more clever class with the idea of an
        // asynchronous "start"
        if constexpr (IsTls<Adaptor>::value)
        {
            adaptor.async_handshake(boost::asio::ssl::stream_base::server,
                                    [this, self(shared_from_this())](
                                        const boost::system::error_code& ec) {
                if (ec)
                {
                    return;
                }
                afterSslHandshake();
            });
        }
        else
        {
            doReadHeaders();
        }
    }

    void afterSslHandshake()
    {
        // If http2 is enabled, negotiate the protocol
        if constexpr (bmcwebEnableHTTP2)
        {
            const unsigned char* alpn = nullptr;
            unsigned int alpnlen = 0;
            SSL_get0_alpn_selected(adaptor.native_handle(), &alpn, &alpnlen);
            if (alpn != nullptr)
            {
                std::string_view selectedProtocol(
                    std::bit_cast<const char*>(alpn), alpnlen);
                BMCWEB_LOG_DEBUG("ALPN selected protocol \"{}\" len: {}",
                                 selectedProtocol, alpnlen);
                if (selectedProtocol == "h2")
                {
                    auto http2 =
                        std::make_shared<HTTP2Connection<Adaptor, Handler>>(
                            std::move(adaptor), handler, getCachedDateStr);
                    http2->start();
                    return;
                }
            }
        }

        doReadHeaders();
    }

    void handle()
    {
        std::error_code reqEc;
        if (!parser)
        {
            return;
        }
        req = crow::Request(parser->release(), reqEc);
        if (reqEc)
        {
            BMCWEB_LOG_DEBUG("Request failed to construct{}", reqEc.message());
            res.result(boost::beast::http::status::bad_request);
            completeRequest(res);
            return;
        }
        req.session = userSession;

        // Fetch the client IP address
        readClientIp();

        // Check for HTTP version 1.1.
        if (req.version() == 11)
        {
            if (req.getHeaderValue(boost::beast::http::field::host).empty())
            {
                res.result(boost::beast::http::status::bad_request);
                completeRequest(res);
                return;
            }
        }

        BMCWEB_LOG_INFO("Request:  {} HTTP/{}.{} {} {} {}", logPtr(this),
                        req.version() / 10, req.version() % 10,
                        req.methodString(), req.target(),
                        req.ipAddress.to_string());

        req.ioService = static_cast<decltype(req.ioService)>(
            &adaptor.get_executor().context());

        if (res.completed)
        {
            completeRequest(res);
            return;
        }
        keepAlive = req.keepAlive();
        if constexpr (!std::is_same_v<Adaptor, boost::beast::test::stream>)
        {
#ifndef BMCWEB_INSECURE_DISABLE_AUTHX
            if (persistent_data::getConfig().isTLSAuthEnabled())
            {
                if (!crow::authentication::isOnAllowlist(req.url().path(),
                                                         req.method()) &&
                    req.session == nullptr)
                {
                    BMCWEB_LOG_WARNING("Authentication failed");
                    forward_unauthorized::sendUnauthorized(
                        req.url().encoded_path(),
                        req.getHeaderValue("X-Requested-With"),
                        req.getHeaderValue("Accept"), res);

                    std::string user = getUser(req);
                    if (user.empty())
                    {
                        completeRequest(res);
                        return;
                    }
                    auto asyncResp =
                        std::make_shared<bmcweb::AsyncResp>(std::move(res));
                    BMCWEB_LOG_DEBUG("Setting completion handler");
                    asyncResp->res.setCompleteRequestHandler(
                        [self(shared_from_this())](crow::Response& thisRes) {
                        self->completeRequest(thisRes);
                    });
                    redfish::handleAccountLocked(user, asyncResp, req);
                    return;
                }
            }
#endif // BMCWEB_INSECURE_DISABLE_AUTHX
        }
        auto asyncResp = std::make_shared<bmcweb::AsyncResp>();
        BMCWEB_LOG_DEBUG("Setting completion handler");
        asyncResp->res.setCompleteRequestHandler(
            [self(shared_from_this())](crow::Response& thisRes) {
            self->completeRequest(thisRes);
        });
        bool isSse =
            isContentTypeAllowed(req.getHeaderValue("Accept"),
                                 http_helpers::ContentType::EventStream, false);
        std::string_view upgradeType(
            req.getHeaderValue(boost::beast::http::field::upgrade));
        if ((req.isUpgrade() &&
             bmcweb::asciiIEquals(upgradeType, "websocket")) ||
            isSse)
        {
            asyncResp->res.setCompleteRequestHandler(
                [self(shared_from_this())](crow::Response& thisRes) {
                if (thisRes.result() != boost::beast::http::status::ok)
                {
                    // When any error occurs before handle upgradation,
                    // the result in response will be set to respective
                    // error. By default the Result will be OK (200),
                    // which implies successful handle upgrade. Response
                    // needs to be sent over this connection only on
                    // failure.
                    self->completeRequest(thisRes);
                    return;
                }
            });
            handler->handleUpgrade(req, asyncResp, std::move(adaptor));
            return;
        }
        std::string url(req.target());
        std::size_t dumpPos = url.rfind("Dump");
        std::string_view expected =
            req.getHeaderValue(boost::beast::http::field::if_none_match);
        if (!expected.empty())
        {
            res.setExpectedHash(expected);
        }

#ifdef BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG

        if (dumpPos == std::string::npos)
        {
            dumpPos = url.rfind("FaultLog");
        }
#endif // BMCWEB_ENABLE_REDFISH_SYSTEM_FAULTLOG_DUMP_LOG

        std::size_t attachmentPos = url.rfind("attachment");
        std::size_t satellitesPos = std::string::npos;

#ifdef BMCWEB_ENABLE_REDFISH_AGGREGATION
        satellitesPos = url.rfind(redfishAggregationPrefix);
#endif

        if ((dumpPos != std::string::npos) &&
            (attachmentPos != std::string::npos) &&
            (satellitesPos == std::string::npos))
        {
            BMCWEB_LOG_DEBUG("upgrade stream connection");
            handler->handleUpgrade(req, asyncResp, std::move(adaptor));
            return;
        }

        handler->handle(req, asyncResp);
    }

    void close()
    {
        if constexpr (IsTls<Adaptor>::value)
        {
            adaptor.next_layer().close();
#ifdef BMCWEB_ENABLE_MUTUAL_TLS_AUTHENTICATION
            if (persistent_data::getConfig().isTLSAuthEnabled() &&
                userSession != nullptr)
#else
            if (mtlsSession != nullptr)
#endif
            {
                BMCWEB_LOG_DEBUG("{} Removing TLS session: {}", logPtr(this),
                                 mtlsSession->uniqueId);
                persistent_data::SessionStore::getInstance().removeSession(
                    mtlsSession);
            }
        }
        else
        {
            adaptor.close();
        }
    }

    void completeRequest(crow::Response& thisRes)
    {
        res = std::move(thisRes);
        res.keepAlive(keepAlive);

        completeResponseFields(req, res);
        res.addHeader(boost::beast::http::field::date, getCachedDateStr());

        doWrite();

        // delete lambda with self shared_ptr
        // to enable connection destruction
        res.setCompleteRequestHandler(nullptr);
    }

    void readClientIp()
    {
        boost::asio::ip::address ip;
        boost::system::error_code ec = getClientIp(ip);
        if (ec)
        {
            return;
        }
        req.ipAddress = ip;
    }

    boost::system::error_code getClientIp(boost::asio::ip::address& ip)
    {
        boost::system::error_code ec;
        BMCWEB_LOG_DEBUG("Fetch the client IP address");

        if constexpr (!std::is_same_v<Adaptor, boost::beast::test::stream>)
        {
            boost::asio::ip::tcp::endpoint endpoint =
                boost::beast::get_lowest_layer(adaptor).remote_endpoint(ec);

            if (ec)
            {
                // If remote endpoint fails keep going. "ClientOriginIPAddress"
                // will be empty.
                BMCWEB_LOG_ERROR(
                    "Failed to get the client's IP Address. ec : {}", ec);
                return ec;
            }
            ip = endpoint.address();
        }
        return ec;
    }

  private:
    void doReadHeaders()
    {
        BMCWEB_LOG_DEBUG("{} doReadHeaders", logPtr(this));
        if (!parser)
        {
            return;
        }
        // Clean up any previous Connection.
        boost::beast::http::async_read_header(
            adaptor, buffer, *parser,
            [this,
             self(shared_from_this())](const boost::system::error_code& ec,
                                       std::size_t bytesTransferred) {
            BMCWEB_LOG_DEBUG("{} async_read_header {} Bytes", logPtr(this),
                             bytesTransferred);

            if (ec)
            {
                cancelDeadlineTimer();

                if (ec == boost::beast::http::error::end_of_stream)
                {
                    BMCWEB_LOG_WARNING("{} Error while reading: {}",
                                       logPtr(this), ec.message());
                }
                else
                {
                    BMCWEB_LOG_ERROR("{} Error while reading: {}", logPtr(this),
                                     ec.message());
                }
                close();
                BMCWEB_LOG_DEBUG("{} from read(1)", logPtr(this));
                return;
            }

            readClientIp();

            boost::asio::ip::address ip;
            if (getClientIp(ip))
            {
                BMCWEB_LOG_DEBUG("Unable to get client IP");
            }
            if constexpr (!std::is_same_v<Adaptor, boost::beast::test::stream>)
            {
#ifndef BMCWEB_INSECURE_DISABLE_AUTHX
                if (persistent_data::getConfig().isTLSAuthEnabled())
                {
                    boost::beast::http::verb method = parser->get().method();
                    userSession = crow::authentication::authenticate(
                        ip, res, method, parser->get().base(), mtlsSession);

                    bool loggedIn = userSession != nullptr;
                    if (!loggedIn)
                    {
                        const boost::optional<uint64_t> contentLength =
                            parser->content_length();
                        if (contentLength &&
                            *contentLength > loggedOutPostBodyLimit)
                        {
                            BMCWEB_LOG_DEBUG(
                                "Content length greater than limit {}",
                                *contentLength);
                            close();
                            return;
                        }

                        BMCWEB_LOG_DEBUG("Starting quick deadline");
                    }
                }
#endif // BMCWEB_INSECURE_DISABLE_AUTHX
            }

            if (parser->is_done())
            {
                handle();
                return;
            }

            doRead();
        });
    }

    void doRead()
    {
        BMCWEB_LOG_DEBUG("{} doRead", logPtr(this));
        if (!parser)
        {
            return;
        }
        startDeadline();
        boost::beast::http::async_read_some(
            adaptor, buffer, *parser,
            [this,
             self(shared_from_this())](const boost::system::error_code& ec,
                                       std::size_t bytesTransferred) {
            BMCWEB_LOG_DEBUG("{} async_read_some {} Bytes", logPtr(this),
                             bytesTransferred);

            if (ec)
            {
                BMCWEB_LOG_ERROR("{} Error while reading: {}", logPtr(this),
                                 ec.message());
                close();
                BMCWEB_LOG_DEBUG("{} from read(1)", logPtr(this));
                return;
            }

            // If the user is logged in, allow them to send files incrementally
            // one piece at a time. If authentication is disabled then there is
            // no user session hence always allow to send one piece at a time.
            if (userSession != nullptr)
            {
                cancelDeadlineTimer();
            }
            if (!parser->is_done())
            {
                doRead();
                return;
            }

            cancelDeadlineTimer();
            handle();
        });
    }

    void afterDoWrite(const std::shared_ptr<self_type>& /*self*/,
                      const boost::system::error_code& ec,
                      std::size_t bytesTransferred)
    {
        BMCWEB_LOG_DEBUG("{} async_write wrote {} bytes, ec={}", logPtr(this),
                         bytesTransferred, ec);

        cancelDeadlineTimer();

        if (ec == boost::system::errc::operation_would_block ||
            ec == boost::system::errc::resource_unavailable_try_again)
        {
            doWrite();
            return;
        }
        if (ec)
        {
            BMCWEB_LOG_DEBUG("{} from write(2)", logPtr(this));
            return;
        }
        if (!keepAlive)
        {
            close();
            BMCWEB_LOG_DEBUG("{} from write(1)", logPtr(this));
            return;
        }

        BMCWEB_LOG_DEBUG("{} Clearing response", logPtr(this));
        res.clear();
        parser.emplace(std::piecewise_construct, std::make_tuple());
        parser->body_limit(httpReqBodyLimit); // reset body limit for
                                              // newly created parser
        buffer.consume(buffer.size());

        userSession = nullptr;

        // Destroy the Request via the std::optional
        req.clear();
        doReadHeaders();
    }

    void doWrite()
    {
        BMCWEB_LOG_DEBUG("{} doWrite", logPtr(this));
        res.preparePayload();

        startDeadline();
        serializer.emplace(res.response);
        boost::beast::http::async_write(
            adaptor, *serializer,
            std::bind_front(&self_type::afterDoWrite, this,
                            shared_from_this()));
    }

    void cancelDeadlineTimer()
    {
        timer.cancel();
    }

    void startDeadline()
    {
        cancelDeadlineTimer();

        std::chrono::seconds timeout(bmcwebResponseTimeoutSeconds);
        // allow slow uploads for logged in users
        bool loggedIn = userSession != nullptr;
        if (loggedIn)
        {
            timeout = std::chrono::seconds(bmcwebResponseTimeoutSeconds);
            return;
        }
        std::weak_ptr<Connection<Adaptor, Handler>> weakSelf = weak_from_this();
        timer.expires_after(timeout);
        timer.async_wait([weakSelf](const boost::system::error_code& ec) {
            // Note, we are ignoring other types of errors here;  If the timer
            // failed for any reason, we should still close the connection
            std::shared_ptr<Connection<Adaptor, Handler>> self =
                weakSelf.lock();
            if (!self)
            {
                BMCWEB_LOG_CRITICAL("{} Failed to capture connection",
                                    logPtr(self.get()));
                return;
            }

            self->timerStarted = false;

            if (ec == boost::asio::error::operation_aborted)
            {
                // Canceled wait means the path succeeded.
                return;
            }
            if (ec)
            {
                BMCWEB_LOG_CRITICAL("{} timer failed {}", logPtr(self.get()),
                                    ec);
            }

            BMCWEB_LOG_WARNING("{}Connection timed out, closing",
                               logPtr(self.get()));

            self->close();
        });

        timerStarted = true;
        BMCWEB_LOG_DEBUG("{} timer started", logPtr(this));
    }
    /* @brief : This function is used to get the user from the Authorization
     * header and return the user name If the header is not present or the
     * header is not in the correct format then an empty string is returned The
     * header is expected to be in the format: Authorization: Basic <base64
     * encoded user:password> The user name is extracted from the header and
     * returned
     * @param[in] req - The request object
     * @return std::string - The user name
     */
    std::string getUser(crow::Request& req)
    {
        std::string_view authHeader = req.getHeaderValue("Authorization");
        if (!authHeader.starts_with("Basic "))
        {
            return {};
        }

        std::string_view param = authHeader.substr(strlen("Basic "));
        std::string authData;

        if (!crow::utility::base64Decode(param, authData))
        {
            return {};
        }
        std::size_t separator = authData.find(':');
        if (separator == std::string::npos)
        {
            return {};
        }

        std::string user = authData.substr(0, separator);
        separator += 1;
        if (separator > authData.size())
        {
            return {};
        }

        BMCWEB_LOG_DEBUG("Basic authentication user name: {}", user);
        return user;
    }

    Adaptor adaptor;
    Handler* handler;
    // Making this a std::optional allows it to be efficiently destroyed and
    // re-created on Connection reset
    std::optional<boost::beast::http::request_parser<bmcweb::HttpBody>> parser;
    std::optional<boost::beast::http::response_serializer<bmcweb::HttpBody>>
        serializer;

    boost::beast::flat_static_buffer<8192> buffer;

    crow::Request req;
    crow::Response res;

    std::shared_ptr<persistent_data::UserSession> userSession;
    std::shared_ptr<persistent_data::UserSession> mtlsSession;

    boost::asio::steady_timer timer;

    bool keepAlive = true;

    bool timerStarted = false;

    std::function<std::string()>& getCachedDateStr;

    using std::enable_shared_from_this<
        Connection<Adaptor, Handler>>::shared_from_this;

    using std::enable_shared_from_this<
        Connection<Adaptor, Handler>>::weak_from_this;
};
} // namespace crow
