#pragma once
/**
 * @file mock_llm_client.h
 * @brief LLMClient giả lập (test double) — không gọi mạng, trả lời theo kịch bản.
 *
 * Mục đích:
 *  1) Cho phép unit test AgentLoop/HarnessRunner mà không cần Ollama server thật.
 *  2) CHỨNG MINH bằng thực nghiệm rằng interface LLMClient đủ tổng quát để có
 *     nhiều triển khai khác nhau cùng tồn tại (Ollama thật <-> Mock giả lập)
 *     mà phần còn lại của hệ thống (AgentLoop, HarnessRunner, Evaluator...)
 *     không cần sửa một dòng nào — đúng yêu cầu 4.4 của đề bài.
 *  3) Cho phép demo `benchmark/run_eval` chạy "offline" khi máy chấm bài không
 *     có sẵn Ollama, vẫn thấy được toàn bộ pipeline Trajectory -> Evaluator -> JSON.
 *
 * KHÔNG dùng để thay thế việc chạy thật với Ollama — README có hướng dẫn cấu
 * hình OllamaClient trỏ tới server thật (local hoặc tunnel từ Colab/Kaggle).
 */
#include <deque>
#include <expected>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "llm_client.h"
#include "llm_types.h"

namespace agent {

class MockLLMClient : public LLMClient {
public:
    using ResponderFn = std::function<ChatResult(const std::vector<ChatMessage>&, const ChatOptions&)>;

    /// Thêm một phản hồi cố định vào hàng đợi (FIFO) — mỗi lần chat() được gọi
    /// sẽ "pop" một phần tử, theo đúng thứ tự đã enqueue.
    void enqueue_response(ChatResult result);

    /// Tiện ích: enqueue một Action theo cú pháp ReAct (xem agent_loop.cpp)
    /// yêu cầu gọi tool `tool_name` với tham số JSON `args_json`.
    void enqueue_tool_call(const std::string& thought, const std::string& tool_name,
                            const std::string& args_json);

    /// Tiện ích: enqueue một Final Answer theo cú pháp ReAct.
    void enqueue_final_answer(const std::string& thought, const std::string& answer);

    /// Cách nâng cao: cung cấp hàm sinh phản hồi động dựa trên lịch sử hội thoại
    /// (dùng khi kịch bản phụ thuộc vào những gì đã xảy ra trước đó).
    void set_responder(ResponderFn fn);

    [[nodiscard]] std::expected<ChatResult, std::string> chat(
        const std::vector<ChatMessage>& messages, const ChatOptions& options) override;

    [[nodiscard]] std::expected<std::vector<float>, std::string> embed(const std::string& text) override;

    [[nodiscard]] std::string provider_name() const override { return "mock"; }

    [[nodiscard]] int call_count() const noexcept { return call_count_; }
    [[nodiscard]] const std::vector<std::vector<ChatMessage>>& call_history() const noexcept {
        return history_;
    }

private:
    mutable std::mutex mutex_;  ///< bảo vệ toàn bộ state bên dưới — cho phép nhiều
                                 ///< sub-agent thread (SubAgentOrchestrator, mục 10.3)
                                 ///< cùng chia sẻ một MockLLMClient an toàn khi test.
    std::deque<ChatResult> queue_;
    ResponderFn responder_;
    int call_count_ = 0;
    std::vector<std::vector<ChatMessage>> history_;
};

}  // namespace agent
