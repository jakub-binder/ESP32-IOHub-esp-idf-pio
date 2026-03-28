# Instrukce pro agenty

## Účel
Tento projekt je modulární firmware pro ESP32 platformu postavený na:

- ESP-IDF frameworku
- PlatformIO

Firmware je psaný v jazyce C a je navržen jako:
- více board variant (HW desky)
- více fixture (HW / test konfigurace)

---

## Jazyková pravidla

- dokumentace je psaná česky
- instrukce pro agenty jsou psané česky
- zdrojový kód je psaný anglicky
- názvy funkcí, proměnných a typů jsou v angličtině

---

## Architektura

Projekt je rozdělen na tři vrstvy:

### 1. Board
Definuje HW:
- pin mapping
- dostupné periferie

Umístění:
include/board/

Výběr:
BOARD_ESP32S3
BOARD_ESP32WROOM32
BOARD_ESP32DEV_16MB

---

### 2. Fixture
Definuje konkrétní HW konfiguraci a chování firmware.

Fixture určuje:
- které moduly jsou použity (I2C, ADC, DAC…)
- které periferie se inicializují
- které funkce jsou aktivní

Příklad:
- fixture A → pouze I2C
- fixture B → I2C + ADC + DAC

Umístění:
src/fixtures/

Každá fixture implementuje:
void fixture_setup(void);
void fixture_loop(void);

---

### 3. Runtime

Soubor:
src/main.c

Obsahuje pouze:
- inicializaci systému
- výběr fixture
- hlavní smyčku

---

## Priority

1. čitelnost
2. stabilní architektura
3. jednoduchost
4. předvídatelnost
5. konzistence

---

## Zásady návrhu

- fixture pouze skládá moduly dohromady
- logika HW patří do komponent (`components/`)
- žádný přímý přístup k HW mimo driver vrstvu
- žádné skryté závislosti mezi moduly
- každý modul má jasné API
- žádné globální side efekty

---

## Coding style (C)

Tento projekt preferuje konzervativní a čitelný styl jazyka C. Prioritou není maximální „chytrost“ kódu, ale snadná údržba, čitelnost a bezpečné rozšiřování.

### Obecné zásady

- psát jednoduchý, čitelný a konzistentní C kód
- preferovat malé funkce s jasným účelem
- nepoužívat C++ konstrukce
- vyhýbat se skrytým side effectům

### Soubory

- jeden modul = jeden `.c` + jeden `.h`
- názvy souborů malými písmeny s podtržítky

### Funkce

- názvy anglicky
- prefix podle modulu
- interní funkce `static`

### Proměnné

- srozumitelné názvy
- minimalizovat globální proměnné
- `static` pro interní globální

### Konstanty

- makra VELKÝMI písmeny
- nepoužívat magická čísla

### Typy

- používat `_t`
- používat `stdint.h`, `stdbool.h`

### Hlavičky

- include guard
- jen veřejné API

### Formátování

- 4 mezery
- závorky na nový řádek

### Logování

- používat ESP_LOG*
- vlastní TAG pro modul

---

## Pravidla

- neměnit strukturu projektu bez důvodu
- nepřidávat C++
- nepřidávat logiku do main.c
- neobcházet board abstraction
- nevytvářet duplikace

---

## Co agent NESMÍ dělat

- měnit build flags bez důvodu
- měnit board makra
- slučovat fixture
- přesouvat logiku mezi vrstvami

---

## Co agent MŮŽE dělat

- přidávat komponenty
- přidávat fixture
- zlepšovat čitelnost
