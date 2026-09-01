#pragma once
/**
 * @file http_tool.h
 * @brief Tool "http_fetch" — tool bổ sung #3/3 (loại "network"), khác với
 * web_search (tìm kiếm + tóm tắt) ở chỗ tool này lấy NGUYÊN VĂN nội dung tại
 * một URL cụ thể do LLM cung cấp (giống chức năng "fetch" trong nhiều agent
 * framework hiện đại để đọc tài liệu/API công khai).
 */
#include <expected>
#include <functional>
#include <string>

#include "tool.h"

namespace agent {

class HttpFetchTool : public Tool {
public:
    /// Cho phép tiêm (inject) hàm fetch tuỳ biến để unit test không cần mạng thật.
    using Fetcher = std::function<std::expected<std::string, std::string>(const std::string& url)>;

    explicit HttpFetchTool(Fetcher fetcher = nullptr);

    [[nodiscard]] std::string name() const override { return "http_fetch"; }
    [[nodiscard]] std::string description() const override {
        return "Tải nội dung thô (GET) từ một URL công khai và trả về (đã cắt bớt nếu quá dài). "
               "Dùng khi cần đọc trực tiếp một trang/tài liệu đã biết địa chỉ.";
    }
    [[nodiscard]] std::string parameters_schema() const override { return R"({"url": "<URL bắt đầu bằng http(s)://>"})"; }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;

    /// Fetcher mặc định dùng libcurl thật — public để test/tool khác có thể tái sử dụng.
    static std::expected<std::string, std::string> curl_get(const std::string& url, long timeout_seconds = 15);

private:
    Fetcher fetcher_;
};

}  // namespace agent
