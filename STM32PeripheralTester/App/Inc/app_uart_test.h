#ifndef APP_UART_TEST_H
#define APP_UART_TEST_H

#include "app_protocol.h"

#include <stdint.h>

#define APP_UART_TEST_FAIL    0U
#define APP_UART_TEST_OK      1U

void app_uart_test_init(void);

uint8_t app_uart_test_send_command(const app_protocol_command_t *command);

#endif /* APP_UART_TEST_H */
