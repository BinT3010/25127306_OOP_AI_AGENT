#pragma once
/**
 * @file sub_agent_orchestrator.h
 * @brief BONUS mục 10.3: HarnessRunner (hoặc bất kỳ ai) có thể spawn nhiều
 * sub-agent chạy SONG SONG trên std::thread riêng biệt cho từng subtask, mỗi
 * sub-agent có Environment (sandbox) VÀ ToolRegistry riêng — tránh mọi trạng
 * thái dùng chung giữa các thread (vd: MemoryTool sở hữu một kết nối SQLite
 * không an toàn khi dùng chung giữa nhiều thread). Duy nhất LLMClient được
 * dùng chung: OllamaClient thiết kế mỗi lời gọi tự tạo CURL handle cục bộ
 * (xem ollama_client.cpp) nên an toàn khi gọi đồng thời từ nhiều thread.
 *
 * Giao tiếp giữa các sub-agent với "trung tâm điều phối" đi qua
 * MessageQueue<AgentMessage> dùng chung — mỗi sub-agent đẩy một AgentMessage
 * sau MỖI bước ReAct (tận dụng lại step_hook / Observer Pattern đã có ở
 * AgentLoop) và một message "result" cuối cùng khi hoàn tất.
 */
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../client/llm_client.h"
#include "../client/llm_types.h"
#include "../harness/trajectory.h"
#include "../tools/tool_registry.h"
#include "agent_message.h"
#include "message_queue.h"

namespace agent::multiagent {

class SubAgentOrchestrator {
public:
    using ToolRegistryFactory = std::function<std::unique_ptr<ToolRegistry>()>;

    struct SubTask {
        std::string agent_id;
        std::string instruction;
        int max_steps = 10;
    };

    struct SubAgentResult {
        std::string agent_id;
        Trajectory trajectory;
    };

    /// @param registry_factory  hàm tạo một ToolRegistry MỚI, ĐỘC LẬP cho mỗi
    ///        sub-agent — đây là điểm mấu chốt đảm bảo an toàn luồng (mỗi
    ///        thread có Tool instance riêng, không đối tượng nào bị chia sẻ
    ///        đáng lo ngại giữa hai thread cùng lúc).
    SubAgentOrchestrator(LLMClient& llm, ToolRegistryFactory registry_factory, ChatOptions chat_options,
                          std::filesystem::path runs_root);

    /// Chạy toàn bộ `sub_tasks` ĐỒNG THỜI (mỗi task 1 std::thread), chờ tất cả
    /// hoàn tất (join), rồi trả kết quả theo ĐÚNG THỨ TỰ đầu vào (không phải
    /// thứ tự hoàn thành thực tế — tiện cho việc so khớp kết quả với subtask).
    [[nodiscard]] std::vector<SubAgentResult> run_parallel(const std::vector<SubTask>& sub_tasks);

    /// Toàn bộ AgentMessage đã trao đổi trong lần run_parallel() gần nhất
    /// (dùng để demo / log lại luồng giao tiếp giữa các sub-agent).
    [[nodiscard]] const std::vector<AgentMessage>& last_run_messages() const noexcept {
        return collected_messages_;
    }

private:
    LLMClient& llm_;
    ToolRegistryFactory registry_factory_;
    ChatOptions chat_options_;
    std::filesystem::path runs_root_;
    MessageQueue<AgentMessage> message_queue_;
    std::vector<AgentMessage> collected_messages_;

    void run_one_sub_agent(const SubTask& task, SubAgentResult& out_result);
};

}  // namespace agent::multiagent
