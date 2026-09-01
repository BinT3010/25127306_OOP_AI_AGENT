#pragma once
/**
 * @file datetime_tool.h
 * @brief Tool "datetime" — tool bổ sung #2/3 (loại "utility/thời gian").
 * Đây cũng chính là tool phục vụ nhóm task benchmark "lấy thời gian" (mục 7.3).
 * Minh hoạ C++20 <chrono> calendar (std::chrono::year_month_day, sys_days).
 */
#include "tool.h"

namespace agent {

class DateTimeTool : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "datetime"; }
    [[nodiscard]] std::string description() const override {
        return "Lấy ngày giờ hiện tại (action=now), hoặc tính số ngày giữa 2 mốc "
               "(action=diff_days), hoặc cộng/trừ số ngày vào một mốc (action=add_days).";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"action": "now|diff_days|add_days", "date": "YYYY-MM-DD" (tuỳ chọn cho now), )"
               R"("other_date": "YYYY-MM-DD" (bắt buộc cho diff_days), )"
               R"("days": <số nguyên, bắt buộc cho add_days>})";
    }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;
};

}  // namespace agent
