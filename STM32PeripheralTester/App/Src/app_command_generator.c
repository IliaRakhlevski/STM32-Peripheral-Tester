#include "app_command_generator.h"
#include "app_command_queue.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "io_tools.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


/**
 * @file app_command_generator.c
 * @brief Generates test commands locally for debugging.
 *
 * This module replaces the Linux UDP server by generating test commands
 * directly on the STM32. It is intended for standalone debugging when
 * the Ethernet connection or the server is unavailable.
 */


#define APP_CMD_GEN_DEFAULT_ITERATIONS    	255U
#define APP_CMD_GEN_PATTERN_LEN         	255U
#define APP_CMD_GEN_TASK_DELAY_MS    		30000U

static uint32_t app_cmd_gen_next_test_id = 1UL;

static const uint8_t app_test_peripherals[] =
{
	APP_PROTOCOL_PERIPHERAL_TIMER,
    APP_PROTOCOL_PERIPHERAL_ADC,
    APP_PROTOCOL_PERIPHERAL_UART,
	APP_PROTOCOL_PERIPHERAL_I2C,
	APP_PROTOCOL_PERIPHERAL_SPI
};

#define APP_TEST_PERIPHERALS_COUNT (sizeof(app_test_peripherals) / sizeof(app_test_peripherals[0]))


/**
 * @brief Generates a test pattern.
 *
 * Fills the supplied buffer with deterministic test data.
 *
 * @param[out] buffer Buffer to fill.
 * @param[in] length Number of bytes to generate.
 */
static void app_command_generator_fill_pattern(uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    if ((buffer == NULL) || (length == 0U))
        return;

    for (i = 0U; i < length; i++)
        buffer[i] = i;
}

/**
 * @brief Builds a simulated command for the selected peripheral.
 *
 * The generated command has the same logical structure as a command
 * that will later be received from the PC over UDP.
 *
 * @param[out] command Pointer to command structure to fill.
 * @param[in] peripheral Peripheral to be tested.
 */
void app_command_generator_build(app_protocol_command_t *command, app_protocol_peripheral_t peripheral)
{
    if (command == NULL)
        return;

    memset(command, 0, sizeof(*command));

    command->test_id = app_cmd_gen_next_test_id++;
    command->peripheral = peripheral;
    command->iterations = APP_CMD_GEN_DEFAULT_ITERATIONS;
    command->pattern_len = APP_CMD_GEN_PATTERN_LEN;

    app_command_generator_fill_pattern(command->pattern, command->pattern_len);
}

/**
 * @brief Periodically generates simulated commands and sends them to queue.
 *
 * This task temporarily replaces the future UDP command source.
 *
 * @param[in] argument FreeRTOS task argument, not used.
 */
void app_command_generator_task(void *argument)
{
    app_protocol_command_t command;
    uint8_t peripheral_index = 0U;

    (void)argument;

    for (;;)
	{
		for (peripheral_index = 0U; peripheral_index < APP_TEST_PERIPHERALS_COUNT; peripheral_index++)
		{
			app_command_generator_build(
				&command,
				app_test_peripherals[peripheral_index]);

			if (app_command_queue_send(&command) != 1U)
			{
				APP_LOG("Command queue send failed\r\n");
			}
		}

		vTaskDelay(pdMS_TO_TICKS(APP_CMD_GEN_TASK_DELAY_MS));
	}
}

