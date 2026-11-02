#include "EasyBLE.h"
#include <stdlib.h>
#include <Stepper.h>
#include <Adafruit_Fingerprint.h>
#include "Arduino.h"
#include "uRTCLib.h"
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Firebase_ESP_Client.h>

EasyBLE ble;
uRTCLib rtc(0x68);


#include <EEPROM.h>
#include "PillEEPROM.h"
#include "PillFCM.h"
////////////////
#include "Adafruit_TCS34725.h"

#define LED_PIN 19 
Adafruit_TCS34725 tcs = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

struct SlotColor {
  const char* name;
  float nr; 
  float ng; 
  float nb; 
};

SlotColor slots[15] = {
  {"Slot 0",  0.189, 0.341, 0.469},
  {"Slot 1",  0.268, 0.440, 0.293},
  {"Slot 2",  0.243, 0.405, 0.352},
  {"Slot 3",  0.225, 0.443, 0.333},
  {"Slot 4",  0.378, 0.358, 0.265},
  {"Slot 5",  0.341, 0.326, 0.332}, 
  {"Slot 6",  0.235, 0.366, 0.398},
  {"Slot 7",  0.340, 0.321, 0.340},
  {"Slot 8",  0.278, 0.476, 0.246},
  {"Slot 9",  0.160, 0.357, 0.484},
  {"Slot 10", 0.328, 0.237, 0.435},
  {"Slot 11", 0.231, 0.443, 0.326},
  {"Slot 12", 0.175, 0.358, 0.467},
  {"Slot 13", 0.392, 0.276, 0.332},
  {"Slot 14", 0.361, 0.248, 0.391}
};

const int NUM_SLOTS = 15;
int currentMotorSlot = 0;

int slotCount = 0;
// ... (Existing Include) ...
String data;
String slotdata;
bool fingerp = false;
String timeStr;
String dateStr;
PillEEPROM pillMemory;
PillFCM fcm;
int misscount = 0;
const int stepsPerRevolution = 1028;
Stepper myStepper(2048, 26, 25, 33, 32);
hd44780_I2Cexp lcd;

  #define API_KEY "AIzaSyCRADLj-oh7qC5HsDlo619GxUNdJUJ1tF0"
  #define DATABASE_URL "https://smart-pill-box-by-techiedo-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

HardwareSerial FingerSerial(2);  // UART2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerSerial);
int ENROLL_ID = 1;  // Change ID for new fingerprints
String ENROLL_IDstr = "";
String savedStatus = "";
int togle = 0;
#define buzzer 13
#define TOUCH_PIN 4
#define alertButton 27
unsigned long lastMessageTime = 0;
unsigned long lastDashboardUpdate = 0;
String path = "sensor/alert";

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print("SMART PILL BOX");
  lcd.setCursor(0, 1);
  lcd.print("try to connect wifi.......  ");

  pinMode(buzzer, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(alertButton, INPUT_PULLUP);

  pillMemory.begin();
  FingerSerial.begin(57600, SERIAL_8N1, 16, 17);
  ble.begin("Smart Pill Box");

  // Load WiFi from EEPROM
  String storedSSID = pillMemory.readSSID();
  
  // Basic sanity check: Is it longer than 1 char AND does it start with a normal letter/number?
  // EEPROM returns 255 (ÿ) when empty, which passes length checks but crashes WiFi.begin()
  bool isValidSSID = (storedSSID.length() > 1 && storedSSID[0] >= 32 && storedSSID[0] <= 126);

  if (isValidSSID) {  
    String storedPass = pillMemory.readPassword();
    Serial.println("Loading Saved WiFi: " + storedSSID);
    fcm.begin(storedSSID, storedPass);


  } else {
    Serial.println("No Saved WiFi, using default");
    fcm.begin("hp1", "12345678");
  }

  WiFi.setSleep(false);

  myStepper.setSpeed(5); // Slower speed to prevent motor heating!

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // LED off by default
  
  /*
  if(!tcs.begin()){
    Serial.println("TCS34725 not detected. Check I2C wiring!");
  }
  */

  if (finger.verifyPassword()) {
    Serial.println("AS608 Fingerprint Sensor Detected");
  } else {
    Serial.println("Sensor NOT detected");
    // Removed infinite loop here to ensure Bluetooth STILL STARTS even if fingerprint sensor is loose
  }
  finger.setSecurityLevel(1);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Bluetooth started! Waiting for app...");
  lastMessageTime = millis();
}

