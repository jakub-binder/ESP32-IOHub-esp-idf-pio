# SPI_BUS_IMPLEMENTATION_REPORT

## Vytvořené soubory

- `components/spi_bus/CMakeLists.txt`
- `components/spi_bus/README.md`
- `components/spi_bus/include/spi_bus.h`
- `components/spi_bus/spi_bus.c`
- `docs/reports/SPI_BUS_IMPLEMENTATION_REPORT.md`

## Rozhodnutí během implementace

- Použití `spi_bus_initialize(...)` a `spi_bus_free(...)` pro lifecycle sběrnice.
- Použití `spi_bus_add_device(...)` a `spi_bus_remove_device(...)` pro lifecycle zařízení.
- `spi_device_transfer(...)` používá `spi_device_transmit(...)` s `length` v bytech a bez dalšího wrapperu.
- Validace `host_id` je postavena na `SOC_SPI_PERIPH_NUM`.
- SPI DMA je nastaveno na `SPI_DMA_CH_AUTO`.
- CS je konfigurován v `spi_device_init(...)` přes `spics_io_num`.
- Konfigurační struktury jsou pojmenované `spi_bus_cfg_t` a `spi_device_cfg_t` kvůli kolizi s ESP-IDF typem `spi_bus_config_t`.

## Nalezené problémy

- Baseline build `pio run` nelze v sandboxu spustit (příkaz `pio` není dostupný).

## Otevřené otázky pro ADS7961 a DAC7564

- Jak přesně budou drivery řešit pipeline čtení ADS7961 a potřebu dummy TX dat.
- Zda budou potřeba další device-level parametry (CS timing, bit order, half-duplex).
- Jaká maximální velikost transferu bude v praxi potřeba pro ADS7961 a DAC7564.
- Zda některé sekvence budou vyžadovat ruční kontrolu CS mimo hardware SPI periférie.
