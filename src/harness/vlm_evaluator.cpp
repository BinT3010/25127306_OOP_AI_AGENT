/**
 * @file vlm_evaluator.cpp
 * @see vlm_evaluator.h
 */
#include "vlm_evaluator.h"

#include <nlohmann/json.hpp>
#include <sstream>

#include "../util/base64.h"

namespace agent {

std::string VLMEvaluator::build_judge_prompt(const Trajectory& trajectory, const Task& task) {
    std::ostringstream oss;
    oss << "Bạn là giám khảo chấm điểm một AI agent. Dựa trên YÊU CẦU nhiệm vụ và "
           "DIỄN BIẾN thực tế agent đã thực hiện bên dưới (kèm ảnh bằng chứng nếu có), "
           "hãy phán quyết agent có hoàn thành đúng yêu cầu hay không.\n\n";
    oss << "YÊU CẦU NHIỆM VỤ:\n" << task.instruction << "\n\n";
    oss << "CÂU TRẢ LỜI CUỐI CÙNG CỦA AGENT:\n"
        << (trajectory.final_answer.empty() ? "(không có)" : trajectory.final_answer) << "\n\n";
    oss << "CÁC BƯỚC AGENT ĐÃ THỰC HIỆN (" << trajectory.steps.size() << " bước):\n";
    for (const auto& s : trajectory.steps) {
        oss << "- Bước " << s.step_id << ": ";
        if (s.action.type == "tool_call") {
            oss << "gọi tool '" << s.action.tool << "' với " << s.action.args << " -> kết quả: " << s.tool_result;
        } else {
            oss << s.action.type;
        }
        oss << "\n";
    }
    oss << "\nHãy trả lời DUY NHẤT một JSON object theo đúng schema sau, không thêm "
           "văn bản nào khác:\n"
           R"({"passed": <true|false>, "score": <số thực 0.0-1.0>, "reason": "<giải thích ngắn gọn>"})";
    return oss.str();
}

EvalResult VLMEvaluator::parse_judge_response(const std::string& raw_content) {
    // Model đôi khi bọc JSON trong ```json ... ``` dù đã yêu cầu JSON thuần —
    // cắt bỏ phần bọc đó trước khi parse để tăng độ bền (robustness).
    std::string content = raw_content;
    if (auto pos = content.find('{'); pos != std::string::npos) {
        content = content.substr(pos);
    }
    if (auto pos = content.rfind('}'); pos != std::string::npos) {
        content = content.substr(0, pos + 1);
    }

    try {
        auto j = nlohmann::json::parse(content);
        bool passed = j.value("passed", false);
        double score = j.value("score", passed ? 1.0 : 0.0);
        std::string reason = j.value("reason", "(giám khảo không cung cấp giải thích)");
        return EvalResult{passed, score, reason};
    } catch (const nlohmann::json::exception&) {
        return EvalResult::fail("Không parse được phản hồi giám khảo (VLM) thành JSON: " +
                                 raw_content.substr(0, 200));
    }
}

EvalResult VLMEvaluator::evaluate(const Trajectory& trajectory, const Task& task) {
    if (!trajectory.success) {
        return EvalResult::fail("Trajectory kết thúc không thành công: " + trajectory.failure_reason, 0.0);
    }

    std::string prompt = build_judge_prompt(trajectory, task);
    ChatMessage msg = ChatMessage::user(prompt);
    if (task.image_path && !task.image_path->empty()) {
        std::string b64 = util::base64_encode_file(*task.image_path);
        if (!b64.empty()) msg = ChatMessage::user_with_images(prompt, {b64});
    }

    ChatOptions opts;
    opts.model = judge_model_;
    opts.temperature = 0.0;  // giám khảo cần nhất quán, không cần sáng tạo
    opts.json_mode = true;

    auto result = judge_client_.chat({ChatMessage::system("Bạn là một giám khảo chấm điểm nghiêm túc, "
                                                            "khách quan, luôn trả lời đúng định dạng JSON được yêu cầu."),
                                       msg},
                                      opts);
    if (!result.has_value()) {
        return EvalResult::fail("Không gọi được LLM giám khảo: " + result.error());
    }
    return parse_judge_response(result->content);
}

}  // namespace agent