void loop() {
  // Maintain WiFi
  fcm.loop();
  rtc.refresh();
  // Check if data is available (just like Serial.available())

  if (ble.available()) {

    data = ble.readString();
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("SMART PILL BOX");
    lcd.setCursor(1, 1);
    lcd.print("BLE Connected");
    Serial.print("Received: ");
    Serial.println(data);
    // --- YOUR LOGIC HERE ---

    // 0. Save Slot Data (Command: "SLOT:ID:TIME:DATE")
    if (data.startsWith("SLOT:")) {
      // Expected: SLOT:1:03:51 PM:Feb 6
      slotdata = data;
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Receiving Data");
      lcd.setCursor(2, 1);
      lcd.print("From App....");

      int c1 = data.indexOf(':');
      int c2 = data.indexOf(':', c1 + 1);
      int c3 = data.indexOf(':', c2 + 1);
      int c4 = data.indexOf(':', c3 + 1);

      // Make sure all required separators exist
      if (c1 > 0 && c2 > 0 && c3 > 0 && c4 > 0) {

        // -------- Extract Slot ID --------
        String idStr = data.substring(c1 + 1, c2);
        int id = idStr.toInt();

        if (id > 0) {

          // -------- Extract Time --------
          String hourStr = data.substring(c2 + 1, c3);  // 03
          String minAmPm = data.substring(c3 + 1, c4);  // 51 PM
          String timeStr = hourStr + ":" + minAmPm;     // 03:51 PM

          // -------- Extract Date --------
          String dateStr = data.substring(c4 + 1);  // Feb 6

          // -------- Rotate Stepper to Exact Slot --------
          moveToSlot(id);

          // -------- Save to EEPROM --------
          pillMemory.saveSlot(id, timeStr, dateStr);
          lcd.clear();
          lcd.setCursor(1, 0);
          lcd.print("Slot " + idStr + " Saved");
          lcd.setCursor(1, 1);
          lcd.print(timeStr + " " + dateStr);
          updateSlotStatus(id, "scheduled");
        }
      } else {
        Serial.println("Invalid SLOT format received");
      }
    }

    else if (data.startsWith("WIFI:")) {

      int c1 = data.indexOf(':');          // after WIFI
      int c2 = data.indexOf(':', c1 + 1);  // after SSID

      if (c1 > 0 && c2 > 0) {
        String ssid = data.substring(c1 + 1, c2);
        String password = data.substring(c2 + 1);

        Serial.print("Received WIFI update: ");
        Serial.println(ssid);

        // Save to EEPROM (WiFi area)
        pillMemory.saveWiFi(ssid, password);

        // Reconnect immediately
        fcm.begin(ssid, password);
      }
    }

    else if (data == "edit") {
      // Clear all stored slot data (time/date/status) so we start fresh
      pillMemory.clear();

      if (slotCount == 0 || slotCount != 1) {
        // Just go back to home base 0 when editing starts
        moveToSlot(0);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Adding new datas");
        lcd.setCursor(3, 1);
        lcd.print("Waiting...");
      }
    } else if (data == "cancel") {
      Serial.println(slotdata[5]);
      //  String slotdataStr= slotdata[5];
      int slotnumint = String(slotdata[5]).toInt();
      moveToSlot(0);
    }

    // Command: give_data
    else if (data == "give_data") {
      Serial.println("Command: give_data -> Syncing...");
      for (int i = 1; i <= 15; i++) {
        String val = pillMemory.readSlot(i);  // "08:00 AM|Jan 14|scheduled"
        if (val.length() > 0) {
          // Parse Time|Date|Status
          int firstPipe = val.indexOf('|');
          int secondPipe = val.indexOf('|', firstPipe + 1);

          String t = "";
          String d = "";
          String st = "scheduled";  // default

          if (firstPipe > 0) {
            t = val.substring(0, firstPipe);
            if (secondPipe > 0) {
              d = val.substring(firstPipe + 1, secondPipe);
              st = val.substring(secondPipe + 1);
            } else {
              d = val.substring(firstPipe + 1);
            }
          }

          // Send to App: "SLOT_DATA:ID:TIME:DATE:STATUS"
          String resp = "SLOT_DATA:" + String(i) + ":" + t + ":" + d + ":" + st;
          ble.print(resp);
          Serial.println("Sent: " + resp);
          delay(50);  // Throttle
        }
      }
      ble.print("SYNC_DONE");
    }

    // 1. Handshake
    else if (data == "fingerprint") {
      Serial.println("Handshake -> Sending 'ok'");
      ble.print("ok");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Adding new");
      lcd.setCursor(0, 1);
      lcd.print("FingerPrint");
      fingerp = true;


    } else if (fingerp == true) {
      ENROLL_IDstr = data;
      ENROLL_ID = ENROLL_IDstr.toInt();
      Serial.println("enrollid: ");
      Serial.println(ENROLL_ID);
      enrollFingerprint(ENROLL_ID);
      fingerp = false;
    }
    lastMessageTime = millis();
  }

  checkReminders();
  if (digitalRead(alertButton) == 0) {

    path = "sensor/alert";
    if (Firebase.RTDB.setString(&fbdo, path, 1)) {
      Serial.println("Status Updated OK");
    } else {
      Serial.println("Update Failed");
      Serial.println(fbdo.errorReason());
    }
  }
  // Show LCD Dashboard if idle
  if (millis() - lastMessageTime > 3000) {
    updateLCDDashboard();
  }

  delay(10);  // Small delay for stability
}
uint8_t enrollFingerprint(uint8_t id) {
  int p = -1;

  ble.print("ok");

  Serial.println("\n👉 Place finger on sensor (keep steady)");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place your finger");
  lcd.setCursor(3, 1);
  lcd.print("on sensor");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("❌ First image conversion failed");
    ble.print("fail");
    return 1;
  }

  Serial.println("✅ First image captured");
  Serial.println("✋ Remove finger");
  ble.print("ok");
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("✅ First image");
  lcd.setCursor(4, 1);
  lcd.print("captured");
  delay(1000);
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Remove");
  lcd.setCursor(5, 1);
  lcd.print("finger");
  delay(3000);

  // Wait until finger is fully removed
  while (finger.getImage() != FINGERPRINT_NOFINGER)
    ;

  delay(1000);
  ble.print("ok");
  Serial.println("👉 Place SAME finger again (same position)");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place SAME finger");
  lcd.setCursor(0, 1);
  lcd.print("again");

  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("❌ Second image conversion failed");
    ble.print("fail");
    return 1;
  }
  ble.print("ok");
  Serial.println("✅ Second image captured");
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("✅ Second image");
  lcd.setCursor(0, 1);
  lcd.print("captured");

  /* ---------- CREATE MODEL ---------- */
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("🎉 Fingerprint matched");
    ble.print("ok");
  } else {
    Serial.println("❌ Fingerprints did NOT match");
    ble.print("fail");
    return 1;
  }

  /* ---------- STORE MODEL ---------- */
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("✅ Fingerprint stored successfully!");
    ble.print("ok");
  } else {
    Serial.println("❌ Failed to store fingerprint");
    ble.print("fail");
    return 1;
  }

  return 0;
  lastMessageTime = millis();
}
uint8_t getFingerprintID(int misscount, int targetSlotId) {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Matched ID: ");
    Serial.println(finger.fingerID);
    Serial.print("Confidence: ");
    Serial.println(finger.confidence);


    if (misscount == 0) {  // miss ayillaki

      moveToSlot(targetSlotId);
      slotCount++;
      if (slotCount >= 16) {
        slotCount = 0;
      }
    } else {
      moveToSlot(targetSlotId);
      // slotCount = slotCount + misscount;
      misscount = 0;

      if (slotCount >= 16) {
        slotCount = 0;
      }
      //delay(100);
      // myStepper.step(stepsPerRevolution);
    }
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Finger Matched");
    lcd.setCursor(0, 1);
    lcd.print("Pill Taken");
    Serial.print("Taken");
  } else {
    Serial.println("No match found");
    Serial.print("Not_Taken");
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("FingerPrint");
    lcd.setCursor(2, 1);
    lcd.print("Not Matchced");
  }
  lastMessageTime = millis();
  return p;
}
// Helper to get month name
String getMonthName(int m) {
  switch (m) {
    case 1: return "Jan";
    case 2: return "Feb";
    case 3: return "Mar";
    case 4: return "Apr";
    case 5: return "May";
    case 6: return "Jun";
    case 7: return "Jul";
    case 8: return "Aug";
    case 9: return "Sep";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
    default: return "";
  }
}

