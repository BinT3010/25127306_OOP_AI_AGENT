# docs/ — Sơ đồ thiết kế (UML, dùng Mermaid)

| File | Nội dung |
| --- | --- |
| [`class_diagram.md`](class_diagram.md) | Sơ đồ lớp tổng thể — toàn bộ class, quan hệ kế thừa/kết hợp, đối chiếu 6 design pattern |
| [`sequence_diagram_agent.md`](sequence_diagram_agent.md) | Sơ đồ tuần tự — một lượt chạy Agent đơn lẻ (CLI `agent`) |
| [`sequence_diagram_harness.md`](sequence_diagram_harness.md) | Sơ đồ tuần tự — Harness chạy batch benchmark (`run_eval`) |
| [`component_diagram.md`](component_diagram.md) | Sơ đồ thành phần — các module `src/*` và hướng phụ thuộc cho phép |

Các sơ đồ dùng cú pháp [Mermaid](https://mermaid.js.org/), hiển thị trực tiếp
khi xem trên GitHub/GitLab. Để xem cục bộ:

- **VS Code**: cài extension "Markdown Preview Mermaid Support", mở file rồi
  `Ctrl+Shift+V`.
- **Trình duyệt**: dán khối code trong dấu ```` ```mermaid ... ``` ```` vào
  [mermaid.live](https://mermaid.live).

Toàn bộ 4 sơ đồ đã được kiểm tra cú pháp hợp lệ bằng `mermaid.parse()`
(Node.js) trước khi nộp bài — không có lỗi parse.
