#pragma once
/**
 * @file sandbox_environment.h
 * @brief Môi trường "cách ly" — giới hạn tool trong một thư mục gốc cố định
 * và chặn danh sách lệnh nguy hiểm. HarnessRunner dùng SandboxEnvironment khi
 * chạy benchmark tự động (agent do LLM điều khiển, không nên tin tưởng tuyệt
 * đối) để tránh việc agent (do model "ảo giác" hoặc bị prompt injection từ
 * kết quả web_search) vô tình/cố ý thao túng ra ngoài phạm vi cho phép.
 */
#include <unordered_set>

#include "environment.h"

namespace agent {

class SandboxEnvironment : public Environment {
public:
    explicit SandboxEnvironment(std::filesystem::path sandbox_root);

    [[nodiscard]] std::filesystem::path working_directory() const override { return sandbox_root_; }
    [[nodiscard]] std::filesystem::path resolve_path(const std::string& raw_path) const override;
    [[nodiscard]] bool is_command_allowed(const std::string& command) const override;
    void setup() override;
    void teardown() override;
    [[nodiscard]] std::string kind() const override { return "sandbox"; }

    /// Cho phép test / người dùng nâng cao thêm từ khoá bị chặn tuỳ biến.
    void add_denied_keyword(std::string keyword);

private:
    std::filesystem::path sandbox_root_;
    std::vector<std::string> denied_keywords_{"rm -rf /", "mkfs", ":(){:|:&};:", "dd if=/dev/zero",
                                                "> /dev/sda", "shutdown", "reboot", "sudo "};
};

}  // namespace agent
