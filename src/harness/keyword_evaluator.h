#pragma once
/**
 * @file keyword_evaluator.h
 * @brief Chấm điểm dựa trên việc Final Answer có chứa các từ khoá mong đợi
 * hay không (Task::expected_keywords). Phù hợp cho task có đáp án dạng văn
 * bản/số mà không cần kiểm tra hiệu ứng phụ trên hệ thống file.
 */
#include "evaluator.h"

namespace agent {

class KeywordEvaluator : public Evaluator {
public:
    /// @param require_all  true: phải khớp TẤT CẢ từ khoá mới pass (mặc định,
    ///        nghiêm ngặt hơn); false: chỉ cần khớp ít nhất 1 từ khoá.
    explicit KeywordEvaluator(bool require_all = true) : require_all_(require_all) {}

    [[nodiscard]] EvalResult evaluate(const Trajectory& trajectory, const Task& task) override;
    [[nodiscard]] std::string name() const override { return "keyword"; }

private:
    bool require_all_;
};

}  // namespace agent
