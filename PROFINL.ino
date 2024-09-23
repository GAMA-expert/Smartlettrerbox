#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>

// Set the LCD I2C address to 0x27 or 0x3F (depending on your module), 16 columns and 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

void connectToWiFi();
void verifyLetter(String letterCode, String boxId);

const char* ssid = "vivo 1935";     // Your WiFi SSID
const char* password = "12345678";  // Your WiFi Password

#define RST_PIN 2  // Reset pin for MFRC522
#define SS_PIN 5   // Slave Select pin for MFRC522

MFRC522 rfid(SS_PIN, RST_PIN);  // Create an MFRC522 instance

byte allowedUID[4] = { 0x93, 0xF5, 0x8D, 0xFB };  // Allowed RFID UID

Servo myServo;
const int servoPin = 4;        // GPIO pin connected to the servo
const int closedPosition = 0;  // Closed (0 degrees)
const int openPosition = 75;   // Open (120 degrees)
int State = 0;                 // State variable to track operation
char Regcode[5];               // Increased array size to store 4 digits + null terminator
int Regpostnum = 0;            // To store the input number of reg posts
String letterCode = "";        // String to hold the 4-digit code

String boxId = "B0001";    // box id defined
bool servoLocked = false;  // Flag to lock the servo motor

// Keypad setup
const byte ROWS = 4;  // Four rows
const byte COLS = 4;  // Four columns
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 13, 12, 14, 27 };  // Connect to the row pins of the keypad
byte colPins[COLS] = { 26, 25, 33, 32 };  // Connect to the column pins of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);

  // Initialize the servo
  myServo.attach(servoPin);
  myServo.write(closedPosition);  // Initialize servo in the closed position
  Serial.println("Servo initialized");

  // Initialize the LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("HELLO");
  lcd.setCursor(0, 1);
  lcd.print("Scan Your ID");

  SPI.begin();      // Initialize SPI bus
  rfid.PCD_Init();  // Initialize MFRC522

  connectToWiFi();  // Connect to WiFi at the beginning

  Serial.println("Place your RFID card near the reader...");
}

void loop() {
  char key = keypad.getKey();  // Continuously get the key
                               // Look for new cards continuously
  if (State == 0 && rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.println("Card detected");

    // Print the detected RFID tag UID
    Serial.print("Card UID:");
    String rfidCode = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      rfidCode += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
    }
    Serial.println(rfidCode);

    // Check if the scanned UID matches the allowed UID
    if (rfid.uid.size == 4) {  // Ensure that UID length matches
      bool accessGranted = true;
      for (byte i = 0; i < 4; i++) {
        if (rfid.uid.uidByte[i] != allowedUID[i]) {
          accessGranted = false;
          break;
        }
      }

      if (accessGranted) {
        Serial.println("Access Granted!");
        lcd.clear();
        lcd.setCursor(0, 0);  // Set the cursor
        lcd.print("Access Granted");
        delay(1000);

        // Automatically prompt for number of registered posts
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No of Reg posts");
        lcd.setCursor(0, 1);
        lcd.print("Normalpost= D");

        while (true) {  // Collect number of posts
          key = keypad.getKey();

          // Handle D key to manually open without input
          if (key == 'D') {               // Ensure state is in the initial state
            myServo.write(openPosition);  // Open the servo
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("INPUT LETTERS");
            lcd.setCursor(0, 1);
            lcd.print("Press '*' to close");
            delay(1000);
            State = 0;  // Update state to indicate it's waiting for further action
          }


          // Handle closing the servo with '*' after pressing 'D'
          if (key == '*') {
            myServo.write(closedPosition);  // Close the servo
            lcd.clear();
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Any Reg Posts");
            delay(1000);
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("NO = 0");
            delay(1000);
          }

          if (key == '0') {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Have a Nice Day");
            delay(3000);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("SCAN YOUR ID");
            delay(3000);


            State = 0;  // Reset state
            return;
          }



          if (key >= '1' && key <= '9') {  // Ensure it's a number
            Regpostnum = key - '0';        // Convert key character to an integer
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Posts: ");
            lcd.print(Regpostnum);
            delay(1000);
            State = 0;
            break;
          }
        }

        // Automatically move to collect codes for each post
        for (int i = 0; i < Regpostnum; i++) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter code for ");
          for (int j = 0; j < 4; j++) {

            while (true) {
              key = keypad.getKey();
              if (key >= '0' && key <= '9') {
                Regcode[j] = key;  // Store the digit
                lcd.setCursor(j, 1);
                lcd.print(key);
                break;
              }
            }
          }
          Regcode[4] = '\0';  // Null-terminate the code

          letterCode = String(Regcode);  // Store the full code
          Serial.println("Code: " + letterCode);
          lcd.clear();
          lcd.print("Code: " + letterCode);

          verifyLetter(letterCode, boxId);  // Send code for verification
          delay(2000);


          if (servoLocked) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Servo Locked!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("SCAN YOUR ID");
            delay(1000);
            myServo.write(closedPosition);  // Open the servo
            delay(2000);
            return;

          } else {
            myServo.write(openPosition);  // Open the servo
          }
          delay(2000);
        }

        // After all codes entered, prompt to close
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Process Done!");
        delay(2000);

        lcd.setCursor(0, 1);
        lcd.print("Press'*'to close");
        delay(3000);

        key = NO_KEY;  // Reset key after processing
        State = 2;

        while (true) {  // Collect number of posts
          key = keypad.getKey();
          servoLocked = false;  // unlock the servo
          // Handle D key to manually open without input
          if (key == 'D') {               // Ensure state is in the initial state
            myServo.write(openPosition);  // Open the servo
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("INPUT LETTERS");
            lcd.setCursor(0, 1);
            lcd.print("Press'*'to close");
            delay(5000);
            State = 0;  // Update state to indicate it's waiting for further action
          }

          // Handle closing the servo with '*' after pressing 'D'
          if (key == '*') {
            myServo.write(closedPosition);  // Close the servo
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Have a nice day!");
            delay(3000);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("SCAN YOUR ID");
            State = 0;  // Reset state
            delay(1000);
            return;
          }
          
        }



      } else {
        Serial.println("Access Denied");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Access Denied");
        delay(2000);
      }
      // Halt PICC and stop encryption after processing
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      // Check if '*' is pressed to close the servo
      if (key == '*' && State == 2) {
        myServo.write(closedPosition);  // Close the servo
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Have a nice day!");
        delay(5000);
        State = 0;  // Reset state
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SCAN YOUR ID");

        return;
      }
    }
  }
}

void connectToWiFi() {

  Serial.print("Connecting");

  Serial.print("to WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


  Serial.print("WiFi Connected!");
  delay(2000);
}

void verifyLetter(String letterCode, String boxId) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String serverPath = "https://8705-2402-4000-b270-50f-d140-9b76-4fa1-56a8.ngrok-free.app/api/verify-letter?letter_code=R" + letterCode + "&letter_box_physical_id=" + boxId;
    http.begin(serverPath.c_str());

    // add ngrok header to bypass browser warning
    http.addHeader("ngrok-skip-browser-warning", "true");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Response=" + response);

      if (response == "true") {
        Serial.println("Letter verified successfully.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Letter Verified!");
      } else {
        Serial.println("Letter verification failed.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Verification Fail!");
        servoLocked = true;  // Lock the servo if verification fails
      }
    } else {
      Serial.print("Error on sending request: ");
      Serial.println(httpResponseCode);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("HTTP Error!");
      servoLocked = true;  // Lock the servo on HTTP error
    }

    http.end();  // Free resources
  } else {
    Serial.println("WiFi Disconnected");
  }
}