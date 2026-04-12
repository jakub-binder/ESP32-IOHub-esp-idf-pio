//xlib-Eeprom-24CS02.h
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <vector>

// Nastaveni sbernice + adres najednou (doporučeno volat v setup())
void eepromConfigure(TwoWire &w, uint8_t eepromAddr, uint8_t usnAddr);

// --- Řízený init/deinit I2C z knihovny ---
bool eepromI2CInitOn(TwoWire &w, int sda, int scl, uint32_t freqHz,
                     uint8_t eepromAddr, uint8_t usnAddr, String &err);
bool eepromI2CDeinit(String &err);

// --- util ---
bool parseHexByte(const String &hexStr, uint8_t &value);

// ======= API deklarace =======
bool applyWriteEepromData(const String &cmd, String &err);
bool writeEepromData(uint8_t startAddress, const uint8_t *data, uint8_t length, String &errorMessage);
bool readEepromData(String &errorMessage);
std::vector<String> getEepromData();
// Čtení 16B od zadané adresy (addr musí být násobek 16)
bool readEepromDataAt(uint8_t address, String &errorMessage);
// Vrátí jeden řádek (16B) z interního EEPROM bufferu pro danou adresu
String formatEepromLineAt(uint8_t address);
// ======= EXT (Verifying writing to eeprom) =======
bool applyWriteEepromDataExt(const String &cmd, String &err);
bool writeEepromDataExt(uint8_t startAddress, const uint8_t *data, uint8_t length, String &errorMessage);

bool readEepromSerialNumber(String &errorMessage);
String printEepromSerialNumber();

bool applySetEepromAddrCmd(const String &cmd, String &err);

// Dispatcher pro všechny EEPROM příkazy
//  - vrací true při úspěchu a plní outLines (každý prvek = jeden řádek pro Serial.println)
//  - při chybě vrátí false a naplní err
bool Eeprom_ApplyCommand(const String &cmd, std::vector<String> &outLines, String &err);
