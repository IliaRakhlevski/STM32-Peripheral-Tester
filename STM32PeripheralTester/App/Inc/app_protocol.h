#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdint.h>

#define APP_PROTOCOL_MAX_PATTERN_LEN    255U

#define APP_PROTOCOL_TEST_SUCCESS    1U
#define APP_PROTOCOL_TEST_FAIL       0xFFU

/**
 * @brief Peripheral identifiers used in the command protocol.
 */
typedef enum
{
    APP_PROTOCOL_PERIPHERAL_TIMER = 1U,
    APP_PROTOCOL_PERIPHERAL_UART  = 2U,
    APP_PROTOCOL_PERIPHERAL_SPI   = 4U,
    APP_PROTOCOL_PERIPHERAL_I2C   = 8U,
    APP_PROTOCOL_PERIPHERAL_ADC   = 16U
} app_protocol_peripheral_t;

/**
 * @brief Command received from the PC testing program.
 */
#pragma pack(push, 1)
typedef struct
{
    uint32_t test_id;
    uint8_t peripheral;
    uint8_t iterations;
    uint8_t pattern_len;
    uint8_t pattern[APP_PROTOCOL_MAX_PATTERN_LEN];
} app_protocol_command_t;

_Static_assert(sizeof(app_protocol_command_t) == 262U, "Unexpected command packet size");

/**
 * @brief Test result response sent from the UUT to the PC.
 */
typedef struct
{
    uint32_t test_id;
    uint8_t result;
} app_protocol_response_t;
#pragma pack(pop)

_Static_assert(sizeof(app_protocol_response_t) == 5U, "Unexpected response packet size");

#endif /* APP_PROTOCOL_H */

