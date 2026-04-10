#include "app_command_transport.h"
#include "app_command_endpoint.h"

#include "app_config.h"
#include "board/board_pins.h"
#include "app_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define TAG "CMD_TRANSPORT"

/* RX ring-buffer size allocated for the peripheral driver (bytes). */
#define TRANSPORT_RX_BUF    512

/* TX ring-buffer size; 0 = blocking TX (no ring buffer).
 * Sufficient for short command responses sent from a single task. */
#define TRANSPORT_TX_BUF    0

/* Max bytes consumed per single uart_read_bytes() / usb_serial_jtag_read_bytes() call. */
#define TRANSPORT_READ_CHUNK    64

/* =====================================================================
 * UART transport  (APP_COMMAND_ENDPOINT_UART0 / UART1)
 * ===================================================================== */

#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART0) || \
    (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART1)

#include "driver/uart.h"
#include "esp_vfs_dev.h"    /* esp_vfs_dev_uart_use_driver() */

#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART0)
/* UART0: use the default pins already wired by the bootloader. */
#  define TRANSPORT_UART_NUM    UART_NUM_0
#  define TRANSPORT_TX_PIN      UART_PIN_NO_CHANGE
#  define TRANSPORT_RX_PIN      UART_PIN_NO_CHANGE
#elif (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART1)
/* UART1: use board-specific pins from the board header. */
#  define TRANSPORT_UART_NUM    UART_NUM_1
#  define TRANSPORT_TX_PIN      BOARD_UART1_TX_PIN
#  define TRANSPORT_RX_PIN      BOARD_UART1_RX_PIN
#endif

static void uart_rx_task(void *arg)
{
    uint8_t buf[TRANSPORT_READ_CHUNK];
    int len;
    (void)arg;

    while (1)
    {
        len = uart_read_bytes(TRANSPORT_UART_NUM, buf, sizeof(buf),
                              pdMS_TO_TICKS(20));
        for (int i = 0; i < len; i++)
        {
            app_command_endpoint_on_char((char)buf[i]);
        }
    }
}

void app_command_transport_init(void)
{
    esp_err_t err;

    const uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    err = uart_param_config(TRANSPORT_UART_NUM, &cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "uart_param_config failed: %d", err);
        return;
    }

    err = uart_set_pin(TRANSPORT_UART_NUM,
                       TRANSPORT_TX_PIN, TRANSPORT_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "uart_set_pin failed: %d", err);
        return;
    }

    err = uart_driver_install(TRANSPORT_UART_NUM,
                              TRANSPORT_RX_BUF, TRANSPORT_TX_BUF,
                              0, NULL, 0);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "uart_driver_install failed: %d", err);
        return;
    }

#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART0)
    /* Route VFS (printf / ESP_LOG*) through the installed UART0 driver
     * so that all UART0 I/O is managed by a single driver instance. */
    esp_vfs_dev_uart_use_driver(TRANSPORT_UART_NUM);
#endif

    BaseType_t ret = xTaskCreate(uart_rx_task, "cmd_transport_rx",
                                 2048, NULL, 5, NULL);
    if (ret != pdPASS)
    {
        APP_LOGE(TAG, "xTaskCreate failed for cmd_transport_rx");
        return;
    }

    APP_LOGI(TAG, "UART%d transport initialised", (int)TRANSPORT_UART_NUM);
}

void app_command_transport_write(const char *text)
{
    if (text != NULL)
    {
        uart_write_bytes(TRANSPORT_UART_NUM, text, strlen(text));
    }
}

/* =====================================================================
 * USB CDC transport  (APP_COMMAND_ENDPOINT_USB_CDC, ESP32-S3)
 *
 * Requires the USB Serial/JTAG driver (driver/usb_serial_jtag.h).
 * The secondary USB Serial JTAG console must be disabled in sdkconfig
 * (CONFIG_ESP_CONSOLE_SECONDARY_NONE=y) to avoid conflicts with the
 * driver; see sdkconfig.esp32s3.
 * ===================================================================== */

#elif (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_USB_CDC)

#include "driver/usb_serial_jtag.h"

static void cdc_rx_task(void *arg)
{
    uint8_t buf[TRANSPORT_READ_CHUNK];
    int len;
    (void)arg;

    while (1)
    {
        len = usb_serial_jtag_read_bytes(buf, sizeof(buf),
                                         pdMS_TO_TICKS(20));
        for (int i = 0; i < len; i++)
        {
            app_command_endpoint_on_char((char)buf[i]);
        }
    }
}

void app_command_transport_init(void)
{
    const usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = TRANSPORT_RX_BUF,
        .tx_buffer_size = 256,
    };

    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "usb_serial_jtag_driver_install failed: %d", err);
        return;
    }

    BaseType_t ret = xTaskCreate(cdc_rx_task, "cmd_transport_rx",
                                 2048, NULL, 5, NULL);
    if (ret != pdPASS)
    {
        APP_LOGE(TAG, "xTaskCreate failed for cmd_transport_rx");
        return;
    }

    APP_LOGI(TAG, "USB CDC transport initialised");
}

void app_command_transport_write(const char *text)
{
    if (text != NULL)
    {
        usb_serial_jtag_write_bytes(text, strlen(text), pdMS_TO_TICKS(20));
    }
}

#else
#error "Unsupported APP_COMMAND_ENDPOINT value in app_command_transport.c"
#endif
