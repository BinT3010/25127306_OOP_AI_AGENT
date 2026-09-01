#include "task.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../util/exceptions.h"

namespace agent {

Task Task::from_json(const nlohmann::json& j) {
    Task t;
    t.id = j.at("id").get<std::string>();
    t.description = j.value("description", "");
    t.instruction = j.at("instruction").get<std::string>();
    t.eval_type = j.value("eval_type", "keyword");
    if (j.contains("eval_script")) t.eval_script = j.at("eval_script").get<std::string>();
    if (j.contains("expected_keywords")) {
        t.expected_keywords = j.at("expected_keywords").get<std::vector<std::string>>();
    }
    if (j.contains("image_path")) t.image_path = j.at("image_path").get<std::string>();
    t.max_steps = j.value("max_steps", 10);
    return t;
}

std::vector<Task> load_tasks_from_file(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs) throw ConfigException("Không mở được file task tại: " + path.string());
    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const nlohmann::json::exception& e) {
        throw ConfigException("File task không phải JSON hợp lệ (" + path.string() + "): " + e.what());
    }
    if (!j.is_array()) throw ConfigException("File task phải là một mảng JSON: " + path.string());

    std::vector<Task> tasks;
    tasks.reserve(j.size());
    for (const auto& item : j) tasks.push_back(Task::from_json(item));
    return tasks;
}

}  // namespace agent
