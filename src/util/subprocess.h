#pragma once
/**
 * @file subprocess.h
 * @brief Helper POSIX (fork/exec/pipe/poll) để chạy lệnh shell có giới hạn
 * thời gian (timeout) và capture output — dùng chung bởi ExecTool và
 * PythonExecTool. Không dùng std::system() vì hàm đó không cho phép áp
 * timeout hay capture stdout mà không phải dùng thêm redirect file tạm.
 */
#include <filesystem>
#include <string>

namespace agent::util {

struct ProcessResult {
    int exit_code = -1;
    std::string output;   ///< stdout + stderr gộp chung (đủ dùng làm "Observation" cho LLM)
    bool timed_out = false;
};

[[nodiscard]] ProcessResult run_shell_command(const std::string& command,
                                               const std::filesystem::path& cwd,
                                               int timeout_seconds);

}  // namespace agent::util
