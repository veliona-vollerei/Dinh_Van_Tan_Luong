#include <Wire.h> // Thư viện I2C Slave
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

//=================== CẤU HÌNH I2C SLAVE & CHÂN NÚT BẤM ===================//
#define SLAVE_ADDR 8 

// Cấu hình chân nút bấm (Input Pullup)
#define BTN_PIN_ENROLL 4
#define BTN_PIN_EDIT  5
#define BTN_PIN_DELETE 6
#define BTN_PIN_LIST  7
#define BTN_PIN_OK  8
#define BTN_PIN_ESCAPE 9 

// Nút bấm mã hóa (Gửi về Uno 1)
#define BTN_ENROLL_CODE  0x01
#define BTN_EDIT_CODE    0x02
#define BTN_DELETE_CODE  0x04
#define BTN_LIST_CODE    0x08
#define BTN_OK_CODE      0x10
#define BTN_ESCAPE_CODE  0x20

//=================== DFPLAYER CONFIG ===================//
// D13 (TX) nối với RX của DFPlayer, D12 (RX) nối với TX của DFPlayer
#define DFPLAYER_TX_PIN 13 
#define DFPLAYER_RX_PIN 12
SoftwareSerial mySerial(DFPLAYER_TX_PIN, DFPLAYER_RX_PIN); 
DFRobotDFPlayerMini myDFPlayer;

//=================== BIẾN TRẠNG THÁI & LỆNH MP3 ===================//
// Lệnh MP3 nhận từ Uno 1 (Master) - Cần đảm bảo các file MP3 tương ứng tồn tại
#define CMD_MP3_SUCCESS 1     // File 0001.mp3
#define CMD_MP3_FAIL 2        // File 0002.mp3
#define CMD_MP3_ENROLL_SUCC 3 // File 0003.mp3
#define CMD_MP3_DELETE_SUCC 4 // File 0004.mp3
#define CMD_MP3_UPDATE_SUCC 5 // File 0005.mp3
#define CMD_MP3_ADMIN 6       // File 0006.mp3
#define CMD_MP3_ERROR_MENU 7
#define CMD_MP3_INIT_FAIL 8

volatile uint8_t mp3Command = 0;
volatile byte buttonState = 0; 

//===================== DFPLAYER HELPERS =====================//
void playMp3(uint8_t id) {
    // Chỉ phát nếu ID nằm trong phạm vi file MP3 bạn có (1-6)
    if (id >= 1 && id <= 8) { // Mở rộng nếu bạn có nhiều hơn 6 file
        myDFPlayer.play(id);
        // Không cần Serial.print trong code chính thức để giữ tốc độ
    }
}

//===================== I2C HANDLERS (SLAVE) =====================//
void receiveEvent(int howMany) {
  if (howMany >= 1) {
    mp3Command = Wire.read(); // Nhận 1 byte lệnh MP3
  }
}

void requestEvent() {
  Wire.write(buttonState); // Gửi 1 byte trạng thái nút bấm
}

//===================== CHỨC NĂNG ĐỌC NÚT BẤM =====================//
void readButtonState() {
  byte currentButtons = 0;
  // Đọc trạng thái (LOW = bấm) và lưu vào byte
  if (digitalRead(BTN_PIN_ENROLL) == LOW) currentButtons |= BTN_ENROLL_CODE; 
  if (digitalRead(BTN_PIN_EDIT)  == LOW) currentButtons |= BTN_EDIT_CODE; 
  if (digitalRead(BTN_PIN_DELETE) == LOW) currentButtons |= BTN_DELETE_CODE; 
  if (digitalRead(BTN_PIN_LIST)  == LOW) currentButtons |= BTN_LIST_CODE; 
  if (digitalRead(BTN_PIN_OK)  == LOW) currentButtons |= BTN_OK_CODE; 
  if (digitalRead(BTN_PIN_ESCAPE) == LOW) currentButtons |= BTN_ESCAPE_CODE; 
  
  buttonState = currentButtons;
}

//===================== SETUP =====================//
void setup() {
  Serial.begin(9600); 
  
  // Khởi tạo các chân nút bấm (INPUT_PULLUP)
  pinMode(BTN_PIN_ENROLL, INPUT_PULLUP);
  pinMode(BTN_PIN_EDIT,  INPUT_PULLUP);
  pinMode(BTN_PIN_DELETE, INPUT_PULLUP);
  pinMode(BTN_PIN_LIST,  INPUT_PULLUP);
  pinMode(BTN_PIN_OK,  INPUT_PULLUP);
  pinMode(BTN_PIN_ESCAPE, INPUT_PULLUP); 

  // Khởi tạo DFPlayer
  mySerial.begin(9600); 
  
  // Kiểm tra kết nối DFPlayer
  if (!myDFPlayer.begin(mySerial, true, false)) { // true: wait for ACK, false: no busy pin
    Serial.println(F("ERROR: DFPlayer failed to init."));
    // Không nên dừng ở đây, vì Slave vẫn cần xử lý nút bấm
  } else {
    Serial.println(F("DFPlayer Mini ready."));
    myDFPlayer.volume(5);
    // Bắt đầu phát MP3 khi khởi động (Tùy chọn)
    // myDFPlayer.play(1); 
  }

  // Khởi tạo I2C Slave SAU khi DFPlayer đã ổn định
  Wire.begin(SLAVE_ADDR); 
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

//===================== LOOP =====================//
void loop() {
  // 1. Đọc trạng thái nút bấm
  readButtonState();
  
  // 2. Xử lý lệnh MP3 nhận được qua I2C (trong receiveEvent)
  if (mp3Command != 0) {
    playMp3(mp3Command);
    mp3Command = 0; // Xóa lệnh sau khi thực hiện
  }
  
  // Giao tiếp I2C nhanh hơn, nên giảm delay
  delay(20);
}

