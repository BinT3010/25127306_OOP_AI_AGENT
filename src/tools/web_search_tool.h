#pragma once
/**
 * @file web_search_tool.h
 * @brief Tool "web_search" — tool bắt buộc #4/5.
 *
 * Thiết kế theo Dependency Injection: WebSearchTool nhận vào một `Fetcher`
 * (hàm gọi HTTP GET) thay vì tự gọi libcurl trực tiếp bên trong hàm execute().
 * Nhờ vậy:
 *   - Unit test (tests/test_tools.cpp) tiêm một fetcher giả lập trả JSON mẫu,
 *     không cần mạng thật, không phụ thuộc một search engine cụ thể còn sống.
 *   - Người dùng thật có thể trỏ `search_endpoint` tới bất kỳ search API nào
 *     trả về JSON dạng {"results": [{"title", "url", "content"}, ...]} —
 *     tương thích SearXNG (self-host, xem README) hoặc middleware tự viết.
 */
#include <expected>
#include <functional>
#include <string>

#include "tool.h"

namespace agent {

class WebSearchTool : public Tool {
public:
    using Fetcher = std::function<std::expected<std::string, std::string>(const std::string& url)>;

    /// @param search_endpoint_template  URL mẫu chứa "{query}", vd:
    ///   "http://localhost:8080/search?q={query}&format=json" (SearXNG JSON API).
    explicit WebSearchTool(std::string search_endpoint_template, Fetcher fetcher = nullptr,
                            int top_k = 5);

    [[nodiscard]] std::string name() const override { return "web_search"; }
    [[nodiscard]] std::string description() const override {
        return "Tìm kiếm trên web và trả về danh sách kết quả (tiêu đề + tóm tắt + URL). "
               "Dùng khi cần thông tin cập nhật hoặc không có trong kiến thức nội tại.";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"query": "<từ khoá tìm kiếm>"})";
    }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;

    static std::string url_encode(const std::string& value);

private:
    std::string endpoint_template_;
    Fetcher fetcher_;
    int top_k_;
};

}  // namespace agent
