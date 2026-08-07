/**
 * @file run_eval.cpp
 * @brief Entry point chạy toàn bộ benchmark/tasks.json qua HarnessRunner và
 * xuất báo cáo (mục 9.3: "Chạy batch benchmark, show file JSON output").
 *
 * Cách dùng:
 *   ./bin/run_eval                                  (chạy thật với Ollama, model mặc định)
 *   ./bin/run_eval --model qwen3:8b --ollama-url http://localhost:11434
 *   ./bin/run_eval --mock                           (offline, dùng MockLLMClient kịch bản sẵn —
 *                                                     minh hoạ toàn bộ pipeline mà KHÔNG cần Ollama)
 */
#include <print>
#include <string>

#include "../src/agent/native_environment.h"
#include "../src/agent/skill_loader.h"
#include "../src/client/mock_llm_client.h"
#include "../src/client/ollama_client.h"
#include "../src/harness/harness_runner.h"
#include "../src/harness/task.h"
#include "../src/tools/calculator_tool.h"
#include "../src/tools/datetime_tool.h"
#include "../src/tools/exec_tool.h"
#include "../src/tools/file_tool.h"
#include "../src/tools/memory_tool.h"
#include "../src/tools/python_exec_tool.h"
#include "../src/tools/tool_registry.h"
#include "../src/util/exceptions.h"

namespace {

void register_tools(agent::ToolRegistry& registry) {
    registry.register_tool_type<agent::CalculatorTool>();
    registry.register_tool_type<agent::FileTool>();
    registry.register_tool_type<agent::ExecTool>();
    registry.register_tool_type<agent::DateTimeTool>();
    registry.register_tool_type<agent::PythonExecTool>();
    registry.register_tool(std::make_unique<agent::MemoryTool>("benchmark_runs/memory.sqlite3"));
}

/// Nạp kịch bản MockLLMClient tương ứng CHÍNH XÁC với 10 task trong tasks.json
/// — cho phép chạy toàn bộ pipeline Harness -> Evaluator -> JSON report một
/// cách xác định (deterministic), có thể tái lập, mà KHÔNG cần một Ollama
/// server thật. Đây là cách nhóm tự kiểm chứng (validate) toàn bộ hệ thống
/// harness/evaluator hoạt động đúng trước khi chạy với model thật (xem README
/// và báo cáo, mục "Giới hạn môi trường chạy thử").
void seed_mock_scenarios(agent::MockLLMClient& mock) {
    // task_001: 15*17 -> result.txt
    mock.enqueue_tool_call("Tính 15*17 trước", "calculator", R"({"expression":"15*17"})");
    mock.enqueue_tool_call("Lưu kết quả vào file", "file", R"({"action":"write_file","path":"result.txt","content":"255"})");
    mock.enqueue_final_answer("Xong", "Đã tính 15*17=255 và lưu vào result.txt");

    // task_002: diff_days
    mock.enqueue_tool_call("Tính số ngày chênh lệch", "datetime",
                            R"({"action":"diff_days","date":"2026-01-01","other_date":"2026-07-09"})");
    mock.enqueue_final_answer("Đã có kết quả", "Số ngày giữa hai mốc là 189");

    // task_003: write greeting
    mock.enqueue_tool_call("Ghi lời chào vào file", "file",
                            R"({"action":"write_file","path":"greeting.txt","content":"Xin chao AI Agent"})");
    mock.enqueue_final_answer("Xong", "Đã ghi lời chào vào greeting.txt");

    // task_004: (8+12)*3-5 = 55
    mock.enqueue_tool_call("Tính biểu thức", "calculator", R"({"expression":"(8 + 12) * 3 - 5"})");
    mock.enqueue_final_answer("Xong", "Kết quả là 55");

    // task_005: 9*9=81 > 50 -> big.txt
    mock.enqueue_tool_call("Tính 9*9", "calculator", R"({"expression":"9*9"})");
    mock.enqueue_tool_call("81 > 50 nên ghi vào big.txt", "file",
                            R"({"action":"write_file","path":"big.txt","content":"81"})");
    mock.enqueue_final_answer("Xong", "81 lớn hơn 50 nên đã lưu vào big.txt");

    // task_006: now + add_days(10) -> future_date.txt
    mock.enqueue_tool_call("Lấy ngày hiện tại", "datetime", R"({"action":"now"})");
    mock.enqueue_tool_call("Cộng thêm 10 ngày", "datetime", R"({"action":"add_days","date":"2026-07-11","days":10})");
    mock.enqueue_tool_call("Lưu vào file", "file",
                            R"({"action":"write_file","path":"future_date.txt","content":"2026-07-21"})");
    mock.enqueue_final_answer("Xong", "Ngày sau khi cộng 10 ngày là 2026-07-21, đã lưu vào future_date.txt");

    // task_007: 12+45+78=135 -> memory_save -> memory_search
    mock.enqueue_tool_call("Tính tổng 3 số", "calculator", R"({"expression":"12+45+78"})");
    mock.enqueue_tool_call("Lưu vào bộ nhớ", "memory", R"({"action":"memory_save","key":"sum_result","content":"135"})");
    mock.enqueue_tool_call("Xác nhận lại", "memory", R"({"action":"memory_search","query":"sum_result"})");
    mock.enqueue_final_answer("Đã xác nhận", "Tổng 3 số là 135, đã lưu và xác nhận trong bộ nhớ");

    // task_008: exec ls | wc -l -> file_count.txt
    mock.enqueue_tool_call("Đếm số file", "exec", R"({"command":"ls -1 | wc -l"})");
    mock.enqueue_tool_call("Lưu số lượng vào file", "file",
                            R"({"action":"write_file","path":"file_count.txt","content":"0"})");
    mock.enqueue_final_answer("Xong", "Đã đếm và lưu số lượng file vào file_count.txt");

    // task_009: python sum(1..100)=5050 -> python_sum.txt
    mock.enqueue_tool_call("Viết và chạy mã Python tính tổng", "python_exec",
                            R"json({"code":"print(sum(range(1,101)))"})json");
    mock.enqueue_tool_call("Lưu kết quả vào file", "file",
                            R"({"action":"write_file","path":"python_sum.txt","content":"5050"})");
    mock.enqueue_final_answer("Xong", "Tổng từ 1 đến 100 là 5050, đã lưu vào python_sum.txt");

    // task_010: python 5!=120 -> factorial.txt -> calculator sqrt(120) -> sqrt_result.txt
    mock.enqueue_tool_call("Viết mã Python tính giai thừa 5", "python_exec",
                            R"json({"code":"import math; print(math.factorial(5))"})json");
    mock.enqueue_tool_call("Lưu giai thừa vào file", "file",
                            R"({"action":"write_file","path":"factorial.txt","content":"120"})");
    mock.enqueue_tool_call("Tính căn bậc hai của 120", "calculator", R"({"expression":"120^0.5"})");
    mock.enqueue_tool_call("Lưu căn bậc hai vào file", "file",
                            R"({"action":"write_file","path":"sqrt_result.txt","content":"10.954"})");
    mock.enqueue_final_answer("Xong", "5! = 120, căn bậc hai của 120 xấp xỉ 10.954");
}

struct CliArgs {
    std::string model = "gemma3";
    std::string ollama_url = "http://localhost:11434";
    bool use_mock = false;
};

CliArgs parse_args(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--model" && i + 1 < argc) {
            args.model = argv[++i];
        } else if (a == "--ollama-url" && i + 1 < argc) {
            args.ollama_url = argv[++i];
        } else if (a == "--mock") {
            args.use_mock = true;
        }
    }
    return args;
}

}  // namespace

