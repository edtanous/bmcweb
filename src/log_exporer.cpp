
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/datagram_protocol.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/file_posix.hpp>

boost::asio::awaitable<void> logger()
{
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::local::datagram_protocol::endpoint ep(
        "/tmp/bmcweb_http_client_requests.sock");
    boost::asio::local::datagram_protocol::socket socket(executor, ep);
    boost::beast::file_posix fileHandle;
    boost::system::error_code ec;
    fileHandle.open("/tmp/bmcweb_http_client_requests.jsonl",
                    boost::beast::file_mode::append, ec);
    if (ec)
    {
        throw std::runtime_error("Failed to open file");
    }
    size_t size = fileHandle.size(ec);
    if (ec)
    {
        throw std::runtime_error("Failed to open file");
    }
    // Max UDP size, plus one possible newline terminator.
    std::array<char, 65535 + 1> data{};
    while (true)
    {
        std::size_t n = co_await socket.async_receive(
            boost::asio::buffer(data.data(), data.size() - 1U),
            boost::asio::use_awaitable);
        // newline terminate the data.
        data[n] = '\n';

        fileHandle.write(static_cast<void*>(data.data()), n + 1, ec);

        if (ec)
        {
            throw std::runtime_error("Failed to write file");
        }
        constexpr size_t rotateSize = 10000;
        if (size > rotateSize)
        {
            // Rotate
        }
    }
}

int main()
{
    boost::asio::io_context io;

    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&io](auto, auto) { io.stop(); });

    boost::asio::co_spawn(io, logger(), boost::asio::detached);

    io.run();
}
