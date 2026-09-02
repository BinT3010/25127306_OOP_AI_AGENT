/**
 * @file test_agent.cpp
 * @brief Unit test cho tầng agent: action_parser (regex ReAct), skill_loader,
 * AgentLoop (Template Method) chạy với MockLLMClient, và AgentLoopBuilder.
 */
#include "doctest.h"

#include <fstream>

#include "../src/agent/action_parser.h"
#include "../src/agent/agent_loop.h"
#include "../src/agent/confirming_agent_loop.h"
#include "../src/agent/native_environment.h"
#include "../src/agent/skill_loader.h"
#include "../src/builder/agent_loop_builder.h"
#include "../src/client/mock_llm_client.h"
#include "../src/tools/calculator_tool.h"
#include "../src/tools/file_tool.h"
#include "../src/tools/word_count_tool.h"

namespace {
std::filesystem::path make_tmp_dir(const std::string& name) {
    auto p = std::filesystem::temp_directory_path() / ("agent_test_" + name);
    std::filesystem::create_directories(p);
    return p;
}
}  // namespace

// ============================== action_parser ==============================

TEST_CASE("parse_action: định dạng ReAct chuẩn -> ToolCallAction") {
    agent::ChatResult r;
    r.content = "Thought: can tinh\nAction: calculator\nAction Input: {\"expression\": \"1+1\"}";
    auto action = agent::parse_action(r);
    REQUIRE(std::holds_alternative<agent::ToolCallAction>(action));
    auto& tc = std::get<agent::ToolCallAction>(action);
    CHECK(tc.tool_name == "calculator");
    CHECK(tc.thought == "can tinh");
    CHECK(tc.args_json == "{\"expression\": \"1+1\"}");
}

TEST_CASE("parse_action: Final Answer -> FinalAnswerAction") {
    agent::ChatResult r;
    r.content = "Thought: xong roi\nFinal Answer: 42";
    auto action = agent::parse_action(r);
    REQUIRE(std::holds_alternative<agent::FinalAnswerAction>(action));
    CHECK(std::get<agent::FinalAnswerAction>(action).answer == "42");
}

TEST_CASE("parse_action: markdown bold labels (**Thought:**) vẫn parse đúng") {
    agent::ChatResult r;
    r.content = "**Thought:** suy nghi\n**Action:** calculator\n**Action Input:** {\"expression\": \"2+2\"}";
    auto action = agent::parse_action(r);
    REQUIRE(std::holds_alternative<agent::ToolCallAction>(action));
    CHECK(std::get<agent::ToolCallAction>(action).thought == "suy nghi");
}

TEST_CASE("parse_action: text không theo định dạng nào -> MalformedAction") {
    agent::ChatResult r;
    r.content = "Toi khong biet phai lam gi.";
    auto action = agent::parse_action(r);
    CHECK(std::holds_alternative<agent::MalformedAction>(action));
}

TEST_CASE("parse_action: native tool_calls được ưu tiên hơn text parsing") {
    agent::ChatResult r;
    r.content = "some preamble text";
    r.tool_calls.push_back({"call_1", "file", "{\"action\":\"read_file\"}"});
    auto action = agent::parse_action(r);
    REQUIRE(std::holds_alternative<agent::ToolCallAction>(action));
    auto& tc = std::get<agent::ToolCallAction>(action);
    CHECK(tc.tool_name == "file");
    CHECK(tc.raw_call_id == "call_1");
}

TEST_CASE("parse_action: chỉ có Thought (không Action/Final Answer) -> ThinkAction") {
    agent::ChatResult r;
    r.content = "Thought: toi can suy nghi them";
    auto action = agent::parse_action(r);
    REQUIRE(std::holds_alternative<agent::ThinkAction>(action));
}

// ============================== SkillLoader ==============================

TEST_CASE("SkillLoader: nạp đúng số skill từ thư mục thật của dự án và chọn theo từ khoá") {
    agent::SkillLoader loader(std::string(PROJECT_ROOT_DIR) + "/skills");
    CHECK(loader.size() >= 3);

    auto selected = loader.select_for_task("Nhiệm vụ này cần lập kế hoạch nhiều bước");
    bool found_planner = false;
    for (auto* s : selected)
        if (s->name == "task_planner") found_planner = true;
    CHECK(found_planner);

    auto none = loader.select_for_task("khong lien quan gi ca xyz123");
    CHECK(none.empty());
}

