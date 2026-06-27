#ifndef APP_CRC_H
#define APP_CRC_H

#include <stdint.h>

uint32_t app_crc_calculate(const uint8_t *data, uint32_t length);

#endif /* APP_CRC_H */
