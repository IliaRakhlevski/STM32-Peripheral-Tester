#ifndef APP_DATA_COMPARE_H
#define APP_DATA_COMPARE_H

#include <stdint.h>

#define APP_DATA_COMPARE_FAIL    0U
#define APP_DATA_COMPARE_OK      1U

uint8_t app_data_compare(const uint8_t *expected, const uint8_t *actual, uint8_t length);

#endif /* APP_DATA_COMPARE_H */
