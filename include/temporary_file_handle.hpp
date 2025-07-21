// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "duplicatable_file_handle.hpp"
#include "logging.hpp"

#include <filesystem>
#include <format>
#include <string_view>

// Class for interacting with a temporary file in the filesystem.
// On destruction, the file is removed.
class TemporaryFileHandle
{
    public:
    std::filesystem::path stringPath;
    DuplicatableFileHandle fileHandle;

    const std::filesystem::path& path() const
    {
        return stringPath;
    }

    explicit TemporaryFileHandle(std::string_view sampleData) :
        TemporaryFileHandle()
    {
        boost::system::error_code ec;
        fileHandle.fileHandle.write(sampleData.data(), sampleData.size(), ec);
        if (ec)
        {
            BMCWEB_LOG_ERROR("Failed to write sample data to temporary file: {}", ec.message());
        }
    }

    TemporaryFileHandle()
    {
        std::error_code ec;
        std::filesystem::create_directories("/tmp/bmcweb", ec);
        // Errors intentionally ignored.  Directory might not exist.

        std::string tmplStr("/tmp/bmcweb/multipart_payload_XXXXXXXXXXX");
        int fd = mkstemp(tmplStr.data());
        if (fd == -1)
        {
            BMCWEB_LOG_ERROR("mkstemp({}) failed: {}", tmplStr, errno);
            return;
        }

        fileHandle.setFd(fd);
        stringPath = tmplStr;
    }

    TemporaryFileHandle(TemporaryFileHandle&&) noexcept = default;
    TemporaryFileHandle(const TemporaryFileHandle& other) = default;
    TemporaryFileHandle& operator=(const TemporaryFileHandle& other) = default;

    bool moveToPath(const std::filesystem::path& destination)
    {
        if (stringPath.empty())
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::rename(stringPath, destination, ec);
        if (ec)
        {
            BMCWEB_LOG_ERROR("Failed to move file from {} to {}- {}",
                             stringPath.string(), destination.string(),
                             ec.message());
            return false;
        }
        stringPath.clear();
        return true;
    }

    ~TemporaryFileHandle()
    {
        if (stringPath.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::remove(stringPath, ec);
        if (ec)
        {
            BMCWEB_LOG_ERROR("Failed to remove temp file {}: {}",
                             stringPath.string(), ec.message());
        }
        else
        {
            BMCWEB_LOG_DEBUG("Removed up {}", stringPath.string());
        }
    }
};