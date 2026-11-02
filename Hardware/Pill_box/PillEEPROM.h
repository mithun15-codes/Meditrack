#ifndef PILLEEPROM_H
#define PILLEEPROM_H

#include <Arduino.h>
#include <EEPROM.h>

#define PILL_EEPROM_SIZE 1024
#define PILL_SLOT_SIZE   64
#define PILL_MAX_SLOTS   15

class PillEEPROM {
public:
    PillEEPROM();
    void begin();

    void saveSlot(int id, String time, String date, String status = "scheduled");
    void updateStatus(int id, String status);
    String readSlot(int id);
    void clear();
    
    // WiFi Credentials (Stored at end of EEPROM)
    void saveWiFi(String ssid, String password);
    String readSSID();
    String readPassword();

    void printAll();

private:
    int _getAddr(int id);
    void _writeSlotString(int addr, const String &data);
    String _readSlotString(int addr);
};

#endif
