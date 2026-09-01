#include "http_tool.h"

#include <curl/curl.h>

#include <nlohmann/json.hpp>

namespace agent {

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}
constexpr std::size_t kMaxBodyChars = 4000;
}  // namespace

HttpFetchTool::HttpFetchTool(Fetcher fetcher) : fetcher_(std::move(fetcher)) {
    if (!fetcher_) fetcher_ = [](const std::string& url) { return curl_get(url); };
}

std::expected<std::string, std::string> HttpFetchTool::curl_get(const std::string& url,
                                                                  long timeout_seconds) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::unexpected("curl_easy_init thất bại");
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OopAgentFramework/1.0");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return std::unexpected(std::string("Lỗi CURL: ") + curl_easy_strerror(res));
    if (status >= 400) return std::unexpected("HTTP " + std::to_string(status) + " khi tải " + url);
    return body;
}

ToolResult HttpFetchTool::execute(const std::string& args_json, Environment& /*env*/) {
    std::string url;
    try {
        auto j = nlohmann::json::parse(args_json);
        url = j.at("url").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho http_fetch: ") + e.what());
    }
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return ToolResult::fail("url phải bắt đầu bằng http:// hoặc https://");
    }

    auto result = fetcher_(url);
    if (!result.has_value()) return ToolResult::fail(result.error());

    std::string body = *result;
    if (body.size() > kMaxBodyChars) {
        body = body.substr(0, kMaxBodyChars) + "\n...[đã cắt bớt, nội dung gốc dài hơn]";
    }
    return ToolResult::ok(body);
}

}  // namespace agent