// Helper to format time like "08:00 AM" or "04:30 PM"
String formatTime(int h, int m) {
  String ampm = "AM";
  int hour12 = h;

  if (h >= 12) {
    ampm = "PM";
    if (h > 12) hour12 = h - 12;
  }
  if (hour12 == 0) hour12 = 12;

  String hStr = (hour12 < 10) ? "0" + String(hour12) : String(hour12);
  String mStr = (m < 10) ? "0" + String(m) : String(m);

  return hStr + ":" + mStr + " " + ampm;
}

// Helper to format date like "Jan 14"
String formatDate(int month, int day) {
  String mName = getMonthName(month);
  String dStr = String(day);
  return mName + " " + dStr;
}


void handlePillReminder(int slotId, String taketime) {
  Serial.print("Starting Reminder for Slot ");
  Serial.println(slotId);

  pillMemory.updateStatus(slotId, "TimeUp");
  updateSlotStatus(slotId, "timeup");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("*** TAKE PILL ***");
  lcd.setCursor(0, 1);
  lcd.print("Slot " + String(slotId) + " " + taketime);

  // 1. Alarm Phase (15 seconds) - Blinking Buzzer
  alarm();
  // 2. Wait Phase (15 minutes) - Wait for Fingerprint
  Serial.println("Alarm done. Waiting 15 mins for fingerprint...");
  unsigned long startWait = millis();
  bool taken = false;

  // 15 minutes = 15 * 60 * 1000 = 900,000 ms
  // while (millis() - startWait < 120000)
  while (millis() - startWait < 30000) {

    fcm.loop();  // Keep WiFi/FCM alive
    yield();     // Allow background tasks to run

    // if (millis() - startWait == 60000)
    if (millis() - startWait == 15000) {
      Serial.println("Remainder2.......");
      unsigned long startAlarm2 = millis();
      while (millis() - startAlarm2 < 15000) {
        digitalWrite(buzzer, HIGH);
        delay(500);  // ON for 500ms
        digitalWrite(buzzer, LOW);
        delay(500);  // OFF for 500ms
        //fcm.loop(); // Keep WiFi/FCM alive
      }
    }
    // Check Touch Sensor (LOW means touched as per user code)
    if (digitalRead(TOUCH_PIN) == 0) {
      Serial.println("Touch detected! Checking fingerprint...");

      // getFingerprintID() rotates stepper if match is found
      int fpStatus = getFingerprintID(misscount, slotId);

      if (fpStatus == FINGERPRINT_OK) {
        Serial.println("Pill Taken successfully.");
        pillMemory.updateStatus(slotId, "Taken");
        updateSlotStatus(slotId, "taken");
        taken = true;

        // Optional: Short beep to confirm
        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      } else {
        Serial.println("Fingerprint not recognized. Try again.");
        delay(1000);  // Prevent spamming
      }
    }

    delay(100);  // Small delay to prevent tight loop
  }

  // 3. Timeout - Mark as Missed
  if (!taken) {
    Serial.println("Time expired! Marking as missed.");
    pillMemory.updateStatus(slotId, "missed");
    updateSlotStatus(slotId, "missed");
    misscount++;
    slotCount++;


    // Send Notification
    //fcm.sendNotification("Missed Pill!", "You missed your pill for Slot " + String(slotId));
  }
  lastMessageTime = millis();
}

