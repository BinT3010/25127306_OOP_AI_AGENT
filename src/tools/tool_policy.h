#pragma once
/**
 * @file tool_policy.h
 * @brief Chính sách allow/deny tool theo tên (mục 3.2 đề bài) + Decorator pattern.
 *
 * ToolPolicy: đối tượng dữ liệu thuần, quyết định một tên tool có được phép
 * gọi hay không.
 * PolicyEnforcedTool: DECORATOR PATTERN — bọc một Tool bất kỳ, chặn execute()
 * nếu policy từ chối, nếu không thì "trong suốt" uỷ quyền (delegate) sang tool
 * gốc. AgentLoop/ToolRegistry gọi PolicyEnforcedTool giống hệt gọi Tool thường
 * — không cần biết có lớp bọc hay không (Liskov Substitution).
 */
#include <memory>
#include <unordered_set>
#include <vector>

#include "tool.h"

namespace agent {

class ToolPolicy {
public:
    enum class Mode { kAllowAll, kAllowList, kDenyList };

    static ToolPolicy allow_all() { return ToolPolicy(Mode::kAllowAll, {}); }
    static ToolPolicy allow_only(std::vector<std::string> names) {
        return ToolPolicy(Mode::kAllowList, std::move(names));
    }
    static ToolPolicy deny(std::vector<std::string> names) {
        return ToolPolicy(Mode::kDenyList, std::move(names));
    }

    [[nodiscard]] bool is_allowed(const std::string& tool_name) const {
        switch (mode_) {
            case Mode::kAllowAll: return true;
            case Mode::kAllowList: return set_.contains(tool_name);
            case Mode::kDenyList: return !set_.contains(tool_name);
        }
        return false;
    }

    [[nodiscard]] Mode mode() const noexcept { return mode_; }

private:
    ToolPolicy(Mode mode, std::vector<std::string> names) : mode_(mode) {
        for (auto& n : names) set_.insert(std::move(n));
    }
    Mode mode_;
    std::unordered_set<std::string> set_;
};

/// DECORATOR: bọc Tool gốc, chặn theo ToolPolicy trước khi uỷ quyền thực thi.
class PolicyEnforcedTool : public Tool {
public:
    PolicyEnforcedTool(std::unique_ptr<Tool> inner, const ToolPolicy& policy)
        : inner_(std::move(inner)), policy_(policy) {}

    [[nodiscard]] std::string name() const override { return inner_->name(); }
    [[nodiscard]] std::string description() const override { return inner_->description(); }
    [[nodiscard]] std::string parameters_schema() const override { return inner_->parameters_schema(); }
    [[nodiscard]] bool is_mutating() const override { return inner_->is_mutating(); }

    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override {
        if (!policy_.is_allowed(inner_->name())) {
            return ToolResult::fail("Tool '" + inner_->name() +
                                     "' bị từ chối bởi ToolPolicy hiện hành (allow/deny list)");
        }
        return inner_->execute(args_json, env);
    }

private:
    std::unique_ptr<Tool> inner_;
    const ToolPolicy& policy_;
};

}  // namespace agent
