#ifndef BOARD_ESP32S3_H
#define BOARD_ESP32S3_H

#define BOARD_NAME                       "ESP32-S3"

/* ---------------------------
 * Board capabilities
 * --------------------------- */
#define BOARD_HAS_USB_CDC                1
#define BOARD_HAS_UART1                  1
#define BOARD_HAS_UART2                  1
#define BOARD_HAS_I2C2                   1
#define BOARD_HAS_VSPI                   1
#define BOARD_HAS_HSPI                   1

/* ---------------------------
 * User LEDs
 * --------------------------- */
#define BOARD_USER_LED_PIN               1
#define BOARD_USER_LED2_PIN              2

/* ---------------------------
 * UART
 * --------------------------- */
#define BOARD_UART0_TX_PIN               43
#define BOARD_UART0_RX_PIN               44

#define BOARD_UART1_TX_PIN               17
#define BOARD_UART1_RX_PIN               18

#define BOARD_UART2_TX_PIN               21
#define BOARD_UART2_RX_PIN               14

/* ---------------------------
 * I2C
 * --------------------------- */
#define BOARD_I2C1_SDA_PIN               16
#define BOARD_I2C1_SCL_PIN               15

#define BOARD_I2C2_SDA_PIN               48
#define BOARD_I2C2_SCL_PIN               47

/* ---------------------------
 * VSPI / SPI3
 * --------------------------- */
#define BOARD_VSPI_MISO_PIN              37
#define BOARD_VSPI_MOSI_PIN              35
#define BOARD_VSPI_SCK_PIN               36
#define BOARD_VSPI_SS_PIN                39

/* ---------------------------
 * HSPI / SPI2
 * --------------------------- */
#define BOARD_HSPI_MISO_PIN              13
#define BOARD_HSPI_MOSI_PIN              11
#define BOARD_HSPI_SCK_PIN               12
#define BOARD_HSPI_SS_PIN                10

#endif /* BOARD_ESP32S3_H */
