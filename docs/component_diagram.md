# Component Diagram — Kiến trúc phân lớp tổng thể

Sơ đồ thể hiện các "module" (thư mục `src/*`) và **hướng phụ thuộc cho phép**
giữa chúng (mục 4.4 đề bài: "Agent Loop KHÔNG include harness_runner.h",
"Tool KHÔNG include agent_loop.h", v.v.). Mũi tên chỉ từ module PHỤ THUỘC
sang module ĐƯỢC PHỤ THUỘC.

```mermaid
flowchart TB
    subgraph ENTRY["Điểm vào (Entry points)"]
        CLI["main.cpp<br/><i>agent — CLI demo</i>"]
        BENCH["benchmark/run_eval.cpp<br/><i>run_eval — batch benchmark</i>"]
        TESTS["tests/*.cpp<br/><i>run_tests — doctest</i>"]
    end

    subgraph HARNESS["src/harness — Orchestration + Evaluation"]
        HR["HarnessRunner"]
        EVAL["Evaluator «Strategy»<br/>Keyword / Functional / VLM"]
        TRAJ["Trajectory · Task<br/>(data classes)"]
    end

    subgraph MULTI["src/multiagent — Bonus 10.3"]
        ORCH["SubAgentOrchestrator"]
        MQ["MessageQueue&lt;T&gt;"]
    end

    subgraph BUILDER["src/builder"]
        BLD["AgentLoopBuilder «Builder»"]
    end

    subgraph AGENT["src/agent — Core ReAct Loop"]
        AL["AgentLoop «Template Method»"]
        AP["action_parser<br/>(regex ReAct + native tool_calls)"]
        LD["LoopDetector"]
        SL["SkillLoader"]
        ENV["Environment «Strategy»<br/>Native / Sandbox"]
    end

    subgraph TOOLS["src/tools — 8 Tool implementations"]
        TR["ToolRegistry «Registry+Factory»"]
        TP["ToolPolicy + PolicyEnforcedTool «Decorator»"]
        TL["Tool «Strategy»<br/>Calculator·File·Exec·DateTime<br/>Memory·PythonExec·Http·WebSearch"]
    end

    subgraph CLIENT["src/client — LLM Abstraction"]
        LLM["LLMClient «Strategy»"]
        OLL["OllamaClient"]
        MOCK["MockLLMClient"]
    end

    subgraph UTIL["src/util — Nền tảng dùng chung"]
        REG["Registry&lt;T&gt; (template)"]
        EXC["Exception hierarchy"]
        LOG["Logger"]
        SUB["subprocess (POSIX fork/exec)"]
        B64["base64"]
    end

    CLI --> AL
    CLI --> TR
    CLI --> ENV
    CLI --> OLL
    CLI --> MOCK

    BENCH --> HR
    BENCH --> TRAJ
    BENCH --> TR

    TESTS -.-> AGENT
    TESTS -.-> TOOLS
    TESTS -.-> HARNESS
    TESTS -.-> MULTI
    TESTS -.-> CLIENT

    HR --> AL
    HR --> EVAL
    HR --> ENV
    HR --> TRAJ
    HR --> LLM
    EVAL --> LLM

    ORCH --> AL
    ORCH --> MQ
    ORCH --> ENV
    ORCH --> TR

    BLD --> AL

    AL --> LLM
    AL --> TR
    AL --> ENV
    AL --> AP
    AL --> LD
    AL --> SL
    AL --> TRAJ

    TR --> TL
    TR --> TP
    TR --> REG
    TL --> ENV

    OLL --> LLM
    MOCK --> LLM

    AGENT --> UTIL
    TOOLS --> UTIL
    CLIENT --> UTIL
    HARNESS --> UTIL

    style AL fill:#e8f4ff,stroke:#4a90d9,stroke-width:2px
    style HR fill:#fff4e6,stroke:#d9924a,stroke-width:2px
    style TR fill:#eaffea,stroke:#4ad97a,stroke-width:2px
    style LLM fill:#f5e6ff,stroke:#a94ad9,stroke-width:2px
```

## Ràng buộc kiến trúc quan trọng (đã kiểm chứng bằng cách đọc `#include`)

| Ràng buộc | Trạng thái |
| --- | --- |
| `src/agent/*` KHÔNG include `src/harness/harness_runner.h` | ✅ Đúng — `AgentLoop` chỉ expose `step_hook_` chung chung |
| `src/tools/*` KHÔNG include `src/agent/agent_loop.h` | ✅ Đúng — `Tool::execute()` chỉ nhận `Environment&`, không biết AgentLoop |
| `src/client/*` KHÔNG include bất kỳ gì ở tầng trên | ✅ Đúng — `LLMClient` là tầng thấp nhất, độc lập hoàn toàn |
| `HarnessRunner` là nơi DUY NHẤT nối `AgentLoop` + `Evaluator` | ✅ Đúng — cả hai chỉ "gặp nhau" bên trong `run_task()` |

Việc tách lớp nghiêm ngặt này là điều kiện để `MockLLMClient` có thể thay thế
`OllamaClient` trong toàn bộ test suite (`tests/*.cpp`) mà không cần sửa một
dòng nào ở các tầng `agent/`, `tools/`, `harness/` — minh chứng thực nghiệm
rằng ranh giới trừu tượng hoá thực sự hoạt động, không chỉ nằm trên giấy.
