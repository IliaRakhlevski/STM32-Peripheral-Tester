#include "app_response_queue.h"
#include "app_response_sender.h"
#include "app_config.h"
#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "io_tools.h"
#include <stddef.h>
#include <stdio.h>


/* Response queue configuration. */
#define APP_RESPONSE_QUEUE_LENGTH        10U
#define APP_RESPONSE_QUEUE_SEND_WAIT_MS  100U

/* Queue used to deliver test results for transmission. */
static QueueHandle_t app_response_queue_handle = NULL;

/**
 * @brief Initializes the response queue and sender task.
 *
 * Creates the response queue and starts the response sender task.
 */
void app_response_queue_init(void)
{
    app_response_queue_handle = xQueueCreate(APP_RESPONSE_QUEUE_LENGTH, sizeof(app_protocol_response_t));

    if (app_response_queue_handle == NULL)
    {
    	APP_LOG("FATAL: Failed to create response queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_response_sender_task,
                    "RespSender",
                    APP_TASK_STACK_SIZE_WORDS,
                    NULL,
                    APP_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
    	APP_LOG("FATAL: Failed to create response sender task\r\n");
        Error_Handler();
    }
}

/**
 * @brief Sends a response to the response queue.
 *
 * @param[in] response Pointer to response structure.
 *
 * @return APP_RESPONSE_QUEUE_OK on success, APP_RESPONSE_QUEUE_FAIL otherwise.
 */
uint8_t app_response_queue_send(const app_protocol_response_t *response)
{
    if ((response == NULL) || (app_response_queue_handle == NULL))
    {
        return APP_RESPONSE_QUEUE_FAIL;
    }

    if (xQueueSend(app_response_queue_handle, response, pdMS_TO_TICKS(APP_RESPONSE_QUEUE_SEND_WAIT_MS)) != pdPASS)
    {
        return APP_RESPONSE_QUEUE_FAIL;
    }

    return APP_RESPONSE_QUEUE_OK;
}

/**
 * @brief Receives a response from the response queue.
 *
 * @param[out] response Pointer to response structure.
 * @param[in] timeout_ms Receive timeout in milliseconds.
 *
 * @return APP_RESPONSE_QUEUE_OK on success, APP_RESPONSE_QUEUE_FAIL otherwise.
 */
uint8_t app_response_queue_receive(
    app_protocol_response_t *response,
    uint32_t timeout_ms)
{
    if ((response == NULL) || (app_response_queue_handle == NULL))
    {
        return APP_RESPONSE_QUEUE_FAIL;
    }

    if (xQueueReceive(app_response_queue_handle, response, pdMS_TO_TICKS(timeout_ms)) != pdPASS)
    {
        return APP_RESPONSE_QUEUE_FAIL;
    }

    return APP_RESPONSE_QUEUE_OK;
}
