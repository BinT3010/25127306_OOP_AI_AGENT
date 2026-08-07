---
name: task_planner
keywords: kế hoạch, lập kế hoạch, nhiều bước, multi-step, plan, tuần tự, các bước, quy trình
---
# Kỹ năng: Lập kế hoạch trước khi hành động

Khi nhiệm vụ có vẻ cần **nhiều hơn một hành động** để hoàn thành (ví dụ: vừa
tính toán vừa ghi file, hoặc phải đọc dữ liệu trước rồi mới xử lý), hãy tuân
thủ quy trình sau:

1. **Phân rã nhiệm vụ**: Trong bước Thought đầu tiên, liệt kê ngắn gọn các
   bước con cần thực hiện theo đúng thứ tự phụ thuộc (bước nào cần kết quả
   của bước nào trước).
2. **Một hành động mỗi lượt**: Dù đã có kế hoạch nhiều bước, mỗi lượt chỉ được
   gọi **đúng một tool**. Đừng cố nhồi nhiều thao tác vào một lời gọi.
3. **Xác nhận trước khi kết thúc**: Trước khi đưa ra Final Answer, hãy tự hỏi:
   "Tất cả các bước trong kế hoạch ban đầu đã hoàn tất và có bằng chứng
   (Observation) xác nhận chưa?" Nếu còn bước nào chưa làm, đừng kết thúc.
4. **Ưu tiên tool chuyên biệt**: Nếu nhiệm vụ có phép tính số học, dùng tool
   `calculator` thay vì tự nhẩm trong đầu — kết quả từ tool luôn đáng tin cậy
   hơn suy luận ngôn ngữ tự nhiên của bạn.
5. **Không lặp lại hành động đã có kết quả**: Nếu một bước trong kế hoạch đã
   được thực hiện và có Observation hợp lệ, đừng gọi lại tool đó với cùng
   tham số — hãy dùng thẳng kết quả đã có để đi tiếp bước kế.

Ví dụ áp dụng: nhiệm vụ "Tính 15 nhân 17 rồi lưu kết quả vào file result.txt"
cần đúng 2 hành động theo thứ tự: (1) gọi `calculator` với "15*17", (2) gọi
`file` với action=write_file và content là kết quả vừa nhận được ở bước (1).
