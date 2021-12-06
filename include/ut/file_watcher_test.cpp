#include <file_watcher.hpp>

#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <thread>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <random>

static void ioContextWorker(const std::shared_ptr<boost::asio::io_context> &ioCtx)
{
    ioCtx->run();
}

std::string randomFilename()
{
    std::ostringstream oss;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(10, 100000);
    oss << "test_file_" << dist(rng);
    return oss.str();
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenNoSetupAndNoPath_Watch_DoesntInvokeCallback)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto success = true;
    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.watch([&](const std::vector<FileWatcherEvent> &) {
        success = false;
        FAIL();
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop(); 
    }); 
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    EXPECT_TRUE(success);

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenNoSetupAndPathWithAllEventsParameter_Watch_DoesntInvokeCallback)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto success = true;
    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.addPath("./", IN_ALL_EVENTS);
    watcher.watch([&](const std::vector<FileWatcherEvent> &) {
        success = false;
        FAIL();
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop(); 
    }); 
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    EXPECT_TRUE(success);

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithAllEventsParameterAndNewFileCreated_Watch_InvokesCallback)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_ALL_EVENTS);
    watcher.watch([&](const std::vector<FileWatcherEvent> &) {
        ioCtx->stop();
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop();
        FAIL() << "10ms timeout hit";
    });
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithAllEventsParameterAndNewFileCreated_Watch_GivesParameters)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_ALL_EVENTS);
    watcher.watch([&](const std::vector<FileWatcherEvent> &evs) {
        for (const auto &ev : evs) {
            EXPECT_EQ(ev.path, "./");
            EXPECT_EQ(ev.name, name);
        }
        ioCtx->stop();
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop();
        FAIL() << "10ms timeout hit";
    });
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithAllEventsParameterAndNewFileCreatedAndClosed_Watch_GivesCreateEvent)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_ALL_EVENTS);
    watcher.watch([&](const std::vector<FileWatcherEvent> &evs) {
        for (const auto &ev : evs) {
            if (ev.mask & IN_CREATE) {
                ioCtx->stop();
            }
        }
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop();
        FAIL() << "10ms timeout hit";
    });
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithCreateParameterAndNewFileCreatedAndClosed_Watch_GivesCreateEvent)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_CREATE);
    watcher.watch([&](const std::vector<FileWatcherEvent> &evs) {
        for (const auto &ev : evs) {
            if (ev.mask & IN_CREATE) {
                ioCtx->stop();
            }
        }
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop();
        FAIL() << "10ms timeout hit";
    });
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithCloseWriteParameterAndNewFileCreatedAndClosed_Watch_GivesNoOtherEvents)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto success = true;
    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_CLOSE_WRITE);
    watcher.watch([&](const std::vector<FileWatcherEvent> &evs) {
        for (const auto &ev : evs) {
            if (ev.mask & (IN_ALL_EVENTS & ~IN_CLOSE_WRITE)) {
                success = false;
                FAIL();
            }
        }
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop(); 
    }); 
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    EXPECT_TRUE(success);

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithAllEventsParameterAndNewFileCreatedAndWrittenToAndClosed_Watch_GivesInModifyWriteEvent)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_ALL_EVENTS);
    watcher.watch([&](const std::vector<FileWatcherEvent> &evs) {
        for (const auto &ev : evs) {
            if (ev.mask & IN_MODIFY) {
                ioCtx->stop();
            }
        }
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop();
        FAIL() << "10ms timeout hit";
    });
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fwrite("test_content", sizeof(char), sizeof("test_content"), fp);
    fclose(fp);

    ioThread.join();

    remove(name.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FileWatcher, GivenSetupWithContextAndPathWithAllEventsParameterAndNewFileCreatedAndClosed_Watch_GivesInCloseWriteEvent)
{
    auto ioCtx = std::make_shared<boost::asio::io_context>();
    boost::asio::deadline_timer tim(*ioCtx, boost::posix_time::milliseconds(10));

    auto name = randomFilename();

    InotifyFileWatcher watcher;
    watcher.setup(ioCtx);
    watcher.addPath("./", IN_ALL_EVENTS);
    watcher.watch([&](const std::vector<FileWatcherEvent> &evs) {
        for (const auto &ev : evs) {
            if (ev.mask & IN_CLOSE_WRITE) {
                ioCtx->stop();
            }
        }
    });

    tim.async_wait([&](const boost::system::error_code &) { 
        ioCtx->stop();
        FAIL() << "10ms timeout hit";
    });
    std::thread ioThread(&ioContextWorker, ioCtx);

    auto fp = fopen(name.c_str(), "w");
    fclose(fp);

    ioThread.join();

    remove(name.c_str());
}