#pragma once
/**
 * @file logger.h
 * @brief Logger tối giản, thread-safe, dùng chung cho toàn hệ thống.
 *
 * Không dùng Singleton cổ điển (global mutable state khó test) mà expose
 * một instance thường, được truyền qua tham chiếu — Harness/AgentLoop có thể
 * tiêm (inject) một Logger riêng cho mỗi lần chạy test, tránh side-effect
 * toàn cục giữa các test case.
 */
#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

namespace agent::util {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

class Logger {
public:
    explicit Logger(std::string component, LogLevel min_level = LogLevel::kInfo)
        : component_(std::move(component)), min_level_(min_level) {}

    void set_min_level(LogLevel lvl) { min_level_ = lvl; }
    void set_silent(bool silent) { silent_ = silent; }

    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::kDebug, std::format(fmt, std::forward<Args>(args)...));
    }
    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::kInfo, std::format(fmt, std::forward<Args>(args)...));
    }
    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::kWarn, std::format(fmt, std::forward<Args>(args)...));
    }
    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::kError, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    static std::string_view level_tag(LogLevel lvl) {
        switch (lvl) {
            case LogLevel::kDebug: return "DEBUG";
            case LogLevel::kInfo:  return "INFO ";
            case LogLevel::kWarn:  return "WARN ";
            case LogLevel::kError: return "ERROR";
        }
        return "?????";
    }

    void log(LogLevel lvl, const std::string& message) {
        if (silent_ || lvl < min_level_) return;
        static std::mutex mtx;
        std::lock_guard _(mtx);   // C++26: placeholder variable '_' (P2169R4)
        auto now = std::chrono::system_clock::now();
        std::cerr << std::format("[{:%H:%M:%S}] [{}] [{}] {}\n",
                                  std::chrono::floor<std::chrono::seconds>(now),
                                  level_tag(lvl), component_, message);
    }

    std::string component_;
    LogLevel min_level_;
    bool silent_ = false;
};

}  // namespace agent::util
