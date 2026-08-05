#pragma once
/**
 * @file environment.h
 * @brief Trừu tượng hoá "nơi" mà Tool được thực thi (working directory, quyền hạn).
 *
 * Tách Environment khỏi Tool giúp cùng một Tool (vd: FileTool, ExecTool) có thể
 * chạy trong NativeEnvironment (không giới hạn, dùng khi phát triển) hoặc
 * SandboxEnvironment (giới hạn trong một thư mục tạm + chặn lệnh nguy hiểm,
 * dùng khi HarnessRunner chạy benchmark tự động) mà không cần sửa code Tool.
 */
#include <filesystem>
#include <string>
#include <vector>

#include "../util/exceptions.h"

namespace agent {

class Environment {
public:
    virtual ~Environment() = default;

    /// Thư mục làm việc hiện hành mà tool nên xem là "gốc" tương đối.
    [[nodiscard]] virtual std::filesystem::path working_directory() const = 0;

    /// Quy đổi một đường dẫn (tương đối hoặc tuyệt đối) do LLM cung cấp thành
    /// đường dẫn tuyệt đối hợp lệ. Ném EnvironmentViolationException nếu
    /// đường dẫn cố tình thoát ra ngoài phạm vi cho phép (path traversal).
    [[nodiscard]] virtual std::filesystem::path resolve_path(const std::string& raw_path) const = 0;

    /// Kiểm tra một dòng lệnh shell có được phép chạy hay không (ExecTool dùng).
    [[nodiscard]] virtual bool is_command_allowed(const std::string& command) const = 0;

    /// Chuẩn bị môi trường trước khi AgentLoop chạy (vd: tạo thư mục tạm).
    virtual void setup() = 0;

    /// Dọn dẹp môi trường sau khi AgentLoop chạy xong (vd: xoá thư mục tạm).
    virtual void teardown() = 0;

    [[nodiscard]] virtual std::string kind() const = 0;
};

}  // namespace agent
