#pragma once
/**
 * @file loop_detector.h
 * @brief Phát hiện Agent bị "kẹt vòng lặp" (loop) — mục 3.5 đề bài.
 *
 * Phát hiện tối thiểu 2 loại loop:
 *   1) Generic repeat: cùng một "chữ ký hành động" (tool + tham số) xuất hiện
 *      liên tiếp N lần — dấu hiệu agent gọi lại y hệt thao tác vừa thất bại.
 *   2) Ping-pong: hai chữ ký hành động khác nhau A, B lặp lại theo chu kỳ
 *      A,B,A,B,... — dấu hiệu agent "phân vân" giữa 2 lựa chọn không hội tụ.
 *
 * Ngưỡng (threshold) tách biệt WARNING (chỉ log cảnh báo, agent tiếp tục
 * chạy) và CRITICAL (AgentLoop dừng hẳn, coi như thất bại) — có thể cấu hình
 * qua Config, không hardcode.
 */
#include <cstddef>
#include <deque>
#include <string>

namespace agent {

enum class LoopSeverity { kNone, kWarning, kCritical };
enum class LoopKind { kNone, kGenericRepeat, kPingPong };

struct LoopDetectionResult {
    LoopSeverity severity = LoopSeverity::kNone;
    LoopKind kind = LoopKind::kNone;
    std::string message;

    [[nodiscard]] bool should_log() const noexcept { return severity != LoopSeverity::kNone; }
    [[nodiscard]] bool should_abort() const noexcept { return severity == LoopSeverity::kCritical; }
};

class LoopDetector {
public:
    struct Config {
        int repeat_warning_threshold = 2;    ///< số lần lặp liên tiếp để cảnh báo
        int repeat_critical_threshold = 4;   ///< số lần lặp liên tiếp để dừng agent
        int pingpong_warning_cycles = 2;      ///< số chu kỳ A,B để cảnh báo
        int pingpong_critical_cycles = 4;     ///< số chu kỳ A,B để dừng agent
        std::size_t history_window = 12;      ///< số hành động gần nhất được lưu để phân tích
    };

    explicit LoopDetector(Config config);
    LoopDetector() : LoopDetector(Config{}) {}

    /// Ghi nhận một hành động mới (dạng chữ ký chuẩn hoá: "tool_name|args_json")
    /// và trả về kết quả phát hiện loop tính đến thời điểm này.
    [[nodiscard]] LoopDetectionResult record_and_check(const std::string& action_signature);

    /// Reset lịch sử — dùng khi bắt đầu một lượt AgentLoop::run() mới.
    void reset();

    [[nodiscard]] const Config& config() const noexcept { return config_; }

private:
    [[nodiscard]] int count_trailing_repeats() const;
    [[nodiscard]] int count_trailing_pingpong_cycles() const;

    Config config_;
    std::deque<std::string> history_;
};

}  // namespace agent
