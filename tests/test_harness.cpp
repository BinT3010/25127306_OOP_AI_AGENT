/**
 * @file test_harness.cpp
 * @brief Unit test cho tầng harness: Trajectory/Task (de)serialization,
 * 3 chiến lược Evaluator (Strategy pattern), và HarnessRunner tích hợp đầy đủ.
 */
#include "doctest.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../src/agent/native_environment.h"
#include "../src/client/mock_llm_client.h"
#include "../src/harness/functional_evaluator.h"
#include "../src/harness/harness_runner.h"
#include "../src/harness/keyword_evaluator.h"
#include "../src/harness/task.h"
#include "../src/harness/vlm_evaluator.h"
#include "../src/tools/calculator_tool.h"
#include "../src/tools/file_tool.h"

namespace {
std::filesystem::path make_tmp_dir(const std::string& name) {
    auto p = std::filesystem::temp_directory_path() / ("agent_test_" + name);
    std::filesystem::create_directories(p);
    return p;
}
}  // namespace

// ============================== Trajectory / Task ==============================

TEST_CASE("Trajectory: to_json/from_json round-trip giữ nguyên dữ liệu") {
    agent::Trajectory t;
    t.task_id = "tX";
    t.model = "gemma3";
    t.success = true;
    t.final_answer = "ket qua";
    t.total_tokens = 999;
    t.total_time_ms = 1234;
    agent::Step s;
    s.step_id = 0;
    s.thought = "suy nghi";
    s.action = {"tool_call", "calculator", "{}"};
    s.tool_result = "ok";
    s.tokens_used = 10;
    s.latency_ms = 5;
    t.steps.push_back(s);

    auto j = t.to_json();
    auto t2 = agent::Trajectory::from_json(j);

    CHECK(t2.task_id == t.task_id);
    CHECK(t2.success == t.success);
    CHECK(t2.final_answer == t.final_answer);
    CHECK(t2.total_tokens == t.total_tokens);
    REQUIRE(t2.steps.size() == 1);
    CHECK(t2.steps[0].action.tool == "calculator");
}

TEST_CASE("Trajectory: save_to_file với tên file trần (không thư mục) không crash") {
    agent::Trajectory t;
    t.task_id = "bare";
    auto old_cwd = std::filesystem::current_path();
    auto tmp = make_tmp_dir("traj_bare");
    std::filesystem::current_path(tmp);
    t.save_to_file("bare_output.json");  // path KHÔNG có parent — đây là bug đã sửa
    CHECK(std::filesystem::exists(tmp / "bare_output.json"));
    std::filesystem::current_path(old_cwd);
}

TEST_CASE("Task::from_json và load_tasks_from_file nạp đúng tasks.json thật của dự án") {
    auto tasks = agent::load_tasks_from_file(std::string(PROJECT_ROOT_DIR) + "/benchmark/tasks.json");
    CHECK(tasks.size() == 10);
    CHECK(tasks[0].id == "task_001");
    CHECK(tasks[0].eval_type == "functional");
    REQUIRE(tasks[0].eval_script.has_value());
}

TEST_CASE("load_tasks_from_file: file không tồn tại ném ConfigException") {
    CHECK_THROWS_AS(agent::load_tasks_from_file("khong_ton_tai/tasks.json"), agent::ConfigException);
}

// ============================== Evaluators (Strategy) ==============================

TEST_CASE("KeywordEvaluator: require_all=true mặc định, thiếu 1 từ khoá là fail") {
    agent::Trajectory t;
    t.success = true;
    t.final_answer = "Ket qua la 255 va da luu file";
    agent::Task task;
    task.expected_keywords = std::vector<std::string>{"255", "khong_co_trong_cau"};

    agent::KeywordEvaluator ev;
    auto r = ev.evaluate(t, task);
    CHECK_FALSE(r.passed);
    CHECK(r.score == doctest::Approx(0.5));
}

TEST_CASE("KeywordEvaluator: trajectory thất bại thì luôn fail bất kể nội dung") {
    agent::Trajectory t;
    t.success = false;
    t.failure_reason = "loi gi do";
    agent::Task task;
    task.expected_keywords = std::vector<std::string>{"255"};
    agent::KeywordEvaluator ev;
    CHECK_FALSE(ev.evaluate(t, task).passed);
}

TEST_CASE("FunctionalEvaluator: chạy eval_script thật, kiểm tra file trên đĩa") {
    auto dir = make_tmp_dir("functional_eval");
    { std::ofstream f(dir / "result.txt"); f << "255"; }

    agent::Trajectory t;
    t.success = true;
    agent::Task task;
    task.eval_script = "test -f result.txt && grep -q 255 result.txt && echo PASS";
    agent::FunctionalEvaluator ev(dir);
    auto r = ev.evaluate(t, task);
    CHECK(r.passed);
}

