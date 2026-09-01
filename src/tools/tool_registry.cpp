/**
 * @file tool_registry.cpp
 * @see tool_registry.h
 */
#include "tool_registry.h"

#include <sstream>

#include "../util/exceptions.h"

namespace agent {

void ToolRegistry::register_factory(const std::string& name, ToolFactory factory) {
    factory_registry_.register_type(name, std::move(factory));
}

void ToolRegistry::instantiate(const std::string& name) {
    auto tool = factory_registry_.create(name);
    if (!tool) throw ToolNotFoundException(name);
    register_tool(std::move(tool));
}

void ToolRegistry::instantiate_all() {
    for (const auto& name : factory_registry_.names()) instantiate(name);
}

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    std::string tool_name = tool->name();
    if (policy_.has_value()) {
        tool = std::make_unique<PolicyEnforcedTool>(std::move(tool), *policy_);
    }
    tools_[tool_name] = std::move(tool);
}

void ToolRegistry::set_policy(ToolPolicy policy) { policy_ = std::move(policy); }

Tool* ToolRegistry::get(const std::string& name) const {
    auto it = tools_.find(name);
    return it == tools_.end() ? nullptr : it->second.get();
}

Tool& ToolRegistry::require(const std::string& name) const {
    Tool* t = get(name);
    if (!t) throw ToolNotFoundException(name);
    return *t;
}

bool ToolRegistry::has(const std::string& name) const { return tools_.contains(name); }

std::vector<std::string> ToolRegistry::list_names() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());
    for (const auto& [key, _] : tools_) names.push_back(key);
    std::ranges::sort(names);
    return names;
}

std::string ToolRegistry::render_tools_prompt() const {
    std::ostringstream oss;
    for (const auto& name : list_names()) {
        const Tool& t = *tools_.at(name);
        oss << "- " << t.name() << ": " << t.description() << "\n"
            << "  Tham số: " << t.parameters_schema() << "\n";
    }
    return oss.str();
}

}  // namespace agent
