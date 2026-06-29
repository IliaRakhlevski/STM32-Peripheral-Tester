#include "app_timer_test.h"
#include "app_config.h"
#include "app_response_queue.h"
#include "app_protocol.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "main.h"
#include "io_tools.h"
#include <stddef.h>
#include <stdio.h>


/* Timer test queue configuration. */
#define APP_TIMER_QUEUE_LENGTH          10U
#define APP_TIMER_QUEUE_SEND_WAIT_MS    100U

/* Timer test timing parameters. */
#define APP_TIMER_TEST_DELAY_MS         100U
#define APP_TIMER_TEST_EXPECTED_US      100000U
#define APP_TIMER_TEST_TOLERANCE_US     1000U

/* Hardware timer used by the test. */
#define APP_TIMER_TEST_HW_TIMER         htim2

/* Queue used to receive timer test requests. */
static QueueHandle_t app_timer_queue_handle = NULL;

static void app_timer_test_task(void *argument);


/**
 * @brief Returns current TIM2 counter value in microseconds.
 *
 * TIM2 is configured to run at 1 MHz, therefore one counter tick
 * corresponds to one microsecond.
 *
 * @return Current timer value in microseconds.
 */
static inline uint32_t app_timer_time_us(void)
{
    return __HAL_TIM_GET_COUNTER(&APP_TIMER_TEST_HW_TIMER);
}

/**
 * @brief Executes a timer test command.
 *
 * This function performs the timer test associated with the
 * received command and reports the result.
 *
 * @param[in] command Pointer to timer test command.
 */
static void app_timer_test_run(const app_protocol_command_t *command)
{
	uint32_t start_us;
	uint32_t end_us;
	uint32_t delta_us;
	uint8_t i;
	app_protocol_response_t response;
	uint8_t test_passed = 1U;

	if (command == NULL)
	    return;

	response.test_id = command->test_id;
	response.result = APP_PROTOCOL_TEST_SUCCESS;

	APP_LOG_DEBUG("Running timer test %lu\r\n", command->test_id);

    for ( i = 0U; i < command->iterations; i++)
	{
		start_us = app_timer_time_us();

		vTaskDelay(pdMS_TO_TICKS(APP_TIMER_TEST_DELAY_MS));

		end_us = app_timer_time_us();
		delta_us = end_us - start_us;

		APP_LOG_DEBUG("Timer iteration %u delta = %lu us\r\n", (uint8_t)(i + 1U), delta_us);

		if ((delta_us < (APP_TIMER_TEST_EXPECTED_US - APP_TIMER_TEST_TOLERANCE_US)) ||
			(delta_us > (APP_TIMER_TEST_EXPECTED_US + APP_TIMER_TEST_TOLERANCE_US)))
		{
			test_passed = 0U;
			break;
		}
	}

    if (test_passed)
    {
    	APP_LOG_DEBUG("Timer test PASS\r\n");
    }
    else
    {
    	APP_LOG("Timer test FAIL\r\n");
        response.result = APP_PROTOCOL_TEST_FAIL;
    }

    if (app_response_queue_send(&response) != APP_RESPONSE_QUEUE_OK)
    {
    	APP_LOG("Failed to send timer response\r\n");
    }
}

/**
 * @brief Starts the hardware timer used by the timer test module.
 *
 * The timer is configured to run at 1 MHz, therefore one timer
 * tick corresponds to one microsecond.
 *
 * The function stops the system by calling Error_Handler()
 * if the timer cannot be started.
 */
static void app_timer_test_start_hw_timer(void)
{
    if (HAL_TIM_Base_Start(&APP_TIMER_TEST_HW_TIMER) != HAL_OK)
    {
    	APP_LOG("FATAL: Failed to start timer test hardware timer\r\n");
        Error_Handler();
    }
}

/**
 * @brief Initializes the timer test module.
 *
 * Creates the timer command queue and starts the timer test task.
 */
void app_timer_test_init(void)
{
	app_timer_test_start_hw_timer();

    app_timer_queue_handle = xQueueCreate(APP_TIMER_QUEUE_LENGTH, sizeof(app_protocol_command_t));

    if (app_timer_queue_handle == NULL)
    {
    	APP_LOG("FATAL: Failed to create timer test queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_timer_test_task, "TimerTest",APP_TASK_STACK_SIZE_WORDS, NULL, APP_TASK_PRIORITY, NULL) != pdPASS)
    {
    	APP_LOG("FATAL: Failed to create timer test task\r\n");
        Error_Handler();
    }
}

/**
 * @brief Timer test task.
 *
 * Waits for timer test commands and handles them one by one.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
static void app_timer_test_task(void *argument)
{
    app_protocol_command_t command;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(app_timer_queue_handle, &command, portMAX_DELAY) == pdPASS)
        {
        	APP_LOG_DEBUG("Timer task received: test_id=%lu iterations=%u len=%u\r\n",
						   command.test_id,
						   command.iterations,
						   command.pattern_len);

        	app_timer_test_run(&command);
        }
    }
}

/**
 * @brief Sends a timer test command to the timer test task.
 *
 * @param[in] command Pointer to command to send.
 *
 * @return 1U on success, 0U otherwise.
 */
uint8_t app_timer_test_send_command(const app_protocol_command_t *command)
{
    if ((command == NULL) || (app_timer_queue_handle == NULL))
    {
        return APP_TIMER_TEST_FAIL;
    }

    if (xQueueSend(app_timer_queue_handle, command, pdMS_TO_TICKS(APP_TIMER_QUEUE_SEND_WAIT_MS)) != pdPASS)
    {
        return APP_TIMER_TEST_FAIL;
    }

    return APP_TIMER_TEST_OK;
}
