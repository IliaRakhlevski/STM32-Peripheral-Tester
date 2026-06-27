#include "app_crc.h"
#include "crc.h"
#include <stddef.h>

/**
 * @brief Calculates CRC for a data buffer.
 *
 * @param[in] data Pointer to input data.
 * @param[in] length Data length in bytes.
 *
 * @return Calculated CRC value.
 */
uint32_t app_crc_calculate(const uint8_t *data, uint32_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return 0U;
    }

    return HAL_CRC_Calculate(&hcrc, (uint32_t *)data, length);
}
