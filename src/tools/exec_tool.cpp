#include "exec_tool.h"

#include <nlohmann/json.hpp>

#include "../util/exceptions.h"
#include "../util/subprocess.h"

namespace agent {

namespace {
constexpr std::size_t kMaxOutputChars = 4000;

std::string truncate(std::string s) {
    if (s.size() > kMaxOutputChars) {
        s.resize(kMaxOutputChars);
        s += "\n...[output đã bị cắt bớt, vượt quá " + std::to_string(kMaxOutputChars) + " ký tự]";
    }
    return s;
}
}  // namespace

ToolResult ExecTool::execute(const std::string& args_json, Environment& env) {
    std::string command;
    try {
        auto j = nlohmann::json::parse(args_json);
        command = j.at("command").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho exec: ") + e.what());
    }
    if (command.empty()) return ToolResult::fail("Tham số 'command' rỗng");

    if (!env.is_command_allowed(command)) {
        return ToolResult::fail("Lệnh bị Environment từ chối (khớp từ khoá bị cấm): " + command);
    }

    auto pr = util::run_shell_command(command, env.working_directory(), timeout_seconds_);
    std::string out = truncate(pr.output);

    if (pr.timed_out) return ToolResult::fail(out);

    std::string prefix = (pr.exit_code == 0) ? "" : "[exit_code=" + std::to_string(pr.exit_code) + "] ";
    return ToolResult::ok(prefix + (out.empty() ? "(không có output)" : out));
}

}  // namespace agent
