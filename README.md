# MediTrack: Intelligent Dose Tracking with Heart Rate Sensor and Fall Surveillance

**MediTrack** is an IoT-based intelligent medication management and patient surveillance system engineered to address medication non-adherence and improve safety for elderly and chronically ill patients.

Unlike conventional reminder-only pill organizers, MediTrack combines **automated 15-slot rotary pill dispensing**, **color-sensor compartment alignment**, **biometric (fingerprint) intake verification**, **dual wearable wristbands (Patient & Caregiver/Bystander)** for continuous **heart rate monitoring and fall detection**, **Firebase cloud synchronization**, and a **Flutter mobile application** into a unified healthcare platform.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Key Features](#key-features)
3. [System Architecture](#system-architecture)
4. [End-to-End Workflow](#end-to-end-workflow)
5. [Repository Structure](#repository-structure)
6. [Hardware Components & Pin Configuration](#hardware-components--pin-configuration)
7. [Software & Firmware Modules](#software--firmware-modules)
8. [Firebase & BLE Communication Protocol](#firebase--ble-communication-protocol)
9. [Getting Started](#getting-started)
10. [Project Team](#project-team)

---

## Project Overview

Non-adherence to prescribed medication schedules is one of the most persistent challenges in long-term healthcare, often leading to preventable complications and hospitalizations. Traditional aids—such as basic pillboxes or simple phone alarms—cannot verify whether the patient actually accessed the medication, nor do they monitor the patient's physical well-being or alert caregivers in real time when doses are missed.

**MediTrack** bridges this gap through a multi-node ESP32 IoT ecosystem paired with a cross-platform Flutter mobile application:

- **Automated & Verified Dispensing:** Rotates a 15-compartment carousel to the scheduled slot using a stepper motor and verifies exact alignment using an RGB color sensor (`TCS34725`).
- **Biometric Intake Confirmation:** Requires the patient to authenticate via an optical fingerprint sensor (`AS608`) before confirming a dose as `taken`, preventing false adherence logs and unauthorized access.
- **Continuous Health & Fall Surveillance:** A dedicated **Patient Wristband** monitors real-time heart rate (`MAX30105`) and detects sudden falls (`LSM6DS0` 3-axis accelerometer/gyroscope), immediately escalating emergency alerts.
- **Caregiver Connectivity:** A dedicated **Bystander/Caregiver Wristband** and the **Smart Pill Box Mobile App** receive instant real-time notifications via Firebase Realtime Database and Firebase Cloud Messaging (FCM) whenever a dose is due, missed, or an emergency/fall event is triggered.

---

## Key Features

- **15-Slot Automated Pill Dispenser:** Circular turbine-style pill container driven by a stepper motor with closed-loop RGB color-sensor feedback for zero-drift compartment positioning.
- **Fingerprint Authentication (`AS608`):** Enrolls and verifies up to 3 user fingerprints directly from the mobile app via Bluetooth Low Energy (BLE) to confirm actual medication intake.
- **Multi-Modal Dose Reminders:** Alerts the patient using an onboard audible buzzer, a 16x2 I2C LCD display, mobile push notifications, and haptic vibration patterns on the patient wristband.
- **Missed Dose Escalation:** Automatically marks a scheduled dose as `missed` if fingerprint authentication is not completed within the designated waiting window, triggering instant caregiver alerts.
- **Wearable Vital & Fall Monitoring:** Tracks live Beats Per Minute (BPM) and average BPM while continuously calculating 3-axis acceleration magnitude to detect falls.
- **Dual Wristband Ecosystem:** Distinct haptic vibration patterns (`timeup`, `missed`, and urgent `sensor`/SOS alerts) for both the patient and the caregiver/bystander wristbands.
- **Emergency SOS Button:** Physical panic button on the pill box and automatic fall trigger on the patient band that immediately broadcast an emergency state (`/sensor/alert = 1`) to Firebase.
- **Dual Connectivity (BLE + Wi-Fi/Cloud):** Uses BLE for initial setup, Wi-Fi credential provisioning, local schedule syncing, and fingerprint enrollment; uses Wi-Fi and Firebase Realtime Database for remote monitoring and cloud persistence.
- **Offline Resilience:** Stores Wi-Fi credentials and 15-slot medication schedules in non-volatile `EEPROM` on the hardware and `SharedPreferences` / Firebase offline persistence on the mobile app.

---

## System Architecture

MediTrack is organized into four functional layers:

| Layer | Core Modules & Responsibilities |
| :--- | :--- |
| **1. Sensing Layer** | • Real-Time Clock (`DS3231` / `uRTCLib`)<br>• Optical Fingerprint Sensor (`AS608`)<br>• RGB Color Sensor (`TCS34725`)<br>• Capacitive Touch Sensor & Emergency SOS Button<br>• Heart Rate Sensor (`MAX30105`)<br>• 3-Axis Accelerometer & Gyroscope (`LSM6DS0`) |
| **2. Processing Layer** | • **Pill Box ESP32 Controller:** Scheduling, Stepper/Color Control, Biometrics, BLE<br>• **Server ESP32 Node:** Firebase RTDB Stream Listener & FCM Webhook Dispatcher<br>• **Patient Wristband ESP32:** BPM Calculation, Fall Detection, Haptic Alerts<br>• **Bystander Wristband ESP32:** Real-Time Caregiver Stream & Haptic Alerts |
| **3. Actuation Layer** | • 28BYJ-48 Stepper Motor (15-Slot Carousel)<br>• 16x2 I2C LCD Display<br>• Pill Box Audible Buzzer<br>• Patient & Caregiver Wristband Vibration Motors |
| **4. Communication Layer** | • Bluetooth Low Energy (BLE)<br>• Wi-Fi & Firebase Realtime Database<br>• Google Apps Script FCM Bridge<br>• Flutter Mobile Application (Android / iOS) |

---

## End-to-End Workflow

1. **Schedule Configuration**
   The caregiver or user configures medication timings, dates, and compartments (Slots 1–15) in the Flutter mobile app. Schedules are transmitted via BLE (`SLOT:<ID>:<TIME>:<DATE>`) to the Pill Box EEPROM and synced to Firebase Realtime Database (`/pill_slots/<1..15>`).

2. **Real-Time Clock Matching**
   The Pill Box ESP32 continuously polls the onboard RTC module (`0x68`). When the current date and time match a slot marked as `scheduled`, the reminder sequence begins.

3. **Alert & Verification Window**
   - The slot status updates to `timeup` in EEPROM and Firebase.
   - The LCD displays `*** TAKE PILL ***`, the onboard buzzer sounds an alarm pattern, the Patient & Bystander wristbands vibrate, and an FCM push notification is sent to the mobile app.
   - The system enters a waiting window listening for the capacitive touch sensor (`GPIO 4`).

4. **Biometric Authentication & Dispensing**
   - When the patient touches the sensor and places their finger on the `AS608` scanner, the system verifies their identity.
   - Upon a valid match, the stepper motor rotates the carousel forward while the `TCS34725` RGB color sensor reads normalized `(r, g, b)` values to stop precisely on the target compartment (`Slot 0` to `Slot 14`).
   - The dose status is logged as `taken` in EEPROM and Firebase.

5. **Missed Dose Handling**
   If the waiting window expires without valid fingerprint authentication, the system marks the slot as `missed`, increments the missed counter, vibrates the caregiver wristband, and dispatches a **"Missed Medicine"** push notification.

6. **Continuous Health & Fall Surveillance**
   - The Patient Wristband continuously samples IR readings from `MAX30105` to compute live and average BPM (synced to `/sensor/bpm` and `/sensor/avg_bpm` every 60 seconds).
   - Simultaneously, it reads 3-axis acceleration `(ax, ay, az)` from `LSM6DS0`. If the acceleration vector magnitude exceeds the impact threshold (`> 25000`), or if the physical SOS button (`GPIO 27`) on the pill box is pressed, `/sensor/alert` is immediately set to `"1"`, triggering urgent vibrations on the caregiver wristband and a high-priority alert on the mobile app.

---

## Repository Structure

```text
Meditrack/
├── App/                                      # Cross-platform Flutter Mobile Application
│   ├── android/                              # Android native configuration & Gradle scripts
│   ├── ios/                                  # iOS native configuration
│   ├── linux/ macos/ windows/ web/           # Desktop & Web platform targets
│   ├── assets/
│   │   └── sounds/                           # Custom notification/alarm audio assets
│   ├── arduino_esp/                          # Prototype hardware sketches & pre-built APK
│   │   ├── app-release.apk                   # Compiled Android release APK
│   │   ├── esp32_ble_app/                    # Early ESP32 BLE + Pill Box prototype
│   │   ├── esp32_watch/                      # Early ESP32 watch prototype
│   │   └── uno_r4_ble_app/                   # Arduino UNO R4 BLE prototype
│   ├── lib/
│   │   ├── main.dart                         # App entry point, Firebase init & Material 3 theme
│   │   ├── components/
│   │   │   └── turbine_widget.dart           # Custom 15-slot circular rotary dispenser UI widget
│   │   ├── screens/
│   │   │   ├── splash_screen.dart            # Initial animated splash & routing screen
│   │   │   ├── dashboard_screen.dart         # Live BPM monitor, 15-slot manager, BLE & FCM handler
│   │   │   ├── device_scan_screen.dart       # BLE scanner for "Smart Pill Box" & "Smart Pill Band"
│   │   │   ├── fingerprint_screen.dart       # Biometric fingerprint enrollment UI (3 user slots)
│   │   │   ├── medicine_schedule_screen.dart # Visual status overview of the 15-slot turbine
│   │   │   └── settings_screen.dart          # Device pairing & BLE Wi-Fi provisioning screen
│   │   └── services/
│   │       └── firebase_service.dart         # Firebase Realtime Database streams & slot updates
│   └── pubspec.yaml                          # Flutter dependencies and asset declarations
│
└── Hardware/                                 # Production ESP32 Arduino Firmware Sketches
    ├── Pill_box/                             # Main Smart Pill Box Controller Firmware
    │   ├── Pill_box.ino                      # Core scheduling, stepper/color alignment, biometric & LCD logic
    │   ├── EasyBLE.h / EasyBLE.cpp           # Custom BLE UART-like wrapper for app communication
    │   ├── PillEEPROM.h / PillEEPROM.cpp     # EEPROM manager for 15 slots and Wi-Fi credentials
    │   └── PillFCM.h / PillFCM.cpp           # Wi-Fi connection manager and notification helper
    ├── Patient/                              # Patient Wearable Wristband Firmware
    │   └── Patient.ino                       # MAX30105 heart rate, LSM6DS0 fall detection & haptic alerts
    ├── Bystander/                            # Caregiver / Bystander Wearable Wristband Firmware
    │   └── Bystander.ino                     # Firebase stream listener & caregiver vibration alerts
    └── Server/                               # Dedicated Cloud-to-FCM Notification Bridge Node
        └── Server.ino                        # Monitors Firebase RTDB streams & triggers Google Script FCM push
```

---

## Hardware Components & Pin Configuration

### 1. Smart Pill Box (`Hardware/Pill_box/Pill_box.ino`)

| Component | Model / Interface | ESP32 Pin / Address |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32 Dev Module | — |
| **Real-Time Clock (RTC)** | DS3231 (`uRTCLib` via I2C) | `SDA: GPIO 21`, `SCL: GPIO 22` (`0x68`) |
| **LCD Display** | 16x2 I2C LCD (`hd44780_I2Cexp`) | `SDA: GPIO 21`, `SCL: GPIO 22` |
| **RGB Color Sensor** | Adafruit `TCS34725` (I2C) | `SDA: GPIO 21`, `SCL: GPIO 22`, `LED: GPIO 19` |
| **Fingerprint Sensor** | `AS608` Optical Reader (`UART2`) | `RX: GPIO 16`, `TX: GPIO 17` (`57600 baud`) |
| **Stepper Motor** | 28BYJ-48 + ULN2003 Driver | `IN1–IN4: GPIO 26, 25, 33, 32` |
| **Touch Sensor** | Capacitive Touch Input | `GPIO 4` |
| **Emergency SOS Button** | Push Button (`INPUT_PULLUP`) | `GPIO 27` |
| **Audible Buzzer** | Active/Passive Buzzer | `GPIO 13` |

### 2. Patient Wristband (`Hardware/Patient/Patient.ino`)

| Component | Model / Interface | ESP32 Pin / Address |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32 Dev Module | — |
| **Heart Rate Sensor** | SparkFun `MAX30105` (I2C) | `SDA: GPIO 21`, `SCL: GPIO 22` |
| **3-Axis Accel & Gyro** | `LSM6DS0` (I2C) | `SDA: GPIO 21`, `SCL: GPIO 22` (`0x6B`) |
| **Alert Indicator LED** | Status / Alert LED | `GPIO 19` |
| **Vibration Motor** | Haptic Feedback Motor | `GPIO 27` |

### 3. Bystander / Caregiver Wristband (`Hardware/Bystander/Bystander.ino`)

| Component | Model / Interface | ESP32 Pin / Address |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32 Dev Module | — |
| **Vibration Motor / Buzzer** | Haptic / Audible Alert | `GPIO 27` |
| **Status Indicator LED** | Onboard LED | `GPIO 2` |

---

## Software & Firmware Modules

### Mobile Application (`App/`)
Built with **Flutter (Dart SDK `^3.0.0`)** and **Material 3**:

- `flutter_blue_plus` — Scans, connects, and exchanges commands with the Pill Box and Wristbands over BLE.
- `firebase_core` & `firebase_database` — Real-time bidirectional sync of `/pill_slots` and `/sensor` data with offline persistence enabled.
- `firebase_messaging` & `flutter_local_notifications` — Subscribes to the `pillbox_users` FCM topic and displays high-importance heads-up notifications.
- `shared_preferences` — Caches paired BLE device IDs, 15-slot schedules, and enrolled fingerprint names locally.
- `google_fonts`, `intl`, `permission_handler`, `wakelock_plus`, `audioplayers` — UI typography, date formatting, runtime permissions, screen wake-lock, and audio playback.

### Hardware Firmware (`Hardware/`)
Built with the **Arduino framework for ESP32**:

- **`Pill_box.ino`**
  - Uses normalized RGB Euclidean distance matching across 15 pre-calibrated color profiles (`Slot 0` through `Slot 14`) with two-stage verification to eliminate mechanical stepper drift.
  - Supports multi-step interactive fingerprint enrollment (`Place finger` -> `Remove finger` -> `Place same finger` -> `Model stored`) synchronized with both the 16x2 LCD and the mobile app via BLE notifications.
- **`Patient.ino`**
  - Computes real-time BPM using `checkForBeat()` with a 12-sample rolling average (`RATE_SIZE = 12`).
  - Computes 3D acceleration magnitude `sqrt(ax² + ay² + az²)` to detect sudden impacts/falls (`> 25000`) and immediately pushes emergency flags to Firebase.
- **`Bystander.ino` & `Server.ino`**
  - Automatically disable BLE (`BLEDevice::deinit(true)`) once Wi-Fi connects to reclaim heap memory for secure TLS/HTTPS Firebase streams and Google Apps Script webhook calls.

---

## Firebase & BLE Communication Protocol

### 1. Smart Pill Box BLE Service (`EasyBLE`)
- **Device Name:** `Smart Pill Box`
- **Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- **Characteristic UUID:** `beb5483e-36e1-4688-b7f5-ea07361b26a8`

| BLE Command (App -> Pill Box) | Description |
| :--- | :--- |
| `SLOT:<id>:<HH:MM AM/PM>:<MMM D>` | Saves schedule for slot `1–15` and rotates carousel to the slot for loading (e.g., `SLOT:1:08:00 AM:Feb 6`). |
| `WIFI:<SSID>:<PASSWORD>` | Saves Wi-Fi credentials to EEPROM and reconnects. |
| `edit` | Clears stored slot schedules in EEPROM and homes the carousel to `Slot 0`. |
| `cancel` | Returns the carousel to `Slot 0`. |
| `give_data` | Requests all 15 slot schedules from EEPROM (returns `SLOT_DATA:<id>:<time>:<date>:<status>` followed by `SYNC_DONE`). |
| `fingerprint` + `<1\|2\|3>` | Initiates biometric fingerprint enrollment for the selected user ID. |

### 2. Smart Pill Band BLE Service (`Patient.ino`, `Bystander.ino`, `Server.ino`)
- **Device Name:** `Smart Pill Band`
- **Service UUID:** `12345678-1234-1234-1234-1234567890ab`
- **Characteristic UUID:** `abcdef01-1234-5678-1234-1234567890abcdef`

| BLE Command (App -> Band) | Description |
| :--- | :--- |
| `WIFI:<SSID>:<PASSWORD>` | Saves Wi-Fi credentials to EEPROM and reboots the ESP32 to start Firebase streaming. |

### 3. Firebase Realtime Database Schema

```json
{
  "pill_slots": {
    "1": {
      "slot": "1",
      "time": "08:00 AM",
      "date": "Feb 6",
      "status": "scheduled | timeup | taken | missed | empty"
    }
  },
  "sensor": {
    "bpm": 74,
    "avg_bpm": 72,
    "alert": "0 | 1"
  }
}
```

---

## Getting Started

### 1. Mobile Application Setup (`App/`)

1. Ensure the [Flutter SDK](https://docs.flutter.dev/get-started/install) (`^3.0.0`) is installed.
2. Navigate to the app directory and fetch dependencies:
   ```bash
   cd App
   flutter pub get
   ```
3. Connect an Android/iOS device (with Bluetooth and Location permissions enabled) and run:
   ```bash
   flutter run
   ```
   *(Alternatively, a pre-compiled Android package is available at `App/arduino_esp/app-release.apk`.)*

### 2. Hardware Firmware Setup (`Hardware/`)

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and add the **ESP32 Board Manager** package.
2. Install the required Arduino libraries:
   - `Firebase ESP Client` (by Mobizt)
   - `Adafruit Fingerprint Sensor Library`
   - `Adafruit TCS34725`
   - `SparkFun MAX3010x Pulse and Proximity Sensor Library`
   - `uRTCLib`
   - `hd44780` (by Bill Perry)
   - `Stepper` (built-in)
3. Flash each ESP32 board with its respective sketch:
   - `Hardware/Pill_box/Pill_box.ino` — Main Pill Dispenser unit
   - `Hardware/Patient/Patient.ino` — Patient Wristband unit
   - `Hardware/Bystander/Bystander.ino` — Caregiver Wristband unit
   - `Hardware/Server/Server.ino` — Notification Server node
4. Open the **Smart Pill Box** mobile app, scan for the devices over Bluetooth, configure Wi-Fi credentials in **Settings**, enroll a fingerprint in **Fingerprint Settings**, and schedule your medication slots on the **Dashboard**.

---

## Project Team

**Authors:**
- **Sinan S Sayed**
- **Mithun Mohan**
- **Godly John**
- **Vigneswar Das**

**Guided By:**
- **Asst. Prof. Aswathy R**

*Department of Computer Science Engineering, St. Thomas College of Engineering and Technology, Chengannur, Kerala, India.*
