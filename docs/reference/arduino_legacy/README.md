# include/

Tato složka obsahuje sdílené hlavičky používané napříč fixture a knihovnami.

## Co zde je

- `board_pins.h`
  - Přepíná pinové mapy podle `BOARD_*` maker z `build_flags`.
  - Směřuje na konkrétní variantu v `include/variants/`.

- `serial_routing.h`
  - Definuje příkazové streamy podle režimu `APP_MODE_DEBUG` / `APP_MODE_PROD`.
  - Umožňuje mít stejný firmware routing pro USB i UART nasazení.

- `variants/`
  - Obsahuje konkrétní pin mapping pro podporované desky:
    - `pins_esp32s3.h`
    - `pins_esp32wroom32.h`
    - `pins_esp32wroom32E_16MB.h`

## Vazba na build_flags

V `platformio.ini` se přes `build_flags` volí:
- cílová deska (`BOARD_ESP32S3`, `BOARD_ESP32WROOM32`, `BOARD_ESP32WROOM32_16MB`),
- režim směrování příkazů (`APP_MODE_DEBUG` nebo `APP_MODE_PROD`),
- fixture (`FIXTURE_*`).

`src/main.cpp` pouze vybírá fixture; konkrétní piny a serial routing se berou z hlaviček v `include/`.
