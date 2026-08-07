/**
 * @file ollama_client.cpp
 * @see ollama_client.h
 */
#include "ollama_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstring>

using json = nlohmann::json;

namespace agent {

namespace {

/// RAII guard cho curl_global_init/cleanup — chỉ init đúng một lần nhờ
/// "magic static" (C++11 thread-safe static local initialization).
struct CurlGlobalGuard {
    CurlGlobalGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalGuard() { curl_global_cleanup(); }
    CurlGlobalGuard(const CurlGlobalGuard&) = delete;
    CurlGlobalGuard& operator=(const CurlGlobalGuard&) = delete;
};
void ensure_curl_global_init() {
    static CurlGlobalGuard guard;
    (void)guard;
}

/// RAII wrapper cho CURL* — đảm bảo curl_easy_cleanup luôn được gọi kể cả khi
/// có exception, tránh leak tài nguyên hệ thống (không chỉ bộ nhớ heap).
struct CurlHandle {
    CURL* handle;
    CurlHandle() : handle(curl_easy_init()) {}
    ~CurlHandle() {
        if (handle) curl_easy_cleanup(handle);
    }
    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
    explicit operator bool() const { return handle != nullptr; }
};

/// RAII wrapper cho curl_slist* (danh sách header).
struct CurlSlist {
    curl_slist* list = nullptr;
    ~CurlSlist() {
        if (list) curl_slist_free_all(list);
    }
    void append(const char* header) { list = curl_slist_append(list, header); }
};

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string role_to_ollama(const std::string& role) { return role; }  // đã tương thích 1-1

json message_to_json(const ChatMessage& m) {
    json j;
    j["role"] = role_to_ollama(m.role);
    j["content"] = m.content;
    if (m.images && !m.images->empty()) j["images"] = *m.images;
    // Ollama không có field "tool_call_id" chuẩn hoá cho mọi model; ta nhúng
    // tên tool vào đầu content khi trả kết quả tool để model dễ bám ngữ cảnh.
    return j;
}

}  // namespace

OllamaClient::OllamaClient(Config config) : config_(std::move(config)) { ensure_curl_global_init(); }

std::expected<std::string, std::string> OllamaClient::http_post_json(const std::string& path,
                                                                       const std::string& json_body,
                                                                       long timeout_seconds) {
    CurlHandle curl;
    if (!curl) return std::unexpected("Không thể khởi tạo CURL handle (curl_easy_init trả nullptr)");

    const std::string url = config_.base_url + path;
    std::string response_body;
    char error_buffer[CURL_ERROR_SIZE] = {0};

    CurlSlist headers;
    headers.append("Content-Type: application/json");

    curl_easy_setopt(curl.handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl.handle, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl.handle, CURLOPT_CONNECTTIMEOUT, config_.connect_timeout_seconds);
    curl_easy_setopt(curl.handle, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl.handle, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = CURLE_OK;
    int attempt = 0;
    do {
        res = curl_easy_perform(curl.handle);
        ++attempt;
    } while (res != CURLE_OK && attempt <= config_.max_retries &&
             (res == CURLE_COULDNT_CONNECT || res == CURLE_OPERATION_TIMEDOUT));

    if (res != CURLE_OK) {
        std::string reason = (std::strlen(error_buffer) > 0) ? error_buffer : curl_easy_strerror(res);
        switch (res) {
            case CURLE_COULDNT_CONNECT:
                return std::unexpected("Không kết nối được tới Ollama server tại " + config_.base_url +
                                        " (connection refused). Hãy chắc chắn Ollama đang chạy và "
                                        "base_url đã được cấu hình đúng (vd: tunnel từ Colab/Kaggle). Chi tiết: " +
                                        reason);
            case CURLE_OPERATION_TIMEDOUT:
                return std::unexpected("Yêu cầu tới Ollama hết thời gian chờ (timeout=" +
                                        std::to_string(timeout_seconds) + "s). Chi tiết: " + reason);
            default:
                return std::unexpected("Lỗi CURL khi gọi " + url + ": " + reason);
        }
    }

    long http_status = 0;
    curl_easy_getinfo(curl.handle, CURLINFO_RESPONSE_CODE, &http_status);
    if (http_status != 200) {
        std::string snippet = response_body.substr(0, 300);
        return std::unexpected("Ollama trả HTTP " + std::to_string(http_status) + ": " + snippet);
    }

    return response_body;
}

std::expected<std::string, std::string> OllamaClient::http_get(const std::string& path) {
    CurlHandle curl;
    if (!curl) return std::unexpected("Không thể khởi tạo CURL handle");
    const std::string url = config_.base_url + path;
    std::string response_body;
    curl_easy_setopt(curl.handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl.handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl.handle, CURLOPT_CONNECTTIMEOUT, config_.connect_timeout_seconds);
    curl_easy_setopt(curl.handle, CURLOPT_NOSIGNAL, 1L);
    CURLcode res = curl_easy_perform(curl.handle);
    if (res != CURLE_OK) return std::unexpected(curl_easy_strerror(res));
    return response_body;
}

bool OllamaClient::health_check() {
    auto result = http_get("/api/tags");
    return result.has_value();
}

std::expected<ChatResult, std::string> OllamaClient::chat(const std::vector<ChatMessage>& messages,
                                                            const ChatOptions& options) {
    json body;
    body["model"] = options.model;
    body["stream"] = false;
    body["options"] = {{"temperature", options.temperature}, {"num_predict", options.max_tokens}};
    if (options.json_mode) body["format"] = "json";

    json msgs = json::array();
    for (const auto& m : messages) msgs.push_back(message_to_json(m));
    body["messages"] = msgs;

    const auto t_start = std::chrono::steady_clock::now();
    auto post_result = http_post_json("/api/chat", body.dump(), options.timeout_seconds);
    const auto t_end = std::chrono::steady_clock::now();
    const auto latency_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    if (!post_result.has_value()) return std::unexpected(post_result.error());

    json parsed;
    try {
        parsed = json::parse(post_result.value());
    } catch (const json::parse_error& e) {
        return std::unexpected(std::string("Phản hồi từ Ollama không phải JSON hợp lệ: ") + e.what());
    }

    if (parsed.contains("error")) {
        return std::unexpected("Ollama báo lỗi: " + parsed.value("error", "unknown"));
    }
    if (!parsed.contains("message") || !parsed["message"].is_object()) {
        return std::unexpected("Phản hồi Ollama thiếu field 'message' bắt buộc: " + post_result.value().substr(0, 200));
    }

    ChatResult result;
    result.content = parsed["message"].value("content", "");
    result.prompt_tokens = parsed.value("prompt_eval_count", 0);
    result.completion_tokens = parsed.value("eval_count", 0);
    result.latency_ms = latency_ms;
    result.raw_model_name = parsed.value("model", options.model);

    // Native tool calling: một số model (vd: qwen*, llama3.1+) trả tool_calls
    // có cấu trúc. Nếu không có, AgentLoop sẽ tự fallback sang parse text.
    if (parsed["message"].contains("tool_calls") && parsed["message"]["tool_calls"].is_array()) {
        int idx = 0;
        for (const auto& tc : parsed["message"]["tool_calls"]) {
            ToolCallRequest req;
            req.id = "call_" + std::to_string(idx++);
            if (tc.contains("function") && tc["function"].is_object()) {
                req.tool_name = tc["function"].value("name", "");
                const auto& args = tc["function"].value("arguments", json::object());
                req.arguments_json = args.is_string() ? args.get<std::string>() : args.dump();
            }
            if (!req.tool_name.empty()) result.tool_calls.push_back(std::move(req));
        }
    }

    return result;
}

std::expected<std::vector<float>, std::string> OllamaClient::embed(const std::string& text) {
    json body;
    body["model"] = "nomic-embed-text";
    body["prompt"] = text;

    auto post_result = http_post_json("/api/embeddings", body.dump(), 30);
    if (!post_result.has_value()) return std::unexpected(post_result.error());

    json parsed;
    try {
        parsed = json::parse(post_result.value());
    } catch (const json::parse_error& e) {
        return std::unexpected(std::string("Phản hồi embedding không phải JSON hợp lệ: ") + e.what());
    }
    if (!parsed.contains("embedding") || !parsed["embedding"].is_array()) {
        return std::unexpected("Phản hồi Ollama thiếu field 'embedding'");
    }
    std::vector<float> vec;
    vec.reserve(parsed["embedding"].size());
    for (const auto& v : parsed["embedding"]) vec.push_back(v.get<float>());
    return vec;
}

}  // namespace agent
