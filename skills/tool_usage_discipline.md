---
name: tool_usage_discipline
keywords: tool, công cụ, action input, json, định dạng, format, gọi hàm, function call
---
# Kỹ năng: Kỷ luật khi gọi tool (Action Input)

Để hệ thống có thể parse chính xác lời gọi tool của bạn, hãy tuân thủ nghiêm
ngặt định dạng ReAct sau trong mỗi lượt:

```
Thought: <suy luận ngắn gọn, giải thích tại sao chọn hành động này>
Action: <đúng một tên tool trong danh sách được cung cấp>
Action Input: <một object JSON hợp lệ trên một dòng, đúng schema của tool>
```

Hoặc khi đã có đủ thông tin để trả lời:

```
Thought: <suy luận ngắn gọn>
Final Answer: <câu trả lời cuối cùng, đầy đủ, dành cho người dùng>
```

**Quy tắc bắt buộc:**
- Action Input LUÔN LUÔN là JSON hợp lệ — dùng dấu ngoặc kép `"` cho chuỗi và
  tên trường, không dùng dấu nháy đơn `'`.
- Chỉ dùng tên tool xuất hiện đúng nguyên văn trong danh sách "Tools khả dụng"
  ở system prompt. Không tự bịa tên tool.
- Không viết Action và Final Answer trong cùng một lượt — mỗi lượt chỉ chọn
  một trong hai.
- Nếu tool trả về Observation là dữ liệu số hoặc văn bản, hãy trích dẫn đúng
  giá trị đó khi dùng lại (đừng làm tròn hoặc suy diễn lại bằng trí nhớ).
- Với các tool có tham số tuỳ chọn (ví dụ `append` của tool `file`), chỉ cần
  thêm vào JSON khi thực sự cần, nếu không có thể bỏ qua để dùng giá trị mặc
  định.
