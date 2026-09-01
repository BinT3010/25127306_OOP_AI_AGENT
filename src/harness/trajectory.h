#pragma once
/**
 * @file trajectory.h
 * @brief Data classes Step + Trajectory (mục 7.1 đề bài) — ghi lại toàn bộ
 * diễn biến một lần AgentLoop chạy để phục vụ Evaluator và báo cáo benchmark.
 *
 * Đây là các "plain data class" thuần tuý (không hành vi phức tạp) theo đúng
 * tinh thần đề bài mục 4.1 ("Trajectory + Step (data classes)"). Việc
 * (de)serialize JSON được đặt ngay trong lớp vì đó là trách nhiệm tự nhiên
 * của kiểu dữ liệu (tương tự các thư viện json hiện đại: to_json/from_json
 * "ADL hook" của nlohmann::json), không cần một lớp Serializer riêng.
 */
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace agent {

struct StepAction {
    std::string type;   ///< "tool_call" | "final_answer" | "think" | "malformed"
    std::string tool;    ///< tên tool (rỗng nếu type != tool_call)
    std::string args;    ///< JSON tham số thô (rỗng nếu type != tool_call)
};

struct Step {
    int step_id = 0;
    std::string thought;
    StepAction action;
    std::string tool_result;     ///< Observation trả về cho LLM (rỗng nếu final_answer/think)
    bool tool_success = true;
    int tokens_used = 0;
    long long latency_ms = 0;
};

struct Trajectory {
    std::string task_id;
    std::string model;
    bool success = false;
    std::string final_answer;
    std::string failure_reason;   ///< lý do thất bại/dừng sớm (rỗng nếu success == true)
    int total_tokens = 0;
    long long total_time_ms = 0;
    std::vector<Step> steps;

    [[nodiscard]] nlohmann::json to_json() const;
    static Trajectory from_json(const nlohmann::json& j);

    /// Ghi ra file JSON có định dạng dễ đọc (pretty-print, indent=2) — đúng
    /// tên file gợi ý trajectory_{task_id}.json (mục 7.1).
    void save_to_file(const std::filesystem::path& path) const;
};

}  // namespace agent
