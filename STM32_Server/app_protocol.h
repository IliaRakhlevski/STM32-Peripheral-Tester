#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdint.h>

#define APP_PROTOCOL_PATTERN_MAX_LEN 255U

#define APP_PROTOCOL_PERIPHERAL_TIMER 1U
#define APP_PROTOCOL_PERIPHERAL_UART  2U
#define APP_PROTOCOL_PERIPHERAL_SPI   4U
#define APP_PROTOCOL_PERIPHERAL_I2C   8U
#define APP_PROTOCOL_PERIPHERAL_ADC   16U

#define APP_PROTOCOL_TEST_SUCCESS     1U
#define APP_PROTOCOL_TEST_FAIL        0xFFU

#pragma pack(push, 1)

/**
 * @brief Test command received from the PC.
 */
typedef struct
{
    uint32_t test_id;      /**< Unique test identifier. */
    uint8_t peripheral;    /**< Peripheral under test. */
    uint8_t iterations;    /**< Number of test iterations. */
    uint8_t pattern_len;   /**< Number of valid bytes in pattern[]. */
    uint8_t pattern[APP_PROTOCOL_PATTERN_MAX_LEN]; /**< Test data pattern. */
} app_protocol_command_t;

/**
 * @brief Test response sent back to the PC.
 */
typedef struct
{
    uint32_t test_id;      /**< Identifier of the completed test. */
    uint8_t result;        /**< Test result (PASS/FAIL). */
} app_protocol_response_t;

#pragma pack(pop)

const char *app_protocol_peripheral_to_string(uint8_t peripheral);

#endif