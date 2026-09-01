#pragma once
/**
 * @file harness_runner.h
 * @brief HarnessRunner — tầng orchestration cấp cao nhất (mục 3.6): setup
 * environment → run agent → evaluate → record, cho từng task, và tổng hợp
 * thành batch report (success rate) cho toàn bộ benchmark.
 *
 * Đây chính là nơi thể hiện rõ OBSERVER/HOOK PATTERN theo mục 4.2: HarnessRunner
 * tiêm một `step_hook` vào AgentLoop để "nghe" từng Step theo thời gian thực
 * (vd: log tiến độ) — trong khi AgentLoop hoàn toàn không biết (và không cần
 * include) harness_runner.h, chỉ expose một std::function<void(const Step&)>
 * chung chung (đúng nguyên tắc tách lớp mục 4.4).
 *
 * Mỗi task chạy trong một SandboxEnvironment RIÊNG (thư mục con độc lập dưới
 * Config::sandbox_root) để tránh việc 2 task ghi đè file lẫn nhau — trong khi
 * ToolRegistry (kể cả MemoryTool có trạng thái) được CHIA SẺ xuyên suốt batch,
 * mô phỏng đúng ngữ cảnh "persistent memory qua nhiều lượt chạy" (mục 10.2).
 */
#include <filesystem>
#include <memory>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "../agent/skill_loader.h"
#include "../util/exceptions.h"
#include "../client/llm_client.h"
#include "../tools/tool_registry.h"
#include "../util/logger.h"
#include "evaluator.h"
#include "task.h"
#include "trajectory.h"

namespace agent {

struct TaskRunResult {
    Task task;
    Trajectory trajectory;
    EvalResult eval_result;

    /// C++20 three-way comparison — dùng để sắp xếp kết quả theo điểm số giảm
    /// dần khi in bảng tổng kết (xem benchmark/run_eval.cpp). Cần định nghĩa
    /// kèm operator== (không chỉ <=>) vì std::ranges::less thực chất yêu cầu
    /// concept totally_ordered_with, mà bản thân nó đòi hỏi cả equality_comparable.
    [[nodiscard]] auto operator<=>(const TaskRunResult& other) const {
        return other.eval_result.score <=> eval_result.score;
    }
    [[nodiscard]] bool operator==(const TaskRunResult& other) const {
        return eval_result.score == other.eval_result.score;
    }
};

struct BatchReport {
    int total = 0;
    int passed = 0;
    double success_rate = 0.0;
    std::vector<TaskRunResult> results;

    [[nodiscard]] nlohmann::json to_json() const;
    void save_to_file(const std::filesystem::path& path) const;
};

class HarnessRunner {
public:
    struct Config {
        std::filesystem::path sandbox_root = "benchmark_runs/sandboxes";
        std::filesystem::path trajectory_output_dir = "benchmark_runs/trajectories";
        std::string vlm_judge_model;  ///< rỗng = dùng lại chat_options_.model làm giám khảo VLM
    };

    HarnessRunner(LLMClient& llm, ToolRegistry& tools, SkillLoader* skill_loader, ChatOptions chat_options,
                  Config config);
    HarnessRunner(LLMClient& llm, ToolRegistry& tools, SkillLoader* skill_loader, ChatOptions chat_options)
        : HarnessRunner(llm, tools, skill_loader, std::move(chat_options), Config{}) {}

    /// setup environment → run agent → evaluate → record, cho MỘT task.
    [[nodiscard]] TaskRunResult run_task(const Task& task);

    /// Batch evaluation: chạy toàn bộ tập task tuần tự, tính success rate.
    [[nodiscard]] BatchReport run_batch(const std::vector<Task>& tasks);

private:
    LLMClient& llm_;
    ToolRegistry& tools_;
    SkillLoader* skill_loader_;
    ChatOptions chat_options_;
    Config config_;
    util::Logger logger_;

    [[nodiscard]] std::unique_ptr<Evaluator> make_evaluator(const Task& task,
                                                             const std::filesystem::path& task_working_dir);
};

}  // namespace agent
