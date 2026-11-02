#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

/* 1. Define global WiFi credentials */
String ssid = "";
String pass = "";

#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs for "Smart Band"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-567890abcdef"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Helper to read String from EEPROM
String readString(int add) {
  char data[100];  //Max 100 Bytes
  int len = 0;
  unsigned char k;

  k = EEPROM.read(add);
  if (k == 0xFF) return "";

  while (k != '\0' && len < 95) {
    k = EEPROM.read(add + len);
    if (k == 0xFF) break;
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

// Helper to write String to EEPROM
void writeString(int add, String data) {
  int _size = data.length();
  for (int i = 0; i < _size; i++) {
    EEPROM.write(add + i, data[i]);
  }
  EEPROM.write(add + _size, '\0');
}

/* 2. Define the Firebase API Key and Database URL */
#define API_KEY "AIzaSyCRADLj-oh7qC5HsDlo619GxUNdJUJ1tF0"
#define DATABASE_URL "smart-pill-box-by-techiedo-default-rtdb.firebaseio.com"

/* 3. Firebase Service Account Credentials for FCM (100% Free, No Card Required)
 * Go to Firebase Console > Project Settings > Service Accounts > Generate New Private Key
 * Copy the client_email and private_key exactly as they appear in the JSON file.
 */
#define FIREBASE_CLIENT_EMAIL "firebase-adminsdk-fbsvc@smart-pill-box-by-techiedo.iam.gserviceaccount.com"
const char PROGMEM FIREBASE_PRIVATE_KEY[] = "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDkyS4DzA4uRMrf\nUQqqUkqKbKU5pQqBxnsnWsFjQ1+KF4Chmfy7NvJMYBxV04ty/cQCSGkpRL/nIQei\nFUSNdUCCFY8Zgmhs20ZcNHP/pCHUsuV6VeoZ6lTFx9YsGH7yNXBHRTAYBZCTkyrz\nte7yqAt7/1FR9qGxUyWEEy3/1YNjDfbGDoGZpX+/0a7uDfywfDDrLe1xHD/dcndH\niCl/Fun5pUoViweoEKZVwgi4+pe2aFn8hQ85FQ0yJ+JPVmJ81ok++TePb1UOJzg5\nzoaQw2l9y6PtLYiznqDDFrIQfRQrCcH7MV2fK3ySesF8k9hw0K5RSGBxK0d7wrFX\nCtuyLe2hAgMBAAECggEAD4nGUUWzlNrhOGeFpOmv3FTip9aYrxWu2Y5lYft6aqOL\n/e26Z3Ck5iEE66NRbj7j6A/I7J+ZtPXeUdjF7iGPfwY90Mq1kucVkT+SvUMFdS+t\nPDDHlHkAxun6lsYXIRYWCREe4hLGre8p1KF/mAVnukUt3bY1ZxixjJXM4vo6USjJ\nwKFnPemU59Tzmq1fqdQflgmtPvJ8XuUrFJ/NEmRIRBijSp9lvgM0EnseaJXlWfGj\nctaIyska3hYhPYNnfIKyI9GrXERz2dYJP/evfcINhBAdkAWIpttUwzKZYWOejD8t\ndSNANrooGVhjwGGJ1jIXzF6UZ1DhVQHzjEgXuZfSPQKBgQD0+FqIceErv2tmAidd\nRc7RMe3gl/wPcjo41jT7Bkju2xHdPDrMMSAEcFcV4JF1K69hy4DO4kiSON4YcQV/\nxPnoDv6AadLSv/PiAF8Cub+1fPipfoPe1SnvcYy+h7DbbHCr3T/0uw/KmUa0Y5T7\nqEOxm03nI0JQfyrst/GOkjOCewKBgQDvFkc4LH5I/gy1apupLsI6YxPOAXWUtA93\nkTH0iwQ8x60N/cvA/cI2edp6FJyOn0GLogjua0eyozAs7hh2mDfPsATiocwf8By6\neRvN1h7VpJdMjlkahMppglkz5P6byxg2P/OEf+U9BCkeIqQ0vUgkT+y9o2szCfpC\nc4Xjt7qzkwKBgAy0gMWI2nvRXrmYrP/6aeKRq1u1pj4K15gxC+hlCA1TUVbfE9GZ\nTqbMSMf6sLCaBmuVCWYxeEAVMPeljPp2dduiQ3LXijah+/0htcvHZHj+NUtdI2vA\nNH+VrQ4IrZSDyEgxnmqL9SK11mPuskBnFrO0cVLmkj7oiYpjc+TIEmHXAoGBANgm\nugitIRb4p6C0WzqfTXXf8R3D7/uatokQuasuq4wPsgL+OuGmCZmanMzWKZw/mtCU\nk6KJVu7H3lwgfflqMLsRlbkXMiaFvFNR/rMfLA1WmnraDvCMsTqmX+Ms2osbGC6j\nrpNR1XxOcKYQB4iuv7QklhH01ov0L084hgmUJ7UFAoGAGMPlGKNpFDpVMdErfLjM\nGHHR3PuaBcmGWKdCATUXJVBNoJrO3htmnUot51zzCBfIexC4nwRf5wzbE+PBbc54\njTsSuPWadgOBWD9bAOkuCnRhMNhPXuKv1xhdxveeBncdLyJU1Y66QidvuNgaWPYW\n0S4SeGmO0gb89PTLIOU8nZI=\n-----END PRIVATE KEY-----\n";
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
const char* serverURL = "https://script.google.com/macros/s/AKfycbzXiDNcMpn0yc_aRYz1zM4Kaa15ijjFuhog6Yz-lETuBkYkjJzT-HDGPFFswQDjl_FJ-w/exec";

FirebaseData streamData;
FirebaseAuth auth;
FirebaseConfig config;

// Function to trigger physical vibration patterns on GPIO 27
void vibrateAlert(String type) {
    // Both alerts use GPIO 27 for the buzzer/vibration motor
    int vibePin = 27;
    pinMode(vibePin, OUTPUT);

    if (type == "timeup") {
        Serial.println(">>> TRIGGERING LONG MEDICINE VIBRATION PATTERN <<<");
        // Medicine Reminder Pattern: 15 seconds (5 long pulses)
        for(int i = 0; i < 5; i++) {
            digitalWrite(vibePin, HIGH); 
            delay(1500); 
            digitalWrite(vibePin, LOW); 
            delay(1500); 
        }
    }
    else if (type == "missed"){
      for(int i = 0; i < 5; i++) {
            digitalWrite(vibePin, HIGH); 
            delay(150); 
            digitalWrite(vibePin, LOW); 
            delay(300); 
        }
    } 
    else if (type == "sensor") {
        Serial.println(">>> TRIGGERING URGENT SENSOR VIBRATION PATTERN <<<");
        // Urgent Sensor Pattern: 3 fast, short pulses repeated twice
        for(int j = 0; j < 2; j++) {
            for(int i = 0; i < 3; i++) {
                digitalWrite(vibePin, HIGH); 
                delay(5000);
                digitalWrite(vibePin, LOW); 
                
            }
            delay(1000); 
        }
    }

    // Ensure it's fully off when done
    digitalWrite(vibePin, LOW);
}

void connectToWiFi() {
  ssid = readString(0);
  pass = readString(100);

  // Clean strings (sometimes \0 issues occur)
  if (ssid.length() > 0) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(pass);

    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      digitalWrite(2, HIGH); // Turn LED ON
      delay(150);
      digitalWrite(2, LOW);  // Turn LED OFF
      delay(150);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      digitalWrite(2, LOW); // Ensure LED is off
                                        
      // **CRITICAL FIX**: Turn off Bluetooth completely so we have enough memory 
      // for the Google Scripts HTTPS Push Notification!
      BLEDevice::deinit(true);
      Serial.println("Bluetooth safely disabled. Memory reclaimed for Push Notifications.");

    } else {
      Serial.println("");
      Serial.println("WiFi Connection Failed. Waiting for Bluetooth setup...");
    }
  } else {
    Serial.println("No WiFi Credentials in EEPROM. Please connect via Bluetooth.");
  }
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Bluetooth Device Connected");
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Bluetooth Device Disconnected");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // Handle WiFi Config
      if (value.startsWith("WIFI:")) {
        // value format: WIFI:SSID:PASS
        int firstColon = value.indexOf(':');
        int secondColon = value.indexOf(':', firstColon + 1);

        if (secondColon != -1) {
          String newSsid = value.substring(firstColon + 1, secondColon);
          String newPass = value.substring(secondColon + 1);

          Serial.println("--- NEW WIFI CREDENTIALS ---");
          Serial.print("SSID: ");
          Serial.println(newSsid);
          Serial.print("PASS: ");
          Serial.println(newPass);

          // Save to EEPROM
          writeString(0, newSsid);
          writeString(100, newPass);
          if (EEPROM.commit()) {
            Serial.println("Saved to EEPROM!");
            delay(500);  // Give flash time to settle
          } else {
            Serial.println("EEPROM Commit Failed!");
          }

          // Must restart to apply fresh Firebase setup with new WiFi
          Serial.println("Restarting to apply new WiFi credentials...");
          delay(1000);
          ESP.restart();
        }
      }
    }
  }
};

