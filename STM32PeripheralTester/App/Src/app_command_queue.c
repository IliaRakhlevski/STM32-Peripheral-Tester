#include "FreeRTOS.h"
#include "queue.h"
#include "app_command_queue.h"
#include "app_dispatcher.h"
#include "main.h"
#include "io_tools.h"
#include "app_config.h"
#include <stddef.h>
#include <stdio.h>


/* Command queue configuration. */
#define APP_COMMAND_QUEUE_LENGTH      10U
#define APP_COMMAND_QUEUE_SEND_WAIT   100U

/* Queue used to deliver generated commands to the processing task. */
static QueueHandle_t app_command_queue_handle = NULL;

/**
 * @brief Creates the application command queue.
 *
 * The queue is used as a common input channel for commands generated
 * locally or later received from UDP.
 */
void app_command_queue_init(void)
{
    app_command_queue_handle = xQueueCreate(APP_COMMAND_QUEUE_LENGTH, sizeof(app_protocol_command_t));

    if (app_command_queue_handle == NULL)
    {
    	APP_LOG("ERROR: Failed to create command queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_dispatcher_task, "Dispatcher", APP_TASK_STACK_SIZE_WORDS, NULL, APP_TASK_PRIORITY, NULL) != pdPASS)
    {
    	APP_LOG("ERROR: Failed to create dispatcher task\r\n");
        Error_Handler();
    }
}

/**
 * @brief Sends a command to the common command queue.
 *
 * @param[in] command Pointer to command to send.
 *
 * @return APP_COMMAND_QUEUE_OK on success, APP_COMMAND_QUEUE_FAIL otherwise.
 */
uint8_t app_command_queue_send(const app_protocol_command_t *command)
{
    if ((command == NULL) || (app_command_queue_handle == NULL))
    {
        return APP_COMMAND_QUEUE_FAIL;
    }

    if (xQueueSend(app_command_queue_handle, command, pdMS_TO_TICKS(APP_COMMAND_QUEUE_SEND_WAIT)) != pdPASS)
    {
        return APP_COMMAND_QUEUE_FAIL;
    }

    return APP_COMMAND_QUEUE_OK;
}

/**
 * @brief Receives a command from the common command queue.
 *
 * @param[out] command Pointer to command structure.
 * @param[in] timeout_ms Receive timeout in milliseconds.
 *
 * @return APP_COMMAND_QUEUE_OK on success, APP_COMMAND_QUEUE_FAIL otherwise.
 */
uint8_t app_command_queue_receive(app_protocol_command_t *command, uint32_t timeout_ms)
{
    if ((command == NULL) || (app_command_queue_handle == NULL))
    {
        return APP_COMMAND_QUEUE_FAIL;
    }

    if (xQueueReceive(app_command_queue_handle, command, pdMS_TO_TICKS(timeout_ms)) != pdPASS)
    {
        return APP_COMMAND_QUEUE_FAIL;
    }

    return APP_COMMAND_QUEUE_OK;
}
