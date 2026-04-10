#ifndef APP_SYSTEM_INFO_H
#define APP_SYSTEM_INFO_H

#include "app_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print system information in KEY=VALUE format.
 * Each key/value pair is written as a separate line via the output callback.
 *
 * @param output  Command output callback; no-op when NULL.
 */
void app_system_info_print(app_command_output_fn output);

#ifdef __cplusplus
}
#endif

#endif /* APP_SYSTEM_INFO_H */
