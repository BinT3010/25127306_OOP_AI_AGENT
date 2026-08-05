/**
 * @file action_parser.cpp
 * @see action_parser.h
 */
#include "action_parser.h"

#include <algorithm>
#include <map>
#include <regex>

namespace agent {

namespace {

std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Nhãn ReAct được nhận diện, theo đúng thứ tự ưu tiên khi cùng xuất hiện.
enum class Label { kThought, kAction, kActionInput, kFinalAnswer };

struct LabelMatch {
    Label label;
    std::size_t start;   ///< vị trí bắt đầu nội dung (ngay sau dấu ':')
};

}  // namespace

Action parse_action(const ChatResult& result) {
    // ---- Ưu tiên 1: native tool_calls do provider trả về có cấu trúc sẵn ----
    if (!result.tool_calls.empty()) {
        const ToolCallRequest& call = result.tool_calls.front();
        // Phần "content" đi kèm tool_calls (nếu có) thường chính là lời giải
        // thích/Thought của model trước khi gọi tool.
        std::string thought = trim(result.content);
        return ToolCallAction{thought, call.tool_name, call.arguments_json, call.id};
    }

    // ---- Ưu tiên 2: parse text theo định dạng ReAct bằng std::regex ----
    // Regex bắt các nhãn ở đầu dòng (cho phép khoảng trắng/markdown '**' bao quanh),
    // không phân biệt hoa thường, dùng std::regex::multiline để '^' khớp mỗi dòng.
    static const std::regex label_re(
        R"(^[ \t]*\**(Thought|Action Input|Action|Final Answer)\**[ \t]*:[ \t]*\**[ \t]*)",
        std::regex::icase | std::regex::multiline);

    auto label_from_text = [](std::string text) -> Label {
        std::ranges::transform(text, text.begin(), [](unsigned char c) { return std::tolower(c); });
        if (text == "thought") return Label::kThought;
        if (text == "action") return Label::kAction;
        if (text == "action input") return Label::kActionInput;
        return Label::kFinalAnswer;
    };

    std::vector<LabelMatch> matches;
    for (auto it = std::sregex_iterator(result.content.begin(), result.content.end(), label_re);
         it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        matches.push_back({label_from_text(m[1].str()), static_cast<std::size_t>(m.position(0) + m.length(0))});
    }

    if (matches.empty()) {
        return MalformedAction{result.content, "Không nhận diện được nhãn ReAct nào (Thought/Action/"
                                                "Action Input/Final Answer) trong phản hồi của model."};
    }

    std::map<Label, std::string> fields;
    for (std::size_t i = 0; i < matches.size(); ++i) {
        std::size_t content_start = matches[i].start;
        std::size_t content_end = (i + 1 < matches.size())
                                       ? [&] {
                                             // lùi lại về đầu dòng chứa nhãn kế tiếp để không dính "\n" thừa
                                             return result.content.rfind('\n', matches[i + 1].start);
                                         }()
                                       : result.content.size();
        if (content_end == std::string::npos || content_end < content_start) content_end = result.content.size();
        std::string value = trim(result.content.substr(content_start, content_end - content_start));
        // Nếu nhãn xuất hiện nhiều lần, giữ lần xuất hiện SAU CÙNG (model có thể
        // tự sửa lại suy nghĩ trong cùng một lượt).
        fields[matches[i].label] = value;
    }

    std::string thought = fields.contains(Label::kThought) ? fields[Label::kThought] : "";

    if (fields.contains(Label::kFinalAnswer)) {
        return FinalAnswerAction{thought, fields[Label::kFinalAnswer]};
    }
    if (fields.contains(Label::kAction)) {
        std::string tool_name = fields[Label::kAction];
        std::string args = fields.contains(Label::kActionInput) ? fields[Label::kActionInput] : "{}";
        return ToolCallAction{thought, tool_name, args, ""};
    }
    if (fields.contains(Label::kThought)) {
        return ThinkAction{thought};
    }
    return MalformedAction{result.content, "Chỉ tìm thấy nhãn nhưng thiếu Action/Final Answer hợp lệ."};
}

}  // namespace agent
