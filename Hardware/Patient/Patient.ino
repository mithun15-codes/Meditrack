
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/* 1. SENSOR GLOBALS & CONFIG */
MAX30105 particleSensor;

const byte RATE_SIZE = 12;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

// PIN CONFIGURATION
#define SDA_PIN 21
#define SCL_PIN 22
#define ALERT_LED_PIN 19

// LSM6DS0
#define LSM6DS0_ADDR 0x6B
#define CTRL_REG1_G   0x10
#define CTRL_REG6_XL  0x20
#define OUT_X_L_G     0x18
#define OUT_X_L_XL    0x28

int16_t ax, ay, az;
int16_t gx, gy, gz;

/* 2. NETWORK & FIREBASE GLOBALS */
String ssid = "";
String pass = "";

// BLE UUIDs for "Smart Band"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-567890abcdef"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

volatile bool fallDetected = false;
unsigned long lastFirebaseUpdate = 0;

#define API_KEY "AIzaSyCRADLj-oh7qC5HsDlo619GxUNdJUJ1tF0"
#define DATABASE_URL "smart-pill-box-by-techiedo-default-rtdb.firebaseio.com"
#define FIREBASE_CLIENT_EMAIL "firebase-adminsdk-fbsvc@smart-pill-box-by-techiedo.iam.gserviceaccount.com"
const char PROGMEM FIREBASE_PRIVATE_KEY[] = "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDkyS4DzA4uRMrf\nUQqqUkqKbKU5pQqBxnsnWsFjQ1+KF4Chmfy7NvJMYBxV04ty/cQCSGkpRL/nIQei\nFUSNdUCCFY8Zgmhs20ZcNHP/pCHUsuV6VeoZ6lTFx9YsGH7yNXBHRTAYBZCTkyrz\nte7yqAt7/1FR9qGxUyWEEy3/1YNjDfbGDoGZpX+/0a7uDfywfDDrLe1xHD/dcndH\niCl/Fun5pUoViweoEKZVwgi4+pe2aFn8hQ85FQ0yJ+JPVmJ81ok++TePb1UOJzg5\nzoaQw2l9y6PtLYiznqDDFrIQfRQrCcH7MV2fK3ySesF8k9hw0K5RSGBxK0d7wrFX\nCtuyLe2hAgMBAAECggEAD4nGUUWzlNrhOGeFpOmv3FTip9aYrxWu2Y5lYft6aqOL\n/e26Z3Ck5iEE66NRbj7j6A/I7J+ZtPXeUdjF7iGPfwY90Mq1kucVkT+SvUMFdS+t\nPDDHlHkAxun6lsYXIRYWCREe4hLGre8p1KF/mAVnukUt3bY1ZxixjJXM4vo6USjJ\nwKFnPemU59Tzmq1fqdQflgmtPvJ8XuUrFJ/NEmRIRBijSp9lvgM0EnseaJXlWfGj\nctaIyska3hYhPYNnfIKyI9GrXERz2dYJP/evfcINhBAdkAWIpttUwzKZYWOejD8t\ndSNANrooGVhjwGGJ1jIXzF6UZ1DhVQHzjEgXuZfSPQKBgQD0+FqIceErv2tmAidd\nRc7RMe3gl/wPcjo41jT7Bkju2xHdPDrMMSAEcFcV4JF1K69hy4DO4kiSON4YcQV/\nxPnoDv6AadLSv/PiAF8Cub+1fPipfoPe1SnvcYy+h7DbbHCr3T/0uw/KmUa0Y5T7\nqEOxm03nI0JQfyrst/GOkjOCewKBgQDvFkc4LH5I/gy1apupLsI6YxPOAXWUtA93\nkTH0iwQ8x60N/cvA/cI2edp6FJyOn0GLogjua0eyozAs7hh2mDfPsATiocwf8By6\neRvN1h7VpJdMjlkahMppglkz5P6byxg2P/OEf+U9BCkeIqQ0vUgkT+y9o2szCfpC\nc4Xjt7qzkwKBgAy0gMWI2nvRXrmYrP/6aeKRq1u1pj4K15gxC+hlCA1TUVbfE9GZ\nTqbMSMf6sLCaBmuVCWYxeEAVMPeljPp2dduiQ3LXijah+/0htcvHZHj+NUtdI2vA\nNH+VrQ4IrZSDyEgxnmqL9SK11mPuskBnFrO0cVLmkj7oiYpjc+TIEmHXAoGBANgm\nugitIRb4p6C0WzqfTXXf8R3D7/uatokQuasuq4wPsgL+OuGmCZmanMzWKZw/mtCU\nk6KJVu7H3lwgfflqMLsRlbkXMiaFvFNR/rMfLA1WmnraDvCMsTqmX+Ms2osbGC6j\nrpNR1XxOcKYQB4iuv7QklhH01ov0L084hgmUJ7UFAoGAGMPlGKNpFDpVMdErfLjM\nGHHR3PuaBcmGWKdCATUXJVBNoJrO3htmnUot51zzCBfIexC4nwRf5wzbE+PBbc54\njTsSuPWadgOBWD9bAOkuCnRhMNhPXuKv1xhdxveeBncdLyJU1Y66QidvuNgaWPYW\n0S4SeGmO0gb89PTLIOU8nZI=\n-----END PRIVATE KEY-----\n";

