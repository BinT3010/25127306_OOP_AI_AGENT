#pragma once
#include "tool.h"

namespace agent {
    class WordCountTool : public Tool {
        public:
            [[nodiscard]] std::string name() const override { return "word_count"; }
            [[nodiscard]] std::string description() const override {
                return "Đếm số từ trong một đoạn văn bản, tách theo khoảng trắng (whitespace-"
                       "delimited token). Với tiếng Việt, mỗi 'tiếng' (âm tiết) cách nhau bởi dấu "
                       "cách được tính là một từ riêng — ví dụ từ ghép 'lập trình' được tính là 2, "
                       "'đối tượng' được tính là 2 — KHÔNG phải phân tách từ ghép theo ngữ nghĩa.";
            }
            [[nodiscard]] std::string parameters_schema() const override {
                return R"({"text": "<đoạn văn bản cần đếm từ>"})";
            }
            [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;
};
}