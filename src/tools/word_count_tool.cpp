#include "word_count_tool.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace agent {

ToolResult WordCountTool::execute(const std::string& args_json, Environment& /*env*/) {
    std::string text;
    try {
        auto j = nlohmann::json::parse(args_json);
        text = j.at("text").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ: ") + e.what());
    }

    std::istringstream iss(text);
    std::string word;
    int count = 0;
    while (iss >> word) ++count;

    return ToolResult::ok("Số từ: " + std::to_string(count));
}

}  // namespace agent