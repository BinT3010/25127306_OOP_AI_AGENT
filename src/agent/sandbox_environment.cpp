#include "sandbox_environment.h"

#include <algorithm>

#include "../util/exceptions.h"

namespace agent {

SandboxEnvironment::SandboxEnvironment(std::filesystem::path sandbox_root)
    : sandbox_root_(std::move(sandbox_root)) {}

void SandboxEnvironment::setup() { std::filesystem::create_directories(sandbox_root_); }

void SandboxEnvironment::teardown() {
    std::error_code ec;
    std::filesystem::remove_all(sandbox_root_, ec);  // best-effort, không ném nếu lỗi khi dọn dẹp
}

std::filesystem::path SandboxEnvironment::resolve_path(const std::string& raw_path) const {
    std::filesystem::path candidate =
        std::filesystem::path(raw_path).is_absolute()
            ? std::filesystem::path(raw_path)
            : sandbox_root_ / raw_path;
    std::filesystem::path normalized = candidate.lexically_normal();

    const std::string root_str = sandbox_root_.lexically_normal().string();
    const std::string norm_str = normalized.string();
    if (norm_str.compare(0, root_str.size(), root_str) != 0) {
        throw EnvironmentViolationException("Đường dẫn '" + raw_path +
                                             "' nằm ngoài phạm vi sandbox cho phép (" + root_str + ")");
    }
    return normalized;
}

bool SandboxEnvironment::is_command_allowed(const std::string& command) const {
    return std::ranges::none_of(denied_keywords_, [&](const std::string& kw) {
        return command.find(kw) != std::string::npos;
    });
}

void SandboxEnvironment::add_denied_keyword(std::string keyword) {
    denied_keywords_.push_back(std::move(keyword));
}

}  // namespace agent
