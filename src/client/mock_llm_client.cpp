/**
 * @file mock_llm_client.cpp
 * @see mock_llm_client.h
 */
#include "mock_llm_client.h"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace agent {

void MockLLMClient::enqueue_response(ChatResult result) {
    std::lock_guard lock(mutex_);
    queue_.push_back(std::move(result));
}

void MockLLMClient::enqueue_tool_call(const std::string& thought, const std::string& tool_name,
                                       const std::string& args_json) {
    ChatResult r;
    r.content = "Thought: " + thought + "\nAction: " + tool_name + "\nAction Input: " + args_json;
    r.prompt_tokens = 42;
    r.completion_tokens = 16;
    r.latency_ms = 5;
    r.raw_model_name = "mock-model";
    enqueue_response(std::move(r));
}

void MockLLMClient::enqueue_final_answer(const std::string& thought, const std::string& answer) {
    ChatResult r;
    r.content = "Thought: " + thought + "\nFinal Answer: " + answer;
    r.prompt_tokens = 42;
    r.completion_tokens = 12;
    r.latency_ms = 5;
    r.raw_model_name = "mock-model";
    enqueue_response(std::move(r));
}

void MockLLMClient::set_responder(ResponderFn fn) { responder_ = std::move(fn); }

std::expected<ChatResult, std::string> MockLLMClient::chat(const std::vector<ChatMessage>& messages,
                                                             const ChatOptions& options) {
    std::unique_lock lock(mutex_);
    ++call_count_;
    history_.push_back(messages);

    if (responder_) {
        // Gọi responder_ NGOÀI vùng khoá: hàm này do người dùng test cung cấp và
        // có thể tuỳ ý — tránh giữ mutex_ trong lúc gọi code ngoài tầm kiểm soát
        // (nguyên tắc tránh deadlock khi gọi callback từ trong đoạn code có khoá).
        ResponderFn fn = responder_;
        lock.unlock();
        return fn(messages, options);
    }

    if (queue_.empty()) {
        return std::unexpected(
            "MockLLMClient: hàng đợi phản hồi rỗng — hãy enqueue_tool_call()/enqueue_final_answer() "
            "đủ số lượt trước khi chạy AgentLoop, hoặc set_responder() cho kịch bản động.");
    }
    ChatResult r = std::move(queue_.front());
    queue_.pop_front();
    return r;
}

std::expected<std::vector<float>, std::string> MockLLMClient::embed(const std::string& text) {
    // Sinh vector giả lập xác định (deterministic) từ nội dung text, đủ để unit
    // test cosine similarity mà không cần gọi mạng. KHÔNG mang ý nghĩa ngữ nghĩa
    // thật như embedding model thật.
    std::vector<float> vec(16, 0.0f);
    for (std::size_t i = 0; i < text.size(); ++i) {
        vec[i % vec.size()] += static_cast<float>(static_cast<unsigned char>(text[i]));
    }
    float norm = 0.0f;
    for (float v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-6f)
        for (float& v : vec) v /= norm;
    return vec;
}

}  // namespace agent
