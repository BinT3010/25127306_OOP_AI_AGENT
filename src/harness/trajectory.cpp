/**
 * @file trajectory.cpp
 * @see trajectory.h
 */
#include "trajectory.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace agent {

nlohmann::json Trajectory::to_json() const {
    nlohmann::json j;
    j["task_id"] = task_id;
    j["model"] = model;
    j["success"] = success;
    j["final_answer"] = final_answer;
    if (!failure_reason.empty()) j["failure_reason"] = failure_reason;
    j["total_tokens"] = total_tokens;
    j["total_time_ms"] = total_time_ms;

    auto steps_json = nlohmann::json::array();
    for (const auto& s : steps) {
        nlohmann::json sj;
        sj["step_id"] = s.step_id;
        sj["thought"] = s.thought;
        sj["action"] = {{"type", s.action.type}, {"tool", s.action.tool}, {"args", s.action.args}};
        sj["tool_result"] = s.tool_result;
        sj["tool_success"] = s.tool_success;
        sj["tokens_used"] = s.tokens_used;
        sj["latency_ms"] = s.latency_ms;
        steps_json.push_back(std::move(sj));
    }
    j["steps"] = std::move(steps_json);
    return j;
}

Trajectory Trajectory::from_json(const nlohmann::json& j) {
    Trajectory t;
    t.task_id = j.value("task_id", "");
    t.model = j.value("model", "");
    t.success = j.value("success", false);
    t.final_answer = j.value("final_answer", "");
    t.failure_reason = j.value("failure_reason", "");
    t.total_tokens = j.value("total_tokens", 0);
    t.total_time_ms = j.value("total_time_ms", 0LL);

    if (j.contains("steps")) {
        for (const auto& sj : j.at("steps")) {
            Step s;
            s.step_id = sj.value("step_id", 0);
            s.thought = sj.value("thought", "");
            if (sj.contains("action")) {
                const auto& aj = sj.at("action");
                s.action.type = aj.value("type", "");
                s.action.tool = aj.value("tool", "");
                s.action.args = aj.value("args", "");
            }
            s.tool_result = sj.value("tool_result", "");
            s.tool_success = sj.value("tool_success", true);
            s.tokens_used = sj.value("tokens_used", 0);
            s.latency_ms = sj.value("latency_ms", 0LL);
            t.steps.push_back(std::move(s));
        }
    }
    return t;
}

void Trajectory::save_to_file(const std::filesystem::path& path) const {
    // path.parent_path() rỗng khi `path` chỉ là tên file trần (vd: "out.json",
    // nghĩa là "lưu vào thư mục hiện hành") — std::filesystem::create_directories
    // ném exception với đường dẫn rỗng, nên phải kiểm tra trước khi gọi.
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream ofs(path);
    ofs << to_json().dump(2);
}

}  // namespace agent
