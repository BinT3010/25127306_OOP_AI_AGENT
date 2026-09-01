#pragma once
/**
 * @file registry.h
 * @brief Template class tổng quát dùng để đăng ký & khởi tạo đối tượng theo tên.
 *
 * Đây là minh chứng cho yêu cầu "Template class" trong bảng kĩ thuật C++17+.
 * ToolRegistry (src/tools/tool_registry.h) build trên nền Registry<Tool> này,
 * tránh lặp code Factory/Registry pattern.
 *
 * @tparam T Kiểu cơ sở (thường là một abstract class) mà Registry quản lý.
 */
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace agent::util {

template <typename T>
class Registry {
public:
    using Factory = std::function<std::unique_ptr<T>()>;

    /// Đăng ký một factory function tạo đối tượng T mới theo tên (Factory Pattern).
    void register_type(const std::string& name, Factory factory) {
        factories_[name] = std::move(factory);
    }

    /// Kiểm tra tên đã được đăng ký factory hay chưa.
    [[nodiscard]] bool has(const std::string& name) const {
        return factories_.contains(name);
    }

    /// Khởi tạo một đối tượng T mới từ factory đã đăng ký. Trả nullptr nếu không có.
    [[nodiscard]] std::unique_ptr<T> create(const std::string& name) const {
        auto it = factories_.find(name);
        if (it == factories_.end()) return nullptr;
        return (it->second)();
    }

    /// Liệt kê tất cả tên đã đăng ký (đã sắp xếp để output ổn định, dễ test/log).
    [[nodiscard]] std::vector<std::string> names() const {
        std::vector<std::string> result;
        result.reserve(factories_.size());
        for (const auto& [key, _] : factories_) result.push_back(key);
        std::ranges::sort(result);
        return result;
    }

    [[nodiscard]] std::size_t size() const noexcept { return factories_.size(); }

private:
    std::unordered_map<std::string, Factory> factories_;
};

}  // namespace agent::util
