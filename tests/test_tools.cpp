/**
 * @file test_tools.cpp
 * @brief Unit test cho các Tool: calculator, file, datetime, exec, tool_registry,
 * tool_policy (Decorator), và sandbox path-escape protection.
 */
#include "doctest.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../src/agent/native_environment.h"
#include "../src/agent/sandbox_environment.h"
#include "../src/tools/calculator_tool.h"
#include "../src/tools/datetime_tool.h"
#include "../src/tools/exec_tool.h"
#include "../src/tools/file_tool.h"
#include "../src/tools/http_tool.h"
#include "../src/tools/memory_tool.h"
#include "../src/tools/tool_registry.h"
#include "../src/tools/web_search_tool.h"
#include "../src/tools/word_count_tool.h"

using json = nlohmann::json;

namespace {
std::filesystem::path make_tmp_dir(const std::string& name) {
    auto p = std::filesystem::temp_directory_path() / ("agent_test_" + name);
    std::filesystem::create_directories(p);
    return p;
}
}  // namespace

TEST_CASE("CalculatorTool: các phép toán cơ bản và ưu tiên toán tử") {
    agent::CalculatorTool calc;
    agent::NativeEnvironment env(make_tmp_dir("calc"));

    auto run = [&](const std::string& expr) { return calc.execute(json{{"expression", expr}}.dump(), env); };

    CHECK(run("2 + 3").output == "5");
    CHECK(run("(8 + 12) * 3 - 5").output == "55");
    CHECK(run("2^3^2").output == "512");  // luỹ thừa kết hợp phải: 2^(3^2) = 2^9 = 512
    CHECK(run("-5 + 3").output == "-2");
    CHECK(run("3.5 * 2").output == "7");
}

TEST_CASE("CalculatorTool: xử lý lỗi (chia 0, cú pháp sai) trả fail chứ không crash") {
    agent::CalculatorTool calc;
    agent::NativeEnvironment env(make_tmp_dir("calc_err"));
    auto run = [&](const std::string& expr) { return calc.execute(json{{"expression", expr}}.dump(), env); };

    auto r1 = run("10 / 0");
    CHECK_FALSE(r1.success);
    CHECK(r1.error.has_value());

    auto r2 = run("2 + + +");
    CHECK_FALSE(r2.success);

    auto r3 = run("(2 + 3");  // thiếu dấu đóng ngoặc
    CHECK_FALSE(r3.success);
}

TEST_CASE("FileTool: ghi rồi đọc lại đúng nội dung, hỗ trợ append") {
    agent::FileTool ft;
    agent::NativeEnvironment env(make_tmp_dir("file"));

    auto w1 = ft.execute(json{{"action", "write_file"}, {"path", "a.txt"}, {"content", "hello"}}.dump(), env);
    CHECK(w1.success);

    auto r1 = ft.execute(json{{"action", "read_file"}, {"path", "a.txt"}}.dump(), env);
    CHECK(r1.success);
    CHECK(r1.output == "hello");

    auto w2 =
        ft.execute(json{{"action", "write_file"}, {"path", "a.txt"}, {"content", " world"}, {"append", true}}.dump(), env);
    CHECK(w2.success);
    auto r2 = ft.execute(json{{"action", "read_file"}, {"path", "a.txt"}}.dump(), env);
    CHECK(r2.output == "hello world");
}

TEST_CASE("FileTool: đọc file không tồn tại trả fail rõ ràng") {
    agent::FileTool ft;
    agent::NativeEnvironment env(make_tmp_dir("file_missing"));
    auto r = ft.execute(json{{"action", "read_file"}, {"path", "khong_ton_tai.txt"}}.dump(), env);
    CHECK_FALSE(r.success);
    CHECK(r.error.has_value());
}