void checkReminders() {
  // Get current time strings in the format saved in EEPROM
  int currentH = rtc.hour();
  int currentM = rtc.minute();
  int currentMonth = rtc.month();
  int currentDay = rtc.day();

  String currentTimeStr = formatTime(currentH, currentM);
  String currentDateStr = formatDate(currentMonth, currentDay);

  // Iterate all slots
  for (int i = 1; i <= 15; i++) {
    String slotData = pillMemory.readSlot(i);
    // Format: "TIME|DATE|STATUS" e.g., "08:00 AM|Jan 14|scheduled"

    if (slotData.length() > 0) {
      int firstPipe = slotData.indexOf('|');
      int secondPipe = slotData.indexOf('|', firstPipe + 1);

      if (firstPipe > 0) {
        String savedTime = slotData.substring(0, firstPipe);
        String savedDate = "";
        String savedStatus = "scheduled";

        if (secondPipe > 0) {
          savedDate = slotData.substring(firstPipe + 1, secondPipe);
          savedStatus = slotData.substring(secondPipe + 1);
        } else {
          savedDate = slotData.substring(firstPipe + 1);
        }


        // Check Match
        // IMPORTANT: Check that we haven't already handled this (status == "scheduled")
        if (savedStatus == "scheduled") {
          if (savedTime == currentTimeStr && savedDate == currentDateStr) {
            Serial.print("REMINDER MATCH! Slot: ");
            Serial.println(i);

            // Trigger the blocking sequence
            handlePillReminder(i, savedTime);
          }
        }
      }
    }
  }
}

