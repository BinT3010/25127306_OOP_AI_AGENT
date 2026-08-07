#pragma once
/**
 * @file agent_loop_builder.h
 * @brief BUILDER PATTERN (bonus, mục 4.2 ghi chú): dựng AgentLoop qua chuỗi
 * gọi hàm (fluent interface) thay vì một constructor với quá nhiều tham số.
 * Hữu ích vì AgentLoop có nhiều tham số tuỳ chọn (skill loader, max_steps,
 * temperature, prefix...) — Builder giúp code gọi ở main.cpp/harness_runner.cpp
 * đọc rõ ràng theo tên, không cần nhớ đúng thứ tự tham số.
 *
 * Minh hoạ kỹ thuật C++23 "explicit object parameter" (deducing this,
 * mục V nâng cao): mỗi hàm with_*() chỉ viết MỘT định nghĩa duy nhất nhưng
 * vẫn hoạt động đúng cho cả lvalue lẫn rvalue builder — ví dụ điển hình:
 *   AgentLoopBuilder().with_llm(x).with_tools(y).build();   // toàn rvalue chain
 *   AgentLoopBuilder b; b.with_llm(x); b.build();            // lvalue
 * `build(this auto&& self)` sau đó "di chuyển" (std::move) các thành viên
 * lớn (ChatOptions, Config) ra khỏi self thay vì sao chép, tận dụng đúng
 * value-category thực tế tại điểm gọi.
 */
#include <memory>
#include <string>

#include "../agent/agent_loop.h"
#include "../agent/confirming_agent_loop.h"
#include "../util/exceptions.h"

namespace agent {

class AgentLoopBuilder {
public:
    decltype(auto) with_llm(this auto&& self, LLMClient& llm) {
        self.llm_ = &llm;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_tools(this auto&& self, ToolRegistry& tools) {
        self.tools_ = &tools;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_environment(this auto&& self, Environment& env) {
        self.env_ = &env;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_skill_loader(this auto&& self, SkillLoader& loader) {
        self.skill_loader_ = &loader;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_model(this auto&& self, std::string model) {
        self.chat_options_.model = std::move(model);
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_temperature(this auto&& self, double temperature) {
        self.chat_options_.temperature = temperature;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_max_tokens(this auto&& self, int max_tokens) {
        self.chat_options_.max_tokens = max_tokens;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_max_steps(this auto&& self, int max_steps) {
        self.config_.max_steps = max_steps;
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) with_system_prompt_prefix(this auto&& self, std::string prefix) {
        self.config_.system_prompt_prefix = std::move(prefix);
        return std::forward<decltype(self)>(self);
    }
    /// Bật chế độ "thận trọng" (ConfirmingAgentLoop) — xem confirming_agent_loop.h.
    decltype(auto) confirming(this auto&& self, bool enable = true) {
        self.use_confirming_ = enable;
        return std::forward<decltype(self)>(self);
    }

    /// Dựng AgentLoop cuối cùng. Ném ConfigException nếu thiếu tham số bắt buộc.
    [[nodiscard]] std::unique_ptr<AgentLoop> build(this auto&& self) {
        if (self.llm_ == nullptr) throw ConfigException("AgentLoopBuilder: thiếu LLMClient — gọi with_llm()");
        if (self.tools_ == nullptr) throw ConfigException("AgentLoopBuilder: thiếu ToolRegistry — gọi with_tools()");
        if (self.env_ == nullptr) throw ConfigException("AgentLoopBuilder: thiếu Environment — gọi with_environment()");

        if (self.use_confirming_) {
            return std::make_unique<ConfirmingAgentLoop>(*self.llm_, *self.tools_, *self.env_,
                                                           std::move(self.chat_options_), self.skill_loader_,
                                                           std::move(self.config_));
        }
        return std::make_unique<AgentLoop>(*self.llm_, *self.tools_, *self.env_, std::move(self.chat_options_),
                                            self.skill_loader_, std::move(self.config_));
    }

private:
    LLMClient* llm_ = nullptr;
    ToolRegistry* tools_ = nullptr;
    Environment* env_ = nullptr;
    SkillLoader* skill_loader_ = nullptr;
    ChatOptions chat_options_;
    AgentLoop::Config config_;
    bool use_confirming_ = false;
};

}  // namespace agent
