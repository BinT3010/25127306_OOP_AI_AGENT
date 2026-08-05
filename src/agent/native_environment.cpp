#include "native_environment.h"

namespace agent {

NativeEnvironment::NativeEnvironment(std::filesystem::path root) : root_(std::move(root)) {}

std::filesystem::path NativeEnvironment::resolve_path(const std::string& raw_path) const {
    std::filesystem::path p(raw_path);
    if (p.is_absolute()) return p.lexically_normal();
    return (root_ / p).lexically_normal();
}

bool NativeEnvironment::is_command_allowed(const std::string& /*command*/) const {
    return true;  // NativeEnvironment tin tưởng người dùng chạy thủ công, không chặn.
}

}  // namespace agent