TEST_CASE("SandboxEnvironment: chặn path traversal thoát ra ngoài sandbox") {
    auto root = make_tmp_dir("sandbox_escape") / "inner";
    agent::SandboxEnvironment sbox(root);
    sbox.setup();
    agent::FileTool ft;

    auto r = ft.execute(json{{"action", "read_file"}, {"path", "../../../etc/passwd"}}.dump(), sbox);
    CHECK_FALSE(r.success);
    REQUIRE(r.error.has_value());
    CHECK(r.error->find("sandbox") != std::string::npos);

    // Thao tác hợp lệ TRONG sandbox vẫn phải hoạt động bình thường.
    auto w = ft.execute(json{{"action", "write_file"}, {"path", "ok.txt"}, {"content", "fine"}}.dump(), sbox);
    CHECK(w.success);
    sbox.teardown();
}

TEST_CASE("SandboxEnvironment: is_command_allowed chặn lệnh nguy hiểm") {
    agent::SandboxEnvironment sbox(make_tmp_dir("sandbox_cmd"));
    CHECK_FALSE(sbox.is_command_allowed("sudo rm -rf /"));
    CHECK(sbox.is_command_allowed("echo hello"));
}

TEST_CASE("WordCountTool: đếm đúng với khoảng trắng ASCII bình thường") {
    agent::WordCountTool wc;
    agent::NativeEnvironment env(make_tmp_dir("wordcount_ascii"));
    auto run = [&](const std::string& text) { return wc.execute(json{{"text", text}}.dump(), env); };

    CHECK(run("Tôi yêu lập trình hướng đối tượng").output == "Số từ: 7");
    CHECK(run("lập trình hướng đối tượng").output == "Số từ: 5");
    CHECK(run("").output == "Số từ: 0");
    CHECK(run("   Tôi   yêu   ").output == "Số từ: 2");  // nhiều dấu cách liên tiếp / thừa đầu-cuối
    CHECK(run("Tôi\tyêu\nlập").output == "Số từ: 3");    // tab và xuống dòng cũng là dấu phân cách
}

TEST_CASE("WordCountTool: không đếm thiếu khi 2 từ bị nối bởi non-breaking space "
          "(U+00A0 — Word hay tự chèn) thay vì dấu cách thường") {
    agent::WordCountTool wc;
    agent::NativeEnvironment env(make_tmp_dir("wordcount_nbsp"));
    auto run = [&](const std::string& text) { return wc.execute(json{{"text", text}}.dump(), env); };

    // "Tôi<NBSP>yêu lập" — cách đếm cũ (istringstream, chỉ nhận whitespace ASCII)
    // sẽ gộp "Tôi" và "yêu" thành 1 token và trả về 2 (SAI). Phải ra 3.
    CHECK(run("Tôi\xC2\xA0yêu lập").output == "Số từ: 3");
    CHECK(run("một\xE2\x80\x83hai" "\xE3\x80\x80" "ba").output == "Số từ: 3");  // en-space, ideographic space
}

TEST_CASE("DateTimeTool: diff_days và add_days tính đúng theo lịch") {
    agent::DateTimeTool dt;
    agent::NativeEnvironment env(make_tmp_dir("datetime"));

    auto r1 = dt.execute(json{{"action", "diff_days"}, {"date", "2026-01-01"}, {"other_date", "2026-07-09"}}.dump(), env);
    CHECK(r1.success);
    CHECK(r1.output == "189 ngày");

    auto r2 = dt.execute(json{{"action", "add_days"}, {"date", "2026-07-09"}, {"days", 30}}.dump(), env);
    CHECK(r2.success);
    CHECK(r2.output == "2026-08-08");

    auto r3 = dt.execute(json{{"action", "diff_days"}, {"date", "ngay-sai"}, {"other_date", "2026-01-01"}}.dump(), env);
    CHECK_FALSE(r3.success);
}