int timeToMinutes(String timeStr) {
  // Expected: "08:00 AM" or "12:30 PM"
  if (timeStr.length() < 8) return -1;

  int colonIndex = timeStr.indexOf(':');
  int spaceIndex = timeStr.indexOf(' ');

  if (colonIndex == -1 || spaceIndex == -1) return -1;

  int h = timeStr.substring(0, colonIndex).toInt();
  int m = timeStr.substring(colonIndex + 1, spaceIndex).toInt();
  String ampm = timeStr.substring(spaceIndex + 1);
  ampm.trim();  // Just in case

  if (ampm == "PM" && h != 12) h += 12;
  if (ampm == "AM" && h == 12) h = 0;

  return (h * 60) + m;
}

int detectSlot(float nr, float ng, float nb){
  float minDist = 1000;
  int best = -1;

  for(int i = 0; i < NUM_SLOTS; i++){
    float dist = sqrt(
      pow(nr - slots[i].nr, 2) +
      pow(ng - slots[i].ng, 2) +
      pow(nb - slots[i].nb, 2)
    );

    if(dist < minDist){
      minDist = dist;
      best = i;
    }
  }

  // Threshold set slightly lower to help differentiate close colors
  if (best == 0) {
    // Slot 0 skips sequential verification, so it needs a STRICTER threshold to prevent false positives from Slot 10
    if (minDist < 0.04) return best;
    else return -1;
  } else {
    // Normal slots: tightened from 0.10 to 0.08 to reject fuzzy edges
    if (minDist < 0.08) return best; 
    else return -1;
  }
}

void moveToSlot(int targetSlot) {
  if (targetSlot < 0 || targetSlot >= NUM_SLOTS) return;
  if (targetSlot == currentMotorSlot) return;

  Serial.print("\nSearching for Slot ");
  Serial.print(targetSlot);
  Serial.println("... (Rotating FORWARD and validating sequentially)");

  int dir = 1; // Always dispense moving forward
  int maxAttempts = 1500; 

  digitalWrite(LED_PIN, HIGH);
  delay(50); // let LED brighten

  int expectedSlot = (currentMotorSlot + 1) % NUM_SLOTS;
  bool reachedTarget = false;

  while (!reachedTarget && maxAttempts > 0) {
    // Keep WiFi alive while we search!
    fcm.loop();
    yield();

    // Move motor chunk (reduced from 20 to 10 to take finer, smoother steps)
    myStepper.step(10 * dir);

    // Wait 60ms for the color sensor's 50ms integration time 
    // to capture a clean stable frame after the motor stops moving
    delay(60); 

    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    long totalLight = r + g + b;
    if (totalLight >= 150) {
      float sum = (float)totalLight;
      float nr = r / sum;
      float ng = g / sum;
      float nb = b / sum;

      int detectedSlot = detectSlot(nr, ng, nb);

      // If we detect ANY valid slot that is not our current one
      if (detectedSlot != -1 && detectedSlot != currentMotorSlot) {

        // Verify it's a deep match and not just the edge (reduced length so it stays perfectly on center)
        myStepper.step(10 * dir); 
        delay(60); // Allow sensor to stabilize
        tcs.getRawData(&r, &g, &b, &c);

        if (r + g + b >= 150) {
            sum = (float)(r + g + b);
            int secondCheckSlot = detectSlot(r/sum, g/sum, b/sum);

            if (secondCheckSlot == detectedSlot) {
              Serial.print("Confirmed seeing Slot ");
              Serial.println(detectedSlot);

              currentMotorSlot = detectedSlot; // Update our absolute position

              // Are we at the final target yet?
              if (currentMotorSlot == targetSlot) {
                myStepper.step(15 * dir); // Center it perfectly
                reachedTarget = true;
              } else {
                // Move our expected slot forward to the next one in the sequence!
                expectedSlot = (currentMotorSlot + 1) % NUM_SLOTS;

                // Move slightly past to avoid catching the same slot again
                myStepper.step(150 * dir);
              }
            }
          }
      }
    }
    maxAttempts--;
  }

  digitalWrite(LED_PIN, LOW);

  currentMotorSlot = targetSlot; // Update our known position
  Serial.println("\nSUCCESS! Motor stopped exactly at Slot " + String(currentMotorSlot));
}



