#pragma once
/**
 * @file tool_registry.h
 * @brief REGISTRY + FACTORY PATTERN: nơi duy nhất tra cứu & khởi tạo Tool theo tên.
 *
 * - Factory: `register_factory()` lưu một hàm khởi tạo trễ (lazy) — hữu ích khi
 *   Tool tốn tài nguyên để dựng (vd: mở kết nối DB) và ta chỉ muốn tạo khi cần.
 * - Registry: `register_tool()` đăng ký trực tiếp một instance đã dựng sẵn.
 * Cả hai cùng tồn tại trong một API vì hai nhu cầu này đều xuất hiện thực tế:
 * benchmark/run_eval.cpp dùng factory để mỗi task có instance MemoryTool DB
 * riêng (tránh chia sẻ trạng thái giữa các task), còn main.cpp CLI dùng
 * register_tool() để đăng ký nhanh bộ tool cố định cho một phiên chạy.
 */
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../util/registry.h"
#include "tool.h"
#include "tool_policy.h"
#include "../util/exceptions.h"

namespace agent {

/// C++20 CONCEPT: ràng buộc kiểu T phải là một Tool cụ thể (kế thừa từ Tool)
/// và khởi tạo được — dùng để giới hạn template `register_tool_type<T>()`
/// bên dưới, cho lỗi biên dịch RÕ RÀNG ngay tại điểm gọi nếu T không hợp lệ
/// (vd: "constraints not satisfied"), thay vì lỗi khó hiểu sâu trong
/// std::make_unique. Đây là ví dụ thực dụng của Concepts: không dùng để thay
/// thế virtual dispatch (Tool vẫn cần polymorphism thật để ToolRegistry lưu
/// nhiều loại Tool khác nhau trong cùng một container), mà để RÀNG BUỘC biên
/// dịch tại điểm gọi template.
template <typename T>
concept ToolLike = std::derived_from<T, Tool> && std::constructible_from<T>;

class ToolRegistry {
public:
    using ToolFactory = std::function<std::unique_ptr<Tool>()>;

    ToolRegistry() = default;

    /// Tiện ích cú pháp dựa trên concept ToolLike: tương đương
    /// `register_tool(std::make_unique<T>())` nhưng ngắn gọn hơn tại call site
    /// (xem main.cpp/run_eval.cpp). Không tồn tại nếu T không phải Tool hợp lệ.
    template <ToolLike T>
    void register_tool_type() {
        register_tool(std::make_unique<T>());
    }

    /// FACTORY PATTERN: đăng ký cách khởi tạo tool theo tên (chưa tạo instance ngay).
    void register_factory(const std::string& name, ToolFactory factory);

    /// Khởi tạo ngay một instance từ factory đã đăng ký và đưa vào registry hoạt động.
    void instantiate(const std::string& name);

    /// Khởi tạo tất cả tool đã đăng ký factory (tiện lợi khi setup nhanh).
    void instantiate_all();

    /// REGISTRY PATTERN: đăng ký trực tiếp một instance Tool đã dựng sẵn.
    /// Nếu ToolPolicy đã được set (set_policy), tool sẽ tự động bị bọc bởi
    /// PolicyEnforcedTool (Decorator) trước khi lưu vào registry.
    void register_tool(std::unique_ptr<Tool> tool);

    /// Gán policy áp dụng cho MỌI tool đăng ký SAU thời điểm gọi hàm này.
    void set_policy(ToolPolicy policy);

    [[nodiscard]] Tool* get(const std::string& name) const;
    [[nodiscard]] Tool& require(const std::string& name) const;  ///< ném ToolNotFoundException nếu thiếu
    [[nodiscard]] bool has(const std::string& name) const;

    [[nodiscard]] std::vector<std::string> list_names() const;

    /// Sinh đoạn mô tả tool (tên + description + schema) để nhúng vào system prompt.
    [[nodiscard]] std::string render_tools_prompt() const;

    [[nodiscard]] std::size_t size() const noexcept { return tools_.size(); }

private:
    util::Registry<Tool> factory_registry_;
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
    std::optional<ToolPolicy> policy_;
};

}  // namespace agent