TEST_CASE("ExecTool: chạy lệnh shell và trả về đúng stdout") {
    agent::ExecTool exec;
    agent::NativeEnvironment env(make_tmp_dir("exec"));
    auto r = exec.execute(json{{"command", "echo hello_from_exec"}}.dump(), env);
    CHECK(r.success);
    CHECK(r.output.find("hello_from_exec") != std::string::npos);
}

TEST_CASE("ExecTool: lệnh bị SandboxEnvironment từ chối") {
    agent::ExecTool exec;
    agent::SandboxEnvironment sbox(make_tmp_dir("exec_denied"));
    sbox.setup();
    auto r = exec.execute(json{{"command", "sudo reboot"}}.dump(), sbox);
    CHECK_FALSE(r.success);
    sbox.teardown();
}

TEST_CASE("WebSearchTool: dùng fetcher tiêm sẵn (dependency injection), không cần mạng thật") {
    agent::WebSearchTool tool("http://fake/search?q={query}",
                               [](const std::string& url) -> std::expected<std::string, std::string> {
                                   CHECK(url.find("fake/search") != std::string::npos);
                                   json resp;
                                   resp["results"] = json::array(
                                       {{{"title", "Kết quả 1"}, {"content", "nội dung mẫu"}, {"url", "http://x.com"}}});
                                   return resp.dump();
                               });
    agent::NativeEnvironment env(make_tmp_dir("websearch"));
    auto r = tool.execute(json{{"query", "test"}}.dump(), env);
    CHECK(r.success);
    CHECK(r.output.find("Kết quả 1") != std::string::npos);
}

TEST_CASE("HttpFetchTool: dùng fetcher tiêm sẵn, từ chối URL không hợp lệ") {
    agent::HttpFetchTool tool([](const std::string&) -> std::expected<std::string, std::string> {
        return std::string("<html>nội dung giả lập</html>");
    });
    agent::NativeEnvironment env(make_tmp_dir("http"));

    auto r1 = tool.execute(json{{"url", "https://example.com"}}.dump(), env);
    CHECK(r1.success);
    CHECK(r1.output.find("nội dung giả lập") != std::string::npos);

    auto r2 = tool.execute(json{{"url", "ftp://khong-hop-le.com"}}.dump(), env);
    CHECK_FALSE(r2.success);
}

TEST_CASE("MemoryTool: lưu và tìm lại bằng khớp chuỗi con (không có embedding)") {
    auto db_path = make_tmp_dir("memory") / "test.sqlite3";
    std::filesystem::remove(db_path);
    agent::MemoryTool mem(db_path);
    agent::NativeEnvironment env(make_tmp_dir("memory_env"));

    auto s = mem.execute(json{{"action", "memory_save"}, {"key", "fact1"}, {"content", "Thu do Viet Nam la Ha Noi"}}.dump(),
                          env);
    CHECK(s.success);

    auto q = mem.execute(json{{"action", "memory_search"}, {"query", "Ha Noi"}}.dump(), env);
    CHECK(q.success);
    CHECK(q.output.find("fact1") != std::string::npos);
}

TEST_CASE("MemoryTool: bật vector search khi có embedding_fn, xếp hạng theo cosine similarity") {
    auto db_path = make_tmp_dir("memory_vec") / "test.sqlite3";
    std::filesystem::remove(db_path);

    // embedding giả: vector 2 chiều [x, y], mã hoá "gan giong nhau" thành các
    // vector gần nhau trong không gian — đủ để kiểm chứng cosine similarity
    // xếp hạng đúng thứ tự mà không cần model thật.
    auto fake_embed = [](const std::string& text) -> std::expected<std::vector<float>, std::string> {
        if (text.find("meo") != std::string::npos) return std::vector<float>{1.0f, 0.0f};
        if (text.find("cho") != std::string::npos) return std::vector<float>{0.9f, 0.1f};
        return std::vector<float>{0.0f, 1.0f};
    };
    agent::MemoryTool mem(db_path, fake_embed);
    CHECK(mem.vector_search_enabled());
    agent::NativeEnvironment env(make_tmp_dir("memory_vec_env"));

    mem.execute(json{{"action", "memory_save"}, {"key", "k1"}, {"content", "con meo den"}}.dump(), env);
    mem.execute(json{{"action", "memory_save"}, {"key", "k2"}, {"content", "con cho nau"}}.dump(), env);
    mem.execute(json{{"action", "memory_save"}, {"key", "k3"}, {"content", "may bay phan luc"}}.dump(), env);

    auto q = mem.execute(json{{"action", "memory_search"}, {"query", "con meo"}, {"top_k", 1}}.dump(), env);
    CHECK(q.success);
    CHECK(q.output.find("k1") != std::string::npos);  // gần "meo" nhất phải xếp đầu
}

