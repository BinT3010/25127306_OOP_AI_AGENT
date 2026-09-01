#pragma once
/**
 * @file evaluator.h
 * @brief STRATEGY PATTERN: interface chung cho mọi cách chấm điểm Trajectory.
 *
 * Đúng nguyên tắc tách lớp (mục 4.4): Evaluator chỉ nhìn vào Trajectory (kết
 * quả cuối) và Task (tiêu chí mong đợi) — KHÔNG hề biết AgentLoop/Environment
 * đã thực thi thế nào để ra được Trajectory đó. Nhờ vậy HarnessRunner có thể
 * hoán đổi Evaluator tự do (KeywordEvaluator/FunctionalEvaluator/VLMEvaluator)
 * mà không đụng tới agent logic — đây chính là bản chất của Strategy Pattern.
 */
#include <string>

#include "task.h"
#include "trajectory.h"

namespace agent {

struct EvalResult {
    bool passed = false;
    double score = 0.0;   ///< [0.0, 1.0] — cho phép chấm điểm từng phần (partial credit)
    std::string reason;   ///< giải thích ngắn gọn, phục vụ debug & báo cáo

    static EvalResult pass(std::string reason = "", double score = 1.0) {
        return EvalResult{true, score, std::move(reason)};
    }
    static EvalResult fail(std::string reason, double score = 0.0) {
        return EvalResult{false, score, std::move(reason)};
    }
};

class Evaluator {
public:
    virtual ~Evaluator() = default;

    /// Chấm điểm một Trajectory đã hoàn thành dựa trên tiêu chí của `task`.
    [[nodiscard]] virtual EvalResult evaluate(const Trajectory& trajectory, const Task& task) = 0;

    /// Tên định danh (dùng để log / match với Task::eval_type).
    [[nodiscard]] virtual std::string name() const = 0;
};

}  // namespace agent
