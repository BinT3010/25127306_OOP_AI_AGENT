#include "keyword_evaluator.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace agent {

namespace {
std::string to_lower(const std::string& s) {
    std::string out = s;
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return (c < 128) ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    return out;
}
}  // namespace

EvalResult KeywordEvaluator::evaluate(const Trajectory& trajectory, const Task& task) {
    if (!task.expected_keywords || task.expected_keywords->empty()) {
        return EvalResult::fail("Task không khai báo expected_keywords — không thể chấm bằng KeywordEvaluator");
    }
    if (!trajectory.success) {
        return EvalResult::fail("Trajectory kết thúc không thành công: " + trajectory.failure_reason, 0.0);
    }

    std::string haystack = to_lower(trajectory.final_answer);
    int matched = 0;
    std::vector<std::string> missing;
    for (const auto& kw : *task.expected_keywords) {
        if (haystack.find(to_lower(kw)) != std::string::npos) {
            ++matched;
        } else {
            missing.push_back(kw);
        }
    }

    int total = static_cast<int>(task.expected_keywords->size());
    double score = total == 0 ? 0.0 : static_cast<double>(matched) / total;
    bool passed = require_all_ ? (matched == total) : (matched > 0);

    std::ostringstream reason;
    reason << "Khớp " << matched << "/" << total << " từ khoá mong đợi.";
    if (!missing.empty()) {
        reason << " Thiếu: ";
        for (std::size_t i = 0; i < missing.size(); ++i) {
            reason << (i > 0 ? ", " : "") << missing[i];
        }
    }
    return EvalResult{passed, score, reason.str()};
}

}  // namespace agent
