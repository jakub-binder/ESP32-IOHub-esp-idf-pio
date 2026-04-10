#include "app_command_transport.h"
#include "app_command_endpoint.h"
#include "app_commands.h"

#include "app_config.h"
#include "board/board_pins.h"
#include "app_log.h"
#include "app_serial_routing.h"

#include "driver/uart.h"
#include "driver/uart_vfs.h"
#if APP_DIAG_USBJTAG
#include "driver/gpio.h"
#endif
#if CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED
#include "driver/usb_serial_jtag.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define TAG "CMD_TRANSPORT"

/* Ring-buffer sizes for UART drivers. */
#define TRANSPORT_RX_BUF        512

/* TX ring-buffer size; 0 = blocking TX (no ring buffer).
 * Sufficient for short command responses sent from a single task. */
#define TRANSPORT_TX_BUF        0

/* Max bytes consumed per single uart_read_bytes() call. */
#define TRANSPORT_READ_CHUNK    64
#define TRANSPORT_DIAG_LOG_PERIOD_MS       2000
#define TRANSPORT_DIAG_TASK_STACK_SIZE     3072
#define TRANSPORT_DIAG_TASK_PRIORITY       1
/* Keep same sizing as UART RX tasks for comparable runtime footprint. */
#define TRANSPORT_USB_DIAG_STACK_SIZE      4096
#define TRANSPORT_USB_DIAG_PRIORITY        5
#define TRANSPORT_USB_RX_BUF               256
#define TRANSPORT_USB_TX_BUF               256
#if APP_DIAG_USBJTAG
#define TRANSPORT_USB_HEARTBEAT_PERIOD_MS  1000
#define TRANSPORT_USB_HEARTBEAT_STACK_SIZE 3072
#define TRANSPORT_USB_HEARTBEAT_PRIORITY   1
#endif
#define TRANSPORT_USB_RX_TIMEOUT_MS        20

static uint32_t s_uart0_rx_bytes;
static uint32_t s_uart1_rx_bytes;
static uint32_t s_usb_rx_bytes;

/* =====================================================================
 * Static endpoint instances – one per active port.
 * ===================================================================== */

static app_command_endpoint_t s_ep_debug;   /* UART0 – debug port   */
static app_command_endpoint_t s_ep_prod;    /* UART1 – production port */
static app_command_endpoint_t s_ep_usb;     /* USB Serial JTAG – optional debug port */

/* =====================================================================
 * Output callbacks – one per port.
 * ===================================================================== */

static void transport_write_uart0(const char *text)
{
    if (text != NULL)
    {
        uart_write_bytes(UART_NUM_0, text, strlen(text));
    }
}

static void transport_write_uart1(const char *text)
{
    if (text != NULL)
    {
        uart_write_bytes(UART_NUM_1, text, strlen(text));
    }
}

#if CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED
static void transport_write_usb_serial_jtag(const char *text)
{
    if (text != NULL)
    {
        usb_serial_jtag_write_bytes((const uint8_t *)text, strlen(text), pdMS_TO_TICKS(20));
    }
}
#endif

/* =====================================================================
 * RX tasks – one per port.
 * ===================================================================== */

static void uart0_rx_task(void *arg)
{
    uint8_t buf[TRANSPORT_READ_CHUNK];
    int len;
    (void)arg;

    while (1)
    {
        len = uart_read_bytes(UART_NUM_0, buf, sizeof(buf),
                              pdMS_TO_TICKS(20));
        if (len > 0)
        {
            __atomic_fetch_add(&s_uart0_rx_bytes, (uint32_t)len, __ATOMIC_RELAXED);
        }
        for (int i = 0; i < len; i++)
        {
            app_command_endpoint_on_char(&s_ep_debug, (char)buf[i]);
        }
    }
}

