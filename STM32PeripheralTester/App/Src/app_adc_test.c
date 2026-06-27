#include "app_adc_test.h"
#include "app_config.h"
#include "app_response_queue.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "io_tools.h"
#include <stddef.h>
#include <stdio.h>


/* ADC test queue configuration. */
#define APP_ADC_QUEUE_LENGTH             10U
#define APP_ADC_QUEUE_SEND_WAIT_MS       100U

/* ADC and DAC handles used by the test. */
#define APP_ADC_TEST_ADC                 hadc1
#define APP_ADC_TEST_DAC                 hdac

/* ADC test timing configuration. */
#define APP_ADC_TEST_TIMEOUT_MS          1000U
#define APP_ADC_TEST_DAC_SETTLE_MS       5U

/* Allowed raw ADC difference between expected and measured values. */
#define APP_ADC_TEST_TOLERANCE_RAW       50U

/* DAC output values used as known ADC input levels. */
static const uint16_t app_adc_test_values[] =
{
    128U,
    512U,
    1024U,
    2048U,
    3072U,
    3584U,
    3968U
};

#define APP_ADC_TEST_NUM_VALUES          (sizeof(app_adc_test_values) / sizeof(app_adc_test_values[0]))

/* Queue used to receive ADC conversion results from the callback. */
static QueueHandle_t app_adc_queue_handle = NULL;

static void app_adc_test_task(void *argument);
static void app_adc_test_run(const app_protocol_command_t *command);
static uint8_t app_adc_test_run_polling(const app_protocol_command_t *command);


/**
 * @brief Initializes the ADC test module.
 *
 * Creates the ADC command queue and starts the ADC test task.
 */
void app_adc_test_init(void)
{
    app_adc_queue_handle = xQueueCreate(APP_ADC_QUEUE_LENGTH, sizeof(app_protocol_command_t));

    if (app_adc_queue_handle == NULL)
    {
    	APP_LOG("FATAL: Failed to create ADC test queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_adc_test_task,
                    "ADCTest",
                    APP_TASK_STACK_SIZE_WORDS,
                    NULL,
                    APP_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
    	APP_LOG("FATAL: Failed to create ADC test task\r\n");
        Error_Handler();
    }
}

/**
 * @brief Sends an ADC test command to the ADC test task.
 *
 * @param[in] command Pointer to command to send.
 *
 * @return APP_ADC_TEST_OK on success, APP_ADC_TEST_FAIL otherwise.
 */
uint8_t app_adc_test_send_command(const app_protocol_command_t *command)
{
    if ((command == NULL) || (app_adc_queue_handle == NULL))
    {
        return APP_ADC_TEST_FAIL;
    }

    if (xQueueSend(app_adc_queue_handle,
                   command,
                   pdMS_TO_TICKS(APP_ADC_QUEUE_SEND_WAIT_MS)) != pdPASS)
    {
        return APP_ADC_TEST_FAIL;
    }

    return APP_ADC_TEST_OK;
}

/**
 * @brief ADC test task.
 *
 * Waits for ADC test commands and executes them one by one.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
static void app_adc_test_task(void *argument)
{
    app_protocol_command_t command;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(app_adc_queue_handle,
                          &command,
                          portMAX_DELAY) == pdPASS)
        {
        	APP_LOG_DEBUG("ADC task received: test_id=%lu iterations=%u len=%u\r\n",
						   command.test_id,
						   command.iterations,
						   command.pattern_len);

            app_adc_test_run(&command);
        }
    }
}

/**
 * @brief Executes an ADC test command and sends a response.
 *
 * Runs the ADC test backend and places the final test result
 * into the response queue.
 *
 * @param[in] command Pointer to ADC test command.
 */
static void app_adc_test_run(const app_protocol_command_t *command)
{
    app_protocol_response_t response;

    if (command == NULL)
        return;

    response.test_id = command->test_id;
    response.result = app_adc_test_run_polling(command);

    if (app_response_queue_send(&response) != APP_RESPONSE_QUEUE_OK)
    {
    	APP_LOG("Failed to send ADC response\r\n");
    }
}

/**
 * @brief Executes ADC test using polling mode.
 *
 * @param[in] command Pointer to ADC test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success,
 *         APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_adc_test_run_polling(const app_protocol_command_t *command)
{
	uint16_t dac_value;
	uint32_t adc_value;
	//int32_t difference;

    if (command == NULL)
        return APP_PROTOCOL_TEST_FAIL;

    APP_LOG_DEBUG("Running ADC polling test %lu\r\n", command->test_id);

    if (HAL_DAC_Start(&APP_ADC_TEST_DAC, DAC_CHANNEL_1) != HAL_OK)
    {
       APP_LOG("ADC test FAIL: DAC start failed\r\n");
	   return APP_PROTOCOL_TEST_FAIL;
    }

    for (uint8_t iteration = 0U; iteration < command->iterations; iteration++)
	{
    	APP_LOG_DEBUG("ADC iteration %u\r\n", (uint8_t)(iteration + 1U));

		for (uint8_t value_index = 0U; value_index < APP_ADC_TEST_NUM_VALUES; value_index++)
		{
			dac_value = app_adc_test_values[value_index];

			if (HAL_DAC_SetValue(&APP_ADC_TEST_DAC,
								 DAC_CHANNEL_1,
								 DAC_ALIGN_12B_R,
								 dac_value) != HAL_OK)
			{
				APP_LOG("ADC iteration %u FAIL: DAC set failed\r\n", (uint8_t)(iteration + 1U));
				return APP_PROTOCOL_TEST_FAIL;
			}

			vTaskDelay(pdMS_TO_TICKS(APP_ADC_TEST_DAC_SETTLE_MS));

			if (HAL_ADC_Start(&APP_ADC_TEST_ADC) != HAL_OK)
			{
				APP_LOG("ADC iteration %u FAIL: ADC start failed\r\n", (uint8_t)(iteration + 1U));
				return APP_PROTOCOL_TEST_FAIL;
			}

			if (HAL_ADC_PollForConversion(&APP_ADC_TEST_ADC, APP_ADC_TEST_TIMEOUT_MS) != HAL_OK)
			{
				(void)HAL_ADC_Stop(&APP_ADC_TEST_ADC);

				APP_LOG("ADC iteration %u FAIL: ADC timeout\r\n", (uint8_t)(iteration + 1U));
				return APP_PROTOCOL_TEST_FAIL;
			}

			adc_value = HAL_ADC_GetValue(&APP_ADC_TEST_ADC);

			(void)HAL_ADC_Stop(&APP_ADC_TEST_ADC);

			if ((adc_value > ((uint32_t)dac_value + APP_ADC_TEST_TOLERANCE_RAW)) ||
				(adc_value + APP_ADC_TEST_TOLERANCE_RAW < (uint32_t)dac_value))
			{
				APP_LOG("ADC iteration %u FAIL: DAC=%lu ADC=%lu DIFF=%ld TOL=%u\r\n",
					    (uint8_t)(iteration + 1U),
					    dac_value,
					    adc_value,
					    (int32_t)adc_value - (int32_t)dac_value,
					    APP_ADC_TEST_TOLERANCE_RAW);

				return APP_PROTOCOL_TEST_FAIL;
			}
		}

		APP_LOG_DEBUG("ADC iteration %u PASS\r\n", (uint8_t)(iteration + 1U));
	}

    return APP_PROTOCOL_TEST_SUCCESS;
}
