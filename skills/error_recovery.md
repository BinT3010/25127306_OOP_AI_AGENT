---
name: error_recovery
keywords: lỗi, thất bại, error, fail, không thành công, retry, thử lại, sửa lỗi
---
# Kỹ năng: Xử lý khi tool trả về lỗi

Khi Observation cho biết một tool thất bại (success=false, có kèm `error`),
**không được** gọi lại y hệt cùng một tool với cùng tham số — điều đó chỉ tạo
ra vòng lặp vô ích và sẽ bị hệ thống phát hiện, buộc dừng agent.

Thay vào đó, hãy làm theo trình tự chẩn đoán sau:

1. **Đọc kỹ thông điệp lỗi**: Thông điệp lỗi thường chỉ rõ nguyên nhân (thiếu
   tham số, sai định dạng JSON, đường dẫn không hợp lệ, lệnh bị chặn bởi
   policy...). Đừng đoán mò — hãy sửa đúng nguyên nhân được nêu.
2. **Kiểm tra lại định dạng Action Input**: Lỗi phổ biến nhất là JSON sai cú
   pháp (thiếu dấu ngoặc kép, thiếu dấu phẩy). Hãy viết lại Action Input dưới
   dạng JSON hợp lệ, đúng schema mà mô tả tool đã cung cấp.
3. **Nếu tool báo "không tìm thấy" hoặc "bị từ chối"**: Kiểm tra lại tên tool
   có đúng chính tả như trong danh sách tool khả dụng không. Nếu tool bị
   chính sách (policy) từ chối, hãy tìm một tool khác có thể đạt cùng mục
   đích, hoặc giải thích trong Final Answer rằng thao tác đó không được phép.
4. **Nếu lỗi đến từ môi trường** (file không tồn tại, lệnh không có sẵn):
   Cân nhắc dùng tool `exec` hoặc `file` để kiểm tra tình trạng thực tế
   (ví dụ: liệt kê thư mục) trước khi thử lại thao tác ban đầu với thông tin
   chính xác hơn.
5. **Biết khi nào nên dừng**: Nếu đã thử tối đa 2 cách khắc phục khác nhau mà
   vẫn thất bại, hãy đưa ra Final Answer trung thực báo rằng nhiệm vụ không
   thể hoàn thành và nêu rõ lý do — không nên tiếp tục thử vô hạn.
