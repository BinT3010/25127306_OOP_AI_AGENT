#pragma once
/**
 * @file tool.h
 * @brief Interface trừu tượng cho mọi "khả năng" mà Agent có thể gọi.
 *
 * Nguyên tắc tách lớp (mục 4.4 đề bài): Tool KHÔNG được include agent_loop.h,
 * KHÔNG biết gì về AgentLoop/HarnessRunner. Tool chỉ nhận vào một chuỗi JSON
 * tham số + một Environment để thao tác, và trả về ToolResult thuần dữ liệu.
 * Điều này cho phép viết unit test cho từng Tool hoàn toàn độc lập.
 */
#include <optional>
#include <string>

#include "../agent/environment.h"

namespace agent {

struct ToolResult {
    bool success = false;
    std::string output;               ///< nội dung "Observation" trả về cho LLM
    std::optional<std::string> error;  ///< thông điệp lỗi ngắn gọn (nếu success == false)

    static ToolResult ok(std::string output) { return ToolResult{true, std::move(output), std::nullopt}; }
    static ToolResult fail(std::string error_message) {
        return ToolResult{false, "", std::move(error_message)};
    }
};

class Tool {
public:
    virtual ~Tool() = default;

    /// Tên định danh duy nhất, dùng để LLM gọi qua "Action: <name>".
    [[nodiscard]] virtual std::string name() const = 0;

    /// Mô tả ngắn gọn chức năng — được nhúng vào system prompt để LLM biết khi nào nên dùng.
    [[nodiscard]] virtual std::string description() const = 0;

    /// Mô tả schema tham số dạng văn bản/JSON mẫu — giúp LLM sinh đúng "Action Input".
    [[nodiscard]] virtual std::string parameters_schema() const = 0;

    /// Thực thi tool với tham số dạng chuỗi JSON thô. Implementation tự parse
    /// theo schema riêng của mình và PHẢI bắt hết exception nội bộ, trả về
    /// ToolResult::fail(...) thay vì để exception thoát ra AgentLoop — quy ước
    /// này giữ cho AgentLoop không cần try/catch quanh mỗi lần gọi tool.
    [[nodiscard]] virtual ToolResult execute(const std::string& args_json, Environment& env) = 0;

    /// Tool có thực hiện thay đổi trạng thái ngoài ý muốn (ghi file, exec lệnh...)
    /// hay không — ToolPolicy/UI có thể dùng để cảnh báo hoặc yêu cầu xác nhận.
    [[nodiscard]] virtual bool is_mutating() const { return false; }
};

}  // namespace agent
