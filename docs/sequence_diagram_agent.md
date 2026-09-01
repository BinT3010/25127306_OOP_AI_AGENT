# Sequence Diagram — Một lượt chạy Agent (CLI)

Minh hoạ luồng `./bin/agent "Tính 15 nhân 17 rồi lưu vào result.txt"` — từ lúc
người dùng gõ lệnh tới khi nhận được Trajectory hoàn chỉnh. Đây là ví dụ với
**đúng 1 tool call rồi Final Answer**; vòng lặp `Think → Act → Observe` có thể
lặp lại nhiều lượt tuỳ nhiệm vụ (xem khung `loop` bên dưới).

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant CLI as main.cpp
    participant AL as AgentLoop
    participant Parser as action_parser
    participant Detector as LoopDetector
    participant LLM as LLMClient
    participant Registry as ToolRegistry
    participant T as Tool
    participant Env as Environment

    User->>CLI: ./bin/agent "Tính 15*17, lưu vào result.txt"
    CLI->>CLI: register_standard_tools(registry)
    CLI->>AL: new AgentLoop(llm, registry, env, options, skill_loader)
    CLI->>AL: run("cli_task", instruction)

    AL->>AL: build_system_prompt(instruction)
    Note right of AL: Nhúng tool list + skill<br/>liên quan (SkillLoader)

    loop Cho tới khi Final Answer hoặc hết max_steps
        AL->>LLM: chat(history, options)
        LLM-->>AL: expected<ChatResult, string>
        AL->>Parser: parse_action(chat_result)
        Parser-->>AL: Action (variant)

        alt Action = ToolCallAction
            AL->>Detector: record_and_check(tool+args)
            Detector-->>AL: LoopDetectionResult
            opt severity == Critical
                AL->>AL: đánh dấu thất bại, dừng vòng lặp
            end
            AL->>Registry: require(tool_name)
            Registry-->>AL: Tool&
            AL->>T: execute(args_json, env)
            T->>Env: resolve_path() / is_command_allowed()
            Env-->>T: path hợp lệ / bool
            T-->>AL: ToolResult
            AL->>AL: observe(step) → step_hook_ (nếu có)
            AL->>LLM: (lượt kế) history += tool_result
        else Action = FinalAnswerAction
            AL->>AL: trajectory.success = true
        else Action = MalformedAction
            AL->>LLM: (lượt kế) history += lời nhắc sửa định dạng
        end
    end

    AL-->>CLI: Trajectory
    CLI->>CLI: trajectory.save_to_file("trajectory_cli_task.json")
    CLI-->>User: In kết quả + Final Answer ra console
```

## Điểm nhấn thiết kế thể hiện trong sơ đồ

- **Template Method**: khung `loop` (Think → parse → Act/Observe) nằm trọn
  trong `AgentLoop::run()` — không lớp nào bên ngoài (CLI, Harness) can thiệp
  vào trình tự này.
- **Strategy**: `LLM` có thể là `OllamaClient` hoặc `MockLLMClient` — sơ đồ
  không đổi dù đổi implementation.
- **Observer/Hook**: bước `observe(step)` là điểm duy nhất AgentLoop "phát tín
  hiệu" ra ngoài qua `step_hook_`, không biết ai đang lắng nghe.
- Tách lớp rõ ràng: `AgentLoop` không hề gọi tới `HarnessRunner` — nó chỉ biết
  `LLMClient`, `ToolRegistry`, `Environment`, `SkillLoader`.
