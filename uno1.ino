#include <Wire.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>
#include <EEPROM.h>

//=================== CẤU HÌNH CHÂN ===================//
SoftwareSerial fingerSerial(2, 3); // RX, TX của R307
Adafruit_Fingerprint finger(&fingerSerial);
Servo gateServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

#define LED_GREEN 6
#define LED_RED 7
#define LED_YELLOW 5
#define BUZZER 8
#define SERVO_PIN 4

//=================== ADMIN CONFIG (THẺ MẶC ĐỊNH) ===================//
// UID của thẻ Admin: 54 4D C9 05
const byte ADMIN_UID[] = { 0x54, 0x4D, 0xC9, 0x05 }; 
const uint8_t ADMIN_ID = 255; // ID đặc biệt

//=================== I2C & NÚT BẤM ===================//
#define SLAVE_ADDR 8 

// Định nghĩa các bit tương ứng với nút bấm từ Slave
#define BTN_ENROLL  0x01
#define BTN_EDIT    0x02
#define BTN_DELETE  0x04
#define BTN_LIST    0x08
#define BTN_OK      0x10
#define BTN_ESCAPE  0x20

//=================== DỮ LIỆU NGƯỜI DÙNG ===================//
struct UserData {
  uint8_t id;
  byte uid[4];
};

UserData users[5];
int userCount = 0;

//=================== BIẾN TRẠNG THÁI MENU ===================//
enum State {
  STATE_IDLE,
  STATE_ENROLL,
  STATE_EDIT,
  STATE_DELETE,
  STATE_LIST
};

State currentState = STATE_IDLE;
uint8_t selectedID = 1;
byte lastButtonState = 0;

//===================== EEPROM HELPERS =====================//
void saveDataToEEPROM() {
  EEPROM.put(0, userCount);
  int addr = sizeof(userCount);
  for (int i = 0; i < userCount; i++) {
    EEPROM.put(addr, users[i]);
    addr += sizeof(UserData);
  }
}

void loadDataFromEEPROM() {
  EEPROM.get(0, userCount);
  if (userCount < 0 || userCount > 5) userCount = 0;
  int addr = sizeof(userCount);
  for (int i = 0; i < userCount; i++) {
    EEPROM.get(addr, users[i]);
    addr += sizeof(UserData);
  }
}

//===================== LẤY TÍN HIỆU NÚT BẤM =====================//
byte readButtons() {
  Wire.requestFrom(SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

//===================== HIỂN THỊ LCD =====================//
void displayIdle() {
  lcd.clear();
  lcd.print(F("San sang quet..."));
  digitalWrite(LED_YELLOW, HIGH);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZER); 
}

void displayMenu(const char* title) {
  lcd.clear();
  lcd.print(title);
  lcd.setCursor(0, 1);
  lcd.print(F("Chon ID: "));
  lcd.print(selectedID);
  lcd.print(F("   "));
}

//===================== MỞ CỬA =====================//
void openGate(uint8_t id) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (id == ADMIN_ID) {
    lcd.print(F("Chao Admin!")); 
  } else {
    lcd.print(F("Cham Cong Thanh Cong"));
    lcd.setCursor(0, 1);
    lcd.print(F("ID: "));
    lcd.print(id);
  }

  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, HIGH);
  tone(BUZZER, 1000);
  gateServo.write(90);
  delay(3000);
  noTone(BUZZER);
  digitalWrite(LED_GREEN, LOW);
  gateServo.write(0);

  currentState = STATE_IDLE;
  displayIdle();
}

//===================== XÁC THỰC ADMIN (ĐÃ SỬA THÊM KIỂM TRA THOÁT) =====================//
bool authenticateAdminCard() {
  lcd.clear();
  lcd.print(F("Xac minh Admin"));
  lcd.setCursor(0, 1);
  lcd.print(F("Quet the..."));
  
  unsigned long startTime = millis();
  
  while (millis() - startTime < 10000) { 
    // Kiểm tra nút thoát Slave trong vòng lặp chặn
    byte buttonState = readButtons();
    if (buttonState & BTN_ESCAPE) {
      // Chuyển về IDLE và thoát khỏi hàm
      currentState = STATE_IDLE;
      displayIdle();
      return false; 
    }
    
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      byte uid[4];
      for (byte i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];
      rfid.PICC_HaltA();
      
      if (memcmp(uid, ADMIN_UID, 4) == 0) {
        lcd.clear();
        lcd.print(F("Admin da xac thuc"));
        digitalWrite(LED_GREEN, HIGH);
        delay(1000);
        digitalWrite(LED_GREEN, LOW);
        return true;
      }
    }
    delay(50);
  }
  
  lcd.clear();
  lcd.print(F("Xac thuc Admin"));
  lcd.setCursor(0, 1);
  lcd.print(F("that bai/het gio"));
  digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
  delay(1500);
  noTone(BUZZER); digitalWrite(LED_RED, LOW);
  
  return false;
}

