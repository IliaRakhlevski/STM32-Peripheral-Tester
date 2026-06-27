#ifndef APP_TIMER_TEST_H
#define APP_TIMER_TEST_H

#include "app_protocol.h"
#include "tim.h"

#define APP_TIMER_TEST_FAIL  0U
#define APP_TIMER_TEST_OK    1U


void app_timer_test_init(void);
uint8_t app_timer_test_send_command(const app_protocol_command_t *command);

#endif /* APP_TIMER_TEST_H */
