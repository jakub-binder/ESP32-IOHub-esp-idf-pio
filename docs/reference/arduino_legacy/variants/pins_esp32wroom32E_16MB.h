#pragma once

// DIAG LED (not connected on denky32)
#define DIAG1 18
#define DIAG2 -1

// UART 0 (DBG, TTL)
#define UART0_TX 1
#define UART0_RX 3

// UART1 (RS232)
#define UART1_TX 17
#define UART1_RX 16

// UART 2 (TTL)
#define UART2_TX 27
#define UART2_RX 33

// I2C 1
#define I2C1_SDA 21
#define I2C1_SCL 22

// I2C 2
#define I2C2_SDA 26
#define I2C2_SCL 25

// VSPI / SPI3
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define VSPI_SCK 18
#define VSPI_SS 5

// HSPI / SPI2
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_SCK 14
#define HSPI_SS 15