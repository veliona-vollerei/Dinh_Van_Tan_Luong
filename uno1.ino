#include <Wire.h> // Thư viện I2C Master
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>
#include <EEPROM.h>

//=================== CẤU HÌNH CHÂN & I2C ===================//
// Vân tay R307: D2 (RX), D3 (TX)
SoftwareSerial fingerSerial(2, 3); 
Adafruit_Fingerprint finger(&fingerSerial);
// Servo gateServo; // ĐÃ XÓA
LiquidCrystal_I2C lcd(0x27, 16, 2);

// I2C Slave Address của Uno 2
#define SLAVE_ADDR 8 

// RFID RC522
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

#define LED_GREEN 6
#define LED_RED 7
#define LED_YELLOW 5
// #define SERVO_PIN 4 // ĐÃ XÓA: Chân Servo không còn

//=================== ADMIN CONFIG ===================//
const byte ADMIN_UID[] = { 0x54, 0x4D, 0xC9, 0x05 }; 
const uint8_t ADMIN_ID = 255; 

//=================== BIẾN I2C VỚI SLAVE ===================//
#define CMD_MP3_SUCCESS 1 
#define CMD_MP3_FAIL 2
#define CMD_MP3_ENROLL_SUCC 3 
#define CMD_MP3_DELETE_SUCC 4
#define CMD_MP3_UPDATE_SUCC 5
#define CMD_MP3_ADMIN 6
#define CMD_MP3_ERROR_MENU 7
#define CMD_MP3_INIT_FAIL 8

// Nút bấm từ Uno 2 (Slave)
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

//===================== I2C COMMANDS (Uno 1 Master -> Uno 2 Slave) =====================//
void sendMP3Command(uint8_t mp3_command_id) {
  // Gửi 1 byte lệnh MP3 tới Slave (Uno 2)
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(mp3_command_id);
  Wire.endTransmission();
}

//===================== LẤY TÍN HIỆU NÚT BẤM (TỪ Uno 2 qua I2C) =====================//
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
// Hàm này chỉ còn điều khiển LED và MP3
void openGate(uint8_t id) {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (id == ADMIN_ID) {
    lcd.print(F("Chao Admin!")); 
    sendMP3Command(CMD_MP3_ADMIN); // MP3 Admin
  } else {
    lcd.print(F("Cham Cong TC")); 
    lcd.setCursor(0, 1);
    lcd.print(F("ID: "));
    lcd.print(id);
    sendMP3Command(CMD_MP3_SUCCESS); // MP3 Success
  }

  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, HIGH);
  
  // Xử lý Mở/Đóng cửa vật lý (nếu có, sẽ dùng Relay hoặc cơ cấu khác không phải Servo)
  // delay(3000) đại diện cho thời gian cửa mở/đóng
  delay(3000); 
  
  digitalWrite(LED_GREEN, LOW);

  currentState = STATE_IDLE;
  displayIdle();
}

