/**
 * @file loop_detector.cpp
 * @see loop_detector.h
 */
#include "loop_detector.h"

namespace agent {

LoopDetector::LoopDetector(Config config) : config_(config) {}

void LoopDetector::reset() { history_.clear(); }

int LoopDetector::count_trailing_repeats() const {
    if (history_.empty()) return 0;
    const std::string& last = history_.back();
    int count = 0;
    for (auto it = history_.rbegin(); it != history_.rend() && *it == last; ++it) ++count;
    return count;
}

int LoopDetector::count_trailing_pingpong_cycles() const {
    // Cần tối thiểu 4 phần tử (A,B,A,B) để có 1 chu kỳ ping-pong hoàn chỉnh.
    if (history_.size() < 4) return 0;
    const std::string& a = history_[history_.size() - 1];  // vị trí lẻ (mới nhất)
    const std::string& b = history_[history_.size() - 2];  // vị trí chẵn
    if (a == b) return 0;  // không phải ping-pong nếu 2 phần tử cuối trùng nhau (đó là generic repeat)

    int cycles = 0;
    std::size_t i = history_.size();
    while (i >= 2) {
        const std::string& x = history_[i - 1];
        const std::string& y = history_[i - 2];
        bool matches_cycle = (x == a && y == b) || (x == b && y == a);
        if (!matches_cycle) break;
        ++cycles;
        i -= 2;
    }
    return cycles;
}

LoopDetectionResult LoopDetector::record_and_check(const std::string& action_signature) {
    history_.push_back(action_signature);
    while (history_.size() > config_.history_window) history_.pop_front();

    LoopDetectionResult result;

    int repeats = count_trailing_repeats();
    if (repeats >= config_.repeat_critical_threshold) {
        result.severity = LoopSeverity::kCritical;
        result.kind = LoopKind::kGenericRepeat;
        result.message = "Phát hiện lặp lại y hệt " + std::to_string(repeats) +
                          " lần liên tiếp cùng một hành động — dừng agent để tránh lãng phí tài nguyên.";
        return result;
    }
    if (repeats >= config_.repeat_warning_threshold) {
        result.severity = LoopSeverity::kWarning;
        result.kind = LoopKind::kGenericRepeat;
        result.message = "Cảnh báo: hành động lặp lại " + std::to_string(repeats) + " lần liên tiếp.";
        return result;
    }

    int cycles = count_trailing_pingpong_cycles();
    if (cycles >= config_.pingpong_critical_cycles) {
        result.severity = LoopSeverity::kCritical;
        result.kind = LoopKind::kPingPong;
        result.message = "Phát hiện ping-pong giữa 2 hành động, " + std::to_string(cycles) +
                          " chu kỳ — dừng agent để tránh lãng phí tài nguyên.";
        return result;
    }
    if (cycles >= config_.pingpong_warning_cycles) {
        result.severity = LoopSeverity::kWarning;
        result.kind = LoopKind::kPingPong;
        result.message = "Cảnh báo: phát hiện ping-pong giữa 2 hành động, " + std::to_string(cycles) +
                          " chu kỳ.";
        return result;
    }

    return result;  // kNone
}

}  // namespace agent
