#pragma once
/**
 * @file exceptions.h
 * @brief Phân cấp exception dùng xuyên suốt hệ thống AI Agent.
 *
 * Nguyên tắc thiết kế: dùng std::expected<T,E> (C++23) cho các lỗi "có thể
 * lường trước và cần xử lý theo luồng bình thường" (vd: LLM trả lỗi mạng,
 * tool không tồn tại khi tra registry) và dùng exception cho các lỗi
 * "vi phạm bất biến / lỗi lập trình / không thể tiếp tục an toàn"
 * (vd: file skill sai định dạng nghiêm trọng, cấu hình rỗng bắt buộc).
 * Xem thêm giải thích trong báo cáo, mục "Exception vs std::expected".
 */
#include <stdexcept>
#include <string>

namespace agent {

/// Lớp cơ sở cho mọi exception phát sinh trong hệ thống agent.
class AgentException : public std::runtime_error {
public:
    explicit AgentException(const std::string& message) : std::runtime_error(message) {}
};

/// Ném ra khi ToolRegistry không tìm thấy tool theo tên được yêu cầu.
class ToolNotFoundException : public AgentException {
public:
    explicit ToolNotFoundException(const std::string& tool_name)
        : AgentException("Không tìm thấy tool: '" + tool_name + "'"), tool_name_(tool_name) {}
    const std::string& tool_name() const noexcept { return tool_name_; }
private:
    std::string tool_name_;
};

/// Ném ra khi một tool bị policy (allow/deny list) từ chối thực thi.
class ToolPolicyDeniedException : public AgentException {
public:
    explicit ToolPolicyDeniedException(const std::string& tool_name)
        : AgentException("Tool '" + tool_name + "' bị từ chối bởi policy hiện hành") {}
};

/// Ném ra khi việc nạp / phân tích một Skill (.md) thất bại nghiêm trọng.
class SkillLoadException : public AgentException {
public:
    explicit SkillLoadException(const std::string& path, const std::string& reason)
        : AgentException("Lỗi nạp skill tại '" + path + "': " + reason) {}
};

/// Ném ra khi cấu hình bắt buộc bị thiếu hoặc không hợp lệ (fail-fast).
class ConfigException : public AgentException {
public:
    explicit ConfigException(const std::string& message) : AgentException("Lỗi cấu hình: " + message) {}
};

/// Ném ra khi Environment (Sandbox) phát hiện một thao tác vượt quyền hạn cho phép.
class EnvironmentViolationException : public AgentException {
public:
    explicit EnvironmentViolationException(const std::string& message)
        : AgentException("Vi phạm ràng buộc môi trường: " + message) {}
};

/// Ném ra khi AgentLoop vượt quá số bước tối đa mà không có FinalAnswer
/// (được bắt và xử lý "graceful" bên trong AgentLoop::run, không lan ra ngoài
/// trong vận hành bình thường — chỉ dùng nội bộ / cho test).
class MaxStepsExceededException : public AgentException {
public:
    explicit MaxStepsExceededException(int max_steps)
        : AgentException("Agent vượt quá số bước tối đa (" + std::to_string(max_steps) + ")") {}
};

}  // namespace agent
