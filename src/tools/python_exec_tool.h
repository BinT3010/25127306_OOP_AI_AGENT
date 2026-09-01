#pragma once
/**
 * @file python_exec_tool.h
 * @brief Tool "python_exec" — tool bổ sung #1/3 (loại "code execution"),
 * tham khảo nhóm "code interpreter" tool trong OpenClaw/Hermes tool catalog.
 * Khác với ExecTool (chạy lệnh shell tổng quát), tool này chuyên chạy mã
 * Python cho các tác vụ tính toán/xử lý dữ liệu mà cú pháp shell không tiện.
 */
#include "tool.h"

namespace agent {

class PythonExecTool : public Tool {
public:
    explicit PythonExecTool(int timeout_seconds = 15) : timeout_seconds_(timeout_seconds) {}

    [[nodiscard]] std::string name() const override { return "python_exec"; }
    [[nodiscard]] std::string description() const override {
        return "Chạy một đoạn mã Python 3 và trả về stdout+stderr. Dùng cho tính toán, xử lý "
               "chuỗi/dữ liệu phức tạp hơn khả năng của tool calculator.";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"code": "<mã nguồn Python 3, bắt buộc>"})";
    }
    [[nodiscard]] bool is_mutating() const override { return true; }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;

private:
    int timeout_seconds_;
};

}  // namespace agent
