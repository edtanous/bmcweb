#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/process.hpp>
#include <boost/process/async.hpp>
#include <boost/process/child.hpp>
#include <openbmc_dbus_rest.hpp>
#include <utils/dbus_utils.hpp>
#include "component_integrity.hpp"

#include "task.hpp"

namespace redfish {

namespace debug_token {

// mctp-vdm-util's stdout output has 131 characters
// rounded up to 256
constexpr const size_t statusSubprocOutputSize = 256;

constexpr const char * mctpEndpointIf = "xyz.openbmc_project.MCTP.Endpoint";

using FinishCallback = std::function<
    void(const std::shared_ptr<task::TaskData>&, const std::string&)>;
using TaskCallback = std::function<bool(boost::system::error_code,
    sdbusplus::message::message&,
    const std::shared_ptr<task::TaskData>&)>;

class DebugTokenBase
{
  public:
    bool isRunning() const
    {
        return isValid();
    }

    DebugTokenBase(const DebugTokenBase&) = delete;
    DebugTokenBase(DebugTokenBase&&) = delete;
    DebugTokenBase& operator=(const DebugTokenBase&) = delete;
    DebugTokenBase& operator=(const DebugTokenBase&&) = delete;

  protected:
    DebugTokenBase(
        const std::string& interface,
        const std::string& taskMatchString,
        FinishCallback&& finishCallback) :
        interface(interface),
        match(taskMatchString),
        enumeratedEntries(0),
        finishCallback(std::move(finishCallback))
    {
    }
    virtual ~DebugTokenBase() = default;

    struct Entry
    {
        std::string object;
        std::string service;
        std::vector<uint8_t> data;
        bool valid;

        Entry(const std::string& object, const std::string& service) :
            object(object), service(service), valid(true)
        {}
    };

    std::shared_ptr<task::TaskData> task;
    std::string interface;
    std::string match;
    std::vector<Entry> entries;
    size_t enumeratedEntries;
    FinishCallback finishCallback;

    virtual void processState() = 0;
    virtual bool run(const crow::Request& req,
        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) = 0;

    bool isValid() const
    {
        return task != nullptr;
    }

    void abort(const std::string& status)
    {
        task->state = "Stopping";
        task->status = status;
        task->messages.emplace_back(
            messages::taskAborted(
                std::to_string(task->index)));
    }

    void addError(const std::string& desc, const std::string& err)
    {
        BMCWEB_LOG_ERROR << desc << " error: " << err;
        if (task) {
            task->messages.emplace_back(
                messages::resourceErrorsDetectedFormatError(desc, err));
        }
        else
        {
            BMCWEB_LOG_ERROR << "addError: task pointer is null";
        }
    }

    virtual void cleanup()
    {
        task = nullptr;
        entries.clear();
        entries.reserve(0);
        enumeratedEntries = 0;
    }

    using EnumerateCallback = std::function<void(Entry&)>;
    void enumerate(EnumerateCallback&& callback)
    {
        const std::string desc =
            "Enumerating objects with " + interface + " interface";
        BMCWEB_LOG_DEBUG << desc;
        if (!isValid())
        {
            addError(desc, "state invalid");
            return;
        }

        crow::connections::systemBus->async_method_call(
            [this, desc, callback{std::forward<EnumerateCallback>(callback)}](
                const boost::system::error_code ec,
                const crow::openbmc_mapper::GetSubTreeType& subtree)
            {
                if (!isValid())
                {
                    addError(desc, "state invalid");
                    return;
                }
                if (ec)
                {
                    addError(desc, ec.message());
                    return;
                }

                if (subtree.empty())
                {
                    processState();
                }
                else
                {
                    entries.reserve(subtree.size());
                    for (const auto& object : subtree)
                    {
                        entries.emplace_back(
                            object.first, object.second[0].first);
                    }
                    if (callback)
                    {
                        for (auto& entry : entries)
                        {
                            callback(entry);
                        }
                    }
                    else
                    {
                        enumeratedEntries = entries.size();
                        processState();
                    }
                }
            },
            dbus_utils::mapperBusName, dbus_utils::mapperObjectPath,
            dbus_utils::mapperIntf, "GetSubTree", "/", 0,
            std::array{interface});
    }

