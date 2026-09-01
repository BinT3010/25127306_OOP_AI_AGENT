#pragma once
/**
 * @file exec_tool.h
 * @brief Tool "exec" — chạy một lệnh shell POSIX. Tool bắt buộc #1/5.
 */
#include "tool.h"

namespace agent {

class ExecTool : public Tool {
public:
    explicit ExecTool(int timeout_seconds = 15) : timeout_seconds_(timeout_seconds) {}

    [[nodiscard]] std::string name() const override { return "exec"; }
    [[nodiscard]] std::string description() const override {
        return "Chạy một lệnh shell (POSIX sh -c) trong thư mục làm việc hiện tại và trả về "
               "stdout+stderr. Dùng khi cần thao tác hệ thống mà các tool khác không hỗ trợ.";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"command": "<chuỗi lệnh shell, bắt buộc>"})";
    }
    [[nodiscard]] bool is_mutating() const override { return true; }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;

private:
    int timeout_seconds_;
};

}  // namespace agent