//===================== XÁC THỰC ADMIN =====================//
bool authenticateAdminCard(bool isEnteringMenu = true) {
  lcd.clear();
  lcd.print(F("Xac minh Admin"));
  lcd.setCursor(0, 1);
  lcd.print(F("Quet the..."));
  
  if (isEnteringMenu) {
      sendMP3Command(CMD_MP3_ADMIN); 
  }

  unsigned long startTime = millis();
  
  while (millis() - startTime < 10000) { 
    byte buttonState = readButtons();
    if (buttonState & BTN_ESCAPE) {
      if (isEnteringMenu) {
        currentState = STATE_IDLE;
        displayIdle();
      }
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
  
  // --- Xử lý thất bại ---
  lcd.clear();
  lcd.print(F("Xac thuc Admin"));
  lcd.setCursor(0, 1);
  lcd.print(F("that bai/het gio"));
  digitalWrite(LED_RED, HIGH); 
  sendMP3Command(CMD_MP3_FAIL); 
  delay(1500);
  digitalWrite(LED_RED, LOW);
  
  if (isEnteringMenu) {
    currentState = STATE_IDLE;
    displayIdle();
  }
  return false;
}

//===================== ĐĂNG KÝ =====================//
void enrollUser(uint8_t id) {
  // 1. Kiểm tra giới hạn/ID tồn tại
  if (userCount >= 5) { lcd.clear(); lcd.print(F("Da full 5 nguoi")); sendMP3Command(CMD_MP3_ERROR_MENU); delay(1200); return; }
  for (int i = 0; i < userCount; i++) { if (users[i].id == id) { lcd.clear(); lcd.print(F("ID da ton tai!")); sendMP3Command(CMD_MP3_ERROR_MENU); delay(1200); return; } }
  
  // 2. Vân tay
  lcd.clear(); lcd.print(F("DK ID:")); lcd.setCursor(11, 0); lcd.print(id);
  lcd.setCursor(0, 1); lcd.print(F("Dat ngon tay lan 1"));
  
  int p = -1;
  while (p != FINGERPRINT_OK) { p = finger.getImage(); if (readButtons() & BTN_ESCAPE) return; }
  p = finger.image2Tz(1);
  
  int search_res = finger.fingerFastSearch();
  if (search_res == FINGERPRINT_OK) {
    lcd.clear(); lcd.print(F("Van tay ID:")); lcd.print(finger.fingerID);
    lcd.setCursor(0, 1); lcd.print(F("DA DANG KY!")); digitalWrite(LED_RED, HIGH); sendMP3Command(CMD_MP3_FAIL); delay(2500); digitalWrite(LED_RED, LOW);
    return;
  }

  lcd.clear(); lcd.print(F("Nho lay tay ra")); delay(1000);
  p = FINGERPRINT_NOFINGER; while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); if (readButtons() & BTN_ESCAPE) return; }
  
  lcd.clear(); lcd.print(F("Dat lai ngon tay")); delay(1000);
  p = -1; while (p != FINGERPRINT_OK) { p = finger.getImage(); if (readButtons() & BTN_ESCAPE) return; }
  p = finger.image2Tz(2);
  p = finger.createModel();
  if (p != FINGERPRINT_OK) { lcd.clear(); lcd.print(F("Loi tao mau!")); sendMP3Command(CMD_MP3_FAIL); delay(1200); return; }
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) { lcd.clear(); lcd.print(F("Loi luu mau (R307)!")); sendMP3Command(CMD_MP3_FAIL); delay(1500); return; }
  
  // 3. RFID
  lcd.clear(); lcd.print(F("Quet the RFID..."));
  byte uid[4]; bool cardFound = false; unsigned long startScan = millis();
  while (millis() - startScan < 10000) {
    if (readButtons() & BTN_ESCAPE) { finger.deleteModel(id); return; }
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      for (byte i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i]; rfid.PICC_HaltA(); cardFound = true; break;
    }
  }
  if (!cardFound) { lcd.clear(); lcd.print(F("Het gio quet the")); sendMP3Command(CMD_MP3_FAIL); delay(1200); finger.deleteModel(id); return; }
  
  if (memcmp(uid, ADMIN_UID, 4) == 0) { lcd.clear(); lcd.print(F("The nay la ADMIN!")); digitalWrite(LED_RED, HIGH); sendMP3Command(CMD_MP3_FAIL); delay(1200); digitalWrite(LED_RED, LOW); finger.deleteModel(id); return; }
  for (int i = 0; i < userCount; i++) { if (memcmp(users[i].uid, uid, 4) == 0) { lcd.clear(); lcd.print(F("The da su dung!")); digitalWrite(LED_RED, HIGH); sendMP3Command(CMD_MP3_FAIL); delay(1200); digitalWrite(LED_RED, LOW); finger.deleteModel(id); return; } }

  // 4. Xác thực Admin và Lưu
  if (!authenticateAdminCard(false)) { finger.deleteModel(id); return; }
  
  users[userCount].id = id;
  memcpy(users[userCount].uid, uid, 4);
  userCount++;
  saveDataToEEPROM();

  lcd.clear(); lcd.print(F("DK thanh cong!"));
  sendMP3Command(CMD_MP3_ENROLL_SUCC); // MP3 Đăng ký thành công
  delay(1500);
  
  currentState = STATE_IDLE;
  displayIdle();
}

//===================== XOÁ NGƯỜI DÙNG =====================//
void deleteUser(uint8_t id) {
  int index = -1;
  for (int i = 0; i < userCount; i++) { if (users[i].id == id) { index = i; break; } }
  if (index == -1) { lcd.clear(); lcd.print(F("ID ko ton tai")); digitalWrite(LED_RED, HIGH); sendMP3Command(CMD_MP3_FAIL); delay(1000); digitalWrite(LED_RED, LOW); return; }

  lcd.clear(); lcd.print(F("Xac nhan de xoa")); lcd.setCursor(0, 1); lcd.print(F("ID: ")); lcd.print(id);
  delay(1500);

  if (!authenticateAdminCard(false)) { return; }
  
  lcd.clear(); lcd.print(F("Dang xoa...")); delay(500);

  for (int i = index; i < userCount - 1; i++) users[i] = users[i + 1];
  userCount--;

  finger.deleteModel(id);
  saveDataToEEPROM();

  lcd.clear(); lcd.print(F("Da xoa thanh cong"));
  digitalWrite(LED_GREEN, HIGH); 
  sendMP3Command(CMD_MP3_DELETE_SUCC); // MP3 Xóa thành công 
  delay(1500); 
  digitalWrite(LED_GREEN, LOW);
  
  currentState = STATE_IDLE;
  displayIdle();
}

