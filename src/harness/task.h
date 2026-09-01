#pragma once
/**
 * @file task.h
 * @brief Data class Task — định nghĩa một nhiệm vụ benchmark (mục 7.2 đề bài).
 */
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "../util/exceptions.h"

namespace agent {

struct Task {
    std::string id;
    std::string description;
    std::string instruction;
    std::string eval_type;  ///< "keyword" | "functional" | "vlm"
    std::optional<std::string> eval_script;                  ///< dùng cho eval_type == "functional"
    std::optional<std::vector<std::string>> expected_keywords;  ///< dùng cho eval_type == "keyword"
    std::optional<std::string> image_path;   ///< tuỳ chọn, dùng cho eval_type == "vlm" (bằng chứng hình ảnh)
    int max_steps = 10;

    static Task from_json(const nlohmann::json& j);
};

/// Đọc toàn bộ danh sách task từ file JSON dạng mảng (xem benchmark/tasks.json).
[[nodiscard]] std::vector<Task> load_tasks_from_file(const std::filesystem::path& path);

}  // namespace agent
