/*
// Copyright (c) 2022 Nvidia Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "logging.hpp"

#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include <boost/process.hpp>
#include <boost/asio.hpp>

enum class MctpVdmUtilCommand
{
    /*background_copy*/
    BACKGROUNDCOPY_INIT,
    BACKGROUNDCOPY_DISABLE,
    BACKGROUNDCOPY_ENABLE,
    BACKGROUNDCOPY_DISABLE_ONE,
    BACKGROUNDCOPY_ENABLE_ONE,
    BACKGROUNDCOPY_STATUS,
    BACKGROUNDCOPY_QUERY_PROGRESS,
    BACKGROUNDCOPY_QUERY_PENDING,
    /*in_band*/
    INBAND_DISABLE,
    INBAND_ENABLE,
    INBAND_STATUS
};

struct MctpVdmUtilStatusResponse
{
    bool isSuccess{false};
    bool enabled{false};
};

struct MctpVdmUtilProgressStatusResponse
{
    bool isSuccess{false};
    std::string status;
};

namespace bp = boost::process;

class MctpVdmUtil
{
    public:
        MctpVdmUtil(uint32_t endpointId, const int timeout = 10):
            endpointId(endpointId),
            timeout(timeout),
            returnStatus(0),
            stdOut(""),
            stdErr(""),
            killed(false),
            stopped(false),
            ioc(),
            deadline_timer(ioc)    
        {}

        /**
         *@brief Execute mctp-vdm-util tool command for
         * relevant MCTP EID
         * @param mctpVdmUtilcommand the enum with commands
         * available for mctp-vdm-util tool 
         * 
         * @return none.
         */
        void run(MctpVdmUtilCommand mctpVdmUtilcommand);

        /**
         *@brief Get the exit code of mctp-vdm-util
         * The method 'run' must be executed before
         * 
         * @return exit code of mctp-vdm-util tool.
         */
        int getReturnStatus();

        /**
         *@brief Get the standard output of mctp-vdm-util tool
         * The method 'run' must be executed before
         * 
         * @return standard output of mctp-vdm-util tool.
         */
        std::string getStdOut();

        /**
         *@brief Get the standard error of mctp-vdm-util tool
         * The method 'run' must be executed before
         * 
         * @return standard error of mctp-vdm-util tool
         */
        std::string getStdErr();

        /**
         *@brief Get flag if mctp-vdm-util tool was terminated
         * 
         * @return true if mctp-vdm-util tool was terminated.
         */
        bool wasKilled();

    private:
        void initLog();
        void timeoutHandler(const boost::system::error_code& ec);
        void kill();
        void translateOperationToCommand(MctpVdmUtilCommand mctpVdmUtilcommand);

        uint32_t endpointId = 0L;

        std::string command;
        const int timeout;
        int returnStatus;
        std::string stdOut;
        std::string stdErr;
        bool killed;
        bool stopped;
        boost::process::group group;
        boost::asio::io_context ioc;
        boost::asio::deadline_timer deadline_timer;
};

void MctpVdmUtil::translateOperationToCommand(MctpVdmUtilCommand mctpVdmUtilcommand)
{
    std::string cmd;

    switch(mctpVdmUtilcommand)
    {
        case MctpVdmUtilCommand::BACKGROUNDCOPY_INIT:
            cmd = "background_copy_init";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_DISABLE:
            cmd = "background_copy_disable";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_ENABLE:
            cmd = "background_copy_enable";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_DISABLE_ONE:
            cmd = "background_copy_disable_one";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_ENABLE_ONE:
            cmd = "background_copy_enable_one";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_STATUS:
            cmd = "background_copy_query_status";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_QUERY_PROGRESS:
            cmd = "background_copy_query_progress";
            break;
        case MctpVdmUtilCommand::BACKGROUNDCOPY_QUERY_PENDING:
            cmd = "background_copy_query_pending";
            break;

        case MctpVdmUtilCommand::INBAND_DISABLE:
            cmd = "in_band_disable";
            break;
        case MctpVdmUtilCommand::INBAND_ENABLE:
            cmd = "in_band_enable";
            break;
        case MctpVdmUtilCommand::INBAND_STATUS:
            cmd = "in_band_query_status";
            break;
        
    }

    command = "mctp-vdm-util -t " + std::to_string(endpointId)  + " -c " + cmd;
}


void MctpVdmUtil::timeoutHandler(const boost::system::error_code& ec)
{
    if (stopped || ec == boost::asio::error::operation_aborted)
    {
        return;
    }

    kill();
    deadline_timer.expires_at(boost::posix_time::pos_infin);
}

void MctpVdmUtil::run(MctpVdmUtilCommand mctpVdmUtilcommand)
{
    translateOperationToCommand(mctpVdmUtilcommand);

    std::future<std::string> dataOut;
    std::future<std::string> dataErr;
    bp::child c(command, bp::std_in.close(),
        bp::std_out > dataOut,
        bp::std_err > dataErr, ioc,
        group,
        bp::on_exit([=, this]
            (int e, const std::error_code& ec) 
            {
                (void)ec;
                deadline_timer.cancel();
                returnStatus = e;
            }));

    deadline_timer.expires_from_now(boost::posix_time::seconds(timeout));
    deadline_timer.async_wait(
        [this](const boost::system::error_code& ec) {
            timeoutHandler(ec);
        });

    ioc.run();
    c.wait();
    stdOut = dataOut.get();
    stdErr = dataErr.get();
}

int MctpVdmUtil::getReturnStatus()
{
    return returnStatus;
}

std::string MctpVdmUtil::getStdOut()
{
    return stdOut;
}

std::string MctpVdmUtil::getStdErr()
{
    return stdErr;
}

void MctpVdmUtil::kill()
{
    std::error_code ec;
    group.terminate(ec);
    if(ec)
    {
        throw std::runtime_error(ec.message());
    }

    killed = true;
    stopped = true;
}

bool MctpVdmUtil::wasKilled()
{
    return killed;
}
