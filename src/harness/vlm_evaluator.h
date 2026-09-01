#pragma once
/**
 * @file vlm_evaluator.h
 * @brief Chấm điểm bằng "LLM-as-judge" — dùng chính LLMClient (multimodal, vd
 * qwen3-vl/gemma3 qua Ollama) để đọc task + Trajectory (và ảnh bằng chứng nếu
 * có, Task::image_path) rồi tự đưa ra phán quyết pass/fail kèm giải thích.
 *
 * Đây là lớp Evaluator thứ 3 bắt buộc theo mục 4.1 & bảng Design Pattern
 * (Strategy). Thiết kế đã sẵn sàng cho hướng mở rộng "GUI Agent" (mục 10.1,
 * +8đ): khi tool `capture_screenshot` được thêm vào, chỉ cần gán đường dẫn
 * ảnh chụp màn hình vào Task::image_path — VLMEvaluator sẽ tự động đính kèm
 * ảnh vào lời chấm mà không cần sửa lớp này.
 */
#include <string>

#include "../client/llm_client.h"
#include "evaluator.h"

namespace agent {

class VLMEvaluator : public Evaluator {
public:
    /// @param judge_client  LLMClient dùng để chấm điểm (nên trỏ tới model hỗ
    ///        trợ vision nếu Task có image_path; vẫn hoạt động với model
    ///        text-only cho các Task không kèm ảnh).
    explicit VLMEvaluator(LLMClient& judge_client, std::string judge_model = "gemma3")
        : judge_client_(judge_client), judge_model_(std::move(judge_model)) {}

    [[nodiscard]] EvalResult evaluate(const Trajectory& trajectory, const Task& task) override;
    [[nodiscard]] std::string name() const override { return "vlm"; }

private:
    LLMClient& judge_client_;
    std::string judge_model_;

    [[nodiscard]] static std::string build_judge_prompt(const Trajectory& trajectory, const Task& task);
    [[nodiscard]] static EvalResult parse_judge_response(const std::string& raw_content);
};

}  // namespace agent
