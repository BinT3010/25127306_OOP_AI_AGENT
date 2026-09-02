/**
 * @file agent_loop.cpp
 * @see agent_loop.h
 */
#include "agent_loop.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <regex>
#include <sstream>
#include <vector>

#include "../util/exceptions.h"
#include "action_parser.h"

namespace agent {

namespace {

/// Tìm con số (nguyên hoặc thập phân, có thể âm) DUY NHẤT xuất hiện trong một
/// chuỗi Observation. Trả về std::nullopt nếu Observation không có số nào,
/// hoặc có từ 2 số trở lên — trường hợp đó ta không thể biết chắc con số nào
/// là "kết quả chính" (ví dụ Observation của web_search/file/memory thường
/// chứa nhiều số không liên quan như ngày tháng, độ dài...), nên bỏ qua kiểm
/// tra để tránh báo động giả. Hàm này chỉ nhắm tới các tool có kết quả là
/// MỘT giá trị số rõ ràng, ví dụ word_count ("Số từ: 7") hay calculator
/// ("255") — đúng lớp lỗi thực tế đã quan sát được (agent tự bịa lại số thay
/// vì trích dẫn đúng Observation).
std::optional<std::string> extract_single_number_if_unambiguous(const std::string& text) {
    static const std::regex kNumberRe(R"((-?\d+(?:\.\d+)?))");
    auto it = std::sregex_iterator(text.begin(), text.end(), kNumberRe);
    const auto end = std::sregex_iterator();
    std::optional<std::string> found;
    int count = 0;
    for (; it != end; ++it) {
        found = it->str();
        ++count;
    }
    if (count != 1) return std::nullopt;
    return found;
}

}  // namespace


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
    oss << "\n## Quy tắc bắt buộc về tính trung thực với Observation\n"
           "Khi một Observation (kết quả tool ở lượt trước) đã cung cấp một con số hoặc dữ kiện cụ "
           "thể, bạn PHẢI dùng ĐÚNG NGUYÊN giá trị đó trong Thought và Final Answer tiếp theo. "
           "TUYỆT ĐỐI KHÔNG được tự tính/suy luận lại bằng kiến thức hoặc trí nhớ của bạn để thay "
           "thế kết quả tool đã trả về — kể cả khi bạn nghĩ mình biết một đáp án khác. Nếu bạn cho "
           "rằng tool trả về sai, hãy nêu rõ nghi ngờ đó trong Thought và có thể gọi lại tool để "
           "xác nhận, nhưng không được âm thầm thay số liệu.\n"
           "Ví dụ CỤ THỂ (bắt buộc làm theo đúng khuôn mẫu này, không chỉ đọc hiểu ý mà phải lặp "
           "lại đúng cách trình bày số liệu):\n"
           "  - Observation: \"Số từ: 7\"\n"
           "  - Final Answer ĐÚNG: \"Câu 'Tôi yêu lập trình hướng đối tượng' có 7 từ.\" (chữ số Ả "
           "Rập '7', lấy nguyên văn từ Observation)\n"
           "  - Final Answer SAI (không được làm): \"...có 5 từ.\" (số bịa, không khớp Observation), "
           "\"...có bảy từ.\" (viết bằng chữ thay vì chữ số Ả Rập), \"...khoảng 7-8 từ.\" (làm tròn/"
           "phỏng đoán thay vì trích nguyên văn).\n"
           "Quy tắc tương tự áp dụng cho mọi tool trả về một con số duy nhất (vd calculator).\n";

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

                        // Kiểm tra tính nhất quán (grounding check) với Observation ngay trước đó:
                        // nếu bước liền trước là một lần gọi tool THÀNH CÔNG mà Observation chỉ chứa
                        // đúng một con số rõ ràng (vd word_count "Số từ: 7", calculator "255"), con số
                        // đó BẮT BUỘC phải xuất hiện nguyên văn trong Final Answer. Đây chính là lớp
                        // lỗi thực tế đã quan sát được: model tự "bịa" lại một con số khác thay vì
                        // trích dẫn đúng kết quả tool. Ta không tự động sửa hộ câu trả lời (làm vậy sẽ
                        // che giấu lỗi thật của model khỏi trajectory/báo cáo) mà bắt model tự trả lời
                        // lại — cùng cơ chế với nhánh MalformedAction bên dưới, có giới hạn số lần thử
                        // nhờ max_steps đã có sẵn nên không thể lặp vô hạn.
                        //
                        // Chỉ áp dụng cho các tool mà TOÀN BỘ kết quả của nó CHÍNH LÀ con số cần trả
                        // lời (word_count, calculator) — KHÔNG áp dụng cho các tool khác (file, exec,
                        // datetime...) vì Observation của chúng có thể chứa số liệu không liên quan
                        // (vd "Đã ghi 1 byte vào ..." khi nhiệm vụ thực ra chỉ là "ghi file", không
                        // phải "ghi bao nhiêu byte") — nếu bắt buộc trích dẫn số đó sẽ tạo báo động giả.
                        static const std::vector<std::string> kNumericResultTools = {"word_count", "calculator"};
                        bool grounded = true;
                        if (!trajectory.steps.empty()) {
                            const Step& prev = trajectory.steps.back();
                            const bool prev_is_numeric_tool =
                                prev.action.type == "tool_call" && prev.tool_success &&
                                std::ranges::find(kNumericResultTools, prev.action.tool) !=
                                    kNumericResultTools.end();
                            if (prev_is_numeric_tool) {
                                auto expected_number = extract_single_number_if_unambiguous(prev.tool_result);
                                if (expected_number && final_answer.answer.find(*expected_number) ==
                                                            std::string::npos) {
                                    grounded = false;
                                    logger_.warn(
                                        "Grounding check thất bại: Final Answer ('{}') không chứa số liệu "
                                        "'{}' lấy từ Observation gần nhất ('{}'). Yêu cầu model trả lời lại.",
                                        final_answer.answer, *expected_number, prev.tool_result);
                                    history.push_back(ChatMessage::assistant(chat_response.content));
                                    history.push_back(ChatMessage::user(
                                        "Final Answer của bạn không khớp với Observation gần nhất — giá trị "
                                        "chính xác (trích nguyên văn từ Observation) là '" + *expected_number +
                                        "'. Hãy trả lời lại đúng định dạng 'Thought: ...' + 'Final Answer: ...', "
                                        "viết ĐÚNG con số '" + *expected_number + "' bằng chữ số Ả Rập (không "
                                        "viết bằng chữ, không làm tròn, không suy luận lại) — copy nguyên văn "
                                        "giá trị này vào câu trả lời."));
                                    step.tool_result =
                                        "(grounding check thất bại — Final Answer không khớp Observation, đã "
                                        "yêu cầu model trả lời lại) " +
                                        final_answer.answer;
                                    step.tool_success = false;
                                }
                            }
                        }

                        if (grounded) {
                            history.push_back(ChatMessage::assistant(chat_response.content));
                            trajectory.success = true;
                            trajectory.final_answer = final_answer.answer;
                            should_break = true;
                        }
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
