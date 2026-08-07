# OOP AI Agent Framework

Đồ án cuối kỳ — Lập trình Hướng đối tượng. Một framework C++ (chuẩn C++23/26)
xây dựng AI Agent kiểu **ReAct** (Reasoning + Acting) chạy trên LLM cục bộ qua
[Ollama](https://ollama.com), có bộ công cụ (Tool) mở rộng được, hệ thống
Skill, và một Harness benchmark tự động chấm điểm.

> 📄 Báo cáo chi tiết (thiết kế, khó khăn, kết quả thực nghiệm): xem
> `docs/BaoCao_DoAn.docx`.
> 📊 Sơ đồ UML: xem [`docs/README.md`](docs/README.md).

## 1. Yêu cầu môi trường

| Thành phần | Phiên bản tối thiểu | Cài trên Ubuntu 24.04 |
| --- | --- | --- |
| Compiler hỗ trợ C++26 | GCC 14+ (hoặc Clang 19+/MSVC tương đương) | `sudo apt install g++-14 gcc-14` |
| CMake | ≥ 3.20 | `sudo apt install cmake` |
| libcurl (dev) | bất kỳ | `sudo apt install libcurl4-openssl-dev` |
| SQLite3 (dev) | bất kỳ | `sudo apt install libsqlite3-dev` |
| nlohmann-json (dev) | ≥ 3.10 | `sudo apt install nlohmann-json3-dev` |
| [Ollama](https://ollama.com) | bất kỳ | xem mục 4 |

> Vì sao cần GCC 14+? Dự án cố tình dùng ít nhất 1 kỹ thuật **C++26** — cụ thể là *placeholder variables* (`_`, P2169R4) trong
> `src/util/logger.h`. GCC mặc định trên Ubuntu 24.04 là bản 13, chưa hỗ trợ.

## 2. Build

```bash
git clone <repo-url> && cd Agent_MSSV1_MSSV2_MSSV3
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=g++-14
make -j$(nproc)
```

`cmake ..` sẽ **báo lỗi rõ ràng ngay lúc configure** nếu compiler không hỗ trợ
`-std=c++26`, thay vì để lỗi cú pháp khó hiểu giữa chừng lúc build.

Kết quả build nằm ở `build/bin/`:

| Executable | Vai trò |
| --- | --- |
| `agent` | CLI chạy một agent task đơn lẻ |
| `run_eval` | Chạy toàn bộ `benchmark/tasks.json` qua Harness, xuất báo cáo |
| `run_tests` | Bộ unit test (doctest) — 54 test case / 162 assertion |

## 3. Chạy thử ngay (không cần Ollama)

Mọi executable đều có cờ `--mock`, dùng `MockLLMClient` với kịch bản xác định
trước (deterministic) — để kiểm chứng toàn bộ pipeline hoạt động đúng mà
**không cần cài Ollama**:

```bash
cd ..   # về thư mục gốc dự án (để tìm skills/, benchmark/tasks.json)
./build/bin/agent --mock "Tính 15 nhân 17 rồi lưu vào result.txt"
./build/bin/run_eval --mock          # chạy đủ 10 task, in bảng PASS/FAIL
./build/bin/run_tests                # 54 test case
```

## 4. Chạy thật với Ollama

### 4.1. Cài & khởi động Ollama cục bộ

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama pull gemma3          # hoặc qwen3:8b, llama3.1, ... (nên chọn model hỗ trợ tool-calling)
ollama serve                # mặc định lắng nghe http://localhost:11434
```

```bash
./build/bin/agent --model gemma3 "Tính 15 nhân 17 rồi lưu vào result.txt"
./build/bin/run_eval --model gemma3
```

### 4.2. Chạy Ollama trên Google Colab / Kaggle (có GPU) + tunnel

Máy cá nhân thường không đủ VRAM cho model lớn. Có thể chạy Ollama trên
Colab/Kaggle (có GPU miễn phí) rồi expose ra ngoài qua tunnel:

```python
# Trong Colab/Kaggle notebook:
!curl -fsSL https://ollama.com/install.sh | sh
!nohup ollama serve > ollama.log 2>&1 &
!ollama pull qwen3:8b

# Cách 1: ngrok
!pip install pyngrok
from pyngrok import ngrok
public_url = ngrok.connect(11434, "http")
print(public_url)   # vd: https://xxxx.ngrok-free.app

# Cách 2: Cloudflare Tunnel (không cần tài khoản)
!wget -q https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64
!chmod +x cloudflared-linux-amd64
!./cloudflared-linux-amd64 tunnel --url http://localhost:11434
```

Sau đó trỏ CLI về URL public đó:

```bash
./build/bin/agent --model qwen3:8b --ollama-url https://xxxx.ngrok-free.app "..."
./build/bin/run_eval --model qwen3:8b --ollama-url https://xxxx.ngrok-free.app
```

### 4.3. Web Search (tuỳ chọn)

Tool `web_search` cần một endpoint JSON dạng
`{"results":[{"title","content","url"}, ...]}`. Cách đơn giản nhất là tự host
[SearXNG](https://docs.searxng.org/) bằng Docker:

```bash
docker run -d -p 8080:8080 searxng/searxng
```

Endpoint mặc định trong `main.cpp`/`run_eval.cpp`:
`http://localhost:8080/search?q={query}&format=json`. Không có SearXNG thì
`web_search` sẽ trả lỗi rõ ràng qua `ToolResult::fail`, không crash chương trình.

## 5. Thêm một tool mới (demo mở rộng)

Toàn bộ việc thêm tool mới chỉ cần 3 bước, không đụng tới `AgentLoop`:

1. Tạo `src/tools/my_tool.h` kế thừa `agent::Tool`, cài 4 hàm ảo
   (`name/description/parameters_schema/execute`).
2. Tạo `src/tools/my_tool.cpp` triển khai `execute()`, trả về `ToolResult`
   (không bao giờ để exception thoát ra ngoài).
3. Thêm đúng **một dòng** vào `register_standard_tools()` trong `main.cpp`
   (và/hoặc `benchmark/run_eval.cpp`):
   ```cpp
   registry.register_tool(std::make_unique<agent::MyTool>());
   ```

`CMakeLists.txt` dùng `GLOB_RECURSE` nên file `.cpp` mới trong `src/tools/` tự
động được biên dịch, không cần sửa CMakeLists.

## 6. Bảng đối chiếu kỹ thuật C++ đã dùng

| Chuẩn | Kỹ thuật | Vị trí tiêu biểu |
| --- | --- | --- |
| C++17 | Template class | `util::Registry<T>` |
| C++17 | `std::variant` + Overloaded visitor | `agent::Action` (`action.h`) |
| C++17 | `std::filesystem` | `Environment`, `FileTool`, `SkillLoader` |
| C++17 | `std::optional` | `ToolResult::error`, `ChatMessage` |
| C++20 | Concepts (`concept ToolLike = derived_from<T,Tool> && constructible_from<T>`) | `tools/tool_registry.h` (ràng buộc `register_tool_type<T>()`) |
| C++20 | Ranges (`std::ranges::sort/none_of/stable_sort`) | `tool_registry.cpp`, `sandbox_environment.cpp`, `skill_loader.cpp` |
| C++20 | `std::span` | `MemoryTool::cosine_similarity(std::span<const float>, ...)` — nhận vector/array/con trỏ đều được |
| C++20 | `std::format` / three-way comparison `<=>` | `Logger`, `TaskRunResult::operator<=>` |
| C++23 | `std::expected<T,E>` | `LLMClient::chat`, `ExprParser`, mọi hàm HTTP |
| C++23 | Deducing `this` (explicit object parameter) | `AgentLoopBuilder` (Builder pattern) |
| C++23 | `std::print`/`std::println` | `main.cpp`, `run_eval.cpp` |
| C++26 | Placeholder variables (`_`) | `util::Logger::log()` (`std::lock_guard _(mtx);`) |

## 7. 6 Design Pattern

| Pattern | Vai trò | File |
| --- | --- | --- |
| **Strategy** | `LLMClient`, `Evaluator`, `Environment`, `Tool` — mọi interface trừu tượng đều hoán đổi implementation tự do | `client/llm_client.h`, `harness/evaluator.h` |
| **Template Method** | `AgentLoop::run()` là khung ReAct cố định; `think/act/observe` là hook override được | `agent/agent_loop.h`, `agent/confirming_agent_loop.h` |
| **Registry + Factory** | Đăng ký & khởi tạo Tool theo tên | `util/registry.h`, `tools/tool_registry.h` |
| **Observer/Hook** | `step_hook_` cho phép `HarnessRunner` quan sát `AgentLoop` mà không có quan hệ include ngược | `agent/agent_loop.h` (`StepHook`) |
| **Decorator** *(bonus)* | `PolicyEnforcedTool` bọc Tool gốc, áp allow/deny list trong suốt | `tools/tool_policy.h` |
| **Builder** *(bonus)* | `AgentLoopBuilder` dựng AgentLoop qua fluent interface, dùng deducing-this | `builder/agent_loop_builder.h` |

## 8. Cấu trúc thư mục

```
Agent_MSSV1_MSSV2_MSSV3/
├── src/
│   ├── agent/        # AgentLoop, Environment, LoopDetector, SkillLoader, action_parser
│   ├── client/        # LLMClient, OllamaClient, MockLLMClient
│   ├── tools/          # 8 Tool + ToolRegistry + ToolPolicy (Decorator)
│   ├── harness/        # Trajectory, Task, Evaluator×3, HarnessRunner
│   ├── multiagent/     # SubAgentOrchestrator (bonus 10.3)
│   ├── builder/         # AgentLoopBuilder (bonus)
│   ├── util/             # Registry<T>, exceptions, Logger, subprocess, base64
│   └── main.cpp           # CLI
├── benchmark/
│   ├── tasks.json          # 10 task (4 đơn giản, 4 trung bình, 2 khó)
│   └── run_eval.cpp
├── skills/                    # 3 file .md (task_planner, error_recovery, tool_usage_discipline)
├── tests/                       # doctest — 54 test case
├── docs/                          # 4 sơ đồ Mermaid + báo cáo .docx
├── third_party/doctest/             # thư viện test vendor sẵn
└── CMakeLists.txt
```

## 9. Giới hạn môi trường phát triển (đọc trước khi đối chiếu số liệu benchmark)

Bộ khung này được phát triển và kiểm thử trong một môi trường **không có
internet ra ngoài** (chỉ truy cập được registry gói — apt/pip/npm — không gọi
được API bên ngoài như Ollama thật hay search engine thật). Vì vậy:

- Toàn bộ 54 unit test và ví dụ `--mock` trong README này chạy bằng
  `MockLLMClient` với kịch bản xác định trước — **đã kiểm chứng logic của
  toàn bộ pipeline (AgentLoop, ToolRegistry, Harness, Evaluator) là đúng**,
  nhưng **không phản ánh chất lượng suy luận của một LLM thật**.
- `benchmark_runs/batch_report.json` đính kèm là kết quả chạy `--mock`
  (10/10 pass) — dùng để xác nhận pipeline hoạt động, KHÔNG phải số liệu
  benchmark một model Ollama thật.
- Sinh viên/giảng viên khi chấm bài trên máy có Ollama cần chạy lại
  `./build/bin/run_eval --model <tên_model>` để có số liệu thật; success rate
  thực tế sẽ phụ thuộc vào khả năng model được chọn tuân thủ định dạng ReAct
  (xem `skills/tool_usage_discipline.md`).
- Xem thêm phân tích chi tiết (bao gồm các model đã thử nghiệm và mức độ
  thành công quan sát được khi tác giả tự chạy trên máy cá nhân) trong
  `docs/BaoCao_DoAn.docx`, mục "Kết quả thực nghiệm".

## 10. Giấy phép thư viện bên thứ ba

- [doctest](https://github.com/doctest/doctest) (MIT) — vendor sẵn tại
  `third_party/doctest/doctest.h`.
- libcurl, SQLite3, nlohmann/json — liên kết động qua CMake `find_package`,
  không vendor mã nguồn.
