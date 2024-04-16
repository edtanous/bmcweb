#!/usr/bin/python3
import os
import re

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
REDFISH_FILES_DIR = os.path.realpath(
    os.path.join(SCRIPT_DIR, "..", "redfish-core", "lib")
)


def main():
    cpp_files = []
    for file in os.listdir(REDFISH_FILES_DIR):
        filepath = os.path.join(REDFISH_FILES_DIR, file)
        if not file.endswith(".hpp"):
            continue
        with open(filepath, "r") as myfile:
            contents = myfile.read()

        path, ext = os.path.splitext(filepath)

        contents = contents.replace("inline ", "")
        contents = contents.replace(
            "#pragma once\n",
            '#include "{}"\n'.format(os.path.basename(filepath)),
        )
        contents = re.sub(
            R"\/\*\n\/\/ Copy[\w\(\s\)\/,\.\";\:\-\n]+\*\/", "", contents
        )
        m = re.finditer(R"(request\w*Routes\w*)\(", contents)

        m = [x for x in m]
        if len(m) == 0:
            continue
        cpp_file = path + ".cpp"
        if cpp_file == "task_payload.hpp":
            continue
        if cpp_file == "ethernet_structs.hpp":
            continue
        if cpp_file == "sensors_common.hpp":
            continue

        cpp_files.append(cpp_file)
        with open(cpp_file, "w") as cpp_file:
            cpp_file.write(contents)

        with open(filepath, "w") as hpp_file:
            hpp_file.write("#pragma once\n" '#include "app.hpp"\n')
            if filepath.endswith("task.hpp"):
                hpp_file.write("#include <sdbusplus/bus/match.hpp>\n")

            hpp_file.write("namespace redfish\n" "{\n")
            replacement = additions.get(os.path.basename(filepath), None)
            if replacement:
                hpp_file.write(replacement)

            for function in m:
                hpp_file.write("    void " + function.group(1) + "(App&);\n")
            hpp_file.write("}\n")

    print("add this to meson")

    for cpp_file in sorted(cpp_files):
        cpp_file = "redfish-core/lib/" + os.path.basename(cpp_file)
        print("  '{}',".format(cpp_file))


# everything below this line is modules that didn't conform to the mechanism of putting common dependencies in a shared file
# in theory each one needs broken out into its own header, but in terms of automation, doing this makes things easier to accomplish that.

additions = {
"service_root.hpp": """
void handleServiceRootGetImpl(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);

""",

"update_service.hpp": """

struct TftpUrl
{
    std::string fwFile;
    std::string tftpServer;
};

std::optional<TftpUrl>
    parseTftpUrl(std::string imageURI,
                 std::optional<std::string> transferProtocol,
                 crow::Response& res);

""",
"chassis.hpp": """
void handleChassisResetActionInfoGet(
    App& app, const crow::Request& req,
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId);
""",
"systems.hpp": """
void afterGetAllowedHostTransitions(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const boost::system::error_code& ec,
    const std::vector<std::string>& allowedHostTransitions);
""",

"metric_report.hpp": """
namespace telemetry
{

using Readings = std::vector<std::tuple<std::string, double, uint64_t>>;
using TimestampReadings = std::tuple<uint64_t, Readings>;

bool fillReport(nlohmann::json& json, const std::string& id,
                       const TimestampReadings& timestampReadings);

}
""",
"manager_diagnostic_data.hpp": """
void
    setBytesProperty(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                     const nlohmann::json::json_pointer& jPtr,
                     const boost::system::error_code& ec, double bytes);

void
    setPercentProperty(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const nlohmann::json::json_pointer& jPtr,
                       const boost::system::error_code& ec, double userCPU);
""",

"hypervisor_system.hpp": """
   void handleHypervisorSystemGet(
      const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);


 void handleHypervisorResetActionGet(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp);
 """,
 "power_subsystem.hpp": """
void doPowerSubsystemCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath);
""",

"log_services.hpp": """
bool
    getTimestampFromID(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       std::string_view entryIDStrView, sd_id128_t& bootID,
                       uint64_t& timestamp, uint64_t& index);
void
    getDumpServiceInfo(const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
                       const std::string& dumpType);
                       """,

"thermal_subsystem.hpp": """
void doThermalSubsystemCollection(
    const std::shared_ptr<bmcweb::AsyncResp>& asyncResp,
    const std::string& chassisId,
    const std::optional<std::string>& validChassisPath);

"""

}

if __name__ == "__main__":
    main()
