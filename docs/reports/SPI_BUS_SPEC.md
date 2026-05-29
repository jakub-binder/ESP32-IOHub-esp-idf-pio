# SPI_BUS_SPEC

Tento dokument popisuje technickou specifikaci komponenty `spi_bus` podle architektonického rozhodnutí VARIANTA B. Fixture zůstává composition root a vytváří jak bus, tak device objekty.

## 1. Veřejné API

### SPI bus

- `spi_bus_init` – inicializuje fyzickou SPI sběrnici podle konfigurace a nastaví interní stav busu.
- `spi_bus_deinit` – uvolní prostředky sběrnice a přepne bus do neinicializovaného stavu.
- `spi_bus_is_initialized` – vrátí informaci, zda je bus inicializovaný.
- `spi_bus_host_id` – vrátí identifikátor SPI hostu přidělený sběrnici (např. pro diagnostiku nebo předávání do dalších komponent).

### SPI device

- `spi_device_init` – vytvoří zařízení na daném busu podle konfigurace zařízení.
- `spi_device_deinit` – uvolní zařízení a zruší vazbu na bus.
- `spi_device_is_initialized` – vrátí informaci, zda je zařízení inicializované.
- `spi_device_bus` – vrátí referenci na parent bus zařízení.
- `spi_device_transfer` – provede synchronní SPI transakci přes dané zařízení s použitím jeho konfigurace.

## 2. Datové struktury

### spi_bus_t

- `host_id` – identifikátor SPI hostu (hodnota z ESP-IDF `spi_host_device_t`, např. SPI2_HOST/SPI3_HOST dle cílového SoC).
- `sclk_pin` – GPIO pin pro SCLK.
- `mosi_pin` – GPIO pin pro MOSI.
- `miso_pin` – GPIO pin pro MISO.
- `initialized` – stav inicializace busu.
- `platform_handle` – interní handle na ESP-IDF bus kontext; pole je viditelné jen kvůli integraci s ESP-IDF, ale ostatní kód jej nesmí používat ani měnit (viz otevřená otázka o opaque typech).

### spi_bus_config_t

- `host_id` – požadovaný SPI host (hodnota z ESP-IDF `spi_host_device_t`, např. SPI2_HOST/SPI3_HOST dle cílového SoC).
- `sclk_pin` – GPIO pin pro SCLK.
- `mosi_pin` – GPIO pin pro MOSI.
- `miso_pin` – GPIO pin pro MISO.

### spi_device_t

- `bus` – reference na parent `spi_bus_t`.
- `cs_pin` – GPIO pin pro chip select zařízení.
- `mode` – SPI mód zařízení (0–3).
- `clock_hz` – SPI frekvence zařízení.
- `initialized` – stav inicializace zařízení.
- `platform_handle` – interní handle na ESP-IDF device kontext; pole je viditelné jen kvůli integraci s ESP-IDF, ale ostatní kód jej nesmí používat ani měnit (viz otevřená otázka o opaque typech).

### spi_device_config_t

- `bus` – reference na bus, ke kterému bude zařízení připojeno.
- `cs_pin` – GPIO pin pro chip select zařízení.
- `mode` – SPI mód zařízení (0–3).
- `clock_hz` – SPI frekvence zařízení.

## 3. Lifecycle

### Vytvoření SPI busu

1. Fixture vytvoří instanci `spi_bus_t`.
2. Fixture vyplní `spi_bus_config_t`.
3. Fixture zavolá `spi_bus_init`.
4. Bus je připraven k použití pro vytváření zařízení.

### Vytvoření SPI zařízení

1. Fixture vytvoří instanci `spi_device_t`.
2. Fixture vyplní `spi_device_config_t` a odkáže na existující `spi_bus_t`.
3. Fixture zavolá `spi_device_init`.
4. Driver zařízení obdrží hotový `spi_device_t`.

### Deinit zařízení

1. Fixture rozhodne o ukončení zařízení.
2. Fixture zavolá `spi_device_deinit`.
3. Driver již zařízení nepoužívá.

### Deinit busu

1. Fixture zajistí, že všechna zařízení na busu jsou deinitializována.
2. Fixture zavolá `spi_bus_deinit`.
3. Bus je uvolněn a nelze přes něj provádět transakce.

