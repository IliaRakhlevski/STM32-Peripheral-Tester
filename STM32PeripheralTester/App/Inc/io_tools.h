#ifndef APP_IO_TOOLS_H
#define APP_IO_TOOLS_H

#define APP_LOG_ENABLE_DEBUG  0U

#if (APP_LOG_ENABLE_DEBUG == 1U)
#define APP_LOG_DEBUG(...)    app_printf(__VA_ARGS__)
#else
#define APP_LOG_DEBUG(...)
#endif

#define APP_LOG(...)          app_printf(__VA_ARGS__)

void app_printf(const char *format, ...);

#endif /* APP_IO_TOOLS_H */
