#ifndef COMMAND_GENERATOR_H
#define COMMAND_GENERATOR_H

#include <stdlib.h>
#include "app_protocol.h"


void command_generator_init(void);
void command_generator_build_next(app_protocol_command_t *command);
uint8_t command_generator_random_range(uint8_t min_value, uint8_t max_value);

#endif