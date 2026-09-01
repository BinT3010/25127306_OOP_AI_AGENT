#pragma once
/**
 * @file file_tool.h
 * @brief Tool "file" — gộp chung read_file/write_file (tool bắt buộc #2/5).
 * Mọi đường dẫn đều đi qua Environment::resolve_path() để tôn trọng ràng buộc
 * sandbox (nếu có) — xem sandbox_environment.h.
 */
#include "tool.h"

namespace agent {

class FileTool : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "file"; }
    [[nodiscard]] std::string description() const override {
        return "Đọc hoặc ghi nội dung file văn bản. action=read_file cần 'path'. "
               "action=write_file cần 'path' và 'content' (append tuỳ chọn, mặc định false = ghi đè).";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"action": "read_file|write_file", "path": string, "content": string (write_file), )"
               R"("append": bool (tuỳ chọn, write_file)})";
    }
    [[nodiscard]] bool is_mutating() const override { return true; }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;
};

}  // namespace agent
