#include "functional_evaluator.h"

#include "../util/subprocess.h"

namespace agent {

EvalResult FunctionalEvaluator::evaluate(const Trajectory& trajectory, const Task& task) {
    if (!task.eval_script || task.eval_script->empty()) {
        return EvalResult::fail("Task không khai báo eval_script — không thể chấm bằng FunctionalEvaluator");
    }
    if (!trajectory.success) {
        return EvalResult::fail("Trajectory kết thúc không thành công, bỏ qua eval_script: " +
                                 trajectory.failure_reason, 0.0);
    }

    auto pr = util::run_shell_command(*task.eval_script, working_directory_, timeout_seconds_);
    // Với quy ước script kiểu "A && B && ... && echo PASS" (mục 7.2), exit_code
    // == 0 chỉ đạt được khi TOÀN BỘ các bước trong chuỗi && đều thành công —
    // đây là tín hiệu pass/fail chính xác và không cần parse chuỗi "PASS" thủ công.
    bool passed = !pr.timed_out && pr.exit_code == 0;
    std::string reason = "eval_script exit_code=" + std::to_string(pr.exit_code) +
                          (pr.timed_out ? " (timeout)" : "") + ", output: " +
                          (pr.output.empty() ? "(rỗng)" : pr.output.substr(0, 300));
    return EvalResult{passed, passed ? 1.0 : 0.0, reason};
}

}  // namespace agent