static void uart1_rx_task(void *arg)
{
    uint8_t buf[TRANSPORT_READ_CHUNK];
    int len;
    (void)arg;

    while (1)
    {
        len = uart_read_bytes(UART_NUM_1, buf, sizeof(buf),
                              pdMS_TO_TICKS(20));
        if (len > 0)
        {
            __atomic_fetch_add(&s_uart1_rx_bytes, (uint32_t)len, __ATOMIC_RELAXED);
        }
        for (int i = 0; i < len; i++)
        {
            app_command_endpoint_on_char(&s_ep_prod, (char)buf[i]);
        }
    }
}

static void cmd_transport_diag_task(void *arg)
{
    uint32_t prev_uart0 = 0;
    uint32_t prev_uart1 = 0;
    uint32_t prev_usb   = 0;
    (void)arg;

    while (1)
    {
        uint32_t now_uart0 = __atomic_load_n(&s_uart0_rx_bytes, __ATOMIC_RELAXED);
        uint32_t now_uart1 = __atomic_load_n(&s_uart1_rx_bytes, __ATOMIC_RELAXED);
        uint32_t now_usb   = __atomic_load_n(&s_usb_rx_bytes, __ATOMIC_RELAXED);

        APP_LOGI(TAG,
                 "diag rx bytes/2s: UART0=%lu(+%lu) UART1=%lu(+%lu) USB_SERIAL_JTAG=%lu(+%lu)",
                 (unsigned long)now_uart0, (unsigned long)(now_uart0 - prev_uart0),
                 (unsigned long)now_uart1, (unsigned long)(now_uart1 - prev_uart1),
                 (unsigned long)now_usb, (unsigned long)(now_usb - prev_usb));

        prev_uart0 = now_uart0;
        prev_uart1 = now_uart1;
        prev_usb = now_usb;

        vTaskDelay(pdMS_TO_TICKS(TRANSPORT_DIAG_LOG_PERIOD_MS));
    }
}

#if CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED
#if APP_DIAG_USBJTAG
static bool cmd_transport_is_valid_led_pin(int pin)
{
    return (pin >= 0) && GPIO_IS_VALID_OUTPUT_GPIO(pin);
}

#if BOARD_HAS_USER_LED
static void cmd_transport_diag_led_init(void)
{
    uint64_t pin_mask = 0;
    if (cmd_transport_is_valid_led_pin(BOARD_USER_LED_PIN))
    {
        pin_mask |= (1ULL << BOARD_USER_LED_PIN);
    }
    if (cmd_transport_is_valid_led_pin(BOARD_USER_LED2_PIN))
    {
        pin_mask |= (1ULL << BOARD_USER_LED2_PIN);
    }
    if (pin_mask == 0)
    {
        return;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&cfg);
    if (cmd_transport_is_valid_led_pin(BOARD_USER_LED_PIN))
    {
        (void)gpio_set_level(BOARD_USER_LED_PIN, 0);
    }
    if (cmd_transport_is_valid_led_pin(BOARD_USER_LED2_PIN))
    {
        (void)gpio_set_level(BOARD_USER_LED2_PIN, 0);
    }
}

static void cmd_transport_diag_led_toggle_heartbeat(void)
{
    static bool heartbeat_on = false;
    heartbeat_on = !heartbeat_on;
    if (cmd_transport_is_valid_led_pin(BOARD_USER_LED_PIN))
    {
        (void)gpio_set_level(BOARD_USER_LED_PIN, heartbeat_on ? 1 : 0);
    }
}

static void cmd_transport_diag_led_toggle_rx(void)
{
    static bool rx_on = false;
    rx_on = !rx_on;
    if (cmd_transport_is_valid_led_pin(BOARD_USER_LED2_PIN))
    {
        (void)gpio_set_level(BOARD_USER_LED2_PIN, rx_on ? 1 : 0);
    }
    else if (cmd_transport_is_valid_led_pin(BOARD_USER_LED_PIN))
    {
        (void)gpio_set_level(BOARD_USER_LED_PIN, rx_on ? 1 : 0);
    }
}
#else
static void cmd_transport_diag_led_init(void) {}
static void cmd_transport_diag_led_toggle_heartbeat(void) {}
static void cmd_transport_diag_led_toggle_rx(void) {}
#endif

