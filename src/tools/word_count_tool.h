#prama once
#include "tools.h"

namespace agent {
    class WordCountTool : public Tool {
        public:
            [[nodiscard]] std::string name() const override { return "word_count"; }
            [[nodiscard]] std::string description() const override {
                return "Đếm số từ trong một đoạn văn bản.";
            }
            [[nodiscard]] std::string parameters_schema() const override {
                return R"({"text": "<đoạn văn bản cần đếm từ>"})";
            }
            [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;
};
}