//===================== ĐĂNG KÝ (ĐÃ THÊM KIỂM TRA TRÙNG VÂN TAY & THOÁT) =====================//
void enrollUser(uint8_t id) {
  // 1. Kiểm tra giới hạn người dùng EEPROM
  if (userCount >= 5) {
    lcd.clear(); lcd.print(F("Da full 5 nguoi"));
    delay(1200);
    return;
  }

  // 2. Kiểm tra ID đã tồn tại trong EEPROM
  for (int i = 0; i < userCount; i++) {
    if (users[i].id == id) {
      lcd.clear(); lcd.print(F("ID da ton tai!"));
      delay(1200);
      return;
    }
  }
  
  int p = -1;
  lcd.clear();
  lcd.print(F("Dang ky ID:"));
  lcd.setCursor(11, 0);
  lcd.print(id);

  // --- Bước 1: Vân tay - Đặt lần 1 ---
  lcd.setCursor(0, 1);
  lcd.print(F("Dat ngon tay lan 1"));
  
  // Chờ cho đến khi lấy được ảnh
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
  }

  // Chuyển ảnh thành mẫu tạm thời 1
  p = finger.image2Tz(1);
  
  // --- KIỂM TRA TRÙNG VÂN TAY ---
  int search_res = finger.fingerFastSearch();
  if (search_res == FINGERPRINT_OK) {
    // Vân tay đã tồn tại trong module R307
    lcd.clear();
    lcd.print(F("Van tay ID:"));
    lcd.print(finger.fingerID);
    lcd.setCursor(0, 1);
    lcd.print(F("DA DANG KY!"));
    digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
    delay(2500);
    noTone(BUZZER); digitalWrite(LED_RED, LOW);
    return; 
  }
  // --- KẾT THÚC BƯỚC KIỂM TRA ---

  // --- Bước 1: Vân tay - Đặt lần 2 (Tiếp tục nếu chưa đăng ký) ---
  lcd.clear(); lcd.print(F("Nho lay tay ra"));
  delay(1000);
  p = FINGERPRINT_NOFINGER; 
  // Vòng lặp chờ lấy tay ra
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
  }

  lcd.clear(); 
  lcd.print(F("Dat lai ngon tay"));
  delay(1000);
  p = -1; 
  // Vòng lặp chờ đặt lại
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
  }

  p = finger.image2Tz(2);
  p = finger.createModel();

  if (p != FINGERPRINT_OK) {
    lcd.clear(); lcd.print(F("Loi tao mau!"));
    delay(1200);
    return;
  }
  
  // LƯU MẪU VÀO BỘ NHỚ FLASH (Đã sửa lỗi thiếu storeModel)
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    lcd.clear(); lcd.print(F("Loi luu mau (R307)!"));
    delay(1500);
    return;
  }
  
  // --- Bước 2: RFID ---
  lcd.clear(); lcd.print(F("Quet the RFID..."));
  byte uid[4];
  bool cardFound = false;
  unsigned long startScan = millis();
  // Vòng lặp chờ RFID
  while (millis() - startScan < 10000) {
    if (readButtons() & BTN_ESCAPE) { 
      finger.deleteModel(id); // Xóa vân tay đã lưu tạm thời
      return; // Thoát ngay
    }
    
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      for (byte i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];
      rfid.PICC_HaltA();
      cardFound = true;
      break;
    }
  }

  if (!cardFound) {
    lcd.clear(); lcd.print(F("Het gio quet the"));
    delay(1200);
    finger.deleteModel(id);
    return;
  }
  
  // Kiểm tra trùng thẻ ADMIN
  if (memcmp(uid, ADMIN_UID, 4) == 0) {
    lcd.clear(); lcd.print(F("The nay la ADMIN!"));
    digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
    digitalWrite(LED_YELLOW, LOW);
    delay(1200);
    noTone(BUZZER); digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, HIGH);
    finger.deleteModel(id);
    return;
  }

  // Kiểm tra trùng thẻ người dùng
  for (int i = 0; i < userCount; i++) {
    if (memcmp(users[i].uid, uid, 4) == 0) {
      lcd.clear(); lcd.print(F("The da su dung!"));
      digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
      delay(1200);
      noTone(BUZZER); digitalWrite(LED_RED, LOW);
      finger.deleteModel(id);
      return;
    }
  }

  // --- Bước 3: XÁC THỰC ADMIN & LƯU ---
  if (!authenticateAdminCard()) { 
    finger.deleteModel(id); 
    return; 
  }
  
  // Lưu vào EEPROM
  users[userCount].id = id;
  memcpy(users[userCount].uid, uid, 4);
  userCount++;
  saveDataToEEPROM();

  lcd.clear(); lcd.print(F("DK thanh cong!"));
  delay(800);
  openGate(id);
}

