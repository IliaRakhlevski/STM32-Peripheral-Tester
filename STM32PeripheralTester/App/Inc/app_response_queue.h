#ifndef APP_RESPONSE_QUEUE_H
#define APP_RESPONSE_QUEUE_H

#include "app_protocol.h"

#include <stdint.h>

#define APP_RESPONSE_QUEUE_FAIL    0U
#define APP_RESPONSE_QUEUE_OK      1U

void app_response_queue_init(void);

uint8_t app_response_queue_send(const app_protocol_response_t *response);

uint8_t app_response_queue_receive(app_protocol_response_t *response, uint32_t timeout_ms);

#endif /* APP_RESPONSE_QUEUE_H */
