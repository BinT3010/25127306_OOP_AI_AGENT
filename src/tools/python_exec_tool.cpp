#include "python_exec_tool.h"

#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

#include "../util/subprocess.h"

namespace agent {

namespace {
std::string random_suffix() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    return std::to_string(rng());
}
}  // namespace

ToolResult PythonExecTool::execute(const std::string& args_json, Environment& env) {
    std::string code;
    try {
        auto j = nlohmann::json::parse(args_json);
        code = j.at("code").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho python_exec: ") + e.what());
    }
    if (code.empty()) return ToolResult::fail("Tham số 'code' rỗng");

    // Ghi mã ra file tạm thay vì truyền qua `python3 -c "..."` để tránh mọi vấn đề
    // escape dấu nháy/newline khi đi qua shell -c.
    auto tmp_path = env.working_directory() / (".agent_py_" + random_suffix() + ".py");
    {
        std::ofstream ofs(tmp_path, std::ios::binary);
        if (!ofs) return ToolResult::fail("Không thể tạo file mã tạm tại " + tmp_path.string());
        ofs << code;
    }

    auto pr = util::run_shell_command("python3 \"" + tmp_path.string() + "\"", env.working_directory(),
                                       timeout_seconds_);
    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);  // dọn dẹp best-effort

    std::string out = pr.output;
    constexpr std::size_t kMax = 4000;
    if (out.size() > kMax) out = out.substr(0, kMax) + "\n...[đã cắt bớt]";

    if (pr.timed_out) return ToolResult::fail(out);
    std::string prefix = (pr.exit_code == 0) ? "" : "[exit_code=" + std::to_string(pr.exit_code) + "] ";
    return ToolResult::ok(prefix + (out.empty() ? "(không có output)" : out));
}

}  // namespace agent