static void cmd_transport_usb_heartbeat_task(void *arg)
{
    (void)arg;

    while (1)
    {
        transport_write_usb_serial_jtag("USBJTAG alive\r\n");
        cmd_transport_diag_led_toggle_heartbeat();
        vTaskDelay(pdMS_TO_TICKS(TRANSPORT_USB_HEARTBEAT_PERIOD_MS));
    }
}
#endif

static void usb_serial_jtag_rx_task(void *arg)
{
    uint8_t buf[TRANSPORT_READ_CHUNK];
    int len;
    (void)arg;

    while (1)
    {
        len = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(TRANSPORT_USB_RX_TIMEOUT_MS));
        if (len > 0)
        {
            __atomic_fetch_add(&s_usb_rx_bytes, (uint32_t)len, __ATOMIC_RELAXED);
#if APP_DIAG_USBJTAG
            cmd_transport_diag_led_toggle_rx();
#endif
            for (int i = 0; i < len; i++)
            {
                app_command_endpoint_on_char(&s_ep_usb, (char)buf[i]);
            }
        }
    }
}

static void cmd_transport_init_usb_endpoint(void)
{
    const usb_serial_jtag_driver_config_t usb_cfg = {
        .tx_buffer_size = TRANSPORT_USB_TX_BUF,
        .rx_buffer_size = TRANSPORT_USB_RX_BUF,
    };
    esp_err_t err = usb_serial_jtag_driver_install(&usb_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "usb_serial_jtag_driver_install failed: %d", err);
        APP_LOGE(TAG, "commands over USB Serial JTAG will not work");
        return;
    }

    {
        const app_command_ctx_t ctx = {
            .source               = APP_CMD_SOURCE_USB_CDC,
            .output               = transport_write_usb_serial_jtag,
            .allow_debug_commands = true,
        };
        app_command_endpoint_init_ex(&s_ep_usb, ctx);
    }

#if APP_DIAG_USBJTAG
    cmd_transport_diag_led_init();

    BaseType_t hb_ret = xTaskCreate(cmd_transport_usb_heartbeat_task, "usb_hb_tx",
                                    TRANSPORT_USB_HEARTBEAT_STACK_SIZE, NULL,
                                    TRANSPORT_USB_HEARTBEAT_PRIORITY, NULL);
    if (hb_ret != pdPASS)
    {
        APP_LOGE(TAG, "xTaskCreate failed for usb_hb_tx: %d", (int)hb_ret);
    }
#endif

    BaseType_t ret = xTaskCreate(usb_serial_jtag_rx_task, "cmd_rx_usbjtag",
                                 TRANSPORT_USB_DIAG_STACK_SIZE, NULL,
                                 TRANSPORT_USB_DIAG_PRIORITY, NULL);
    if (ret != pdPASS)
    {
        APP_LOGE(TAG, "xTaskCreate failed for cmd_rx_usbjtag: %d", (int)ret);
        APP_LOGE(TAG, "commands over USB Serial JTAG will not work");
    }
    else
    {
        APP_LOGI(TAG, "command endpoint initialized: USB Serial JTAG");
    }
}
#endif

/* =====================================================================
 * UART initialisation helper.
 * ===================================================================== */

static bool init_uart(uart_port_t port, int tx_pin, int rx_pin,
                      const char *name)
{
    const uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    esp_err_t err;

    err = uart_param_config(port, &cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "%s uart_param_config failed: %d", name, err);
        return false;
    }

    err = uart_set_pin(port, tx_pin, rx_pin,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "%s uart_set_pin failed: %d", name, err);
        return false;
    }

    err = uart_driver_install(port, TRANSPORT_RX_BUF, TRANSPORT_TX_BUF,
                              0, NULL, 0);
    if (err != ESP_OK)
    {
        APP_LOGE(TAG, "%s uart_driver_install failed: %d", name, err);
        return false;
    }

    return true;
}

