#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "app_config.h"

#if defined(BOARD_ESP32S3)
#include "board_esp32s3.h"
#elif defined(BOARD_ESP32WROOM32)
#include "board_esp32wroom32.h"
#elif defined(BOARD_ESP32DEV_16MB)
#include "board_esp32dev_16mb.h"
#else
#error "Unsupported board selection"
#endif

#endif /* BOARD_PINS_H */