String getNextPillTime() {
  int currentMin = rtc.hour() * 60 + rtc.minute();
  int minDiff = 10000;  // Large number
  String nextTime = "";
  String firstTimeOfDay = "";
  int firstTimeMin = 10000;

  for (int i = 1; i <= 15; i++) {
    String slotData = pillMemory.readSlot(i);
    // Format: "TIME|DATE|STATUS"
    if (slotData.length() > 0) {
      int firstPipe = slotData.indexOf('|');
      if (firstPipe > 0) {
        String tStr = slotData.substring(0, firstPipe);
        int tMin = timeToMinutes(tStr);

        if (tMin != -1) {
          // Check for earliest slot in the day (wrapping case)
          if (tMin < firstTimeMin) {
            firstTimeMin = tMin;
            firstTimeOfDay = tStr;
          }

          // Check for next slot today
          if (tMin > currentMin) {
            int diff = tMin - currentMin;
            if (diff < minDiff) {
              minDiff = diff;
              nextTime = tStr;
            }
          }
        }
      }
    }
  }

  if (nextTime != "") return nextTime;
  if (firstTimeOfDay != "") return firstTimeOfDay;  // Wrap to tomorrow
  return "None";
}

void updateLCDDashboard() {
  // Only update once per second to avoid flickering
  if (millis() - lastDashboardUpdate < 1000) return;
  lastDashboardUpdate = millis();

  lcd.clear();

  // Line 0: Current Time
  lcd.setCursor(0, 0);
  String nowStr = formatTime(rtc.hour(), rtc.minute());
  lcd.print("Time: " + nowStr);

  // Line 1: Next Pill
  lcd.setCursor(0, 1);
  String next = getNextPillTime();
  lcd.print("Next: " + next);
}
void updateSlotStatus(int slotID, String status) {

  String path = "pill_slots/" + String(slotID) + "/status";

  // Retry up to 3 times
  for (int i = 0; i < 3; i++) {
    if (Firebase.ready()) {
      if (Firebase.RTDB.setString(&fbdo, path, status)) {
        Serial.println("Status Updated OK");
        return;  // Success!
      } else {
        Serial.print("Update Failed (Attempt ");
        Serial.print(i + 1);
        Serial.print("): ");
        Serial.println(fbdo.errorReason());

        // If SSL error, wait a bit before retrying
        delay(1000);
      }
    } else {
      Serial.println("Firebase not ready... waiting");
      delay(500);
    }
  }
  Serial.println("Gave up updating status.");
}
void alarm() {
  unsigned long startAlarm = millis();
  while (millis() - startAlarm < 15000) {
    digitalWrite(buzzer, HIGH);
    delay(500);  // ON for 500ms
    digitalWrite(buzzer, LOW);
    delay(500);  // OFF for 500ms
    fcm.loop();  // Keep WiFi/FCM alive
    yield();     // Allow background tasks to run
  }
}
//