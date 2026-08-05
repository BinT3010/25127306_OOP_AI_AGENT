#pragma once
/**
 * @file action.h
 * @brief Biểu diễn "hành động" mà AgentLoop có thể thực hiện sau bước Think.
 *
 * Dùng std::variant (C++17) thay vì một class phân cấp ảo (virtual) vì tập
 * hành động là CỐ ĐỊNH và ĐÓNG (closed set) — ta biết trước chỉ có 3 khả năng.
 * std::variant + std::visit cho phép xử lý exhaustive (đầy đủ mọi nhánh, được
 * kiểm tra tại compile-time nhờ if constexpr trong overloaded visitor) mà
 * không cần chi phí virtual dispatch hay heap allocation.
 */
#include <string>
#include <variant>

namespace agent {

/// Model muốn gọi một tool.
struct ToolCallAction {
    std::string thought;       ///< phần suy luận (Thought:) đi kèm, để ghi vào Trajectory
    std::string tool_name;
    std::string args_json;
    std::string raw_call_id;   ///< id gốc nếu dùng native tool_calls của Ollama, có thể rỗng
};

/// Model đưa ra câu trả lời cuối cùng, kết thúc vòng lặp ReAct.
struct FinalAnswerAction {
    std::string thought;
    std::string answer;
};

/// Model chỉ suy luận thêm (không gọi tool, không kết thúc) — hiếm gặp nhưng
/// hữu ích khi model cần "nghĩ nhiều bước" trước khi hành động; AgentLoop sẽ
/// tự động yêu cầu model tiếp tục ở bước kế.
struct ThinkAction {
    std::string thought;
};

/// Parse thất bại — không khớp bất kỳ định dạng ReAct nào đã biết.
struct MalformedAction {
    std::string raw_content;
    std::string reason;
};

using Action = std::variant<ToolCallAction, FinalAnswerAction, ThinkAction, MalformedAction>;

/// Tiện ích "overloaded" kinh điển để dùng với std::visit (xem agent_loop.cpp).
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace agent
