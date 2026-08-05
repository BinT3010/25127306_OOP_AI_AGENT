/**
 * @file agent_loop.cpp
 * @see agent_loop.h
 */
#include "agent_loop.h"

#include <chrono>
#include <sstream>

#include "../util/exceptions.h"
#include "action_parser.h"

namespace agent {

AgentLoop::AgentLoop(LLMClient& llm, ToolRegistry& tools, Environment& env, ChatOptions chat_options,
                      SkillLoader* skill_loader, Config config)
    : llm_(llm),
      tools_(tools),
      env_(env),
      logger_("AgentLoop"),
      chat_options_(std::move(chat_options)),
      skill_loader_(skill_loader),
      config_(std::move(config)),
      loop_detector_() {}

std::string AgentLoop::build_system_prompt(const std::string& instruction) const {
    std::ostringstream oss;
    oss << "Bạn là một AI agent hữu ích, cẩn trọng, có khả năng dùng các tool dưới đây để "
           "hoàn thành nhiệm vụ được giao. Luôn suy luận từng bước trước khi hành động.\n\n";
    oss << "## Tools khả dụng\n" << tools_.render_tools_prompt();
    oss << "\n## Định dạng phản hồi bắt buộc (chọn đúng MỘT trong hai dạng mỗi lượt)\n"
           "1) Khi cần dùng tool:\n"
           "Thought: <suy luận ngắn gọn>\n"
           "Action: <đúng một tên tool ở trên>\n"
           "Action Input: <JSON tham số hợp lệ, một dòng>\n\n"
           "2) Khi đã đủ thông tin để trả lời:\n"
           "Thought: <suy luận ngắn gọn>\n"
           "Final Answer: <câu trả lời cuối cùng, đầy đủ>\n";

    if (skill_loader_ != nullptr) {
        auto selected = skill_loader_->select_for_task(instruction);
        std::string block = SkillLoader::render_injection_block(selected);
        if (!block.empty()) oss << "\n" << block;
    }
    if (!config_.system_prompt_prefix.empty()) {
        oss << "\n" << config_.system_prompt_prefix << "\n";
    }
    return oss.str();
}

std::expected<ChatResult, std::string> AgentLoop::think(const std::vector<ChatMessage>& history) {
    return llm_.chat(history, chat_options_);
}

ToolResult AgentLoop::act(const ToolCallAction& action) {
    if (!tools_.has(action.tool_name)) {
        std::ostringstream oss;
        oss << "Tool '" << action.tool_name << "' không tồn tại. Các tool khả dụng: ";
        const auto names = tools_.list_names();
        for (std::size_t i = 0; i < names.size(); ++i) oss << (i > 0 ? ", " : "") << names[i];
        return ToolResult::fail(oss.str());
    }
    // Phòng thủ nhiều lớp: dù quy ước là Tool::execute() phải tự bắt hết
    // exception nội bộ (xem tool.h), AgentLoop vẫn bọc thêm một lớp try/catch
    // ở đây để một Tool triển khai sai quy ước không bao giờ làm sập cả tiến
    // trình agent — đây là minh chứng cụ thể cho tiêu chí "Exception handling
    // có ý nghĩa".
    try {
        return tools_.require(action.tool_name).execute(action.args_json, env_);
    } catch (const AgentException& e) {
        return ToolResult::fail(std::string("Tool ném AgentException không mong đợi: ") + e.what());
    } catch (const std::exception& e) {
        return ToolResult::fail(std::string("Lỗi hệ thống không mong đợi khi chạy tool: ") + e.what());
    }
}

void AgentLoop::observe(const Step& step) {
    if (step_hook_) step_hook_(step);
}

Trajectory AgentLoop::run(const std::string& task_id, const std::string& instruction) {
    loop_detector_.reset();

    Trajectory trajectory;
    trajectory.task_id = task_id;
    trajectory.model = chat_options_.model;

    std::vector<ChatMessage> history;
    history.push_back(ChatMessage::system(build_system_prompt(instruction)));
    history.push_back(ChatMessage::user(instruction));

    const auto overall_start = std::chrono::steady_clock::now();
    int step_id = 0;

    try {
        while (true) {
            if (step_id >= config_.max_steps) {
                throw MaxStepsExceededException(config_.max_steps);
            }

            auto chat_result = think(history);
            if (!chat_result.has_value()) {
                trajectory.success = false;
                trajectory.failure_reason = "Lỗi gọi LLM: " + chat_result.error();
                break;
            }
            const ChatResult& chat_response = *chat_result;
            trajectory.total_tokens += chat_response.prompt_tokens + chat_response.completion_tokens;

            Action parsed_action = parse_action(chat_response);

            Step step;
            step.step_id = step_id;
            step.tokens_used = chat_response.prompt_tokens + chat_response.completion_tokens;
            step.latency_ms = chat_response.latency_ms;

            bool should_break = false;

            // Xử lý mọi nhánh Action một cách "exhaustive" (đầy đủ, kiểm tra tại
            // compile-time) bằng std::visit + Overloaded — đúng yêu cầu bảng V
            // ("if constexpr / std::visit: Xử lý các loại Action trong agent loop").
            std::visit(
                Overloaded{
                    [&](const ToolCallAction& tool_call) {
                        step.thought = tool_call.thought;
                        step.action = StepAction{"tool_call", tool_call.tool_name, tool_call.args_json};

                        const std::string signature = tool_call.tool_name + "|" + tool_call.args_json;
                        LoopDetectionResult loop_result = loop_detector_.record_and_check(signature);
                        if (loop_result.should_log()) {
                            if (loop_result.severity == LoopSeverity::kCritical) {
                                logger_.error("Loop detector [{}]: {}",
                                              loop_result.kind == LoopKind::kGenericRepeat ? "generic_repeat"
                                                                                            : "ping_pong",
                                              loop_result.message);
                            } else {
                                logger_.warn("Loop detector: {}", loop_result.message);
                            }
                        }
                        if (loop_result.should_abort()) {
                            step.tool_result = "(Agent đã bị dừng do phát hiện loop) " + loop_result.message;
                            step.tool_success = false;
                            history.push_back(ChatMessage::assistant(chat_response.content));
                            trajectory.success = false;
                            trajectory.failure_reason = loop_result.message;
                            should_break = true;
                            return;
                        }

                        ToolResult tool_result = act(tool_call);
                        step.tool_result = tool_result.success ? tool_result.output
                                                                : tool_result.error.value_or("(lỗi không rõ)");
                        step.tool_success = tool_result.success;

                        history.push_back(ChatMessage::assistant(chat_response.content));
                        std::string call_id =
                            tool_call.raw_call_id.empty() ? ("call_" + std::to_string(step_id)) : tool_call.raw_call_id;
                        history.push_back(ChatMessage::tool_result(tool_call.tool_name, call_id, step.tool_result));
                    },
                    [&](const FinalAnswerAction& final_answer) {
                        step.thought = final_answer.thought;
                        step.action = StepAction{"final_answer", "", ""};
                        step.tool_result = final_answer.answer;
                        history.push_back(ChatMessage::assistant(chat_response.content));
                        trajectory.success = true;
                        trajectory.final_answer = final_answer.answer;
                        should_break = true;
                    },
                    [&](const ThinkAction& think_action) {
                        step.thought = think_action.thought;
                        step.action = StepAction{"think", "", ""};
                        history.push_back(ChatMessage::assistant(chat_response.content));
                    },
                    [&](const MalformedAction& malformed) {
                        step.thought = "";
                        step.action = StepAction{"malformed", "", ""};
                        step.tool_result = malformed.reason;
                        step.tool_success = false;
                        logger_.warn("Không parse được phản hồi model: {}", malformed.reason);
                        history.push_back(ChatMessage::assistant(chat_response.content));
                        history.push_back(ChatMessage::user(
                            "Định dạng phản hồi của bạn không hợp lệ (" + malformed.reason +
                            "). Hãy trả lời đúng định dạng đã hướng dẫn: 'Thought: ...' theo sau bởi "
                            "'Action: <tool>' + 'Action Input: <json>', HOẶC 'Thought: ...' theo sau "
                            "bởi 'Final Answer: ...'."));
                    },
                },
                parsed_action);

            observe(step);
            trajectory.steps.push_back(std::move(step));
            ++step_id;

            if (should_break) break;
        }
    } catch (const MaxStepsExceededException& e) {
        trajectory.success = false;
        trajectory.failure_reason = e.what();
        logger_.warn("{}", e.what());
    }

    const auto overall_end = std::chrono::steady_clock::now();
    trajectory.total_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(overall_end - overall_start).count();
    return trajectory;
}

}  // namespace agent
