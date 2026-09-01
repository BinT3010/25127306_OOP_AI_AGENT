# Sequence Diagram — Harness chạy Batch Benchmark

Minh hoạ luồng `./bin/run_eval` xử lý toàn bộ `benchmark/tasks.json`: với MỖI
task, `HarnessRunner` tự thực hiện đúng chu trình **setup environment → run
agent → evaluate → record** (mục 3.6 đề bài), lặp lại cho 10 task rồi tổng hợp
`BatchReport`.

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant Main as run_eval.cpp
    participant Runner as HarnessRunner
    participant Sandbox as SandboxEnvironment
    participant AL as AgentLoop
    participant Eval as Evaluator
    participant Traj as Trajectory
    participant Report as BatchReport

    User->>Main: ./bin/run_eval [--model ...]
    Main->>Main: load_tasks_from_file("benchmark/tasks.json")
    Main->>Runner: new HarnessRunner(llm, tools, skill_loader, options)
    Main->>Runner: run_batch(tasks)

    loop Với mỗi Task trong tasks (10 task)
        Runner->>Sandbox: new SandboxEnvironment(sandbox_root/task_id)
        Runner->>Sandbox: setup()
        Note right of Sandbox: Thư mục RIÊNG cho từng task,<br/>tránh ghi đè file lẫn nhau

        Runner->>AL: new AgentLoop(llm, tools, sandbox, options, skill_loader)
        Runner->>AL: set_step_hook(log_progress)
        Runner->>AL: run(task.id, task.instruction)
        Note over AL: (Xem sequence "Một lượt chạy Agent"<br/>cho chi tiết Think→Act→Observe)
        AL-->>Runner: Trajectory

        Runner->>Runner: make_evaluator(task.eval_type)
        Note right of Runner: Strategy: keyword \| functional \| vlm
        Runner->>Eval: evaluate(trajectory, task)
        Eval-->>Runner: EvalResult{passed, score, reason}

        Runner->>Traj: save_to_file("trajectory_{task_id}.json")
        Runner->>Sandbox: teardown()
    end

    Runner-->>Main: BatchReport{total, passed, success_rate, results[]}
    Main->>Report: save_to_file("benchmark_runs/batch_report.json")
    Main-->>User: In bảng PASS/FAIL từng task + success rate tổng
```

## Điểm nhấn thiết kế thể hiện trong sơ đồ

- **Separation of concerns (mục 4.4)**: `HarnessRunner` là bên **duy nhất**
  biết cách tạo `AgentLoop` VÀ `Evaluator` cùng lúc — bản thân `AgentLoop`
  không có logic gì liên quan tới việc chấm điểm hay batch.
- **Strategy pattern**: `make_evaluator()` chọn đúng 1 trong 3 Evaluator dựa
  trên `Task::eval_type`, tất cả dùng chung interface `evaluate()`.
- **Cô lập tài nguyên**: mỗi task có `SandboxEnvironment` riêng (setup/teardown
  quanh vòng đời của nó), trong khi `ToolRegistry` (kể cả `MemoryTool` có
  trạng thái SQLite) được chia sẻ xuyên suốt batch — mô phỏng đúng ngữ cảnh
  "bộ nhớ dài hạn qua nhiều lượt chạy" của bonus mục 10.2.
