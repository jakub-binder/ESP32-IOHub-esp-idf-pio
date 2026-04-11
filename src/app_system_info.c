#include "app_system_info.h"

#include <stdio.h>

#include "app_config.h"
#include "app_serial_routing.h"
#include "board/board_pins.h"
#include "fixtures/fixture.h"

void app_system_info_print(app_command_output_fn output)
{
    char line[128];
    const fixture_t *fixture;

    if (output == NULL)
    {
        return;
    }

    snprintf(line, sizeof(line), "FW_BOARD_NAME=%s\r\n", FW_BOARD_NAME);
    output(line);

    snprintf(line, sizeof(line), "BOARD_NAME=%s\r\n", BOARD_NAME);
    output(line);

    output("DEBUG_PORT=UART0\r\n");
    output("PROD_PORT=UART1\r\n");

    snprintf(line, sizeof(line), "UART1_RX=%d\r\n", BOARD_UART1_RX_PIN);
    output(line);

    snprintf(line, sizeof(line), "UART1_TX=%d\r\n", BOARD_UART1_TX_PIN);
    output(line);

    snprintf(line, sizeof(line), "I2C1_SDA=%d\r\n", BOARD_I2C1_SDA_PIN);
    output(line);

    snprintf(line, sizeof(line), "I2C1_SCL=%d\r\n", BOARD_I2C1_SCL_PIN);
    output(line);

    snprintf(line, sizeof(line), "SERIAL_CMD=%s\r\n",
             app_serial_endpoint_to_string(APP_SERIAL_COMMAND_ENDPOINT));
    output(line);

    fixture = fixture_get_selected();
    if (fixture != NULL && fixture->name != NULL)
    {
        snprintf(line, sizeof(line), "FIXTURE=%s\r\n", fixture->name);
        output(line);
    }
}
