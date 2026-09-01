#pragma once
/**
 * @file calculator_tool.h
 * @brief Tool "calculator" — tool bắt buộc #3/5. Tự viết recursive-descent
 * parser (không phụ thuộc thư viện ngoài) hỗ trợ + - * / ^ ( ) và số thực.
 */
#include "tool.h"

namespace agent {

class CalculatorTool : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "calculator"; }
    [[nodiscard]] std::string description() const override {
        return "Tính giá trị một biểu thức số học (+, -, *, /, ^, dấu ngoặc, số thập phân).";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"expression": "<biểu thức, vd: (2 + 3) * 4 - 1>"})";
    }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;
};

}  // namespace agent
