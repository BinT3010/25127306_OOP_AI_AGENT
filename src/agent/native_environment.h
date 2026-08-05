#pragma once
/**
 * @file native_environment.h
 * @brief Môi trường "tự nhiên" — dùng thư mục thật của hệ thống, không giới hạn
 * nhiều. Phù hợp khi lập trình viên tự chạy agent thủ công trên máy mình.
 */
#include "environment.h"

namespace agent {

class NativeEnvironment : public Environment {
public:
    explicit NativeEnvironment(std::filesystem::path root = std::filesystem::current_path());

    [[nodiscard]] std::filesystem::path working_directory() const override { return root_; }
    [[nodiscard]] std::filesystem::path resolve_path(const std::string& raw_path) const override;
    [[nodiscard]] bool is_command_allowed(const std::string& command) const override;
    void setup() override {}
    void teardown() override {}
    [[nodiscard]] std::string kind() const override { return "native"; }

private:
    std::filesystem::path root_;
};

}  // namespace agent
