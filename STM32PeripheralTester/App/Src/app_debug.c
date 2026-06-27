#include "app_debug.h"
#include "io_tools.h"
#include <stdio.h>


/**
 * @brief Print a buffer in hexadecimal format.
 *
 * Displays the supplied buffer together with its title and length.
 * Intended for debugging transmitted and received protocol data.
 */
void app_debug_print_buffer(const char *title,
                            const uint8_t *buffer,
                            uint8_t length)
{
    uint8_t i;

    if ((title == NULL) || (buffer == NULL))
    {
        return;
    }

    APP_LOG_DEBUG("%s [%u]: ", title, length);

    for (i = 0U; i < length; i++)
    {
    	APP_LOG_DEBUG("%02X ", buffer[i]);
    }

    APP_LOG_DEBUG("\r\n");
}
