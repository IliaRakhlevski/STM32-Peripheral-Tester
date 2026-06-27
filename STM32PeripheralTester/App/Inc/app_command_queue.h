#ifndef APP_COMMAND_QUEUE_H
#define APP_COMMAND_QUEUE_H

#include "app_protocol.h"

#include <stdint.h>

#define APP_DISPATCHER_WAIT_FOREVER_MS    	0xFFFFFFFFUL
#define APP_COMMAND_QUEUE_OK          		1U
#define APP_COMMAND_QUEUE_FAIL        		0U

void app_command_queue_init(void);

uint8_t app_command_queue_send(const app_protocol_command_t *command);

uint8_t app_command_queue_receive(app_protocol_command_t *command, uint32_t timeout_ms);

#endif /* APP_COMMAND_QUEUE_H */