// Helper variables for time matching
String lastAlertTime = ""; 

// This callback function triggers AUTOMATICALLY whenever any data changes in Firebase
void streamCallback(FirebaseStream data) {
    String changedPath = data.dataPath();
    String dataType = data.dataType();
    
    Serial.printf("\n[FIREBASE CHANGE DETECTED] Path: %s, Type: %s \n", changedPath.c_str(), dataType.c_str());

    // 1. Check if the Sensor Alert triggered
    if (changedPath.indexOf("/sensor/alert") != -1 || changedPath == "/alert") {
        bool isAlert = false;
        if (dataType == "string" && data.stringData() == "1") {
            isAlert = true;
        } else if (dataType == "int" && data.intData() == 1) {
            isAlert = true;
        } else if (dataType == "double" && data.doubleData() == 1.0) {
            isAlert = true;
        }

        if (isAlert) {
            Serial.println("-> Sensor Alert triggered! Vibrating...");
            vibrateAlert("sensor");
        }
    }
    
    // 2. Check if a Pill Slot Status changed (missed OR timeup)
    if (changedPath.indexOf("/status") != -1) {
        String statusVal = data.stringData();
        if (dataType == "string") {
            if (statusVal == "missed") {
                Serial.println("-> Pill Missed! Vibrating...");
                vibrateAlert("missed");
            } else if (statusVal == "timeup") {
                Serial.println("-> Time to take medicine! Vibrating...");
                vibrateAlert("timeup");
            }
        }
    }
}