//===================== XOÁ NGƯỜI DÙNG =====================//
void deleteUser(uint8_t id) {
  int index = -1;
  for (int i = 0; i < userCount; i++) {
    if (users[i].id == id) { index = i; break; }
  }

  if (index == -1) {
    lcd.clear(); lcd.print(F("ID ko ton tai"));
    digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
    delay(1000);
    noTone(BUZZER); digitalWrite(LED_RED, LOW);
    return;
  }

  lcd.clear(); lcd.print(F("Xac nhan de xoa"));
  lcd.setCursor(0, 1); lcd.print(F("ID: ")); lcd.print(id);
  delay(1500);

  // --- Xác thực Admin trước khi xóa ---
  if (!authenticateAdminCard()) {
    return; 
  }
  
  lcd.clear(); lcd.print(F("Dang xoa..."));
  delay(500);

  for (int i = index; i < userCount - 1; i++)
    users[i] = users[i + 1];
  userCount--;

  finger.deleteModel(id);
  saveDataToEEPROM();

  lcd.clear(); lcd.print(F("Da xoa thanh cong"));
  digitalWrite(LED_GREEN, HIGH); tone(BUZZER, 1000);
  digitalWrite(LED_YELLOW, LOW);
  delay(1000);
  noTone(BUZZER); digitalWrite(LED_GREEN, LOW);
  
  currentState = STATE_IDLE;
  displayIdle();
}

