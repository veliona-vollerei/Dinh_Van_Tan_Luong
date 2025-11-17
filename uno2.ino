#include <Wire.h>

#define SLAVE_ADDR 8 // Phải khớp với Master

// Định nghĩa chân cắm 6 nút
#define BTN_PIN_ENROLL 2
#define BTN_PIN_EDIT   3
#define BTN_PIN_DELETE 4
#define BTN_PIN_LIST   5
#define BTN_PIN_OK     6
#define BTN_PIN_ESCAPE 7 // Nút Thoát (Escape)

// Biến toàn cục để lưu trạng thái nút
volatile byte buttonState = 0;

void setup() {
  // Dùng điện trở kéo nội INPUT_PULLUP cho 6 nút
  pinMode(BTN_PIN_ENROLL, INPUT_PULLUP);
  pinMode(BTN_PIN_EDIT,   INPUT_PULLUP);
  pinMode(BTN_PIN_DELETE, INPUT_PULLUP);
  pinMode(BTN_PIN_LIST,   INPUT_PULLUP);
  pinMode(BTN_PIN_OK,     INPUT_PULLUP);
  pinMode(BTN_PIN_ESCAPE, INPUT_PULLUP); 

  Wire.begin(SLAVE_ADDR); // Khởi động I2C (Slave)
  Wire.onRequest(requestEvent);
}

// Hàm này được gọi bởi ngắt I2C khi Master yêu cầu dữ liệu
void requestEvent() {
  Wire.write(buttonState);
}

// Vòng lặp chính chỉ đọc nút bấm
void loop() {
  byte currentButtons = 0;

  // Kiểm tra 6 nút và thiết lập Bit tương ứng (LOW = nhấn nút)
  if (digitalRead(BTN_PIN_ENROLL) == LOW) currentButtons |= 0x01; // Bit 0
  if (digitalRead(BTN_PIN_EDIT)   == LOW) currentButtons |= 0x02; // Bit 1
  if (digitalRead(BTN_PIN_DELETE) == LOW) currentButtons |= 0x04; // Bit 2
  if (digitalRead(BTN_PIN_LIST)   == LOW) currentButtons |= 0x08; // Bit 3
  if (digitalRead(BTN_PIN_OK)     == LOW) currentButtons |= 0x10; // Bit 4
  if (digitalRead(BTN_PIN_ESCAPE) == LOW) currentButtons |= 0x20; // Bit 5

  // Cập nhật trạng thái
  buttonState = currentButtons;
  
  delay(20); // Delay chống nhiễu
}