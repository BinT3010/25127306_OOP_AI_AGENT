/**
 * @file test_multiagent.cpp
 * @brief Unit test cho SubAgentOrchestrator (bonus mục 10.3): chạy song song
 * nhiều sub-agent bằng std::thread, giao tiếp qua MessageQueue.
 */
#include "doctest.h"

#include "../src/client/mock_llm_client.h"
#include "../src/multiagent/sub_agent_orchestrator.h"
#include "../src/tools/calculator_tool.h"

TEST_CASE("SubAgentOrchestrator: 2 sub-agent chạy song song, mỗi agent có ToolRegistry riêng") {
    agent::MockLLMClient mock;
    mock.set_responder([](const std::vector<agent::ChatMessage>& history, const agent::ChatOptions&) {
        for (const auto& m : history) {
            if (m.role == "tool") {
                agent::ChatResult r;
                r.content = "Thought: xong\nFinal Answer: " + m.content;
                return r;
            }
        }
        std::string instr;
        for (const auto& m : history)
            if (m.role == "user") instr = m.content;
        agent::ChatResult r;
        r.content = "Thought: tinh\nAction: calculator\nAction Input: {\"expression\": \"" + instr + "\"}";
        return r;
    });

    agent::multiagent::SubAgentOrchestrator::ToolRegistryFactory factory = [] {
        auto reg = std::make_unique<agent::ToolRegistry>();
        reg->register_tool(std::make_unique<agent::CalculatorTool>());
        return reg;
    };

    auto run_root = std::filesystem::temp_directory_path() / "agent_test_multiagent_doctest";
    agent::multiagent::SubAgentOrchestrator orch(mock, factory, agent::ChatOptions{.model = "mock"}, run_root);

    std::vector<agent::multiagent::SubAgentOrchestrator::SubTask> tasks = {
        {"agentA", "4*4", 5},
        {"agentB", "7*7", 5},
    };
    auto results = orch.run_parallel(tasks);

    REQUIRE(results.size() == 2);
    bool found_a = false, found_b = false;
    for (auto& r : results) {
        CHECK(r.trajectory.success);
        if (r.agent_id == "agentA") {
            CHECK(r.trajectory.final_answer == "16");
            found_a = true;
        }
        if (r.agent_id == "agentB") {
            CHECK(r.trajectory.final_answer == "49");
            found_b = true;
        }
    }
    CHECK(found_a);
    CHECK(found_b);

    // Mỗi sub-agent phải phát ít nhất 1 message "step" + 1 message "result" qua hàng đợi chung.
    CHECK(orch.last_run_messages().size() >= 4);
}