//===================== SỬA THÔNG TIN (ĐÃ SỬA THÊM KIỂM TRA THOÁT) =====================//
void updateUser(uint8_t id) {
  int index = -1;
  for (int i = 0; i < userCount; i++)
    if (users[i].id == id) { index = i; break; }

  if (index == -1) {
    lcd.clear(); lcd.print(F("ID ko ton tai"));
    delay(1000);
    return;
  }

  // --- Bước 1: Sửa vân tay ---
  lcd.clear(); lcd.print(F("Sua Van Tay"));
  lcd.setCursor(0, 1); lcd.print(F("ID: ")); lcd.print(id);
  delay(1000);
  
  int p = -1;
  lcd.clear();
  lcd.print(F("Dat ngon tay..."));
  // Vòng lặp chờ vân tay lần 1
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
  }
  p = finger.image2Tz(1);
  
  lcd.clear(); lcd.print(F("Nho lay tay ra"));
  delay(1000);
  // Vòng lặp chờ lấy tay ra
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
  }
  
  lcd.clear(); lcd.print(F("Dat lai ngon tay"));
  delay(1000);
  // Vòng lặp chờ đặt lại
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
  }
  p = finger.image2Tz(2);
  p = finger.createModel();

  if (p != FINGERPRINT_OK) {
    lcd.clear(); lcd.print(F("Loi van tay!"));
    delay(1200);
    return;
  }
  
  // Ghi đè vân tay mới lên ID cũ trong module R307
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    lcd.clear(); lcd.print(F("Loi luu mau (R307)!"));
    delay(1500);
    return;
  }
  // ------------------------------------------------------


  // --- Bước 2: Sửa RFID ---
  lcd.clear(); lcd.print(F("Sua the RFID..."));
  byte newUid[4];
  bool cardFound = false;
  unsigned long startScan = millis();
  // Vòng lặp chờ RFID
  while (millis() - startScan < 10000) {
    if (readButtons() & BTN_ESCAPE) return; // Thoát ngay
    
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      for (byte i = 0; i < 4; i++) newUid[i] = rfid.uid.uidByte[i];
      rfid.PICC_HaltA();
      cardFound = true;
      break;
    }
  }

  if (!cardFound) {
    lcd.clear(); lcd.print(F("Het gio quet the"));
    delay(1200);
    return;
  }
  
  if (memcmp(newUid, ADMIN_UID, 4) == 0) {
    lcd.clear(); lcd.print(F("The nay la ADMIN!"));
    digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
    delay(1200);
    noTone(BUZZER); digitalWrite(LED_RED, LOW);
    return;
  }

  for (int i = 0; i < userCount; i++) {
    if (i == index) continue;
    if (memcmp(users[i].uid, newUid, 4) == 0) {
      lcd.clear(); lcd.print(F("The da su dung!"));
      digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
      delay(1200);
      noTone(BUZZER); digitalWrite(LED_RED, LOW);
      return;
    }
  }

  // --- Bước 3: XÁC THỰC ADMIN & LƯU ---
  if (!authenticateAdminCard()) {
    return;
  }

  memcpy(users[index].uid, newUid, 4);
  saveDataToEEPROM();

  lcd.clear(); lcd.print(F("Cap nhat OK!"));
  delay(1500);

  currentState = STATE_IDLE;
  displayIdle();
}

//===================== DANH SÁCH NGƯỜI DÙNG =====================//
void listUsersOnLCD() {
  lcd.clear();
  lcd.print(F("Danh Sach ID:"));
  lcd.setCursor(0, 1);
  if (userCount == 0) {
    lcd.print(F("(Trong)"));
  } else {
    String idList = "";
    for (int i = 0; i < userCount; i++) {
      idList += String(users[i].id);
      if (i < userCount - 1) idList += ",";
    }
    lcd.print(idList);
  }
}

//===================== KIỂM TRA TRUY CẬP =====================//
void checkAccess() {
  if (currentState != STATE_IDLE) return;

  // --- RFID ---
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    byte uid[4];
    for (byte i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];
    rfid.PICC_HaltA();
    
    // KIỂM TRA THẺ ADMIN 
    if (memcmp(uid, ADMIN_UID, 4) == 0) {
        openGate(ADMIN_ID); 
        return;
    }

    // KIỂM TRA NGƯỜI DÙNG BÌNH THƯỜNG
    int index = -1;
    for (int i = 0; i < userCount; i++)
      if (memcmp(users[i].uid, uid, 4) == 0) { index = i; break; }

    if (index != -1) {
      openGate(users[index].id);
    } else {
      lcd.clear(); lcd.print(F("The chua DK!"));
      digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
      delay(1500);
      noTone(BUZZER); digitalWrite(LED_RED, LOW);
      displayIdle();
    }
    return;
  }

  // --- VÂN TAY ---
  int p = finger.getImage();
  if (p == FINGERPRINT_OK) {
    finger.image2Tz();
    int res = finger.fingerFastSearch();
    
    if (res == FINGERPRINT_OK) {
      for (int i = 0; i < userCount; i++) {
        if (finger.fingerID == users[i].id) {
          openGate(users[i].id); 
          return; // Dừng hàm ngay lập tức sau khi mở cửa/báo xanh
        }
      }
    }
    
    // Phần code báo lỗi chỉ chạy khi xác thực vân tay thất bại
    lcd.clear(); 
    if (res == FINGERPRINT_OK) {
        // Vân tay có trên module R307, nhưng không khớp ID trong EEPROM (Đăng ký lỗi)
        lcd.print(F("ID van tay khong"));
        lcd.setCursor(0, 1); lcd.print(F("hop le!"));
    } else {
        // Vân tay hoàn toàn sai
        lcd.print(F("Sai van tay!"));
    }

    digitalWrite(LED_RED, HIGH); tone(BUZZER, 400);
    digitalWrite(LED_YELLOW, LOW);
    delay(1500);
    noTone(BUZZER); digitalWrite(LED_RED, LOW);
    displayIdle();
  }
}

