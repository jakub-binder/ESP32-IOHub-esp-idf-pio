# ADS7961 – implementační report

## Vytvořené soubory

- `components/ads7961/CMakeLists.txt`
- `components/ads7961/README.md`
- `components/ads7961/include/ads7961.h`
- `components/ads7961/ads7961.c`
- `docs/reports/ADS7961_IMPLEMENTATION_REPORT.md`

## Převod Arduino chování do ESP-IDF C driveru

- Arduino SPI transakce (`SPI.transfer16`) byly nahrazeny voláním `spi_device_transfer()` ze sdílené komponenty `spi_bus`.
- Pipeline sekvence zůstává stejná: pro čtení kanálu proběhnou tři identické transfery, přičemž data se berou z posledního RX rámce.
- Sestavení 16bit command wordu, extrakce headeru a dat jsou přepsány do čistého C bez použití Arduino API.
- Přepočet na volty používá `range2x_ref` a `refp_volts` stejně jako původní reference.
- Funkce pro průměrování zachovává 40 vzorků s trimem 4 (bez třídění, pouze odhození prvních/posledních vzorků).

## Mapování funkcí z Arduino reference

- `AdcBuildManualCmd` → `ads7961_build_manual_cmd`
- `AdcExtractHeader4` → `ads7961_extract_header4`
- `AdcExtractData8` → `ads7961_extract_data8`
- `AdcReadChannel` → `ads7961_read_channel`
- `AdcReadChannelAvg40Trim4` → `ads7961_read_channel_avg`
- `AdcCode8ToVolts` → `ads7961_code8_to_volts`

## Napojení driveru ve fixture

Fixture bude:

1. inicializovat `spi_bus` a vytvořit `spi_device_t` pro ADS7961 (SPI mode 0, správná CS a clock),
2. naplnit `ads7961_cfg_t` (`spi_dev`, `range2x_ref`, `refp_volts`),
3. zavolat `ads7961_init()` a uložit `ads7961_t` do kontextu fixtury,
4. používat API driveru pro čtení kanálů.

Driver sám neprovádí žádnou inicializaci SPI ani nevytváří `spi_device_t` – fixture zůstává composition root.

## Doporučené HW testy

- Ověřit inicializaci s platným `spi_device_t` a korektními parametry `refp_volts` / `range2x_ref`.
- Čtení jednotlivých kanálů (0..15) a ověření, že data pochází z posledního RX rámce.
- Ověřit hlavičku (`header4`) a datovou část (`data8`) na známém vstupu.
- Ověřit průměrování `ads7961_read_channel_avg()` (40 vzorků, trim 4) na stabilním DC signálu.
- Porovnat přepočet na volty s kalibrovaným zdrojem a ověřit chování pro `range2x_ref`.
- Zkontrolovat, že SPI režim 0 a CS timing nevedou k chybným headerům.
