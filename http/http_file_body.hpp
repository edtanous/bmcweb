#pragma once

#include "utility.hpp"

#include <unistd.h>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/file_posix.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/system/error_code.hpp>

#include <string_view>

namespace bmcweb
{
struct FileBody
{
    class writer;
    class reader;
    class value_type;

    static std::uint64_t size(const value_type& body);
};

enum class EncodingType
{
    Raw,
    Base64,
};

class FileBody::value_type
{
    boost::beast::file_posix fileHandle;

    std::uint64_t fileSize = 0;

    std::string strBody;

  public:
    EncodingType encodingType = EncodingType::Raw;

    ~value_type() = default;
    value_type() = default;
    explicit value_type(EncodingType enc) : encodingType(enc) {}
    explicit value_type(std::string_view str) : strBody(str) {}

    value_type(value_type&& other) = default;
    value_type& operator=(value_type&& other) = default;

    // Overload copy constructor, because posix doesn't have dup(), but linux
    // does
    value_type(const value_type& other)
    {
        fileSize = other.fileSize;
        strBody = other.strBody;
        encodingType = other.encodingType;
        fileHandle.native_handle(dup(other.fileHandle.native_handle()));
    }
    value_type& operator=(const value_type& other)
    {
        if (this != &other)
        {
            fileSize = other.fileSize;
            strBody = other.strBody;
            encodingType = other.encodingType;
            fileHandle.native_handle(dup(other.fileHandle.native_handle()));
        }
        return *this;
    };

    boost::beast::file_posix& file()
    {
        return fileHandle;
    }

    std::string& str()
    {
        return strBody;
    }

    const std::string& str() const
    {
        return strBody;
    }

    std::uint64_t size() const
    {
        if (!fileHandle.is_open())
        {
            return strBody.size();
        }
        return fileSize;
    }

    void clear()
    {
        strBody.clear();
        strBody.shrink_to_fit();
        fileHandle = boost::beast::file_posix();
        fileSize = 0;
    }

    void open(const char* path, boost::beast::file_mode mode,
              boost::system::error_code& ec)
    {
        fileHandle.open(path, mode, ec);
        fileSize = fileHandle.size(ec);
    }

    void setFd(int fd, boost::system::error_code& ec)
    {
        fileHandle.native_handle(fd);
        fileSize = fileHandle.size(ec);
    }
};

inline std::uint64_t FileBody::size(const value_type& body)
{
    return body.size();
}

class FileBody::writer
{
  public:
    using const_buffers_type = boost::asio::const_buffer;

  private:
    std::string buf;
    crow::utility::Base64Encoder encoder;

    value_type& body;
    std::uint64_t remain;
    constexpr static size_t readBufSize = 4096;
    std::array<char, readBufSize> fileReadBuf{};

  public:
    template <bool IsRequest, class Fields>
    writer(boost::beast::http::header<IsRequest, Fields>& /*header*/,
           value_type& bodyIn) :
        body(bodyIn),
        remain(body.size())
    {}

    static void init(boost::beast::error_code& ec)
    {
        ec = {};
    }

    boost::optional<std::pair<const_buffers_type, bool>>
        get(boost::beast::error_code& ec)
    {
        if (!body.file().is_open())
        {
            size_t toReturn = std::min(readBufSize, remain);
            boost::optional<std::pair<const_buffers_type, bool>> ret2 =
                std::make_pair(
                    const_buffers_type(body.str().data() +
                                           (toReturn - body.str().size()),
                                       toReturn),
                    false);
            remain -= toReturn;
            return ret2;
        }
        size_t toRead = fileReadBuf.size();
        if (remain < toRead)
        {
            toRead = static_cast<size_t>(remain);
        }
        size_t read = body.file().read(fileReadBuf.data(), toRead, ec);
        if (read != toRead || ec)
        {
            return boost::none;
        }
        remain -= read;

        std::string_view chunkView(fileReadBuf.data(), read);

        std::pair<const_buffers_type, bool> ret;
        ret.second = remain > 0;
        if (body.encodingType == EncodingType::Base64)
        {
            buf.clear();
            buf.reserve(
                crow::utility::Base64Encoder::encodedSize(chunkView.size()));
            encoder.encode(chunkView, buf);
            if (!ret.second)
            {
                encoder.finalize(buf);
            }
            ret.first = const_buffers_type(buf.data(), buf.size());
        }
        else
        {
            ret.first = const_buffers_type(chunkView.data(), chunkView.size());
        }
        return ret;
    }
};

class FileBody::reader
{
    value_type& value;

  public:
    template <bool IsRequest, class Fields>
    reader(boost::beast::http::header<IsRequest, Fields>& /*headers*/,
           value_type& body) :
        value(body)
    {}

    void init(const boost::optional<std::uint64_t>& /*content_length*/,
              boost::beast::error_code& ec)
    {
        ec = {};
    }

    template <class ConstBufferSequence>
    std::size_t put(const ConstBufferSequence& buffers,
                    boost::system::error_code& ec)
    {
        ec = {};
        value.str() += boost::beast::buffers_to_string(buffers);
        std::string& body = value.str();

        size_t extra = boost::beast::buffer_bytes(buffers);

        body.reserve(body.size() + extra);

        for (const auto b : boost::beast::buffers_range_ref(buffers))
        {
            body += std::string_view(static_cast<const char*>(b.data()),
                                     b.size());
        }

        return extra;
    }

    void finish(boost::system::error_code& ec)
    {
        ec = {};
    }
};

} // namespace bmcweb
