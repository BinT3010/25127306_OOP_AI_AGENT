#include "subprocess.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>

namespace agent::util {

ProcessResult run_shell_command(const std::string& command, const std::filesystem::path& cwd,
                                 int timeout_seconds) {
    ProcessResult result;
    int out_pipe[2];
    if (pipe(out_pipe) != 0) {
        result.output = "Không thể tạo pipe cho tiến trình con";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]);
        close(out_pipe[1]);
        result.output = "fork() thất bại";
        return result;
    }

    if (pid == 0) {
        // ---- Tiến trình con ----
        setpgid(0, 0);  // process group riêng -> có thể kill cả nhóm (con của con) khi timeout
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(out_pipe[0]);
        close(out_pipe[1]);
        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0) _exit(127);
        }
        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
        _exit(127);  // chỉ chạy tới đây nếu execl thất bại
    }

    // ---- Tiến trình cha ----
    close(out_pipe[1]);
    std::string collected;
    std::array<char, 4096> buf{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    bool process_exited = false;
    int status = 0;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        int remaining_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        pollfd pfd{out_pipe[0], POLLIN, 0};
        int pret = poll(&pfd, 1, std::min(remaining_ms, 200));
        if (pret > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(out_pipe[0], buf.data(), buf.size());
            if (n > 0) collected.append(buf.data(), static_cast<std::size_t>(n));
        }
        pid_t wp = waitpid(pid, &status, WNOHANG);
        if (wp == pid) {
            process_exited = true;
            break;
        }
    }

    if (!process_exited) {
        pid_t wp = waitpid(pid, &status, WNOHANG);
        if (wp != pid) {
            killpg(pid, SIGKILL);
            waitpid(pid, &status, 0);
            result.timed_out = true;
        }
    }

    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);
    ssize_t n;
    while ((n = read(out_pipe[0], buf.data(), buf.size())) > 0)
        collected.append(buf.data(), static_cast<std::size_t>(n));
    close(out_pipe[0]);

    result.output = collected;
    if (result.timed_out) {
        result.output += "\n[Tiến trình bị huỷ do vượt timeout " + std::to_string(timeout_seconds) + "s]";
        result.exit_code = -1;
    } else {
        result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return result;
}

}  // namespace agent::util