TEST_CASE("SkillLoader: render_injection_block rỗng khi không có skill nào được chọn") {
    CHECK(agent::SkillLoader::render_injection_block({}) == "");
}

// ============================== AgentLoop (Template Method) ==============================

TEST_CASE("AgentLoop: happy path - 1 tool call rồi Final Answer") {
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("tinh toan", "calculator", R"({"expression":"6*7"})");
    mock.enqueue_final_answer("xong", "42");

    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    agent::NativeEnvironment env(make_tmp_dir("loop_happy"));

    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});
    auto traj = loop.run("t1", "tinh 6*7");

    CHECK(traj.success);
    CHECK(traj.final_answer == "42");
    REQUIRE(traj.steps.size() == 2);
    CHECK(traj.steps[0].action.type == "tool_call");
    CHECK(traj.steps[0].tool_result == "42");
    CHECK(traj.steps[1].action.type == "final_answer");
}

TEST_CASE("AgentLoop: gọi tool không tồn tại -> Observation báo lỗi, agent phục hồi") {
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("goi nham", "tool_khong_ton_tai", "{}");
    mock.enqueue_final_answer("thoi", "khong lam duoc");

    agent::ToolRegistry reg;  // rỗng, không có tool nào
    agent::NativeEnvironment env(make_tmp_dir("loop_badtool"));
    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});
    auto traj = loop.run("t2", "task");

    REQUIRE(traj.steps.size() == 2);
    CHECK_FALSE(traj.steps[0].tool_success);
    CHECK(traj.steps[0].tool_result.find("không tồn tại") != std::string::npos);
    CHECK(traj.success);  // vẫn kết thúc thành công ở bước 2 (agent tự phục hồi)
}

TEST_CASE("AgentLoop: vượt quá max_steps -> success=false, có failure_reason") {
    agent::MockLLMClient mock;
    mock.set_responder([](const std::vector<agent::ChatMessage>&, const agent::ChatOptions&) {
        agent::ChatResult r;
        r.content = "Thought: t\nAction: calculator\nAction Input: {\"expression\":\"1+1\"}";
        return r;
    });
    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    agent::NativeEnvironment env(make_tmp_dir("loop_maxsteps"));

    agent::AgentLoop::Config cfg;
    cfg.max_steps = 2;
    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"}, nullptr, cfg);
    auto traj = loop.run("t3", "task vo han");

    CHECK_FALSE(traj.success);
    CHECK_FALSE(traj.failure_reason.empty());
    CHECK(traj.steps.size() == 2);
}

TEST_CASE("AgentLoop: loop detector dừng agent khi lặp lại y hệt nhiều lần") {
    agent::MockLLMClient mock;
    mock.set_responder([](const std::vector<agent::ChatMessage>&, const agent::ChatOptions&) {
        agent::ChatResult r;
        r.content = "Thought: t\nAction: calculator\nAction Input: {\"expression\":\"1+1\"}";
        return r;
    });
    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    agent::NativeEnvironment env(make_tmp_dir("loop_detect"));

    agent::AgentLoop::Config cfg;
    cfg.max_steps = 20;  // đủ lớn để loop detector (không phải max_steps) là nguyên nhân dừng
    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"}, nullptr, cfg);
    auto traj = loop.run("t4", "task lap lai");

    CHECK_FALSE(traj.success);
    CHECK(traj.steps.size() < 20);  // phải dừng SỚM hơn max_steps nhờ loop detector
    CHECK(traj.failure_reason.find("lặp") != std::string::npos);
}

TEST_CASE("AgentLoop: step_hook (Observer) được gọi đúng số lần") {
    agent::MockLLMClient mock;
    mock.enqueue_final_answer("t", "ok");
    agent::ToolRegistry reg;
    agent::NativeEnvironment env(make_tmp_dir("loop_hook"));
    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});

    int calls = 0;
    loop.set_step_hook([&](const agent::Step&) { ++calls; });
    loop.run("t5", "task");
    CHECK(calls == 1);
}

TEST_CASE("AgentLoop: đa bước với nhiều tool khác nhau kết hợp (calculator + file)") {
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("b1", "calculator", R"({"expression":"3*4"})");
    mock.enqueue_tool_call("b2", "file", R"({"action":"write_file","path":"out.txt","content":"12"})");
    mock.enqueue_final_answer("b3", "Da luu 12");

    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    reg.register_tool(std::make_unique<agent::FileTool>());
    auto dir = make_tmp_dir("loop_combo");
    agent::NativeEnvironment env(dir);

    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});
    auto traj = loop.run("t6", "task ket hop");

    CHECK(traj.success);
    std::ifstream check(dir / "out.txt");
    std::string content((std::istreambuf_iterator<char>(check)), std::istreambuf_iterator<char>());
    CHECK(content == "12");
}

