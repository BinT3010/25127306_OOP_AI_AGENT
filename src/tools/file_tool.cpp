#include "file_tool.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "../util/exceptions.h"

namespace agent {

namespace {
constexpr std::size_t kMaxReadChars = 8000;
}

ToolResult FileTool::execute(const std::string& args_json, Environment& env) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(args_json);
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho file: ") + e.what());
    }

    std::string action = j.value("action", "");
    if (!j.contains("path")) return ToolResult::fail("Thiếu tham số 'path'");
    std::string raw_path = j.at("path").get<std::string>();

    std::filesystem::path resolved;
    try {
        resolved = env.resolve_path(raw_path);
    } catch (const EnvironmentViolationException& e) {
        return ToolResult::fail(e.what());
    }

    if (action == "read_file") {
        std::ifstream ifs(resolved, std::ios::binary);
        if (!ifs) return ToolResult::fail("Không thể mở file để đọc: " + resolved.string());
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string content = oss.str();
        bool truncated = content.size() > kMaxReadChars;
        if (truncated) content.resize(kMaxReadChars);
        return ToolResult::ok(content + (truncated ? "\n...[đã cắt bớt, file dài hơn giới hạn đọc]" : ""));
    }

    if (action == "write_file") {
        if (!j.contains("content")) return ToolResult::fail("Thiếu tham số 'content' cho write_file");
        std::string content = j.at("content").get<std::string>();
        bool append = j.value("append", false);

        std::filesystem::create_directories(resolved.parent_path());
        auto mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
        std::ofstream ofs(resolved, mode);
        if (!ofs) return ToolResult::fail("Không thể mở file để ghi: " + resolved.string());
        ofs << content;
        return ToolResult::ok("Đã ghi " + std::to_string(content.size()) + " byte vào " + resolved.string());
    }

    return ToolResult::fail("action không hợp lệ: '" + action + "' (chỉ chấp nhận read_file|write_file)");
}

}  // namespace agent