## 4. Ownership

- Fixture vlastní instance `spi_bus_t` a `spi_device_t`, rozhoduje o jejich lifecycle a předává hotové závislosti driverům.
- `spi_bus` vlastní interní bus handle a nízkoúrovňové zdroje sběrnice (pin routing, HW konfiguraci, driver instalaci).
- `spi_device` vlastní interní device handle a device-level konfiguraci (CS, mode, clock).
- ADS7961 driver vlastní pouze svůj driver kontext a drží referenci na `spi_device_t` předanou z fixture.
- DAC7564 driver vlastní pouze svůj driver kontext a drží referenci na `spi_device_t` předanou z fixture.

## 5. Error handling

### Validace

- Ověřit nenulové ukazatele na bus/device a konfiguraci.
- Ověřit validitu pinů (GPIO range, konflikt SCLK/MOSI/MISO/CS).
- Ověřit validitu `host_id` vůči povoleným hodnotám ESP-IDF `spi_host_device_t` pro daný SoC.
- Ověřit `mode` v rozsahu 0–3.
- Ověřit `clock_hz` > 0.
- Ověřit, že `spi_device_init` je voláno nad inicializovaným busem.
- Ověřit, že `spi_device_transfer` je voláno nad inicializovaným zařízením a busem.

### ESP_ERR_INVALID_ARG

- Neplatné nebo chybějící parametry konfigurace.
- Neplatné GPIO piny nebo konfliktní mapování.
- Neplatný `host_id`.
- Neplatný mód nebo frekvence zařízení.

### ESP_ERR_INVALID_STATE

- `spi_bus_init` je voláno na již inicializovaném busu.
- `spi_bus_deinit` je voláno na neinicializovaném busu.
- `spi_device_init` je voláno na neinicializovaném busu.
- `spi_device_init` je voláno na již inicializovaném zařízení.
- `spi_device_deinit` je voláno na neinicializovaném zařízení.
- `spi_device_transfer` je voláno na neinicializovaném zařízení nebo busu.

## 6. Integrace s ADS7961

- ADS7961 driver obdrží `spi_device_t` vytvořený ve fixture.
- Driver používá `spi_device_transfer` pro transakce (pipeline čtení 16bit rámců).
- Driver nevolá ESP-IDF SPI API a nevytváří vlastní SPI device handle.
- Konfigurace módu a frekvence je dána `spi_device_t` (ADS7961 používá SPI mode 0).

## 7. Integrace s DAC7564

- DAC7564 driver obdrží `spi_device_t` vytvořený ve fixture.
- Driver používá `spi_device_transfer` pro zápisy DAC rámců.
- Driver nevolá ESP-IDF SPI API a nevytváří vlastní SPI device handle.
- Konfigurace módu a frekvence je dána `spi_device_t` (DAC7564 používá SPI mode 1).

## 8. Otevřená architektonická rozhodnutí

- Zda API pro transakce bude pouze synchronní, nebo bude potřeba i asynchronní/queued varianta.
- Jak přesně má vypadat datový model pro `spi_device_transfer` (čisté TX/RX buffery vs. abstrakce transakce).
- Zda `spi_bus_config_t` bude rozšířen o parametry jako DMA, maximální velikost transferu nebo další HW flagy.
- Zda `spi_device_config_t` bude rozšířen o parametry jako CS timing, bit order, word size nebo half/full-duplex volby.
- Zda je CS obsluhován čistě HW SPI periferií, nebo bude potřeba explicitní GPIO řízení.
- Zda `spi_bus` interně sleduje aktivní zařízení a blokuje `spi_bus_deinit`, pokud ještě existují.
- Zda `spi_bus` nabídne obdobu `set_pins_state` pro řízení pinů po ukončení práce se sběrnicí.
- Zda `spi_bus_t` a `spi_device_t` mají být dlouhodobě veřejné struktury, nebo zda má být API postavené na opaque typech s privátním stavem.
- Zda existují další požadavky vyplývající z dokumentu `docs/reports/REPORT.md` (soubor aktuálně není v repo k dispozici).
