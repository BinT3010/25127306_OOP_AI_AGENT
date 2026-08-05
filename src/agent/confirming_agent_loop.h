#pragma once
/**
 * @file confirming_agent_loop.h
 * @brief Ví dụ CỤ THỂ chứng minh giá trị thực tế của Template Method Pattern:
 * một biến thể AgentLoop "thận trọng" override đúng một bước (act()) để thêm
 * hành vi ghi log cảnh báo trước khi thực thi tool có khả năng gây thay đổi
 * ngoài ý muốn (Tool::is_mutating() == true), mà HOÀN TOÀN không cần đụng
 * tới logic run() (Observe → Think → Act → Observe, max_steps, loop
 * detection...) — toàn bộ khung sườn thuật toán trong AgentLoop::run() được
 * tái sử dụng nguyên vẹn.
 */
#include "agent_loop.h"

namespace agent {

class ConfirmingAgentLoop : public AgentLoop {
public:
    using AgentLoop::AgentLoop;  // kế thừa toàn bộ constructor của lớp cha

protected:
    [[nodiscard]] ToolResult act(const ToolCallAction& action) override {
        if (Tool* t = tools_.get(action.tool_name); t != nullptr && t->is_mutating()) {
            logger_.warn("[an-toan] Sắp thực thi tool GÂY THAY ĐỔI '{}' với tham số: {}", action.tool_name,
                          action.args_json);
        }
        return AgentLoop::act(action);  // uỷ quyền lại hành vi gốc, chỉ "chêm" thêm log
    }
};

}  // namespace agent