//===================== SỬA THÔNG TIN =====================//
void updateUser(uint8_t id) {
  int index = -1;
  for (int i = 0; i < userCount; i++) if (users[i].id == id) { index = i; break; }
  if (index == -1) { lcd.clear(); lcd.print(F("ID ko ton tai")); sendMP3Command(CMD_MP3_FAIL); delay(1000); return; }

  // 1. Sửa vân tay
  lcd.clear(); lcd.print(F("Sua Van Tay")); lcd.setCursor(0, 1); lcd.print(F("ID: ")); lcd.print(id); delay(1000);
  int p = -1;
  lcd.clear(); lcd.print(F("Dat ngon tay..."));
  while (p != FINGERPRINT_OK) { p = finger.getImage(); if (readButtons() & BTN_ESCAPE) return; }
  p = finger.image2Tz(1);
  
  lcd.clear(); lcd.print(F("Nho lay tay ra")); delay(1000);
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); if (readButtons() & BTN_ESCAPE) return; }
  
  lcd.clear(); lcd.print(F("Dat lai ngon tay")); delay(1000);
  while (p != FINGERPRINT_OK) { p = finger.getImage(); if (readButtons() & BTN_ESCAPE) return; }
  p = finger.image2Tz(2);
  p = finger.createModel();

  if (p != FINGERPRINT_OK) { lcd.clear(); lcd.print(F("Loi van tay!")); sendMP3Command(CMD_MP3_FAIL); delay(1200); return; }
  
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) { lcd.clear(); lcd.print(F("Loi luu mau (R307)!")); sendMP3Command(CMD_MP3_FAIL); delay(1500); return; }
  
  // 2. Sửa RFID
  lcd.clear(); lcd.print(F("Sua the RFID..."));
  byte newUid[4]; bool cardFound = false; unsigned long startScan = millis();
  while (millis() - startScan < 10000) {
    if (readButtons() & BTN_ESCAPE) return; 
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      for (byte i = 0; i < 4; i++) newUid[i] = rfid.uid.uidByte[i]; rfid.PICC_HaltA(); cardFound = true; break;
    }
  }

  if (!cardFound) { lcd.clear(); lcd.print(F("Het gio quet the")); sendMP3Command(CMD_MP3_FAIL); delay(1200); return; }
  if (memcmp(newUid, ADMIN_UID, 4) == 0) { lcd.clear(); lcd.print(F("The nay la ADMIN!")); digitalWrite(LED_RED, HIGH); sendMP3Command(CMD_MP3_FAIL); delay(1200); digitalWrite(LED_RED, LOW); return; }
  for (int i = 0; i < userCount; i++) { if (i == index) continue; if (memcmp(users[i].uid, newUid, 4) == 0) { lcd.clear(); lcd.print(F("The da su dung!")); digitalWrite(LED_RED, HIGH); sendMP3Command(CMD_MP3_FAIL); delay(1200); digitalWrite(LED_RED, LOW); return; } }

  // 3. Xác thực Admin và Lưu
  if (!authenticateAdminCard(false)) { return; }

  memcpy(users[index].uid, newUid, 4);
  saveDataToEEPROM();

  lcd.clear(); lcd.print(F("Cap nhat OK!"));
  sendMP3Command(CMD_MP3_UPDATE_SUCC); // MP3 Cập nhật thành công 
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
    
    if (memcmp(uid, ADMIN_UID, 4) == 0) {
        openGate(ADMIN_ID); 
        return;
    }

    int index = -1;
    for (int i = 0; i < userCount; i++)
      if (memcmp(users[i].uid, uid, 4) == 0) { index = i; break; }

    if (index != -1) {
      openGate(users[index].id);
    } else {
      lcd.clear(); lcd.print(F("The chua DK!"));
      digitalWrite(LED_RED, HIGH); 
      sendMP3Command(CMD_MP3_FAIL); 
      delay(1500);
      digitalWrite(LED_RED, LOW);
      displayIdle();
    }
    return;
  }

  // --- VÂN TAY ---
  int p = finger.getImage();
  if (p != FINGERPRINT_OK && p != FINGERPRINT_NOFINGER && p != FINGERPRINT_PACKETRECIEVEERR) {
        return; 
  }
  
  if (p == FINGERPRINT_OK) {
    finger.image2Tz();
    int res = finger.fingerFastSearch();
    
    bool foundInEEPROM = false;
    uint8_t matchedID = 0;
    
    if (res == FINGERPRINT_OK) {
        uint8_t finger_id_8bit = (uint8_t)finger.fingerID; 
        
        for (int i = 0; i < userCount; i++) {
          if (finger_id_8bit == users[i].id) {
            foundInEEPROM = true;
            matchedID = users[i].id;
            break; 
          }
        }
    }
    
    if (foundInEEPROM) {
      // CHẤM CÔNG THÀNH CÔNG
      openGate(matchedID); 
      return; 
    } 
    else {
      // CHẤM CÔNG THẤT BẠI
      lcd.clear(); 
      if (res == FINGERPRINT_OK) {
          lcd.print(F("ID Van Tay: "));
          lcd.print(finger.fingerID);
          lcd.setCursor(0, 1);
          lcd.print(F("Chua DK/Loi ID"));
      } else {
          lcd.print(F("Sai van tay!"));
      }

      digitalWrite(LED_RED, HIGH); 
      sendMP3Command(CMD_MP3_FAIL); 
      digitalWrite(LED_YELLOW, LOW);
      delay(1500);
      digitalWrite(LED_RED, LOW);
      displayIdle();
    }
  }
}