FirebaseData streamData;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData fbdo;

/* 3. HELPER FUNCTIONS */
String readString(int add) {
  char data[100];
  int len = 0;
  unsigned char k;
  k = EEPROM.read(add);
  if (k == 0xFF) return "";
  while (k != '\0' && len < 95) {
    k = EEPROM.read(add + len);
    if (k == 0xFF) break;
    data[len] = k; len++;
  }
  data[len] = '\0';
  return String(data);
}

void writeString(int add, String data) {
  int _size = data.length();
  for (int i = 0; i < _size; i++) {
    EEPROM.write(add + i, data[i]);
  }
  EEPROM.write(add + _size, '\0');
}

void vibrateAlert(String type) {
    int vibePin = 27; // Use 27 if attached, or onboard LED
    pinMode(vibePin, OUTPUT);

    if (type == "timeup") {
        Serial.println(">>> TRIGGERING LONG MEDICINE VIBRATION PATTERN <<<");
        for(int i = 0; i < 5; i++) {
            digitalWrite(ALERT_LED_PIN, HIGH); delay(1500); 
            digitalWrite(ALERT_LED_PIN, LOW); delay(1500); 
        }
    }
    else if (type == "missed"){
      for(int i = 0; i < 5; i++) {
            digitalWrite(ALERT_LED_PIN, HIGH); delay(150); 
            digitalWrite(ALERT_LED_PIN, LOW); delay(300); 
        }
    } 
    else if (type == "sensor") {
        Serial.println(">>> TRIGGERING URGENT SENSOR VIBRATION/LED PATTERN <<<");
        for(int j = 0; j < 2; j++) {
            for(int i = 0; i < 3; i++) {
                digitalWrite(ALERT_LED_PIN, HIGH); digitalWrite(ALERT_LED_PIN, HIGH); 
                delay(3000);
                digitalWrite(ALERT_LED_PIN, LOW); digitalWrite(ALERT_LED_PIN, LOW); 
            }
            delay(1000); 
        }
    }
    digitalWrite(vibePin, LOW);
    digitalWrite(ALERT_LED_PIN, LOW);
}

void connectToWiFi() {
  ssid = readString(0);
  pass = readString(100);

  if (ssid.length() > 0) {
    Serial.print("Connecting to WiFi: "); Serial.println(ssid);
    Serial.print("Password: "); Serial.println(pass);

    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      digitalWrite(ALERT_LED_PIN, HIGH); delay(150);
      digitalWrite(ALERT_LED_PIN, LOW); delay(150);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      Serial.print("IP Address: "); Serial.println(WiFi.localIP());
      digitalWrite(ALERT_LED_PIN, LOW); 
                                        
      // Deinit BLE to save memory for Firebase!
      BLEDevice::deinit(true);
      Serial.println("Bluetooth safely disabled.");
    } else {
      Serial.println("\nWiFi Connection Failed. Waiting for Bluetooth setup...");
    }
  } else {
    Serial.println("No WiFi Credentials in EEPROM. Please connect via Bluetooth.");
  }
}

// BLE Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true; Serial.println("BLE Connected");
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false; Serial.println("BLE Disconnected");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value.length() > 0) {
      if (value.startsWith("WIFI:")) {
        int firstColon = value.indexOf(':');
        int secondColon = value.indexOf(':', firstColon + 1);
        if (secondColon != -1) {
          String newSsid = value.substring(firstColon + 1, secondColon);
          String newPass = value.substring(secondColon + 1);
          writeString(0, newSsid); writeString(100, newPass);
          EEPROM.commit();
          Serial.println("Saved to EEPROM! Rebooting...");
          delay(1000); ESP.restart();
        }
      }
    }
  }
};

// NEW: Handlers for Firebase Stream Async Callbacks

