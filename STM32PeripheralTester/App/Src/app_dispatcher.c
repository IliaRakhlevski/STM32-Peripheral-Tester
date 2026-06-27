#include "app_dispatcher.h"
#include "app_command_queue.h"
#include "app_protocol.h"
#include "app_timer_test.h"
#include "app_uart_test.h"
#include "app_i2c_test.h"
#include "app_spi_test.h"
#include "app_adc_test.h"
#include "io_tools.h"
#include <stdio.h>


/**
 * @brief Converts a peripheral identifier to a readable string.
 *
 * Returns a constant string corresponding to the specified
 * peripheral identifier. The string is intended for logging
 * and diagnostic messages.
 *
 * @param[in] peripheral Peripheral identifier from
 *                       app_protocol_peripheral_t.
 *
 * @return Pointer to a constant null-terminated string.
 *         Returns "UNKNOWN" if the identifier is not recognized.
 */
static const char *app_protocol_peripheral_to_string(uint8_t peripheral)
{
    switch (peripheral)
    {
        case APP_PROTOCOL_PERIPHERAL_TIMER:
            return "TIMER";

        case APP_PROTOCOL_PERIPHERAL_UART:
            return "UART";

        case APP_PROTOCOL_PERIPHERAL_SPI:
            return "SPI";

        case APP_PROTOCOL_PERIPHERAL_I2C:
            return "I2C";

        case APP_PROTOCOL_PERIPHERAL_ADC:
            return "ADC";

        default:
            return "UNKNOWN";
    }
}


/**
 * @brief Receives commands from the command queue and dispatches them.
 *
 * This task is part of the main application pipeline. It is used both
 * with generated test commands and, later, with real UDP commands.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
void app_dispatcher_task(void *argument)
{
    app_protocol_command_t command;

    (void)argument;

    for (;;)
    {
        if (app_command_queue_receive(&command, APP_DISPATCHER_WAIT_FOREVER_MS) != APP_COMMAND_QUEUE_OK)
        {
        	APP_LOG("Dispatcher receive failed\r\n");
            continue;
        }

        APP_LOG("Command received: test_id=%lu peripheral=%s iterations=%u payload=%u bytes\r\n",
               command.test_id,
			   app_protocol_peripheral_to_string(command.peripheral),
               command.iterations,
               command.pattern_len);

        switch(command.peripheral)
        {
        case APP_PROTOCOL_PERIPHERAL_TIMER:
				if (app_timer_test_send_command(&command) != APP_TIMER_TEST_OK)
				{
					APP_LOG("Failed to send timer command\r\n");
				}
				break;
        case APP_PROTOCOL_PERIPHERAL_UART:

                if(app_uart_test_send_command(&command) != APP_UART_TEST_OK)
                {
                	APP_LOG("Failed to send UART command\r\n");
                }
                break;

        case APP_PROTOCOL_PERIPHERAL_SPI:

                if(app_spi_test_send_command(&command) != APP_SPI_TEST_OK)
				{
                	APP_LOG("Failed to send SPI command\r\n");
				}
                break;

        case APP_PROTOCOL_PERIPHERAL_I2C:

        		if(app_i2c_test_send_command(&command) != APP_I2C_TEST_OK)
        		{
        			APP_LOG("Failed to send I2C command\r\n");
        		}
                break;

        case APP_PROTOCOL_PERIPHERAL_ADC:

        		if(app_adc_test_send_command(&command) != APP_ADC_TEST_OK)
        		{
        			APP_LOG("Failed to send ADC command\r\n");
        		}
                break;

        default:

        	APP_LOG("Unknown peripheral: %u\r\n", command.peripheral);
                break;
        }
    }
}
