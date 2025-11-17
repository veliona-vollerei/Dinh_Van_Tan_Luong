Đề tài: HỆ THỐNG CHẤM CÔNG VÂN TAY SINH 
1. GIỚI THIỆU ĐỀ TÀI
1.1. Bối cảnh và Mục tiêu
Đề tài "Hệ thống chấm công vân tay" được phát triển nhằm giải quyết những hạn chế của các phương pháp chấm công truyền thống (ký tên, thẻ từ), vốn dễ bị gian lận và sai sót dữ liệu.

Hệ thống ứng dụng công nghệ nhận diện vân tay trên nền tảng vi điều khiển Arduino để tự động hóa quy trình quản lý thời gian làm việc.



Mục tiêu chính: Thiết kế và xây dựng mô hình hệ thống chấm công tự động , sử dụng cảm biến vân tay R307 để xác thực danh tính người dùng.



Tính năng bổ sung: Hỗ trợ tính năng quét thẻ RFID RC522 để nhận dạng người dùng nhanh hơn , và điều khiển Servo SG90 để mô phỏng cơ chế mở cửa.


Kiến trúc cốt lõi: Sử dụng kiến trúc Master-Slave qua giao thức I2C để phân tải xử lý và đảm bảo tính ổn định của hệ thống.

1.2. Công nghệ và Phần cứng Sử dụng
Hệ thống được xây dựng dựa trên các linh kiện nhúng sau:

Linh kiện	Vai trò	Chức năng chính
Arduino Uno R3	
Bộ xử lý trung tâm (2 Mạch - Master/Slave) 

Xử lý dữ liệu, điều khiển trạng thái hệ thống.

Cảm biến vân tay R307	
Khối nhận dạng sinh trắc học 

Nhận diện và xác thực vân tay người dùng.

Mạch RFID RC522	
Khối nhận dạng thẻ từ 

Hỗ trợ xác thực bằng thẻ RFID.

LCD 16x2 I2C	
Khối hiển thị 

Hiển thị kết quả xác thực và trạng thái hoạt động.

Servo SG90	
Khối điều khiển cơ khí 

Mô phỏng cơ chế mở cửa khi xác thực thành công.


Xuất sang Trang tính

2. KẾT QUẢ VÀ ĐÁNH GIÁ ĐẠT ĐƯỢC
2.1. Kết quả Thực nghiệm
Sau khi triển khai trên mô hình thử nghiệm, hệ thống đã đạt được các kết quả khả quan, chứng minh tính hiệu quả và độ tin cậy trong môi trường văn phòng nhỏ:


Nhận diện chính xác: Hệ thống nhận diện vân tay với tỷ lệ lỗi thấp, đạt độ chính xác ở mức 95% trong thử nghiệm với 20 nhân viên.




Tốc độ xử lý: Thời gian nhận diện vân tay trung bình là 2–3 giây, đáp ứng yêu cầu về tốc độ của hệ thống chấm công.



Quản lý dữ liệu: Dữ liệu chấm công được lưu trữ tự động, và quá trình đăng ký vân tay diễn ra nhanh chóng, trung bình khoảng 5–7 giây cho mỗi mẫu.


2.2. Đánh giá Hệ thống

Ưu điểm: Hệ thống có độ chính xác cao , giúp tiết kiệm thời gian quản lý và có giao diện thân thiện, dễ sử dụng.




Hạn chế: Hệ thống còn phụ thuộc vào phần cứng (dễ bị ảnh hưởng nếu đầu đọc bẩn/hỏng) và chưa có các tính năng nâng cao (như thông báo tự động, tích hợp di động).


2.3. Kết luận Chung
Đề tài đã hoàn thành các mục tiêu đề ra, ứng dụng thành công công nghệ vân tay và RFID để tự động hóa quy trình chấm công và điều khiển mở cửa. Hệ thống đã chứng minh được 
hiệu quả trong việc quản lý nhân viên và lưu trữ dữ liệu chính xác , có tính khả thi cao và sẵn sàng cho các hướng phát triển nâng cao hơn trong tương lai
(như tích hợp nền tảng di động hoặc sử dụng mã hóa AES cho bảo mật).
