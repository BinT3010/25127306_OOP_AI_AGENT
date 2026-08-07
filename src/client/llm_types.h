#pragma once
/**
 * @file llm_types.h
 * @brief Các kiểu dữ liệu dùng chung giữa mọi LLMClient (Ollama, Mock, ...).
 *
 * Tách riêng file này để LLMClient (client/llm_client.h) không phụ thuộc vào
 * chi tiết triển khai của bất kỳ provider cụ thể nào — đây chính là điều kiện
 * để có thể "thay Ollama bằng OpenAI chỉ bằng 1 class mới" (mục 4.4 đề bài).
 */
#include <optional>
#include <string>
#include <vector>

namespace agent {

/// Một lượt hội thoại trong lịch sử chat, theo định dạng tương thích Ollama
/// (role: "system" | "user" | "assistant" | "tool").
struct ChatMessage {
    std::string role;
    std::string content;
    std::optional<std::string> tool_call_id;   ///< id của tool_call mà message này là kết quả (role=="tool")
    std::optional<std::string> name;           ///< tên tool (role=="tool") hoặc tên người gửi tuỳ provider
    std::optional<std::vector<std::string>> images;  ///< ảnh base64, dùng cho model đa phương thức (multimodal)

    static ChatMessage system(std::string content) { return {"system", std::move(content), {}, {}, {}}; }
    static ChatMessage user(std::string content) { return {"user", std::move(content), {}, {}, {}}; }
    static ChatMessage assistant(std::string content) { return {"assistant", std::move(content), {}, {}, {}}; }
    static ChatMessage tool_result(std::string tool_name, std::string call_id, std::string content) {
        return {"tool", std::move(content), std::move(call_id), std::move(tool_name), {}};
    }
    static ChatMessage user_with_images(std::string content, std::vector<std::string> images_b64) {
        return {"user", std::move(content), {}, {}, std::move(images_b64)};
    }
};

/// Một lời gọi tool mà LLM yêu cầu thực hiện, được LLMClient/AgentLoop phân tích
/// (parse) từ phản hồi thô của model (JSON field "tool_calls" hoặc regex fallback).
struct ToolCallRequest {
    std::string id;              ///< định danh lời gọi (để map kết quả trả về đúng lượt)
    std::string tool_name;
    std::string arguments_json;  ///< chuỗi JSON thô của tham số, Tool tự parse theo schema riêng
};

/// Tham số cấu hình cho một lượt gọi chat completion.
struct ChatOptions {
    std::string model = "gemma3";
    double temperature = 0.7;
    int max_tokens = 2048;
    int timeout_seconds = 60;
    bool json_mode = false;               ///< yêu cầu model trả JSON thuần (nếu provider hỗ trợ)
    std::vector<std::string> tool_names;  ///< tên các tool khả dụng, để provider có thể format lời gọi tool gốc (native)
};

/// Kết quả một lượt chat completion, đã được provider-cụ-thể chuẩn hoá về dạng chung.
struct ChatResult {
    std::string content;
    std::vector<ToolCallRequest> tool_calls;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    long long latency_ms = 0;
    std::string raw_model_name;
};

}  // namespace agent
