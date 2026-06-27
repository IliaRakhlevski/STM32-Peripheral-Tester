#include "app_protocol.h"

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
const char *app_protocol_peripheral_to_string(uint8_t peripheral)
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