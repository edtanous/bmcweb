#pragma once

#include "logging.hpp"

#include <fcntl.h>
#include <linux/limits.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/bind.hpp>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <vector>

struct FileWatcherEvent
{
    std::string path;
    std::string name;
    uint32_t mask{0};

    FileWatcherEvent(std::string path, std::string name, uint32_t mask) :
        path(std::move(path)), name(std::move(name)), mask(mask)
    {}
};

class InotifyFileWatcher
{
  public:
    InotifyFileWatcher() : io(nullptr), sd(nullptr), buf(), watchedDirs() {}

    ~InotifyFileWatcher()
    {
        for (const auto& wdPair : watchedDirs)
        {
            inotify_rm_watch(inotifyFd, wdPair.first);
        }
        close(inotifyFd);
    }

    InotifyFileWatcher(const InotifyFileWatcher&) = delete;
    InotifyFileWatcher& operator=(const InotifyFileWatcher) = delete;
    InotifyFileWatcher& operator=(const InotifyFileWatcher&& fw) = delete;
    InotifyFileWatcher(const InotifyFileWatcher&& fw) = delete;

    void setup(std::shared_ptr<boost::asio::io_context> ioIn)
    {
        io = std::move(ioIn);
        inotifyFd = inotify_init();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (fcntl(inotifyFd, F_SETFL, O_NONBLOCK) < 0)
        {
            BMCWEB_LOG_ERROR("Error initializing inotify.");
            return;
        }
        sd = std::make_unique<boost::asio::posix::stream_descriptor>(*io,
                                                                     inotifyFd);
    }

    void addPath(const std::string& path, uint32_t mask)
    {
        if (sd == nullptr)
        {
            return;
        }

        int inotifyWd = inotify_add_watch(inotifyFd, path.c_str(), mask);
        if (inotifyWd == -1)
        {
            BMCWEB_LOG_ERROR("Could not watch path: {}", path);
            return;
        }
        watchedDirs[inotifyWd] = path;
    }

    void watch(std::function<void(std::vector<FileWatcherEvent>)> handler)
    {
        if (sd == nullptr)
        {
            return;
        }

        sd->async_read_some(
            boost::asio::buffer(buf),
            // NOLINTNEXTLINE(modernize-avoid-bind)
            boost::bind(&InotifyFileWatcher::asyncReadHandler, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        std::move(handler)));
    }

  private:
    int inotifyFd{};
    std::shared_ptr<boost::asio::io_context> io;
    std::unique_ptr<boost::asio::posix::stream_descriptor> sd;
    std::array<char, sizeof(inotify_event) + NAME_MAX + 1> buf;
    std::map<int, std::string> watchedDirs;

    void asyncReadHandler(
        boost::system::error_code ec, std::size_t bytes,
        const std::function<void(std::vector<FileWatcherEvent>)>& handler)
    {
        if (ec)
        {
            BMCWEB_LOG_ERROR("InotifyFileWatcher error code: {}", ec);
            return;
        }

        std::vector<FileWatcherEvent> events;
        size_t offset = 0;

        while (offset < bytes)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
            auto alignedEvp = static_cast<inotify_event*>(
                std::malloc(sizeof(inotify_event) + NAME_MAX + 1 - offset));
            if (alignedEvp == nullptr)
            {
                BMCWEB_LOG_ERROR("InotifyFileWatcher malloc error.");
                return;
            }
            std::memcpy(alignedEvp, &buf[offset],
                        sizeof(inotify_event) + NAME_MAX + 1 - offset);

            FileWatcherEvent fwev(std::string(dirForEvent(*alignedEvp)),
                                  std::string(&alignedEvp->name[0]),
                                  alignedEvp->mask);
            events.push_back(std::move(fwev));

            offset += offsetof(inotify_event, name) + alignedEvp->len;

            // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
            free(alignedEvp);
        }

        handler(events);
        watch(handler);
    }

    std::string& dirForEvent(const inotify_event& event)
    {
        return watchedDirs[event.wd];
    }
};
