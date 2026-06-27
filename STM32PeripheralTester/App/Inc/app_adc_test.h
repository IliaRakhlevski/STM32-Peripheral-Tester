#ifndef APP_ADC_TEST_H
#define APP_ADC_TEST_H

#include "app_protocol.h"

#include <stdint.h>

#define APP_ADC_TEST_FAIL    0U
#define APP_ADC_TEST_OK      1U

void app_adc_test_init(void);

uint8_t app_adc_test_send_command(const app_protocol_command_t *command);

#endif /* APP_ADC_TEST_H */
