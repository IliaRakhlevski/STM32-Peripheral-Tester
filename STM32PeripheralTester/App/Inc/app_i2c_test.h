#ifndef APP_I2C_TEST_H
#define APP_I2C_TEST_H

#include "app_protocol.h"

#include <stdint.h>

#define APP_I2C_TEST_FAIL    0U
#define APP_I2C_TEST_OK      1U

void app_i2c_test_init(void);

uint8_t app_i2c_test_send_command(const app_protocol_command_t *command);

#endif /* APP_I2C_TEST_H */