int main(int argc, char** argv) {
    CliArgs args = parse_args(argc, argv);

    std::vector<agent::Task> tasks;
    try {
        tasks = agent::load_tasks_from_file("benchmark/tasks.json");
    } catch (const agent::ConfigException& e) {
        std::println(stderr, "Lỗi nạp tasks.json: {}", e.what());
        return 1;
    }
    std::println("Đã nạp {} task từ benchmark/tasks.json", tasks.size());

    agent::ToolRegistry registry;
    register_tools(registry);
    agent::SkillLoader skill_loader("skills");

    std::unique_ptr<agent::LLMClient> mock_holder;
    agent::LLMClient* llm = nullptr;
    std::string model_name = args.model;

    if (args.use_mock) {
        auto mock = std::make_unique<agent::MockLLMClient>();
        seed_mock_scenarios(*mock);
        mock_holder = std::move(mock);
        llm = mock_holder.get();
        model_name = "mock-deterministic";
        std::println("Chế độ: MOCK (offline, kịch bản xác định trước — xem seed_mock_scenarios())\n");
    } else {
        static agent::OllamaClient ollama({.base_url = args.ollama_url});
        if (!ollama.health_check()) {
            std::println(stderr,
                          "⚠️  Không kết nối được Ollama tại {}. Dùng --mock để chạy demo offline, "
                          "hoặc khởi động Ollama trước (xem README).",
                          args.ollama_url);
            return 2;
        }
        llm = &ollama;
        std::println("Chế độ: OLLAMA thật tại {} (model={})\n", args.ollama_url, model_name);
    }

    agent::ChatOptions chat_options;
    chat_options.model = model_name;

    agent::HarnessRunner::Config harness_config;
    agent::HarnessRunner runner(*llm, registry, &skill_loader, chat_options, harness_config);

    agent::BatchReport report = runner.run_batch(tasks);

    std::println("\n╔══════════════════════════════════════════════╗");
    std::println("║              KẾT QUẢ BENCHMARK                ║");
    std::println("╚══════════════════════════════════════════════╝");
    std::ranges::sort(report.results);  // C++20 spaceship: sắp điểm giảm dần (xem TaskRunResult::operator<=>)
    for (const auto& r : report.results) {
        std::println("  [{}] {} — {} (score={:.2f}, {} bước, {} tokens)", r.task.id,
                      r.eval_result.passed ? "PASS" : "FAIL", r.task.description, r.eval_result.score,
                      r.trajectory.steps.size(), r.trajectory.total_tokens);
        if (!r.eval_result.passed) std::println("      Lý do: {}", r.eval_result.reason);
    }
    std::println("\nTổng: {}/{} task PASS — success rate = {:.1f}%", report.passed, report.total,
                  report.success_rate * 100.0);

    std::filesystem::path report_path = "benchmark_runs/batch_report.json";
    report.save_to_file(report_path);
    std::println("\nĐã lưu báo cáo chi tiết vào {}", report_path.string());
    std::println("Trajectory từng task được lưu trong benchmark_runs/trajectories/");

    return 0;
}