// NEW: Handlers for Firebase Stream Async Callbacks
volatile bool triggerMissedAlert = false;
volatile bool triggerTimeupAlert = false;
volatile bool triggerSensorAlert = false;

// ...

// FIREBASE STREAM CALLBACK 
void streamCallback(FirebaseStream data) {
    String changedPath = data.dataPath();
    String dataType = data.dataType();
    
    // DEBUG: Show exactly what Firebase is sending to the device
    Serial.printf("\n[FIREBASE NOTIFY] Path: %s, Type: %s \n", changedPath.c_str(), dataType.c_str());
    if (dataType == "string") {
        Serial.printf("[FIREBASE MSG] Value: %s\n", data.stringData().c_str());
    }

    // Check if the Sensor Alert triggered Remotely
    if (changedPath.indexOf("/sensor/alert") != -1 || changedPath == "/alert") {
        bool isAlert = false;
        if (dataType == "string" && data.stringData() == "1") isAlert = true;
        else if (dataType == "int" && data.intData() == 1) isAlert = true;
        else if (dataType == "double" && data.doubleData() == 1.0) isAlert = true;

        if (isAlert) triggerSensorAlert = true; // FLAG IT FOR MAIN LOOP
    }
    
    // Check if a Pill Slot Status changed
    if (changedPath.indexOf("/status") != -1) {
        String statusVal = data.stringData();
        if (dataType == "string") {
            // Trim whitespace just in case!
            statusVal.trim();
            if (statusVal == "missed") triggerMissedAlert = true;   // FLAG IT FOR MAIN LOOP
            else if (statusVal == "timeup") triggerTimeupAlert = true; // FLAG IT FOR MAIN LOOP
        }
    }
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) Serial.println("Firebase Stream timeout, reconnecting...");
}

// =======================
// SENSOR SETUP & READ
// =======================
bool setupLSM6DS0() {
  delay(10);
  Wire.beginTransmission(LSM6DS0_ADDR);
  Wire.write(0x0F); 
  Wire.endTransmission(false);
  Wire.requestFrom(LSM6DS0_ADDR, 1);
  if (Wire.available()) {
    byte chipId = Wire.read();
    Serial.print("LSM6DS0 Found! Chip ID: 0x");
    Serial.println(chipId, HEX);
    if (chipId == 0 || chipId == 0xFF) {
      Serial.println("ERROR: Invalid Chip ID.");
      return false;
    }
  } else {
    Serial.println("CRITICAL ERROR: LSM6DS0 NOT CONNECTED!");
    return false;
  }
  
  Wire.beginTransmission(LSM6DS0_ADDR); Wire.write(0x10); Wire.write(0x60); Wire.endTransmission();
  Wire.beginTransmission(LSM6DS0_ADDR); Wire.write(0x20); Wire.write(0x60); Wire.endTransmission();
  
  // BDU & IF_INC
  Wire.beginTransmission(LSM6DS0_ADDR); Wire.write(0x12); Wire.write(0x44); Wire.endTransmission();
  return true;
}

void readLSM6DS0() {
  Wire.beginTransmission(LSM6DS0_ADDR); 
  Wire.write(OUT_X_L_G); 
  Wire.endTransmission(false);
  if (Wire.requestFrom(LSM6DS0_ADDR, 6) == 6) {
    gx = Wire.read() | (Wire.read() << 8);
    gy = Wire.read() | (Wire.read() << 8);
    gz = Wire.read() | (Wire.read() << 8);
  }

  Wire.beginTransmission(LSM6DS0_ADDR); 
  Wire.write(OUT_X_L_XL); 
  Wire.endTransmission(false);
  if (Wire.requestFrom(LSM6DS0_ADDR, 6) == 6) {
    ax = Wire.read() | (Wire.read() << 8);
    ay = Wire.read() | (Wire.read() << 8);
    az = Wire.read() | (Wire.read() << 8);
  }
}

