# ESP32 IOHub (ESP-IDF + PlatformIO)

Tento projekt je firmware pro ESP32 platformu postavený na:

- ESP-IDF frameworku
- PlatformIO (VSCode)

Cílem je vytvořit:
- přehlednou architekturu
- podporu více desek (board variants)
- podporu více fixtur (HW konfigurací)
- čistý C kód bez Arduino frameworku

---

# Základní koncept

Projekt je rozdělen na tři hlavní části:

## 1. Board (hardware varianta)

Definuje:
- pinout (UART, I2C, SPI…)
- dostupné periferie (např. USER LED)

Výběr probíhá při překladu pomocí makra:

- BOARD_ESP32S3
- BOARD_ESP32WROOM32
- BOARD_ESP32DEV_16MB

Implementace:
include/board/

---

## 2. Fixture (HW / test konfigurace)

Fixture definuje:
jaké moduly a periferie jsou aktivní

Například:

- fixture_default → pouze I2C
- fixture_prod → I2C + ADC + DAC

Každá fixture:
- inicializuje použité moduly
- registruje své CLI příkazy
- definuje runtime chování

Výběr:

- FIXTURE_DEFAULT
- FIXTURE_PROD

Implementace:
src/fixtures/

Každá fixture implementuje:

void fixture_setup(void);
void fixture_loop(void);

---

## 3. Runtime (main)

Soubor:
src/main.c

Obsahuje pouze:
- inicializaci
- výběr fixture
- hlavní smyčku

---

# Build

Výchozí build:

pio run

Konkrétní deska:

pio run -e esp32s3
pio run -e esp32wroom32
pio run -e esp32dev_16mb

---

# Menuconfig

pio run -t menuconfig -e esp32s3

---

# Upload + monitor

pio run -t upload -e esp32s3
pio device monitor

---

# Přidání nové desky

1. Přidat env do platformio.ini
2. Definovat BOARD_xxx
3. Přidat header do include/board
4. Upravit board_pins.h a app_config.h

---

# Přidání nové fixture

1. Vytvořit src/fixtures/fixture_xxx.c
2. Přidat hlavičku
3. Upravit fixture_select.c
4. Přidat build flag -DFIXTURE_XXX

---

# Zásady

- kód = anglicky
- dokumentace = česky
- modulární návrh
- žádné Arduino

---

# Git

Commitovat:
- zdrojové kódy
- platformio.ini
- sdkconfig.*

Ignorovat:
.pio/
.vscode/

---

# Stav projektu

- základní kostra
- více board variant
- fixture architektura
- HAL vrstva
- periferie
 -CLI
