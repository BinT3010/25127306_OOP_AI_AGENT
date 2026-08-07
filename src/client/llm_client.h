#pragma once
/**
 * @file llm_client.h
 * @brief Interface trừu tượng (abstract) cho mọi nguồn suy luận LLM.
 *
 * Đây là lớp trừu tượng hoá quan trọng nhất của tầng Client: AgentLoop,
 * HarnessRunner, VLMEvaluator... đều chỉ phụ thuộc vào LLMClient, KHÔNG BAO
 * GIỜ phụ thuộc trực tiếp vào OllamaClient. Nhờ vậy, MockLLMClient (dùng cho
 * unit test / demo offline) và OllamaClient (dùng khi chạy thật với Ollama)
 * có thể hoán đổi cho nhau tự do — đúng nguyên tắc Dependency Inversion và
 * đúng yêu cầu 4.4 của đề bài.
 */
#include <expected>
#include <string>
#include <vector>

#include "llm_types.h"

namespace agent {

class LLMClient {
public:
    virtual ~LLMClient() = default;

    /// Gửi một lượt hội thoại, nhận về ChatResult hoặc thông điệp lỗi.
    /// Dùng std::expected (C++23) vì lỗi mạng/API là tình huống "được lường
    /// trước", nằm trong luồng xử lý bình thường — không nên dùng exception
    /// (vốn nên dành cho lỗi bất thường / vi phạm bất biến chương trình).
    [[nodiscard]] virtual std::expected<ChatResult, std::string> chat(
        const std::vector<ChatMessage>& messages, const ChatOptions& options) = 0;

    /// Sinh vector embedding cho một đoạn văn bản (dùng cho MemoryTool - vector search).
    /// Mặc định trả lỗi "không hỗ trợ"; chỉ các provider có model embedding mới cần override.
    [[nodiscard]] virtual std::expected<std::vector<float>, std::string> embed(
        const std::string& /*text*/) {
        return std::unexpected("Provider này không hỗ trợ embedding");
    }

    /// Tên định danh của provider, dùng để ghi log / trajectory.
    [[nodiscard]] virtual std::string provider_name() const = 0;
};

}  // namespace agent
