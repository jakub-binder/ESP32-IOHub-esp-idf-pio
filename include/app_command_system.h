#ifndef APP_COMMAND_SYSTEM_H
#define APP_COMMAND_SYSTEM_H

#include "app_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal header – used only by app_commands.c to call into app_command_system.c. */

void app_command_system_firmware(app_command_output_fn output);
void app_command_system_restart(app_command_output_fn output);
void app_command_system_init(app_command_output_fn output);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMAND_SYSTEM_H */
