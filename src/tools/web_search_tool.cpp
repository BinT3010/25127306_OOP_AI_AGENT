#include "web_search_tool.h"

#include <curl/curl.h>

#include <nlohmann/json.hpp>
#include <sstream>

#include "http_tool.h"

namespace agent {

WebSearchTool::WebSearchTool(std::string search_endpoint_template, Fetcher fetcher, int top_k)
    : endpoint_template_(std::move(search_endpoint_template)), fetcher_(std::move(fetcher)), top_k_(top_k) {
    if (!fetcher_) fetcher_ = [](const std::string& url) { return HttpFetchTool::curl_get(url, 10); };
}

std::string WebSearchTool::url_encode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    char* out = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result = out ? out : value;
    if (out) curl_free(out);
    curl_easy_cleanup(curl);
    return result;
}

ToolResult WebSearchTool::execute(const std::string& args_json, Environment& /*env*/) {
    std::string query;
    try {
        auto j = nlohmann::json::parse(args_json);
        query = j.at("query").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho web_search: ") + e.what());
    }
    if (query.empty()) return ToolResult::fail("Tham số 'query' rỗng");

    std::string url = endpoint_template_;
    const std::string placeholder = "{query}";
    if (auto pos = url.find(placeholder); pos != std::string::npos) {
        url.replace(pos, placeholder.size(), url_encode(query));
    }

    auto fetched = fetcher_(url);
    if (!fetched.has_value()) {
        return ToolResult::fail("Không gọi được search endpoint (" + url + "): " + fetched.error());
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(*fetched);
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Phản hồi search endpoint không phải JSON hợp lệ: ") + e.what());
    }
    if (!parsed.contains("results") || !parsed["results"].is_array()) {
        return ToolResult::fail("Phản hồi search endpoint thiếu field 'results' dạng mảng");
    }

    std::ostringstream oss;
    int count = 0;
    for (const auto& item : parsed["results"]) {
        if (count >= top_k_) break;
        std::string title = item.value("title", "(không có tiêu đề)");
        std::string content = item.value("content", "");
        std::string result_url = item.value("url", "");
        oss << (count + 1) << ". " << title << "\n   " << content << "\n   URL: " << result_url << "\n";
        ++count;
    }
    if (count == 0) return ToolResult::ok("Không tìm thấy kết quả nào cho truy vấn: " + query);
    return ToolResult::ok(oss.str());
}

}  // namespace agent
