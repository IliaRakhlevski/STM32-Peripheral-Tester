#include "FreeRTOS.h"
#include "semphr.h"
#include "usart.h"
#include "io_tools.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Redirects printf output to UART.
 *
 * This function is called by the C standard library whenever
 * data is written to stdout or stderr. It transmits the given
 * buffer through USART3 using blocking mode.
 *
 * @param file File descriptor (not used).
 * @param ptr Pointer to the data buffer to transmit.
 * @param len Number of bytes to transmit.
 *
 * @return Number of bytes successfully transmitted.
 */
int _write(int file, char *ptr, int len)
{
	HAL_UART_Transmit(&huart3, (uint8_t*)ptr, len, HAL_MAX_DELAY);

	return len;
}

/**
 * @brief Thread-safe UART logging function.
 *
 * Formats the specified message and transmits it through the
 * debug UART while protecting the entire operation with a mutex.
 * This prevents log messages from multiple FreeRTOS tasks from
 * being interleaved on the serial output.
 *
 * The mutex is created automatically on first use. When the
 * scheduler is not running, the function transmits the message
 * without acquiring the mutex.
 *
 * @param[in] format Printf-style format string.
 * @param[in] ... Variable arguments referenced by the format string.
 *
 * @return None.
 */
void app_printf(const char *format, ...)
{
    static SemaphoreHandle_t printf_mutex = NULL;
    char buffer[256];
    va_list args;
    int len;

    if (printf_mutex == NULL)
    {
        printf_mutex = xSemaphoreCreateMutex();
    }

    if ((printf_mutex != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
    {
        xSemaphoreTake(printf_mutex, portMAX_DELAY);
    }

    va_start(args, format);
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0)
    {
        HAL_UART_Transmit(&huart3,
                          (uint8_t *)buffer,
                          (uint16_t)len,
                          HAL_MAX_DELAY);
    }

    if ((printf_mutex != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
    {
        xSemaphoreGive(printf_mutex);
    }
}