/* =====================================================================
 * Public API
 * ===================================================================== */

void app_command_transport_init(void)
{
    APP_LOGI(TAG, "active command endpoint: %s", app_serial_endpoint_to_string(APP_SERIAL_COMMAND_ENDPOINT));

    /* ---------------------------------------------------------------
     * Debug port: UART0
     * allow_debug_commands = true
     * --------------------------------------------------------------- */
    {
        const app_command_ctx_t ctx = {
            .source               = APP_CMD_SOURCE_UART0,
            .output               = transport_write_uart0,
            .allow_debug_commands = true,
        };
        app_command_endpoint_init_ex(&s_ep_debug, ctx);
    }

    if (init_uart(UART_NUM_0,
                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                  "UART0"))
    {
        /* Route VFS (printf / ESP_LOG*) through the installed UART0 driver
         * so that all UART0 I/O is managed by a single driver instance. */
        uart_vfs_dev_use_driver(UART_NUM_0);

        BaseType_t ret = xTaskCreate(uart0_rx_task, "cmd_rx_uart0",
                                     4096, NULL, 5, NULL);
        if (ret != pdPASS)
        {
            APP_LOGE(TAG, "xTaskCreate failed for cmd_rx_uart0");
        }
        else
        {
            APP_LOGI(TAG, "debug port: UART0 initialised");
        }
    }

    /* ---------------------------------------------------------------
     * Production port: UART1
     * allow_debug_commands = false
     * Each port is initialised independently: a failure on the debug
     * port must not prevent the production port from starting, and
     * vice versa.
     * --------------------------------------------------------------- */
    {
        const app_command_ctx_t ctx = {
            .source               = APP_CMD_SOURCE_UART1,
            .output               = transport_write_uart1,
            .allow_debug_commands = false,
        };
        app_command_endpoint_init_ex(&s_ep_prod, ctx);
    }

    if (init_uart(UART_NUM_1,
                  BOARD_UART1_TX_PIN, BOARD_UART1_RX_PIN,
                  "UART1"))
    {
        BaseType_t ret = xTaskCreate(uart1_rx_task, "cmd_rx_uart1",
                                     4096, NULL, 5, NULL);
        if (ret != pdPASS)
        {
            APP_LOGE(TAG, "xTaskCreate failed for cmd_rx_uart1");
        }
        else
        {
            APP_LOGI(TAG, "production port: UART1 initialised");
        }
    }

    BaseType_t diag_ret = xTaskCreate(cmd_transport_diag_task, "transport_diag",
                                      TRANSPORT_DIAG_TASK_STACK_SIZE, NULL,
                                      TRANSPORT_DIAG_TASK_PRIORITY, NULL);
    if (diag_ret != pdPASS)
    {
        APP_LOGE(TAG, "xTaskCreate failed for transport_diag");
    }

#if (APP_SERIAL_COMMAND_ENDPOINT == APP_SERIAL_ENDPOINT_USB_CDC)
    APP_LOGI(TAG, "APP_COMMAND_ENDPOINT_USB_CDC is implemented via USB Serial JTAG on ESP32-S3");
#if CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED
    cmd_transport_init_usb_endpoint();
#else
    APP_LOGE(TAG, "USB Serial JTAG not supported by current target/config");
    APP_LOGE(TAG, "commands over USB will not work");
#endif
#endif

    /* ---------------------------------------------------------------
     * APP_COMMAND_ENDPOINT_USB_CDC currently maps to USB Serial JTAG
     * transport on ESP32-S3 (single COM port, VID:PID 303A:1001).
     * --------------------------------------------------------------- */
}
