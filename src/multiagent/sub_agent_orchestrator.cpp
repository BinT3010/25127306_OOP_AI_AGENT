/**
 * @file sub_agent_orchestrator.cpp
 * @see sub_agent_orchestrator.h
 */
#include "sub_agent_orchestrator.h"

#include <thread>

#include "../agent/agent_loop.h"
#include "../agent/sandbox_environment.h"

namespace agent::multiagent {

SubAgentOrchestrator::SubAgentOrchestrator(LLMClient& llm, ToolRegistryFactory registry_factory,
                                            ChatOptions chat_options, std::filesystem::path runs_root)
    : llm_(llm),
      registry_factory_(std::move(registry_factory)),
      chat_options_(std::move(chat_options)),
      runs_root_(std::move(runs_root)) {}

void SubAgentOrchestrator::run_one_sub_agent(const SubTask& task, SubAgentResult& out_result) {
    // Mỗi sub-agent sở hữu ToolRegistry RIÊNG (tạo mới từ factory) và
    // SandboxEnvironment RIÊNG — không có state nào (kể cả MemoryTool/SQLite)
    // bị chia sẻ giữa các thread, tránh data race hoàn toàn ở tầng Tool.
    std::unique_ptr<ToolRegistry> tools = registry_factory_();
    SandboxEnvironment env(runs_root_ / task.agent_id);
    env.setup();

    AgentLoop::Config loop_cfg;
    loop_cfg.max_steps = task.max_steps;
    AgentLoop loop(llm_, *tools, env, chat_options_, nullptr, loop_cfg);

    // Tái sử dụng OBSERVER/HOOK sẵn có của AgentLoop: mỗi bước ReAct của
    // sub-agent này được đẩy thành một AgentMessage vào hàng đợi dùng chung —
    // đây chính là "kênh giao tiếp" giữa các sub-agent với bộ điều phối trung
    // tâm (mục 10.3: "Agent giao tiếp qua message queue").
    loop.set_step_hook([this, &task](const Step& step) {
        AgentMessage msg;
        msg.from_agent_id = task.agent_id;
        msg.type = "step";
        msg.payload = "bước " + std::to_string(step.step_id) + ": " + step.action.type +
                      (step.action.tool.empty() ? "" : (" (tool=" + step.action.tool + ")"));
        message_queue_.push(std::move(msg));
    });

    Trajectory trajectory = loop.run(task.agent_id, task.instruction);
    env.teardown();

    AgentMessage final_msg;
    final_msg.from_agent_id = task.agent_id;
    final_msg.type = trajectory.success ? "result" : "error";
    final_msg.payload = trajectory.success ? trajectory.final_answer : trajectory.failure_reason;
    message_queue_.push(std::move(final_msg));

    out_result.agent_id = task.agent_id;
    out_result.trajectory = std::move(trajectory);
}

std::vector<SubAgentOrchestrator::SubAgentResult> SubAgentOrchestrator::run_parallel(
    const std::vector<SubTask>& sub_tasks) {
    std::vector<SubAgentResult> results(sub_tasks.size());
    std::vector<std::thread> threads;
    threads.reserve(sub_tasks.size());

    // Spawn một std::thread cho mỗi sub-task — chạy THỰC SỰ song song (không
    // phải giả lập tuần tự), đúng yêu cầu "spawn sub-agent (thread mới)".
    for (std::size_t i = 0; i < sub_tasks.size(); ++i) {
        threads.emplace_back([this, &sub_tasks, i, &results] { run_one_sub_agent(sub_tasks[i], results[i]); });
    }
    for (std::thread& t : threads) t.join();

    collected_messages_.clear();
    while (std::optional<AgentMessage> msg = message_queue_.try_pop()) {
        collected_messages_.push_back(std::move(*msg));
    }

    return results;
}

}  // namespace agent::multiagent
