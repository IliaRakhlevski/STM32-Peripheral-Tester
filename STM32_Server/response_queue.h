#ifndef RESPONSE_QUEUE_H
#define RESPONSE_QUEUE_H

#include <stdint.h>
#include <time.h>

#include "app_protocol.h"

/**
 * @brief Maximum number of responses that can be stored in the queue.
 */
#define RESPONSE_QUEUE_CAPACITY 128U

/**
 * @brief Response queue operation status.
 */
typedef enum
{
    RESPONSE_QUEUE_OK = 0U,
    RESPONSE_QUEUE_FAIL
} response_queue_status_t;

/**
 * @brief Response record stored in the queue.
 *
 * Contains the received test response together with the
 * timestamp when it was received by the server.
 */
typedef struct
{
    app_protocol_response_t response;
    time_t received_time;
} response_record_t;

response_queue_status_t response_queue_init(void);

response_queue_status_t response_queue_push(const response_record_t *record);

response_queue_status_t response_queue_pop(response_record_t *record);

void response_queue_shutdown(void);

#endif