#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Common task configuration. */
#define APP_TASK_STACK_SIZE_WORDS       512U
#define APP_TASK_PRIORITY               2U

/* Local command generator configuration. */
#define APP_CMD_GEN_MIN_PATTERN_LEN     1U
#define APP_CMD_GEN_MAX_PATTERN_LEN     255U

/* UART test mode selection. */
#define APP_UART_TEST_MODE_IT           1U
#define APP_UART_TEST_MODE_DMA          2U
#define APP_UART_TEST_MODE              APP_UART_TEST_MODE_IT

/* I2C test mode selection. */
#define APP_I2C_TEST_MODE_IT            1U
#define APP_I2C_TEST_MODE_DMA           2U
#define APP_I2C_TEST_MODE               APP_I2C_TEST_MODE_DMA

/* SPI test mode selection. */
#define APP_SPI_TEST_MODE_IT            1U
#define APP_SPI_TEST_MODE_DMA           2U
#define APP_SPI_TEST_MODE               APP_SPI_TEST_MODE_DMA

#endif /* APP_CONFIG_H */
