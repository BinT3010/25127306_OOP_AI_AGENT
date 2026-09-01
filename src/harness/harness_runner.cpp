/**
 * @file harness_runner.cpp
 * @see harness_runner.h
 */
#include "harness_runner.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../agent/agent_loop.h"
#include "../agent/sandbox_environment.h"
#include "../util/exceptions.h"
#include "functional_evaluator.h"
#include "keyword_evaluator.h"
#include "vlm_evaluator.h"

namespace agent {

nlohmann::json BatchReport::to_json() const {
    nlohmann::json j;
    j["total"] = total;
    j["passed"] = passed;
    j["success_rate"] = success_rate;
    auto results_json = nlohmann::json::array();
    for (const auto& r : results) {
        nlohmann::json rj;
        rj["task_id"] = r.task.id;
        rj["description"] = r.task.description;
        rj["eval_type"] = r.task.eval_type;
        rj["passed"] = r.eval_result.passed;
        rj["score"] = r.eval_result.score;
        rj["reason"] = r.eval_result.reason;
        rj["trajectory_success"] = r.trajectory.success;
        rj["total_tokens"] = r.trajectory.total_tokens;
        rj["total_time_ms"] = r.trajectory.total_time_ms;
        rj["num_steps"] = r.trajectory.steps.size();
        results_json.push_back(std::move(rj));
    }
    j["results"] = std::move(results_json);
    return j;
}

void BatchReport::save_to_file(const std::filesystem::path& path) const {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream ofs(path);
    ofs << to_json().dump(2);
}

HarnessRunner::HarnessRunner(LLMClient& llm, ToolRegistry& tools, SkillLoader* skill_loader,
                              ChatOptions chat_options, Config config)
    : llm_(llm),
      tools_(tools),
      skill_loader_(skill_loader),
      chat_options_(std::move(chat_options)),
      config_(std::move(config)),
      logger_("HarnessRunner") {}

std::unique_ptr<Evaluator> HarnessRunner::make_evaluator(const Task& task,
                                                          const std::filesystem::path& task_working_dir) {
    if (task.eval_type == "keyword") {
        return std::make_unique<KeywordEvaluator>();
    }
    if (task.eval_type == "functional") {
        return std::make_unique<FunctionalEvaluator>(task_working_dir);
    }
    if (task.eval_type == "vlm") {
        std::string judge_model = config_.vlm_judge_model.empty() ? chat_options_.model : config_.vlm_judge_model;
        return std::make_unique<VLMEvaluator>(llm_, judge_model);
    }
    throw ConfigException("eval_type không hợp lệ cho task '" + task.id + "': '" + task.eval_type +
                           "' (chỉ chấp nhận keyword|functional|vlm)");
}

TaskRunResult HarnessRunner::run_task(const Task& task) {
    logger_.info("=== Bắt đầu task '{}': {} ===", task.id, task.description);

    // ---- 1) setup environment ----
    std::filesystem::path task_dir = config_.sandbox_root / task.id;
    SandboxEnvironment env(task_dir);
    env.setup();

    // ---- 2) run agent ----
    AgentLoop::Config loop_cfg;
    loop_cfg.max_steps = task.max_steps;
    AgentLoop loop(llm_, tools_, env, chat_options_, skill_loader_, loop_cfg);

    // OBSERVER/HOOK: quan sát tiến độ theo thời gian thực mà không thay đổi
    // giá trị Trajectory cuối cùng do run() trả về.
    loop.set_step_hook([this, &task](const Step& step) {
        logger_.debug("[{}] step {}: action={} tool='{}'", task.id, step.step_id, step.action.type,
                       step.action.tool);
    });

    Trajectory trajectory = loop.run(task.id, task.instruction);

    // ---- 3) evaluate (trong khi sandbox còn tồn tại, TRƯỚC khi teardown) ----
    auto evaluator = make_evaluator(task, env.working_directory());
    EvalResult eval_result = evaluator->evaluate(trajectory, task);

    // ---- 4) record ----
    std::filesystem::path traj_path = config_.trajectory_output_dir / ("trajectory_" + task.id + ".json");
    trajectory.save_to_file(traj_path);

    env.teardown();

    logger_.info("Task '{}': {} (score={:.2f}) — {}", task.id, eval_result.passed ? "PASS" : "FAIL",
                 eval_result.score, eval_result.reason);

    return TaskRunResult{task, std::move(trajectory), eval_result};
}

BatchReport HarnessRunner::run_batch(const std::vector<Task>& tasks) {
    BatchReport report;
    report.total = static_cast<int>(tasks.size());
    report.results.reserve(tasks.size());

    for (const auto& task : tasks) {
        TaskRunResult result = run_task(task);
        if (result.eval_result.passed) ++report.passed;
        report.results.push_back(std::move(result));
    }
    report.success_rate = report.total == 0 ? 0.0 : static_cast<double>(report.passed) / report.total;

    logger_.info("=== Batch hoàn tất: {}/{} pass ({:.1f}%) ===", report.passed, report.total,
                 report.success_rate * 100.0);
    return report;
}

}  // namespace agent