TEST_CASE("FunctionalEvaluator: eval_script fail khi file không đúng nội dung") {
    auto dir = make_tmp_dir("functional_eval_fail");
    { std::ofstream f(dir / "result.txt"); f << "999"; }

    agent::Trajectory t;
    t.success = true;
    agent::Task task;
    task.eval_script = "grep -q 255 result.txt && echo PASS";
    agent::FunctionalEvaluator ev(dir);
    CHECK_FALSE(ev.evaluate(t, task).passed);
}

TEST_CASE("VLMEvaluator: LLM-as-judge parse đúng JSON phán quyết từ MockLLMClient") {
    agent::MockLLMClient mock;
    mock.enqueue_response({R"({"passed": true, "score": 0.85, "reason": "dat yeu cau"})", {}, 5, 5, 1, "mock"});

    agent::Trajectory t;
    t.success = true;
    t.final_answer = "42";
    agent::Task task;
    task.instruction = "tinh 6*7";

    agent::VLMEvaluator ev(mock);
    auto r = ev.evaluate(t, task);
    CHECK(r.passed);
    CHECK(r.score == doctest::Approx(0.85));
}

TEST_CASE("VLMEvaluator: phản hồi JSON không hợp lệ -> fail an toàn, không crash") {
    agent::MockLLMClient mock;
    mock.enqueue_response({"khong phai json hop le", {}, 5, 5, 1, "mock"});
    agent::Trajectory t;
    t.success = true;
    agent::Task task;
    agent::VLMEvaluator ev(mock);
    auto r = ev.evaluate(t, task);
    CHECK_FALSE(r.passed);
}

// ============================== HarnessRunner (tích hợp đầy đủ) ==============================

TEST_CASE("HarnessRunner: setup->run->evaluate->record cho một task functional") {
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("tinh", "calculator", R"({"expression":"5*5"})");
    mock.enqueue_tool_call("luu", "file", R"({"action":"write_file","path":"r.txt","content":"25"})");
    mock.enqueue_final_answer("xong", "25");

    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    reg.register_tool(std::make_unique<agent::FileTool>());

    agent::HarnessRunner::Config cfg;
    auto run_root = make_tmp_dir("harness_run");
    cfg.sandbox_root = run_root / "sandboxes";
    cfg.trajectory_output_dir = run_root / "trajectories";

    agent::HarnessRunner runner(mock, reg, nullptr, agent::ChatOptions{.model = "mock"}, cfg);

    agent::Task task;
    task.id = "ht_001";
    task.instruction = "tinh 5*5 roi luu vao r.txt";
    task.eval_type = "functional";
    task.eval_script = "test -f r.txt && grep -q 25 r.txt && echo PASS";
    task.max_steps = 6;

    auto result = runner.run_task(task);
    CHECK(result.eval_result.passed);
    CHECK(result.trajectory.success);
    CHECK(std::filesystem::exists(run_root / "trajectories" / "trajectory_ht_001.json"));
    // sandbox phải được dọn dẹp (teardown) sau khi evaluate xong
    CHECK_FALSE(std::filesystem::exists(run_root / "sandboxes" / "ht_001" / "r.txt"));
}

TEST_CASE("HarnessRunner: run_batch tính đúng success_rate trên nhiều task") {
    agent::MockLLMClient mock;
    mock.enqueue_final_answer("t", "10");   // task pass (keyword "10")
    mock.enqueue_final_answer("t", "sai");  // task fail (thiếu keyword "20")

    agent::ToolRegistry reg;
    auto run_root = make_tmp_dir("harness_batch");
    agent::HarnessRunner::Config cfg;
    cfg.sandbox_root = run_root / "sandboxes";
    cfg.trajectory_output_dir = run_root / "trajectories";
    agent::HarnessRunner runner(mock, reg, nullptr, agent::ChatOptions{.model = "mock"}, cfg);

    agent::Task t1;
    t1.id = "b1";
    t1.instruction = "tra loi 10";
    t1.eval_type = "keyword";
    t1.expected_keywords = std::vector<std::string>{"10"};

    agent::Task t2;
    t2.id = "b2";
    t2.instruction = "tra loi 20";
    t2.eval_type = "keyword";
    t2.expected_keywords = std::vector<std::string>{"20"};

    auto report = runner.run_batch({t1, t2});
    CHECK(report.total == 2);
    CHECK(report.passed == 1);
    CHECK(report.success_rate == doctest::Approx(0.5));
}

TEST_CASE("HarnessRunner: eval_type không hợp lệ ném ConfigException") {
    agent::MockLLMClient mock;
    mock.enqueue_final_answer("t", "x");
    agent::ToolRegistry reg;
    auto run_root = make_tmp_dir("harness_badtype");
    agent::HarnessRunner::Config cfg;
    cfg.sandbox_root = run_root / "sandboxes";
    cfg.trajectory_output_dir = run_root / "trajectories";
    agent::HarnessRunner runner(mock, reg, nullptr, agent::ChatOptions{.model = "mock"}, cfg);

    agent::Task task;
    task.id = "bad";
    task.instruction = "x";
    task.eval_type = "khong_hop_le";
    CHECK_THROWS_AS(runner.run_task(task), agent::ConfigException);
}