// Stream timeout callback for debugging
void streamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("Firebase Stream timeout, reconnecting...");
    }
}

void setup() {
    Serial.begin(115200);

    // Setup Built-in LED (GPIO 2 on most ESP32 boards)
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    // Initialize EEPROM safely
    if (!EEPROM.begin(512)) {
      Serial.println("Failed to initialize EEPROM");
      return;
    }

    // --- BLE CONFIGURATION SERVER ---
    Serial.println("Starting BLE Configuration Server...");
    BLEDevice::init("Smart Pill Band");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);  // ENABLE SCAN RESPONSE for Name Visibility
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();
    Serial.println("BLE Ready. Waiting for a phone to connect to 'Smart Pill Box Setup'...");

    // Connect to WiFi using saved EEPROM strings
    connectToWiFi();

    // Configure Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // We must use a Service Account to send FCM push notifications
    config.service_account.data.client_email = FIREBASE_CLIENT_EMAIL;
    config.service_account.data.project_id = "smart-pill-box-by-techiedo";
    config.service_account.data.private_key = FIREBASE_PRIVATE_KEY;
    
    // Assign the RTDB URL
    config.database_url = DATABASE_URL;

    
    // Callback used by the library for token generation
    config.token_status_callback = tokenStatusCallback;
    config.signer.test_mode = false;  // Ensure test mode is OFF
    config.timeout.serverResponse = 10 * 1000;

    // Fetch precise time via NTP (Required for FCM Token Generation!)
    Serial.print("Fetching accurate time from NTP...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 100000) { // Wait until time is heavily shifted from 1970
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("\n✅ Time synchronized!");

    // Initialize Firebase using just the config and auth
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    // IMPORTANT: Wait for the token generation to actually complete before streaming!
    Serial.print("Generating Firebase Auth Token...");
    while (!Firebase.ready()) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Token Generated Successfully!");

    Serial.println("Starting Firebase Real-Time Stream monitoring...");
    
    // Begin streaming the entire root database ("/") to catch any changes
    if (!Firebase.RTDB.beginStream(&streamData, "/")) {
        Serial.printf("❌ Stream begin error: %s\n\n", streamData.errorReason().c_str());
    }

    // Set the stream callback functions
    Firebase.RTDB.setStreamCallback(&streamData, streamCallback, streamTimeoutCallback);
}

void loop() {
    // 1. BLE Logic (Keep advertising if a phone disconnects)
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("BLE advertising restarted");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // 2. Firebase Handles itself...
    // We only need to refresh the authentication token when it expires.
    if (Firebase.isTokenExpired()) {
        Firebase.refreshToken(&config);
        Serial.println("Firebase Token Refreshed");
    }

    // Run your other pill box code here (motors, LCD, buttons, etc.)
    // ...
}