//===================== SETUP =====================//
void setup() {
  // Khởi tạo I2C cho Master (Không cần địa chỉ)
  Wire.begin(); 
  
  // Serial Debug (Tùy chọn)
  Serial.begin(9600);

  fingerSerial.begin(57600); 
  SPI.begin();
  rfid.PCD_Init();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  lcd.begin(16, 2);
  lcd.backlight();

  // gateServo.attach(SERVO_PIN); // ĐÃ XÓA
  // gateServo.write(0); // ĐÃ XÓA

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
    sendMP3Command(CMD_MP3_INIT_FAIL); // Thông báo lỗi khởi tạo
    while (1);
  }
  delay(1000);

  loadDataFromEEPROM();
  displayIdle();
}

//===================== VÒNG LẶP CHÍNH =====================//
void loop() {
  if (currentState == STATE_IDLE) {
    checkAccess();
  }

  byte buttonState = readButtons();
  byte newButtons = buttonState & ~lastButtonState;

  if (newButtons & BTN_ESCAPE) {
    if (currentState != STATE_IDLE) {
      currentState = STATE_IDLE;
      displayIdle();
    }
    lastButtonState = buttonState;
    delay(100);
    return;
  }

  switch (currentState) {
    
    case STATE_IDLE:
      if (newButtons & BTN_ENROLL) {
        selectedID = 1; currentState = STATE_ENROLL; displayMenu("1. Dang Ky");
      }
      else if (newButtons & BTN_EDIT) {
        selectedID = 1; currentState = STATE_EDIT; displayMenu("2. Sua Thong Tin");
      }
      else if (newButtons & BTN_DELETE) {
        selectedID = 1; currentState = STATE_DELETE; displayMenu("3. Xoa Nguoi Dung");
      }
      else if (newButtons & BTN_LIST) {
        currentState = STATE_LIST; listUsersOnLCD();
      }
      break;

    case STATE_ENROLL:
    case STATE_EDIT:
    case STATE_DELETE:
      if (buttonState & BTN_EDIT) { 
        selectedID++; if (selectedID > 127) selectedID = 1;
        displayMenu(currentState == STATE_ENROLL ? "1. Dang Ky" : (currentState == STATE_EDIT ? "2. Sua TT" : "3. Xoa ND")); 
      }
      else if (buttonState & BTN_DELETE) {
        selectedID--; if (selectedID < 1) selectedID = 127;
        displayMenu(currentState == STATE_ENROLL ? "1. Dang Ky" : (currentState == STATE_EDIT ? "2. Sua TT" : "3. Xoa ND")); 
      }
      
      if (newButtons & BTN_OK) {
        if (currentState == STATE_ENROLL) enrollUser(selectedID);
        if (currentState == STATE_EDIT) updateUser(selectedID);
        if (currentState == STATE_DELETE) deleteUser(selectedID);
        
        if (currentState != STATE_IDLE) {
            displayMenu(currentState == STATE_ENROLL ? "1. Dang Ky" : (currentState == STATE_EDIT ? "2. Sua TT" : "3. Xoa ND"));
        }
      }
      break;

    case STATE_LIST:
      break;
  }

  lastButtonState = buttonState;
  delay(50);
}