//===================== SETUP =====================//
void setup() {
  finger.begin(57600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  Wire.begin();
  lcd.begin(16, 2);
  lcd.backlight();

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  lcd.setCursor(0, 0);
  lcd.print(F("He thong mo cua"));
  lcd.setCursor(0, 1);
  lcd.print(F("Dang khoi dong..."));
  delay(1500);
  lcd.clear();

  if (finger.verifyPassword()) {
    lcd.setCursor(0, 0);
    lcd.print(F("Cam bien OK"));
  } else {
    lcd.setCursor(0, 0);
    lcd.print(F("Loi cam bien!"));
    while (1);
  }
  delay(1000);

  loadDataFromEEPROM();
  displayIdle();
}

//===================== VÒNG LẶP CHÍNH (STATE MACHINE) =====================//
void loop() {
  // 1. Chỉ kiểm tra truy cập (quét) khi ở trạng thái IDLE
  if (currentState == STATE_IDLE) {
    checkAccess();
  }

  // 2. Đọc trạng thái các nút bấm từ Slave
  byte buttonState = readButtons();
  byte newButtons = buttonState & ~lastButtonState;

  // 3. XỬ LÝ NÚT THOÁT (ƯU TIÊN CAO NHẤT)
  if (newButtons & BTN_ESCAPE) {
    if (currentState != STATE_IDLE) {
      currentState = STATE_IDLE;
      displayIdle();
      noTone(BUZZER); 
    }
    lastButtonState = buttonState;
    delay(100);
    return;
  }

  // 4. Xử lý logic dựa trên trạng thái hiện tại
  switch (currentState) {
    
    case STATE_IDLE:
      if (newButtons & BTN_ENROLL) {
        selectedID = 1;
        currentState = STATE_ENROLL;
        displayMenu("1. Dang Ky");
      }
      else if (newButtons & BTN_EDIT) {
        selectedID = 1;
        currentState = STATE_EDIT;
        displayMenu("2. Sua Thong Tin");
      }
      else if (newButtons & BTN_DELETE) {
        selectedID = 1;
        currentState = STATE_DELETE;
        displayMenu("3. Xoa Nguoi Dung");
      }
      else if (newButtons & BTN_LIST) {
        currentState = STATE_LIST;
        listUsersOnLCD();
      }
      break;

    case STATE_ENROLL:
    case STATE_EDIT:
    case STATE_DELETE:
      // Nút TĂNG (Sửa)
      if (buttonState & BTN_EDIT) { 
        selectedID++;
        if (selectedID > 127) selectedID = 1;
        displayMenu(currentState == STATE_ENROLL ? "1. Dang Ky" : (currentState == STATE_EDIT ? "2. Sua Thong Tin" : "3. Xoa Nguoi Dung"));
        delay(50);
      }
      // Nút GIẢM (Xóa)
      else if (buttonState & BTN_DELETE) {
        selectedID--;
        if (selectedID < 1) selectedID = 127;
        displayMenu(currentState == STATE_ENROLL ? "1. Dang Ky" : (currentState == STATE_EDIT ? "2. Sua Thong Tin" : "3. Xoa Nguoi Dung"));
        delay(50);
      }
      
      // Nút OK (Xác nhận) - Kích hoạt quy trình ĐK/Sửa/Xóa
      if (newButtons & BTN_OK) {
        if (currentState == STATE_ENROLL) enrollUser(selectedID);
        if (currentState == STATE_EDIT) updateUser(selectedID);
        if (currentState == STATE_DELETE) deleteUser(selectedID);
        
        // Nếu các hàm trên không tự chuyển về IDLE (do lỗi/hủy)
        if (currentState != STATE_IDLE) {
            displayMenu(currentState == STATE_ENROLL ? "1. Dang Ky" : (currentState == STATE_EDIT ? "2. Sua Thong Tin" : "3. Xoa Nguoi Dung"));
        }
      }
      break;

    case STATE_LIST:
      break;
  }

  // 5. Lưu trạng thái nút bấm và delay
  lastButtonState = buttonState;
  delay(50);
}