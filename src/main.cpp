/**
 * @file main.cpp
 * @brief CLI entry point — chạy một agent task đơn lẻ từ dòng lệnh (mục 9.3:
 * "chạy một agent task hoàn chỉnh từ command line").
 *
 * Cách dùng:
 *   ./bin/agent "Tính 15 nhân 17 rồi lưu kết quả vào result.txt"
 *   ./bin/agent --model qwen3:8b --ollama-url http://localhost:11434 "..."
 *   ./bin/agent --mock "..."      (chạy offline bằng MockLLMClient, để demo nhanh không cần Ollama)
 */
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include "agent/agent_loop.h"
#include "agent/native_environment.h"
#include "agent/skill_loader.h"
#include "client/mock_llm_client.h"
#include "client/ollama_client.h"
#include "tools/calculator_tool.h"
#include "tools/datetime_tool.h"
#include "tools/exec_tool.h"
#include "tools/file_tool.h"
#include "tools/http_tool.h"
#include "tools/memory_tool.h"
#include "tools/python_exec_tool.h"
#include "tools/tool_registry.h"
#include "tools/web_search_tool.h"
#include "util/exceptions.h"
#include "tools/word_count_tool.h"

namespace {

/// Đăng ký đầy đủ bộ tool tiêu chuẩn của hệ thống (5 tool bắt buộc + 3 tool bổ
/// sung) vào một ToolRegistry — dùng chung bởi main.cpp, run_eval.cpp và tests.
/// Đặt tại đây để "thêm một tool mới" (yêu cầu demo mục 9.3) chỉ cần thêm
/// đúng MỘT dòng register_tool() vào hàm này.
void register_standard_tools(agent::ToolRegistry& registry, const std::filesystem::path& memory_db_path) {
    // Tool khởi tạo mặc định được -> dùng register_tool_type<T>() (ràng buộc
    // bởi concept ToolLike, ngắn gọn hơn). Tool cần tham số constructor (path,
    // endpoint) vẫn dùng register_tool(make_unique<T>(...)) như thường lệ.
    registry.register_tool_type<agent::CalculatorTool>();
    registry.register_tool_type<agent::FileTool>();
    registry.register_tool_type<agent::ExecTool>();
    registry.register_tool_type<agent::DateTimeTool>();
    registry.register_tool_type<agent::PythonExecTool>();
    registry.register_tool_type<agent::HttpFetchTool>();
    registry.register_tool(std::make_unique<agent::MemoryTool>(memory_db_path));
    // web_search cần một search endpoint JSON (vd: SearXNG tự host) — xem README.
    // Mặc định trỏ tới localhost:8080, có thể không hoạt động nếu chưa cấu hình.
    registry.register_tool(
        std::make_unique<agent::WebSearchTool>("http://localhost:8080/search?q={query}&format=json"));
    registry.register_tool_type<agent::WordCountTool>();
}

struct CliArgs {
    std::string model = "gemma3";
    std::string ollama_url = "http://localhost:11434";
    bool use_mock = false;
    int max_steps = 10;
    // Mặc định thấp hơn giá trị mặc định của thư viện (ChatOptions::temperature = 0.7):
    // một agent dùng tool và phải trích dẫn CHÍNH XÁC số liệu Observation (word_count,
    // calculator...) cần độ ngẫu nhiên thấp hơn một chatbot trò chuyện thông thường —
    // temperature càng cao, model càng dễ "diễn giải lại" thay vì chép đúng nguyên văn.
    double temperature = 0.2;
    std::string instruction;
};

CliArgs parse_args(int argc, char** argv) {
    CliArgs args;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--model" && i + 1 < argc) {
            args.model = argv[++i];
        } else if (a == "--ollama-url" && i + 1 < argc) {
            args.ollama_url = argv[++i];
        } else if (a == "--max-steps" && i + 1 < argc) {
            args.max_steps = std::stoi(argv[++i]);
        } else if (a == "--temperature" && i + 1 < argc) {
            args.temperature = std::stod(argv[++i]);
        } else if (a == "--mock") {
            args.use_mock = true;
        } else {
            positional.push_back(a);
        }
    }
    for (std::size_t i = 0; i < positional.size(); ++i) {
        args.instruction += (i > 0 ? " " : "") + positional[i];
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    CliArgs args = parse_args(argc, argv);
    if (args.instruction.empty()) {
        std::println(stderr,
                      "Cách dùng: {} [--model <tên>] [--ollama-url <url>] [--max-steps N] "
                      "[--temperature <0.0-1.0>] [--mock] \"<nhiệm vụ>\"",
                      argv[0]);
        std::println(stderr, "Ví dụ: {} \"Tính 15 nhân 17 rồi lưu kết quả vào result.txt\"", argv[0]);
        return 1;
    }

    std::filesystem::path work_dir = std::filesystem::current_path() / "agent_workspace";
    std::filesystem::create_directories(work_dir);
    agent::NativeEnvironment env(work_dir);

    agent::ToolRegistry registry;
    register_standard_tools(registry, work_dir / "memory.sqlite3");

    agent::SkillLoader skill_loader("skills");

    std::unique_ptr<agent::LLMClient> mock_holder;  // giữ sống nếu dùng --mock
    agent::LLMClient* llm = nullptr;

    if (args.use_mock) {
        auto mock = std::make_unique<agent::MockLLMClient>();
        mock->enqueue_final_answer("Chạy ở chế độ --mock để demo nhanh không cần Ollama.",
                                    "(Đây là câu trả lời giả lập — dùng --model/--ollama-url để chạy thật.)");
        mock_holder = std::move(mock);
        llm = mock_holder.get();
    } else {
        static agent::OllamaClient ollama({.base_url = args.ollama_url});
        if (!ollama.health_check()) {
            std::println(stderr,
                          "⚠️  Không kết nối được tới Ollama tại {}. Hãy đảm bảo Ollama đang chạy "
                          "(xem README để cấu hình Colab/Kaggle tunnel), hoặc dùng cờ --mock để demo offline.",
                          args.ollama_url);
            return 2;
        }
        llm = &ollama;
    }

    agent::ChatOptions chat_options;
    chat_options.model = args.model;
    chat_options.temperature = args.temperature;

    agent::AgentLoop::Config loop_config;
    loop_config.max_steps = args.max_steps;

    agent::AgentLoop loop(*llm, registry, env, chat_options, &skill_loader, loop_config);
    loop.set_step_hook([](const agent::Step& step) {
        std::println("── Bước {} ──────────────────────────", step.step_id);
        if (!step.thought.empty()) std::println("  Thought: {}", step.thought);
        if (step.action.type == "tool_call") {
            std::println("  Action: {}", step.action.tool);
            std::println("  Action Input: {}", step.action.args);
            std::println("  Observation: {}", step.tool_result);
        } else if (step.action.type == "final_answer") {
            std::println("  Final Answer: {}", step.tool_result);
        } else if (step.action.type == "malformed") {
            std::println("  ⚠️  Định dạng không hợp lệ: {}", step.tool_result);
        }
    });

    std::println("\n╔══════════════════════════════════════════════╗");
    std::println("║  OOP AI Agent Framework — chạy nhiệm vụ      ║");
    std::println("╚══════════════════════════════════════════════╝");
    std::println("Model : {}", chat_options.model);
    std::println("Task  : {}\n", args.instruction);

    agent::Trajectory trajectory = loop.run("cli_task", args.instruction);

    std::println("\n════════════════════════════════════════════════");
    std::println("Kết quả : {}", trajectory.success ? "THÀNH CÔNG" : "THẤT BẠI");
    if (!trajectory.success) std::println("Lý do   : {}", trajectory.failure_reason);
    std::println("Số bước : {}", trajectory.steps.size());
    std::println("Tokens  : {}", trajectory.total_tokens);
    std::println("Thời gian: {} ms", trajectory.total_time_ms);
    if (trajectory.success) std::println("\nCâu trả lời cuối cùng:\n{}", trajectory.final_answer);

    std::filesystem::path traj_path = "trajectory_cli_task.json";
    trajectory.save_to_file(traj_path);
    std::println("\n(Đã lưu trajectory vào {})", traj_path.string());

    return trajectory.success ? 0 : 1;
}
