#pragma once
/**
 * @file agent_loop.h
 * @brief TEMPLATE METHOD PATTERN: vòng lặp ReAct cốt lõi của toàn hệ thống.
 *
 * `run()` là "khung sườn" (skeleton) CỐ ĐỊNH, không virtual — không lớp con
 * nào được phép thay đổi trình tự Observe → Think → Act → Observe hay logic
 * max_steps/loop-detection. Ba bước con think()/act()/observe() là các
 * "hook" virtual protected mà lớp con CÓ THỂ override để tuỳ biến từng bước
 * riêng lẻ mà không phá vỡ bất biến tổng thể của thuật toán — đúng tinh thần
 * Template Method (xem thêm ConfirmingAgentLoop trong confirming_agent_loop.h
 * là ví dụ override act() cụ thể).
 *
 * Tuân thủ nguyên tắc tách lớp (mục 4.4): AgentLoop chỉ biết tới LLMClient,
 * ToolRegistry, Environment, SkillLoader (các tầng "ngang hàng") — hoàn toàn
 * KHÔNG include harness_runner.h hay bất kỳ khái niệm nào của Harness. Kênh
 * duy nhất để bên ngoài "quan sát" quá trình chạy là `step_hook_`
 * (OBSERVER/HOOK PATTERN) — HarnessRunner tiêm một hook vào đây để ghi nhận
 * từng Step theo thời gian thực mà AgentLoop không hề biết ai đang lắng nghe.
 */
#include <expected>
#include <functional>
#include <string>
#include <vector>

#include "../client/llm_client.h"
#include "../client/llm_types.h"
#include "../harness/trajectory.h"
#include "../tools/tool.h"
#include "../tools/tool_registry.h"
#include "../util/logger.h"
#include "action.h"
#include "environment.h"
#include "loop_detector.h"
#include "skill_loader.h"

namespace agent {

class AgentLoop {
public:
    using StepHook = std::function<void(const Step&)>;

    struct Config {
        int max_steps = 10;
        std::string system_prompt_prefix;  ///< văn bản bổ sung tuỳ chọn, nối thêm vào system prompt
    };

    /// Constructor đầy đủ tham số. `skill_loader` có thể là nullptr nếu không
    /// cần Skill injection (vd: demo nhanh không cần skill).
    AgentLoop(LLMClient& llm, ToolRegistry& tools, Environment& env, ChatOptions chat_options,
              SkillLoader* skill_loader, Config config);

    /// Tiện ích: dùng Config mặc định, có SkillLoader.
    AgentLoop(LLMClient& llm, ToolRegistry& tools, Environment& env, ChatOptions chat_options,
              SkillLoader* skill_loader)
        : AgentLoop(llm, tools, env, std::move(chat_options), skill_loader, Config{}) {}

    /// Tiện ích: không SkillLoader, Config mặc định.
    AgentLoop(LLMClient& llm, ToolRegistry& tools, Environment& env, ChatOptions chat_options)
        : AgentLoop(llm, tools, env, std::move(chat_options), nullptr, Config{}) {}

    virtual ~AgentLoop() = default;
    AgentLoop(const AgentLoop&) = delete;
    AgentLoop& operator=(const AgentLoop&) = delete;

    /// OBSERVER/HOOK: đăng ký callback được gọi ngay sau mỗi Step hoàn tất.
    void set_step_hook(StepHook hook) { step_hook_ = std::move(hook); }

    /// TEMPLATE METHOD — khung sườn cố định của thuật toán ReAct. KHÔNG virtual.
    [[nodiscard]] Trajectory run(const std::string& task_id, const std::string& instruction);

protected:
    // ---- Các bước con có thể override (Template Method hooks) ----

    /// Bước THINK: gọi LLM với lịch sử hội thoại hiện tại.
    [[nodiscard]] virtual std::expected<ChatResult, std::string> think(
        const std::vector<ChatMessage>& history);

    /// Bước ACT: thực thi một ToolCallAction đã được quyết định. Mặc định tra
    /// ToolRegistry và bắt mọi exception rò rỉ từ Tool, KHÔNG bao giờ để lộ
    /// exception ra run() — luôn trả ToolResult (thành công hoặc thất bại rõ ràng).
    [[nodiscard]] virtual ToolResult act(const ToolCallAction& action);

    /// Bước OBSERVE (hook): gọi ngay sau khi một Step được hoàn thiện. Mặc
    /// định chỉ chuyển tiếp cho step_hook_ nếu có — lớp con override để thêm
    /// logic (vd: log chi tiết hơn) mà vẫn giữ hành vi gốc bằng cách gọi lại
    /// AgentLoop::observe(step).
    virtual void observe(const Step& step);

    LLMClient& llm_;
    ToolRegistry& tools_;
    Environment& env_;
    util::Logger logger_;

private:
    ChatOptions chat_options_;
    SkillLoader* skill_loader_;
    Config config_;
    LoopDetector loop_detector_;
    StepHook step_hook_;

    [[nodiscard]] std::string build_system_prompt(const std::string& instruction) const;
};

}  // namespace agent
