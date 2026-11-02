#include "PillEEPROM.h"

PillEEPROM::PillEEPROM() {}

void PillEEPROM::begin() {
    EEPROM.begin(PILL_EEPROM_SIZE);
}

int PillEEPROM::_getAddr(int id) {
    if (id < 1 || id > PILL_MAX_SLOTS) return -1;
    return (id - 1) * PILL_SLOT_SIZE;
}

//
// SAFE SLOT WRITE (bounded)
//
void PillEEPROM::_writeSlotString(int addr, const String &data) {

    // clear slot first
    for (int i = 0; i < PILL_SLOT_SIZE; i++)
        EEPROM.write(addr + i, 0);

    int len = data.length();
    if (len > PILL_SLOT_SIZE - 1)
        len = PILL_SLOT_SIZE - 1;

    for (int i = 0; i < len; i++)
        EEPROM.write(addr + i, data[i]);

    EEPROM.write(addr + len, '\0');
}

//
// SAFE SLOT READ (bounded)
//
String PillEEPROM::_readSlotString(int addr) {
    char buf[PILL_SLOT_SIZE];
    for (int i = 0; i < PILL_SLOT_SIZE; i++) {
        buf[i] = EEPROM.read(addr + i);
        if (buf[i] == '\0') break;
    }
    buf[PILL_SLOT_SIZE - 1] = '\0';
    return String(buf);
}

//
// SAVE SLOT
//
void PillEEPROM::saveSlot(int id, String time, String date, String status) {
    int addr = _getAddr(id);
    if (addr == -1) return;

    String payload = time + "|" + date + "|" + status;

    _writeSlotString(addr, payload);
    EEPROM.commit();

    Serial.printf("PillEEPROM: Saved Slot %d -> %s\n", id, payload.c_str());
}

//
// UPDATE STATUS ONLY
//
void PillEEPROM::updateStatus(int id, String status) {

    String current = readSlot(id);
    if (current.length() == 0) return;

    int p1 = current.indexOf('|');
    int p2 = current.indexOf('|', p1 + 1);

    if (p1 < 0) return;

    String time = current.substring(0, p1);
    String date = (p2 > 0) ?
        current.substring(p1 + 1, p2) :
        current.substring(p1 + 1);

    String newPayload = time + "|" + date + "|" + status;

    int addr = _getAddr(id);
    _writeSlotString(addr, newPayload);
    EEPROM.commit();

    Serial.printf("PillEEPROM: Updated Status Slot %d -> %s\n",
                  id, status.c_str());
}

//
// READ SLOT
//
String PillEEPROM::readSlot(int id) {
    int addr = _getAddr(id);
    if (addr == -1) return "";
    return _readSlotString(addr);
}

//
// CLEAR EEPROM
//
//
// CLEAR EEPROM (Preserve WiFi)
//
void PillEEPROM::clear() {
    Serial.println("PillEEPROM: Clearing Slots...");
    // Only clear up to the start of WiFi area (15 slots * 64 bytes = 960)
    for (int i = 0; i < 960; i++)
        EEPROM.write(i, 0);
    EEPROM.commit();
    Serial.println("PillEEPROM: Cleared (WiFi preserved)!");
}

//
// WIFI CREDENTIALS (960 - 1023)
//
#define WIFI_ADDR_START 960
#define SSID_LEN 32
#define PASS_LEN 32

void PillEEPROM::saveWiFi(String ssid, String password) {
    // Clear Area
    for (int i = WIFI_ADDR_START; i < PILL_EEPROM_SIZE; i++) EEPROM.write(i, 0);
    
    // Write SSID
    for (int i = 0; i < ssid.length() && i < SSID_LEN - 1; i++) {
        EEPROM.write(WIFI_ADDR_START + i, ssid[i]);
    }
    
    // Write Pass
    for (int i = 0; i < password.length() && i < PASS_LEN - 1; i++) {
        EEPROM.write(WIFI_ADDR_START + SSID_LEN + i, password[i]);
    }
    
    EEPROM.commit();
    Serial.println("WiFi Credentials Saved to EEPROM");
   
}

String PillEEPROM::readSSID() {
    char buf[SSID_LEN];
    for (int i = 0; i < SSID_LEN; i++) {
        buf[i] = EEPROM.read(WIFI_ADDR_START + i);
    }
    buf[SSID_LEN - 1] = '\0';
    return String(buf);
}

String PillEEPROM::readPassword() {
    char buf[PASS_LEN];
    for (int i = 0; i < PASS_LEN; i++) {
        buf[i] = EEPROM.read(WIFI_ADDR_START + SSID_LEN + i);
    }
    buf[PASS_LEN - 1] = '\0';
    return String(buf);
}

//
// DEBUG PRINT
//
void PillEEPROM::printAll() {
    Serial.println("--- PILLEEPROM DUMP ---");
    for (int i = 1; i <= PILL_MAX_SLOTS; i++) {
        String val = readSlot(i);
        Serial.printf("Slot %d: %s\n",
                      i,
                      val.length() ? val.c_str() : "(empty)");
    }
    Serial.println("-----------------------");
}
