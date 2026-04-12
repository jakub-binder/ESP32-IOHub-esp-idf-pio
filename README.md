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
- výběr fixtury
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

# Konfigurace PlatformIO

## Struktura platformio.ini

Soubor platformio.ini je rozdělen do čtyř sekcí:

- `[platformio]` – globální nastavení projektu, například výchozí prostředí (`default_envs`)
- `[env]` – sdílené nastavení pro všechna prostředí (platforma, framework, rychlost monitoru, výchozí `build_flags`)
- `[env:esp32s3]`, `[env:esp32wroom32]`, `[env:esp32dev_16mb]` – prostředí pro konkrétní desky; každé dědí sdílené nastavení z `[env]` a přidává board-specifické `build_flags`

## build_flags

Každý flag musí být definován právě jednou. Kombinace neplatných hodnot způsobí chybu při překladu.

**Výběr fixtury** (povinné, jedno z):

- `-DFIXTURE_DEFAULT` – výchozí HW konfigurace
- `-DFIXTURE_PROD` – produkční HW konfigurace

**Výběr desky** (povinné, jedno z):

- `-DBOARD_ESP32S3`
- `-DBOARD_ESP32WROOM32`
- `-DBOARD_ESP32DEV_16MB`

**Ostatní:**

- `-DFW_BOARD_NAME=\"...\"` – jméno desky zahrnuté do buildu (např. `"Unites32"`)
- `-DAPP_ENABLE_APPLICATION_LOGS=1` – zapíná aplikační logy (0 = vypnuto)
- `-DAPP_COMMAND_ENDPOINT=...` – vybírá komunikační rozhraní pro příkazy (povinné, viz níže)

## Command endpoint

Projekt podporuje tato rozhraní pro příjem příkazů:

- `APP_COMMAND_ENDPOINT_UART0` – UART0 (výchozí pro většinu desek)
- `APP_COMMAND_ENDPOINT_UART1` – UART1 (hardwarový UART, nutná podpora desky)
- `APP_COMMAND_ENDPOINT_USB_CDC` – na ESP32-S3 je implementováno přes USB Serial JTAG (COM port `VID:PID 303A:1001`)

Výběr se nastavuje přes `build_flags` v sekci konkrétního prostředí.
`APP_COMMAND_ENDPOINT` musí být vždy explicitně definován přes
`-DAPP_COMMAND_ENDPOINT=...` v `platformio.ini`.

Pokud zvolené rozhraní deska nepodporuje, překlad selže s chybou.

Debug-only příkazy jsou povoleny pouze na debug portu (`UART0`).
Na production portu (`UART1`) jsou debug-only příkazy odmítnuty.
Na ESP32-S3 USB endpointu (`APP_COMMAND_ENDPOINT_USB_CDC`) jsou debug-only příkazy povolené.

## Command response protokol

Úspěšné odpovědi mají jednotný formát:

- `OK-0` = úspěch bez datových řádků
- `OK-N` = úspěch s `N` datovými řádky, které následují po hlavičce

Chybové odpovědi začínají prefixem `ERR`.

## Příklad: přepnutí command rozhraní z USB na UART1

V sekci `[env:esp32s3]` v `platformio.ini` nahraď:

    -DAPP_COMMAND_ENDPOINT=APP_COMMAND_ENDPOINT_USB_CDC

za:

    -DAPP_COMMAND_ENDPOINT=APP_COMMAND_ENDPOINT_UART1

## Rychlý test pro ESP32-S3 USB Serial JTAG

1. Nahraj firmware pro `env:esp32s3` a otevři COM port s `VID:PID 303A:1001`.
2. Pošli `help\n` (případně `init\n`).
3. Očekávání: vrátí se odpověď z command systému ve formátu `OK-N`/`ERR ...` a neobjevují se periodické diagnostické výpisy.

---

# Přidání nové desky

1. Přidat env do platformio.ini
2. Definovat BOARD_xxx
3. Přidat header do include/board
4. Upravit board_pins.h a app_config.h

---

# Přidání nové fixtury

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
- sdkconfig.defaults.*

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