TEST_CASE("cosine_similarity: các trường hợp cơ bản (std::span nhận vào)") {
    std::vector<float> a{1, 0}, b{1, 0}, c{0, 1}, d{-1, 0}, empty{}, e{1, 2};
    CHECK(agent::cosine_similarity(a, b) == doctest::Approx(1.0));
    CHECK(agent::cosine_similarity(a, c) == doctest::Approx(0.0));
    CHECK(agent::cosine_similarity(a, d) == doctest::Approx(-1.0));
    CHECK(agent::cosine_similarity(empty, e) == -1.0);  // vector rỗng -> quy ước trả -1

    float raw_array[] = {1.0f, 0.0f};  // std::span cũng nhận C-array trực tiếp, không cần vector
    CHECK(agent::cosine_similarity(raw_array, a) == doctest::Approx(1.0));
}

TEST_CASE("ToolRegistry: register/get/require/list_names hoạt động đúng") {
    agent::ToolRegistry reg;
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    reg.register_tool(std::make_unique<agent::DateTimeTool>());

    CHECK(reg.size() == 2);
    CHECK(reg.has("calculator"));
    CHECK(reg.get("khong_ton_tai") == nullptr);
    CHECK_THROWS_AS(reg.require("khong_ton_tai"), agent::ToolNotFoundException);

    auto names = reg.list_names();
    CHECK(names.size() == 2);

    std::string prompt = reg.render_tools_prompt();
    CHECK(prompt.find("calculator") != std::string::npos);
    CHECK(prompt.find("datetime") != std::string::npos);
}

TEST_CASE("ToolPolicy + PolicyEnforcedTool (Decorator): allow/deny list hoạt động đúng") {
    agent::ToolRegistry reg;
    reg.set_policy(agent::ToolPolicy::deny({"exec"}));
    reg.register_tool(std::make_unique<agent::CalculatorTool>());
    reg.register_tool(std::make_unique<agent::ExecTool>());

    agent::NativeEnvironment env(make_tmp_dir("policy"));
    auto ok = reg.require("calculator").execute(json{{"expression", "1+1"}}.dump(), env);
    CHECK(ok.success);

    auto denied = reg.require("exec").execute(json{{"command", "echo hi"}}.dump(), env);
    CHECK_FALSE(denied.success);
    CHECK(denied.error->find("từ chối") != std::string::npos);
}

TEST_CASE("ToolRegistry: register_tool_type<T>() (ràng buộc bởi concept ToolLike)") {
    agent::ToolRegistry reg;
    reg.register_tool_type<agent::CalculatorTool>();
    reg.register_tool_type<agent::DateTimeTool>();
    CHECK(reg.size() == 2);
    CHECK(reg.has("calculator"));
    CHECK(reg.has("datetime"));
}

TEST_CASE("ToolPolicy: allow_only chỉ cho phép đúng danh sách") {
    auto policy = agent::ToolPolicy::allow_only({"calculator"});
    CHECK(policy.is_allowed("calculator"));
    CHECK_FALSE(policy.is_allowed("exec"));
    CHECK_FALSE(policy.is_allowed("file"));
}
