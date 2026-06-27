#ifndef APP_COMMAND_GENERATOR_H
#define APP_COMMAND_GENERATOR_H

#include "app_protocol.h"

void app_command_generator_build(app_protocol_command_t *command, app_protocol_peripheral_t peripheral);

void app_command_generator_task(void *argument);

#endif /* APP_COMMAND_GENERATOR_H */
