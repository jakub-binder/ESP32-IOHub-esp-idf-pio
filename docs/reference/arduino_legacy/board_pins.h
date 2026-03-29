#pragma once

#if defined(BOARD_ESP32S3)
#include "variants/pins_esp32s3.h"
#elif defined(BOARD_ESP32WROOM32)
#include "variants/pins_esp32wroom32.h"
#elif defined(BOARD_ESP32WROOM32_16MB)
#include "variants/pins_esp32wroom32E_16MB.h"
#else
#error "Board variant is not selected. Define BOARD_ESP32S3 or BOARD_ESP32WROOM32."
#endif