// ============================== ConfirmingAgentLoop + Builder ==============================

TEST_CASE("ConfirmingAgentLoop: kế thừa hành vi run() gốc, không phá vỡ Template Method") {
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("ghi file", "file", R"({"action":"write_file","path":"c.txt","content":"x"})");
    mock.enqueue_final_answer("xong", "done");

    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::FileTool>());
    agent::NativeEnvironment env(make_tmp_dir("confirming"));

    agent::ConfirmingAgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});
    auto traj = loop.run("t7", "task");
    CHECK(traj.success);
    CHECK(traj.steps.size() == 2);
}

TEST_CASE("AgentLoopBuilder: fluent chain rvalue, build() dựng đúng AgentLoop") {
    agent::MockLLMClient mock;
    mock.enqueue_final_answer("t", "builder ok");
    agent::ToolRegistry reg;
    agent::NativeEnvironment env(make_tmp_dir("builder"));

    auto loop = agent::AgentLoopBuilder()
                    .with_llm(mock)
                    .with_tools(reg)
                    .with_environment(env)
                    .with_model("test-model")
                    .with_max_steps(5)
                    .build();
    REQUIRE(loop != nullptr);
    auto traj = loop->run("t8", "task");
    CHECK(traj.success);
    CHECK(traj.final_answer == "builder ok");
}

TEST_CASE("AgentLoopBuilder: build() ném ConfigException nếu thiếu tham số bắt buộc") {
    agent::AgentLoopBuilder builder;
    CHECK_THROWS_AS(builder.build(), agent::ConfigException);
}

// ============================== Grounding check (Final Answer vs Observation) ==============================

TEST_CASE("AgentLoop: grounding check từ chối Final Answer mâu thuẫn với Observation của word_count, "
          "buộc model trả lời lại đúng con số") {
    // Tái hiện đúng lỗi thực tế: word_count trả về Observation "Số từ: 7" cho câu
    // "Tôi yêu lập trình hướng đối tượng" (7 token cách nhau bởi khoảng trắng), nhưng
    // lượt đầu model lại tự bịa Final Answer là "5 từ" — mâu thuẫn với Observation.
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("dem tu", "word_count", R"({"text":"Tôi yêu lập trình hướng đối tượng"})");
    mock.enqueue_final_answer("sai", "Câu này có 5 từ.");   // KHÔNG khớp Observation ("Số từ: 7")
    mock.enqueue_final_answer("dung", "Câu này có 7 từ.");  // Sau khi bị từ chối, model trả lời lại đúng

    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::WordCountTool>());
    agent::NativeEnvironment env(make_tmp_dir("grounding_wordcount"));

    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});
    auto traj = loop.run("t9", "dem tu");

    REQUIRE(traj.steps.size() == 3);
    CHECK(traj.steps[0].action.type == "tool_call");
    CHECK(traj.steps[0].tool_result == "Số từ: 7");
    CHECK(traj.steps[1].action.type == "final_answer");
    CHECK_FALSE(traj.steps[1].tool_success);  // lượt trả lời sai ("5 từ") bị grounding check từ chối
    CHECK(traj.steps[2].action.type == "final_answer");
    CHECK(traj.success);
    CHECK(traj.final_answer == "Câu này có 7 từ.");
}

TEST_CASE("AgentLoop: grounding check KHÔNG can thiệp khi Observation không phải kết quả từ "
          "word_count/calculator (vd file trả về số byte không liên quan)") {
    agent::MockLLMClient mock;
    mock.enqueue_tool_call("ghi file", "file", R"({"action":"write_file","path":"c.txt","content":"x"})");
    mock.enqueue_final_answer("xong", "done");  // KHÔNG chứa "1" (số byte) — vẫn phải được chấp nhận

    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::FileTool>());
    agent::NativeEnvironment env(make_tmp_dir("grounding_file_no_false_positive"));

    agent::AgentLoop loop(mock, reg, env, agent::ChatOptions{.model = "mock"});
    auto traj = loop.run("t10", "ghi file");

    REQUIRE(traj.steps.size() == 2);
    CHECK(traj.success);
    CHECK(traj.final_answer == "done");
}

