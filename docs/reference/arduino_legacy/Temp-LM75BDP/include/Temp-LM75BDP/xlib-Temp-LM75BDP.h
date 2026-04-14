// xlib-Temp-LM75BDP.h
#pragma once

#include <Arduino.h>

// Hlavní vstup pro příkazy "temp-*"
// cmd  = celý příkaz (např. "temp-set-treshold 19.0 20.0")
// out  = volitelný textový výstup (např. naměřená teplota)
// err  = popis chyby, pokud funkce vrátí false
bool TempLM75_ApplyCommand(const String &cmd, String &out, String &err);

// Pokud bys chtěl volat přímo (mimo textové příkazy), máš i tyto funkce:

// Inicializace I2C na Wire1 (SDA=26, SCL=25, 100kHz) a ověření komunikace se senzorem
bool TempLM75_Init(String &err);

// Deinicializace – odpojí Wire1 a nastaví piny 26/25 jako vstup (Hi-Z)
bool TempLM75_Deinit(String &err);

// Přečtení teploty – vrací text ve formátu "25.375"
bool TempLM75_ReadData(String &value, String &err);

// Nastavení Thyst a Tos (ve °C). Hlídá, aby Thyst < Tos.
bool TempLM75_SetThresholds(float thyst, float tos, String &err);