# ads7961

## Účel komponenty

Komponenta `ads7961` poskytuje čistý C driver pro ADC **ADS7961** přes SPI.
Využívá sdílenou vrstvu `spi_bus` a pracuje s již připraveným `spi_device_t`
z fixtury (komponenta neinicializuje SPI bus ani nevytváří SPI zařízení).

## Podporovaný čip

- Texas Instruments **ADS7961**

## Architektura

- komponenta **neinicializuje SPI bus**
- komponenta **nevytváří** `spi_device_t` (přebírá ho z fixtury)
- komunikace probíhá přes `spi_device_transfer()`
- fixture zůstává composition root

## Veřejné API

- `esp_err_t ads7961_init(ads7961_t *ctx, const ads7961_cfg_t *cfg);`
  - uloží konfiguraci a ověří připravené SPI zařízení
- `bool ads7961_is_initialized(const ads7961_t *ctx);`
  - kontrola stavu inicializace
- `esp_err_t ads7961_read_channel(ads7961_t *ctx, uint8_t channel, uint8_t *out_code8, uint16_t *out_rx);`
  - čtení kanálu s pipeline sekvencí (3× transfer, výsledek z posledního RX)
- `esp_err_t ads7961_read_channel_avg(ads7961_t *ctx, uint8_t channel, float *out_avg_volts, float *out_avg_code8);`
  - 40 vzorků s trimem 4 (stejné chování jako Arduino reference)
- `esp_err_t ads7961_code8_to_volts(const ads7961_t *ctx, uint8_t code8, float *out_volts);`
  - přepočet na volty s podporou `range2x_ref` a `refp_volts`
- `uint16_t ads7961_build_manual_cmd(const ads7961_t *ctx, uint8_t channel);`
  - sestavení 16bit command word
- `uint8_t ads7961_extract_header4(uint16_t rx);`
  - extrakce 4bit headeru z RX rámce
- `uint8_t ads7961_extract_data8(uint16_t rx);`
  - extrakce 8bit ADC dat z RX rámce

## Poznámky

- `channel` musí být v rozsahu `0..15`
- SPI mode 0 nastavuje fixture při vytváření `spi_device_t`
- přepočet na volty používá `range2x_ref` a `refp_volts`
