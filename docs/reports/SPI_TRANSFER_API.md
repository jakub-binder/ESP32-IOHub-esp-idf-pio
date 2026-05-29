# SPI_TRANSFER_API

Tento dokument navazuje na `SPI_BUS_SPEC.md` a řeší rozhodnutí nad veřejným API funkce `spi_device_transfer()`. Neobsahuje implementaci ani zdrojový kód.

## Kontext a cíle

- `spi_device_transfer()` má být jediný standardní způsob, jak device drivery provedou SPI transakci přes `spi_device_t`.
- API musí podporovat ADS7961 (pipeline čtení 16bit rámců) a DAC7564 (zápisy DAC rámců).
- API má zůstat jednoduché, bez skryté magie a bez přímého používání ESP-IDF SPI API v driverech.

---

## VARIANTA A – `uint8_t buffer[]`

**Popis:** Jediný byte buffer použitý pro TX i RX (in-place).

### Výhody

- Nejjednodušší API, minimální počet parametrů.
- Přirozené pro full‑duplex operace s in‑place bufferem.

### Nevýhody

- Nevyjadřuje rozdíl mezi TX a RX.
- Pro read‑only nebo write‑only operace je neintuitivní (nutnost dummy dat).
- Neřeší různou délku TX a RX (např. command + data).

### Složitost

- Nízká.

### Vhodnost pro ADS7961

- Střední: ADS7961 potřebuje pipeline čtení a často kombinuje zápis command word a čtení výsledků; in‑place buffer je použitelný, ale nečitelný.

### Vhodnost pro DAC7564

- Střední: DAC provádí převážně write‑only přenosy; in‑place buffer je funkčně možný, ale zbytečně vynucuje RX.

### Vhodnost pro budoucí SPI zařízení

- Nízká až střední: jednoduché, ale omezené pro komplexnější transakce.

---

## VARIANTA B – `tx buffer + rx buffer + length`

**Popis:** Oddělené TX a RX buffery, společná délka přenosu.

### Výhody

- Jasně odděluje TX a RX.
- Přirozené pro write‑only (RX = NULL) i read‑only (TX = NULL / dummy).
- Jednoduché na používání a testování.

### Nevýhody

- Neřeší rozdílnou délku TX a RX.
- Nepokrývá pokročilé volby (half‑duplex, bit order, CS timing).

### Složitost

- Nízká až střední.

### Vhodnost pro ADS7961

- Vysoká: ADS7961 typicky používá fixed‑length 16bit rámce; oddělené TX/RX je čitelné.

### Vhodnost pro DAC7564

- Vysoká: write‑only přenosy jsou čisté a jednoduché.

### Vhodnost pro budoucí SPI zařízení

- Střední: pokrývá většinu běžných device případů, ale limituje složitější scénáře.

---

## VARIANTA C – `spi_transaction_t` wrapper

**Popis:** API přijímá projektový wrapper nad ESP-IDF `spi_transaction_t` nebo přímo tento typ.

### Výhody

- Maximální flexibilita (full‑duplex, half‑duplex, varying lengths, flags).
- Snadné mapování na ESP-IDF bez dalších adaptací.

### Nevýhody

- Zvyšuje vazbu na ESP-IDF a unikátní typy frameworku.
- Device drivery musí znát detailní strukturu transakce.
- Snižuje přenositelnost a porušuje cíl “driver nepracuje přímo s ESP‑IDF SPI API”.

### Složitost

- Vysoká (pro používání i pro testování).

### Vhodnost pro ADS7961

- Vysoká funkčně, ale architektonicky slabší kvůli framework dependenci.

### Vhodnost pro DAC7564

- Vysoká funkčně, ale zbytečně těžké pro jednoduché write‑only přenosy.

### Vhodnost pro budoucí SPI zařízení

- Vysoká funkčně, ale riziko, že všechny budoucí drivery budou závislé na ESP-IDF detailech.

---

## VARIANTA D – vlastní projektová `spi_transfer_t` struktura

**Popis:** Vlastní projektový datový typ, který pokrývá potřeby běžných SPI zařízení a zůstává nezávislý na ESP‑IDF.

### Výhody

- Odpovídá architektuře projektu (driver nevidí ESP-IDF).
- Umožní explicitně vyjádřit TX/RX, délku i případné rozšíření (např. half‑duplex, word size).
- Dává prostor pro postupné rozšíření bez změny signatury `spi_device_transfer()`.

### Nevýhody

- Vyžaduje návrh nové struktury a její dokumentaci.
- Příliš bohatý návrh může přidat složitost i tam, kde není potřeba.

### Složitost

- Střední.

### Vhodnost pro ADS7961

- Vysoká: lze čistě vyjádřit 16bit rámce a pipeline čtení bez závislosti na ESP‑IDF.

### Vhodnost pro DAC7564

- Vysoká: write‑only přenosy jsou přirozené.

### Vhodnost pro budoucí SPI zařízení

- Vysoká: poskytuje kontrolovaný růst možností bez frameworkové vazby.

---

## Doporučení

**Doporučená varianta: VARIANTA D (vlastní projektová `spi_transfer_t`).**

**Důvod:**

- Zachovává architektonickou izolaci driverů od ESP‑IDF.
- Umožňuje rozšířit API podle potřeby zařízení, aniž by se měnila signatura `spi_device_transfer()`.
- Je dostatečně čitelná pro ADS7961 i DAC7564 a zároveň připravena pro budoucí SPI zařízení.

---

## Otevřená rozhodnutí k `spi_transfer_t`

- Jaký minimální rozsah polí je potřeba (TX/RX/length vs. full‑duplex/half‑duplex flag).
- Zda podporovat různé délky TX a RX v jedné transakci.
- Zda má struktura obsahovat word size, bit order, nebo zůstane čistě byte‑orientovaná.
- Zda budou potřeba více‑segmentové transfery (např. command + data bez zvednutí CS).
