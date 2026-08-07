#pragma once
/**
 * @file ollama_client.h
 * @brief Triển khai LLMClient gọi tới Ollama local server qua REST API (libcurl).
 *
 * Ollama expose API tương thích với /api/chat (định dạng riêng) — xem
 * https://github.com/ollama/ollama/blob/main/docs/api.md. Client này cấu
 * hình được base_url để có thể trỏ tới một tunnel công khai (vd: ngrok/
 * Cloudflare Tunnel) khi chạy Ollama trên Google Colab / Kaggle (mục I đề bài).
 */
#include <expected>
#include <string>
#include <vector>

#include "llm_client.h"
#include "llm_types.h"

namespace agent {

class OllamaClient : public LLMClient {
public:
    struct Config {
        std::string base_url = "http://localhost:11434";
        long connect_timeout_seconds = 10;
        int max_retries = 1;  ///< số lần thử lại khi lỗi mạng tạm thời (không tính lần đầu)
    };

    explicit OllamaClient(Config config);
    OllamaClient() : OllamaClient(Config{}) {}
    ~OllamaClient() override = default;

    [[nodiscard]] std::expected<ChatResult, std::string> chat(
        const std::vector<ChatMessage>& messages, const ChatOptions& options) override;

    [[nodiscard]] std::expected<std::vector<float>, std::string> embed(
        const std::string& text) override;

    [[nodiscard]] std::string provider_name() const override { return "ollama"; }

    [[nodiscard]] const Config& config() const noexcept { return config_; }

    /// Kiểm tra Ollama server có đang chạy & phản hồi hay không (GET /api/tags).
    [[nodiscard]] bool health_check();

private:
    [[nodiscard]] std::expected<std::string, std::string> http_post_json(
        const std::string& path, const std::string& json_body, long timeout_seconds);
    [[nodiscard]] std::expected<std::string, std::string> http_get(const std::string& path);

    Config config_;
};

}  // namespace agent
