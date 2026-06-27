#include "app_data_compare.h"
#include "app_crc.h"
#include <stddef.h>
#include <string.h>

/* Minimum buffer length for using CRC comparison instead of byte-by-byte verification. */
#define APP_DATA_COMPARE_CRC_MIN_LEN    100U

/**
 * @brief Compares two data buffers.
 *
 * For small buffers the function uses byte-by-byte comparison.
 * For large buffers it compares hardware CRC values.
 *
 * @param[in] expected Pointer to expected data buffer.
 * @param[in] actual Pointer to actual data buffer.
 * @param[in] length Number of bytes to compare.
 *
 * @return APP_DATA_COMPARE_OK if buffers match, APP_DATA_COMPARE_FAIL otherwise.
 */
uint8_t app_data_compare(const uint8_t *expected, const uint8_t *actual, uint8_t length)
{
    if ((expected == NULL) || (actual == NULL) || (length == 0U))
    {
        return APP_DATA_COMPARE_FAIL;
    }

    /* Use CRC for large buffers to reduce comparison time. */
    if (length > APP_DATA_COMPARE_CRC_MIN_LEN)
    {
        if (app_crc_calculate(expected, length) != app_crc_calculate(actual, length))
        {
            return APP_DATA_COMPARE_FAIL;
        }

        return APP_DATA_COMPARE_OK;
    }

    /* Use byte-by-byte comparison for small buffers. */
    if (memcmp(expected, actual, length) != 0)
    {
        return APP_DATA_COMPARE_FAIL;
    }

    return APP_DATA_COMPARE_OK;
}