    void finish()
    {
        if (task)
        {
            if (task->state == "Running")
            {
                abort("Operation timed out");
            }
            // task timer timeout callback already did everything
            if (task->state != "Cancelled")
            {
                task->finishTask();
                task->match.reset();
                task->timer.cancel();
                task->sendTaskEvent(task->state, task->index);
            }
        }
        cleanup();
    }

    bool areItemsPending() const
    {
        return std::find_if(entries.begin(), entries.end(),
            [](const Entry &entry)
            {
                return entry.valid && entry.data.empty();
            }) != entries.end();
    }

    bool setup(const crow::Request& req,
        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
        TaskCallback&& taskCallback)
    {
        if (task != nullptr)
        {
            return false;
        }
        task = task::TaskData::createTask(std::move(taskCallback), match);
        task->payload.emplace(req);
        task->populateResp(asyncResp->res);
        return task != nullptr;
    }
};

class StatusQuery :
    public DebugTokenBase,
    public std::enable_shared_from_this<DebugTokenBase>
{
  public:
    explicit StatusQuery(FinishCallback&& finishCallback) :
        DebugTokenBase(
            std::string(mctpEndpointIf),
            "0",
            std::forward<FinishCallback>(finishCallback)),
        currentEntry(nullptr)
    {
    }

    bool run(const crow::Request& req,
        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) override
    {
        if (setup(req, asyncResp,
                [this, self(shared_from_this())](
                    boost::system::error_code ec,
                    sdbusplus::message::message&,
                    const std::shared_ptr<task::TaskData>&)
                {
                    const std::string desc = "Debug token status query task";
                    BMCWEB_LOG_DEBUG << desc;
                    if (!self->isRunning())
                    {
                        return task::completed;
                    }
                    if (ec)
                    {
                        if (ec != boost::asio::error::operation_aborted)
                        {
                            addError(desc, ec.message());
                        }
                        finish();
                        return task::completed;
                    }
                    return areItemsPending() ?
                        !task::completed : task::completed;
                }))
        {
            subprocessOutput.resize(statusSubprocOutputSize);
            task->startTimer(std::chrono::minutes(3));
            // NOLINTNEXTLINE(modernize-avoid-bind)
            enumerate(std::bind(
                &StatusQuery::enumerateCallback, this, std::placeholders::_1));
            return true;
        }
        return false;
    }

    StatusQuery(const StatusQuery&) = delete;
    StatusQuery(StatusQuery&&) = delete;
    StatusQuery& operator=(const StatusQuery&) = delete;
    StatusQuery& operator=(const StatusQuery&&) = delete;
    ~StatusQuery() override = default;

  private:
    Entry *currentEntry;
    std::unique_ptr<boost::process::child> subprocess;
    std::unique_ptr<boost::asio::steady_timer> subprocessTimer;
    std::vector<char> subprocessOutput;

    std::string eidErrors;

    void cleanup() override
    {
        subprocessCleanup();
        subprocessOutput.resize(0);
        eidErrors.clear();
        DebugTokenBase::cleanup();
    }

    void enumerateCallback(Entry& entry)
    {
        const std::string desc =
            "Getting EID for DBus object " + entry.object;
        BMCWEB_LOG_DEBUG << desc;
        if (!isValid())
        {
            addError(desc, "state invalid");
            return;
        }
        sdbusplus::asio::getProperty<uint32_t>(
            *crow::connections::systemBus,
            entry.service, entry.object, mctpEndpointIf, "EID",
            [this, &entry, desc](
                const boost::system::error_code ec,
                const uint32_t& eid)
            {
                BMCWEB_LOG_DEBUG << "EID: " << eid;
                if (ec)
                {
                    addError(desc, ec.message());
                    entry.valid = false;
                }
                else
                {
                    entry.object = std::to_string(eid);
                }
                if (++enumeratedEntries == entries.size())
                {
                    processState();
                }
            });
    }

    void subprocessCleanup()
    {
        if (subprocess)
        {
            if (subprocess->running())
            {
                subprocess->terminate();
                subprocess->wait();
            }
            subprocess.reset(nullptr);
        }
        if (subprocessTimer)
        {
            subprocessTimer->cancel();
            subprocessTimer.reset(nullptr);
        }
    }

    void subprocessExitCallback(int exitCode, const std::error_code& ec)
    {
        const std::string desc =
            "mctp-vdm-util execution for EID " + currentEntry->object;
        BMCWEB_LOG_DEBUG << desc;
        if (ec)
        {
            addError(desc, ec.message());
            currentEntry->valid = false;
        }
        else if (exitCode == 0)
        {
            boost::interprocess::bufferstream outputStream(
                subprocessOutput.data(), subprocessOutput.size());
            std::string line, rxLine, txLine;
            while (std::getline(outputStream, line))
            {
                if (line.rfind("RX: ", 0) == 0)
                {
                    BMCWEB_LOG_DEBUG << "EID: " << currentEntry->object <<
                        " " << line;
                    rxLine = line.substr(4);
                }
                else if (line.rfind("TX: ", 0) == 0)
                {
                    BMCWEB_LOG_DEBUG << "EID: " << currentEntry->object <<
                        " " << line;
                    txLine = line.substr(4);
                }
            }
            if (rxLine.size() > txLine.size())
            {
                currentEntry->data =
                    std::vector<uint8_t>(rxLine.begin(), rxLine.end());
            }
            else
            {
                addError(desc, "No RX data");
                currentEntry->valid = false;
            }
        }
        else
        {
            addError(desc, "Exit code: " + std::to_string(exitCode));
            currentEntry->valid = false;
        }
        if (!currentEntry->valid)
        {
            eidErrors += " " + currentEntry->object;
        }
        currentEntry = nullptr;
        std::fill(subprocessOutput.begin(), subprocessOutput.end(), 0);
        subprocessCleanup();

        // Continue only if there was no task timeout
        if (task)
        {
            // Trying to continue operation immediately after the subprocess
            // exits often results in various problems (malloc errors, stdout
            // errors etc.) ending with bmcweb crash.
            // Applying a one second delay seems to alleviate the problem, three
            // seconds are used to be on the safer side, as this procedure is
            // not time critical.
            subprocessTimer = std::make_unique<boost::asio::steady_timer>(
                crow::connections::systemBus->get_io_context());
            subprocessTimer->expires_after(std::chrono::seconds(3));
            subprocessTimer->async_wait(
                [this, desc](const boost::system::error_code ec)
                {
                    if (ec && ec != boost::asio::error::operation_aborted)
                    {
                        addError("Subprocess delay timer", ec.message());
                    }
                    processState();
                });
        }
        else
        {
            BMCWEB_LOG_ERROR <<
                "Process exit handler called after task timeout";
        }
    }

    void getStatus()
    {
        const std::string desc =
            "mctp-vdm-util execution for EID " + currentEntry->object;
        BMCWEB_LOG_DEBUG << desc;
        subprocessTimer = std::make_unique<boost::asio::steady_timer>(
                crow::connections::systemBus->get_io_context());
        subprocessTimer->expires_after(std::chrono::seconds(15));
        subprocessTimer->async_wait(
            [this, desc](const boost::system::error_code ec)
            {
                if (ec && ec != boost::asio::error::operation_aborted)
                {
                    if (subprocess)
                    {
                        addError(desc, "Timeout");
                        currentEntry->valid = false;
                    }
                    subprocessCleanup();
                }
            });

        BMCWEB_LOG_DEBUG << "mctp-vdm-util -c debug_token_query -t " <<
            currentEntry->object;
        std::vector<std::string> args =
        {
            "-c", "debug_token_query",
            "-t", currentEntry->object
        };
        try
        {
            // NOLINTNEXTLINE(modernize-avoid-bind)
            auto callback = std::bind(
                &StatusQuery::subprocessExitCallback, this,
                std::placeholders::_1, std::placeholders::_2);
            subprocess = std::make_unique<boost::process::child>(
                "/usr/bin/mctp-vdm-util", args,
                boost::process::std_err > boost::process::null,
                boost::process::std_out >
                    boost::asio::buffer(subprocessOutput),
                crow::connections::systemBus->get_io_context(),
                boost::process::on_exit = std::move(callback));
        }
        catch (const std::runtime_error& e)
        {
            addError(desc, e.what());
            subprocessCleanup();
            processState();
        }
    }

    void processState() override
    {
        const std::string desc =
            "Debug token query data processing";
        BMCWEB_LOG_DEBUG << desc;
        if (!isValid())
        {
            addError(desc, "state invalid");
            return;
        }
        if (entries.size() == 0)
        {
            abort("No MCTP endpoints");
            finish();
            return;
        }
        size_t size = 0;
        int completedEntries = 0;
        int totalEntries = static_cast<int>(entries.size());
        bool waitingForUpdate = false;
        for (auto& entry : entries)
        {
            if (entry.valid)
            {
                if (entry.data.size() == 0)
                {
                    waitingForUpdate = true;
                    if (!currentEntry)
                    {
                        currentEntry = &entry;
                    }
                }
                else
                {
                    task->percentComplete =
                        100 * ++completedEntries / totalEntries;
                    size += entry.data.size() + 1; // +1 for newline
                }
            }
        }
        if (waitingForUpdate)
        {
            if (!subprocess && currentEntry)
            {
                getStatus();
            }
        }
        else if (completedEntries > 0)
        {
            std::string output;
            output.reserve(size);
            for (auto& entry : entries)
            {
                if (entry.valid && entry.data.size() != 0)
                {
                    output += std::string(entry.data.begin(), entry.data.end());
                    output += "\n";
                }
            }
            if (completedEntries == totalEntries)
            {
                task->state = "Completed";
                task->status = "OK";
                task->messages.emplace_back(
                    messages::taskCompletedOK(std::to_string(task->index)));
            }
            else
            {
                task->state = "Exception";
                task->status = "Invalid response for EIDs:" + eidErrors;
                task->messages.emplace_back(
                    messages::taskCompletedWarning(
                        std::to_string(task->index)));
            }
            if (finishCallback)
            {
                finishCallback(task, output);
            }
            finish();
        }
        else
        {
            abort("No valid debug token query responses");
            finish();
        }
    }
};

class Request :
    public DebugTokenBase,
    public std::enable_shared_from_this<Request>
{
  public:
    explicit Request(FinishCallback&& finishCallback) :
        DebugTokenBase(
            std::string(spdmResponderIntf),
            "type='signal',interface='org.freedesktop.DBus.Properties',"
            "member='PropertiesChanged',"
            "path_namespace='/xyz/openbmc_project/SPDM'",
            std::forward<FinishCallback>(finishCallback))
    {
    }

    bool run(const crow::Request& req,
        const std::shared_ptr<bmcweb::AsyncResp>& asyncResp) override
    {
        if (setup(req, asyncResp,
                [this, self(shared_from_this())](
                    boost::system::error_code ec,
                    sdbusplus::message::message& msg,
                    const std::shared_ptr<task::TaskData>&)
                {
                    const std::string desc = "Debug token request task";
                    BMCWEB_LOG_DEBUG << desc;
                    if (!self->isRunning())
                    {
                        return task::completed;
                    }
                    if (ec)
                    {
                        if (ec != boost::asio::error::operation_aborted)
                        {
                            addError(desc, ec.message());
                        }
                        finish();
                        return task::completed;
                    }

                    std::string interface;
                    std::map<std::string, dbus::utility::DbusVariantType> props;
                    msg.read(interface, props);
                    if (interface == spdmResponderIntf)
                    {
                        auto it = props.find("Status");
                        if (it != props.end())
                        {
                            auto status =
                                std::get_if<std::string>(&(it->second));
                            if (status)
                            {
                                update(msg.get_path(), *status);
                            }
                        }
                    }
                    return areItemsPending() ?
                        !task::completed : task::completed;
                }))
        {
            task->startTimer(std::chrono::minutes(1));
            // NOLINTNEXTLINE(modernize-avoid-bind)
            enumerate(std::bind(
                &Request::enumerateCallback, this, std::placeholders::_1));
            return true;
        }

        return false;
    }

    Request(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(const Request&) = delete;
    Request& operator=(const Request&&) = delete;
    ~Request() override = default;

  private:
    #pragma pack(1)
    struct ServerRequestHeader
    {
        /* Versioning for token request structure (0x0001) */
        uint16_t version;
        /* Size of the token request structure */
        uint16_t size;
        /* Signed Transcript of the debug token measurement request */
        std::array<uint8_t, 218> measurementTranscript;
    };
    #pragma pack()

    #pragma pack(1)
    struct FileHeader
    {
        static constexpr uint8_t typeTokenRequest = 1;
        static constexpr uint8_t typeDebugToken = 2;

        /* Set to 0x01 */
        uint8_t version;
        /* Either 1 for token request or 2 for token data */
        uint8_t type;
        /* Count of stored debug tokens / requests */
        uint16_t numberOfRecords;
        /* Equal to sizeof(struct FileHeader) for version 0x01. */
        uint16_t offsetToListOfStructs;
        /* Equal to sum of sizes of a given structure type + sizeof(struct
         * FileHeader) */
        uint32_t fileSize;
        /* Padding */
        std::array<uint8_t, 6> reserved;
    };
    #pragma pack()

    void enumerateCallback(Entry& entry)
    {
        const std::string desc =
            "Refresh call for " + entry.object + " object";
        BMCWEB_LOG_DEBUG << desc;
        if (!isValid())
        {
            addError(desc, "state invalid");
            return;
        }
        std::vector<uint8_t> indices{50};
        crow::connections::systemBus->async_method_call(
            [this, &entry, desc](const boost::system::error_code ec)
            {
                BMCWEB_LOG_DEBUG << "Object: " << entry.object;
                if (ec)
                {
                    addError(desc, ec.message());
                    entry.valid = false;
                }
            },
            spdmBusName, entry.object, spdmResponderIntf, "Refresh",
            static_cast<uint8_t>(0), std::vector<uint8_t>(), indices,
            static_cast<uint32_t>(0));
    }

    void update(const std::string& object, const std::string& status)
    {
        const std::string desc =
            "Update of " + object + " object";
        BMCWEB_LOG_DEBUG << desc;
        if (!isValid())
        {
            addError(desc, "state invalid");
            return;
        }
        auto entry = std::find_if(entries.begin(), entries.end(),
            [object](const Entry &entry)
            {
                return entry.object == object;
            });
        if (entry == entries.end())
        {
            addError(desc, "unknown object");
            return;
        }
        if (!entry->valid)
        {
            return;
        }
        BMCWEB_LOG_DEBUG << object << " " << status;
        if (!entry->data.empty())
        {
            BMCWEB_LOG_INFO << desc << " data already present";
        }
        else if (status ==
            "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Success")
        {
            crow::connections::systemBus->async_method_call(
                [this, entry](
                    const boost::system::error_code ec,
                    const boost::container::flat_map<
                        std::string, dbus::utility::DbusVariantType>& props)
                {
                    const std::string desc =
                        "Reading properties of " + entry->object + " object";
                    BMCWEB_LOG_DEBUG << desc;
                    if (!isValid())
                    {
                        addError(desc, "state invalid");
                        return;
                    }
                    if (ec)
                    {
                        addError(desc, ec.message());
                        return;
                    }
                    auto itSign = props.find("SignedMeasurements");
                    auto itCert = props.find("Certificate");
                    if (itSign == props.end() || itCert == props.end())
                    {
                        addError(desc, "cannot find property");
                        return;
                    }
                    auto sign = std::get_if<
                        std::vector<uint8_t>>(&itSign->second);
                    auto cert = std::get_if<
                        std::vector<std::tuple<uint8_t, std::string>>>(
                            &itCert->second);
                    if (!sign || !cert)
                    {
                        addError(desc, "cannot decode property");
                        return;
                    }
                    auto certSlot = std::find_if(
                        cert->begin(), cert->end(),
                        [](const auto& e)
                        {
                            return std::get<0>(e) == 0;
                        });
                    if (certSlot == cert->end())
                    {
                        addError(desc, "cannot find certificate for slot 0");
                        return;
                    }
                    if (sign->size() !=
                        sizeof(ServerRequestHeader::measurementTranscript))
                    {
                        addError(desc, "wrong SignedMeasurements size: " +
                            std::to_string(sign->size()));
                        return;
                    }
                    const std::string& pem = std::get<1>(*certSlot);
                    size_t size = sizeof(ServerRequestHeader) + pem.size();
                    auto header = std::make_unique<ServerRequestHeader>();
                    header->version = 0x0001;
                    header->size = static_cast<uint16_t>(size);
                    std::copy(sign->begin(), sign->end(),
                        header->measurementTranscript.begin());
                    entry->data.reserve(size);
                    entry->data.resize(sizeof(ServerRequestHeader));
                    std::memcpy(entry->data.data(), header.get(),
                        sizeof(ServerRequestHeader));
                    entry->data.insert(entry->data.end(),
                        pem.begin(), pem.end());
                    processState();
                },
                spdmBusName, object, "org.freedesktop.DBus.Properties",
                "GetAll", spdmResponderIntf);
        }
        else if (startsWithPrefix(
            status, "xyz.openbmc_project.SPDM.Responder.SPDMStatus.Error_"))
        {
            addError(desc, status);
            entry->valid = false;
        }
        processState();
    }

    void processState() override
    {
        const std::string desc =
            "Debug token request data processing";
        BMCWEB_LOG_DEBUG << desc;
        if (!isValid())
        {
            addError(desc, "state invalid");
            return;
        }
        if (entries.size() == 0)
        {
            abort("No SPDM responders");
            finish();
            return;
        }
        size_t size = 0;
        int completedEntries = 0;
        int totalEntries = static_cast<int>(entries.size());
        bool waitingForUpdate = false;
        for (auto& entry : entries)
        {
            if (entry.valid)
            {
                if (entry.data.size() == 0)
                {
                    waitingForUpdate = true;
                }
                else
                {
                    task->percentComplete =
                        100 * ++completedEntries / totalEntries;
                    size += entry.data.size();
                }
            }
        }
        if (waitingForUpdate)
        {
            task->extendTimer(std::chrono::seconds(20));
        }
        else if (completedEntries > 0)
        {
            size += sizeof(FileHeader);
            FileHeader header{};
            header.version = 0x01;
            header.type = FileHeader::typeTokenRequest;
            header.numberOfRecords = static_cast<uint16_t>(completedEntries);
            header.offsetToListOfStructs = sizeof(FileHeader);
            header.fileSize = static_cast<uint32_t>(size);

            std::vector<uint8_t> output;
            output.reserve(size);
            output.resize(sizeof(FileHeader));
            std::memcpy(output.data(), &header, sizeof(FileHeader));
            for (auto& entry : entries)
            {
                if (entry.valid && entry.data.size() != 0)
                {
                    output.insert(output.end(),
                        entry.data.begin(), entry.data.end());
                }
            }

            if (completedEntries == totalEntries)
            {
                task->state = "Completed";
                task->status = "OK";
                task->messages.emplace_back(
                    messages::taskCompletedOK(std::to_string(task->index)));
            }
            else
            {
                task->state = "Exception";
                task->status = "Warning";
                task->messages.emplace_back(
                    messages::taskCompletedWarning(std::to_string(task->index)));
            }
            if (finishCallback)
            {
                finishCallback(task, std::string(output.begin(), output.end()));
            }
            finish();
        }
        else
        {
            abort("No valid debug token request responses");
            finish();
        }
    }
};

}

}