void setup() {
    Serial.begin(115200);
    pinMode(ALERT_LED_PIN, OUTPUT);
    digitalWrite(ALERT_LED_PIN, LOW);

    // Sensor Setup (I2C)
    Wire.begin(SDA_PIN, SCL_PIN);
    if (!setupLSM6DS0()) {
      Serial.println(">>> HARDWARE ERROR: LSM6DS0 (Gyro/Accel) Initialization Failed! <<<");
    } else {
      Serial.println("LSM6DS0 Init OK");
    }
    
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
      Serial.println("MAX30105 not found.");
    } else {
      particleSensor.setup(70, 12, 2, 400, 411, 4096); 
      particleSensor.setPulseAmplitudeGreen(0);
      Serial.println("MAX30105 Init OK");
    }

    if (!EEPROM.begin(512)) {
      Serial.println("Failed to initialize EEPROM");
      return;
    }

    Serial.println("Starting BLE Configuration Server...");
    BLEDevice::init("Smart Pill Band");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);  
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    connectToWiFi();

    // Configure Firebase if WiFi matches
    if(WiFi.status() == WL_CONNECTED) {
      config.api_key = API_KEY;
      config.database_url = DATABASE_URL;
      config.service_account.data.client_email = FIREBASE_CLIENT_EMAIL;
      config.service_account.data.project_id = "smart-pill-box-by-techiedo";
      config.service_account.data.private_key = FIREBASE_PRIVATE_KEY;
      config.token_status_callback = tokenStatusCallback;
      config.signer.test_mode = false;
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
      
      if (!Firebase.RTDB.beginStream(&streamData, "/")) {
          Serial.printf("❌ Stream begin error: %s\n\n", streamData.errorReason().c_str());
      }
      Firebase.RTDB.setStreamCallback(&streamData, streamCallback, streamTimeoutCallback);
    }
}

void loop() {
    // 1. BLE Logic
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // 2. Firebase Handles itself...
    if (Firebase.ready() && WiFi.status() == WL_CONNECTED) {
      if (Firebase.isTokenExpired()) {
          Firebase.refreshToken(&config);
          Serial.println("Firebase Token Refreshed");
      }
      
      // FIREBASE ASYNC ALERT HANDLERS
      // We must fire the vibration delays in the main loop to prevent RTOS crashes
      if (triggerSensorAlert) {
          triggerSensorAlert = false;
          vibrateAlert("sensor");
      }
      if (triggerMissedAlert) {
          triggerMissedAlert = false;
          vibrateAlert("missed");
      }
      if (triggerTimeupAlert) {
          triggerTimeupAlert = false;
          vibrateAlert("timeup");
      }
    }

    // 3. Sensor Loop (Non-blocking)
    long irValue = particleSensor.getIR();
    readLSM6DS0();

    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60000 / delta;
      if (beatsPerMinute < 255 && beatsPerMinute > 40) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }

    static long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();
      Serial.print("IR="); Serial.print(irValue);
      if (beatsPerMinute > 30) {
        Serial.print(", BPM="); Serial.print(beatsPerMinute);
        Serial.print(", Avg BPM="); Serial.print(beatAvg);
      }
      long ax_sq = (long)ax * ax; long ay_sq = (long)ay * ay; long az_sq = (long)az * az;
      float currentMag = sqrt(ax_sq + ay_sq + az_sq);
      Serial.print(" | X:"); Serial.print(ax);
      Serial.print(" Y:"); Serial.print(ay);
      Serial.print(" Z:"); Serial.print(az);
      Serial.print(" | Mag:"); Serial.print(currentMag);
      Serial.println();
    }

    // Track Fall Detection
    long ax_sq = (long)ax * ax; long ay_sq = (long)ay * ay; long az_sq = (long)az * az;
    float accelMag = sqrt(ax_sq + ay_sq + az_sq);
    
    if (accelMag > 25000) {
      Serial.println("\n>>> FALL DETECTED! (Offline Alert) <<<"); 
      vibrateAlert("sensor"); // Flash LED / Vibrate locally
      fallDetected = true; 
    }
    
    // Heartrate reset timeout
    if (millis() - lastBeat > 3000) {
      beatsPerMinute = 0;
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) rates[x] = 0;
    }

    // Handle Firebase Syncs
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
       
       // 1. Handle Fall Detection IMMEDIATELY (Do not wait for the 1 minute timer)
       if (fallDetected) {
         if(Firebase.RTDB.setString(&fbdo, "/sensor/alert", "1")) {
            Serial.println("Fall Alert Synced to Firebase!");
            fallDetected = false; // Reset ONLY if successfully synced up
         }
       }
       
       // 2. Periodically update Firebase with live BPM data (every 1 minute = 60000 ms)
       if (millis() - lastFirebaseUpdate > 60000) {
           lastFirebaseUpdate = millis();
           // Sync BPM
           Firebase.RTDB.setInt(&fbdo, "/sensor/bpm", (int)beatsPerMinute);
           Firebase.RTDB.setInt(&fbdo, "/sensor/avg_bpm", beatAvg);
           Serial.println("Heart Rate Synced to Firebase (1 Minute Update)");
       }
    }
}
                