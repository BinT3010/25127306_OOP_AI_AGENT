#include "datetime_tool.h"

#include <chrono>
#include <expected>
#include <format>
#include <nlohmann/json.hpp>
#include <sstream>

namespace agent {

namespace {
namespace chr = std::chrono;

std::expected<chr::sys_days, std::string> parse_ymd(const std::string& s) {
    int y, m, d;
    char dash1, dash2;
    std::istringstream iss(s);
    iss >> y >> dash1 >> m >> dash2 >> d;
    if (iss.fail() || dash1 != '-' || dash2 != '-') {
        return std::unexpected("Định dạng ngày không hợp lệ (cần YYYY-MM-DD): " + s);
    }
    chr::year_month_day ymd{chr::year{y}, chr::month{static_cast<unsigned>(m)},
                             chr::day{static_cast<unsigned>(d)}};
    if (!ymd.ok()) return std::unexpected("Ngày không hợp lệ trên lịch: " + s);
    return chr::sys_days{ymd};
}
}  // namespace

ToolResult DateTimeTool::execute(const std::string& args_json, Environment& /*env*/) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(args_json);
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho datetime: ") + e.what());
    }
    std::string action = j.value("action", "now");

    if (action == "now") {
        auto now = chr::system_clock::now();
        return ToolResult::ok(std::format("{:%Y-%m-%d %H:%M:%S} UTC", chr::floor<chr::seconds>(now)));
    }

    if (action == "diff_days") {
        if (!j.contains("date") || !j.contains("other_date")) {
            return ToolResult::fail("diff_days cần cả 'date' và 'other_date'");
        }
        auto d1 = parse_ymd(j.at("date").get<std::string>());
        auto d2 = parse_ymd(j.at("other_date").get<std::string>());
        if (!d1) return ToolResult::fail(d1.error());
        if (!d2) return ToolResult::fail(d2.error());
        auto diff = (*d2 - *d1).count();
        return ToolResult::ok(std::to_string(diff) + " ngày");
    }

    if (action == "add_days") {
        if (!j.contains("date") || !j.contains("days")) {
            return ToolResult::fail("add_days cần cả 'date' và 'days'");
        }
        auto d1 = parse_ymd(j.at("date").get<std::string>());
        if (!d1) return ToolResult::fail(d1.error());
        int days = j.at("days").get<int>();
        chr::sys_days result_days = *d1 + chr::days{days};
        chr::year_month_day result_ymd{result_days};
        return ToolResult::ok(std::format("{:%Y-%m-%d}", result_ymd));
    }

    return ToolResult::fail("action không hợp lệ: '" + action + "' (chỉ chấp nhận now|diff_days|add_days)");
}

}  // namespace agent
