#include "app_uart_test.h"
#include "app_config.h"
#include "app_data_compare.h"
#include "app_response_queue.h"
#include "app_debug.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "main.h"
#include "usart.h"
#include "io_tools.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>


/* UART test queue configuration. */
#define APP_UART_QUEUE_LENGTH             10U
#define APP_UART_QUEUE_SEND_WAIT_MS       100U

/* UART instances used by the loopback test. */
#define APP_UART_TEST_A_UART              huart4
#define APP_UART_TEST_B_UART              huart5

/* Use CRC comparison for large data transfers. */
#define APP_CRC_MIN_LEN_BYTES             100U

#define APP_UART_TEST_MAX_BUFFER_LEN      APP_PROTOCOL_MAX_PATTERN_LEN
#define APP_UART_TEST_TIMEOUT_MS          1000U

/* Transfer completion flags updated from UART callbacks. */
static volatile uint8_t app_uart_tx_done = 0U;
static volatile uint8_t app_uart_rx_done = 0U;
static volatile uint8_t app_uart_error = 0U;

/* Receive buffers for both UART interfaces. */
static uint8_t app_uart_a_rx_buffer[APP_UART_TEST_MAX_BUFFER_LEN];
static uint8_t app_uart_b_rx_buffer[APP_UART_TEST_MAX_BUFFER_LEN];

/* Queue used to receive UART test requests. */
static QueueHandle_t app_uart_queue_handle = NULL;

static void app_uart_test_task(void *argument);
static void app_uart_test_run(const app_protocol_command_t *command);

#if (APP_UART_TEST_MODE == APP_UART_TEST_MODE_IT)

static uint8_t app_uart_test_run_it(const app_protocol_command_t *command);

#elif (APP_UART_TEST_MODE == APP_UART_TEST_MODE_DMA)

static uint8_t app_uart_test_run_dma(const app_protocol_command_t *command);

#else

	#error "Invalid UART test mode"

#endif


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
    app_uart_tx_done = 1U;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
    app_uart_rx_done = 1U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
    app_uart_error = 1U;

    APP_LOG("UART error callback: instance=0x%08lX error=0x%08lX\r\n",
                (uint32_t)huart->Instance,
                huart->ErrorCode);
}

/**
 * @brief Initializes the UART test module.
 *
 * Creates the UART command queue and starts the UART test task.
 */
void app_uart_test_init(void)
{
    app_uart_queue_handle = xQueueCreate(APP_UART_QUEUE_LENGTH, sizeof(app_protocol_command_t));

    if (app_uart_queue_handle == NULL)
    {
    	APP_LOG("FATAL: Failed to create UART test queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_uart_test_task,
                    "UartTest",
                    APP_TASK_STACK_SIZE_WORDS,
                    NULL,
                    APP_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
    	APP_LOG("FATAL: Failed to create UART test task\r\n");
        Error_Handler();
    }
}

/**
 * @brief Sends a UART test command to the UART test task.
 *
 * @param[in] command Pointer to command to send.
 *
 * @return APP_UART_TEST_OK on success, APP_UART_TEST_FAIL otherwise.
 */
uint8_t app_uart_test_send_command(
    const app_protocol_command_t *command)
{
    if ((command == NULL) || (app_uart_queue_handle == NULL))
    {
        return APP_UART_TEST_FAIL;
    }

    if (xQueueSend(app_uart_queue_handle, command, pdMS_TO_TICKS(APP_UART_QUEUE_SEND_WAIT_MS)) != pdPASS)
    {
        return APP_UART_TEST_FAIL;
    }

    return APP_UART_TEST_OK;
}

/**
 * @brief UART test task.
 *
 * Waits for UART test commands and executes them one by one.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
static void app_uart_test_task(void *argument)
{
    app_protocol_command_t command;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(app_uart_queue_handle, &command, portMAX_DELAY) == pdPASS)
        {
        	APP_LOG_DEBUG("UART task received: test_id=%lu iterations=%u len=%u\r\n",
                   command.test_id,
                   command.iterations,
                   command.pattern_len);

            app_uart_test_run(&command);
        }
    }
}

/**
 * @brief Executes a UART test command.
 *
 * Currently this function only sends a successful mock response.
 *
 * @param[in] command Pointer to UART test command.
 */
static void app_uart_test_run(const app_protocol_command_t *command)
{
    app_protocol_response_t response;

    if (command == NULL)
    {
        return;
    }

	response.test_id = command->test_id;

#if (APP_UART_TEST_MODE == APP_UART_TEST_MODE_IT)

    response.result = app_uart_test_run_it(command);

#elif (APP_UART_TEST_MODE == APP_UART_TEST_MODE_DMA)

    response.result = app_uart_test_run_dma(command);

#else

	#error "Invalid UART test mode"

#endif

    if (app_response_queue_send(&response) != APP_RESPONSE_QUEUE_OK)
    {
    	APP_LOG("Failed to send UART response\r\n");
    }
}

/**
 * @brief Aborts an active UART transfer and clears transfer state.
 *
 * Stops ongoing transmit and receive operations on both UARTs and
 * resets all internal transfer flags.
 *
 * This function is used when a transfer fails due to a timeout,
 * UART error, or HAL API failure. After the function returns,
 * a new UART transfer can be started safely.
 *
 * @param[in] tx_uart UART used for transmission.
 * @param[in] rx_uart UART used for reception.
 */
static void app_uart_test_abort_transfer(
    UART_HandleTypeDef *tx_uart,
    UART_HandleTypeDef *rx_uart)
{
    (void)HAL_UART_Abort(tx_uart);
    (void)HAL_UART_Abort(rx_uart);

    __HAL_UART_CLEAR_OREFLAG(tx_uart);
    __HAL_UART_CLEAR_FEFLAG(tx_uart);
    __HAL_UART_CLEAR_NEFLAG(tx_uart);
    __HAL_UART_CLEAR_PEFLAG(tx_uart);

    __HAL_UART_CLEAR_OREFLAG(rx_uart);
    __HAL_UART_CLEAR_FEFLAG(rx_uart);
    __HAL_UART_CLEAR_NEFLAG(rx_uart);
    __HAL_UART_CLEAR_PEFLAG(rx_uart);

    (void)HAL_UART_DeInit(tx_uart);
    (void)HAL_UART_Init(tx_uart);

    (void)HAL_UART_DeInit(rx_uart);
    (void)HAL_UART_Init(rx_uart);

    app_uart_tx_done = 0U;
    app_uart_rx_done = 0U;
    app_uart_error = 0U;
}

#if (APP_UART_TEST_MODE == APP_UART_TEST_MODE_IT)

/**
 * @brief Transfers a data buffer between two UARTs using interrupt mode.
 *
 * Starts receive operation first, then starts transmit operation.
 * The function waits until both TX and RX operations are completed
 * or a timeout/error occurs.
 *
 * @param[in] tx_uart UART used for transmission.
 * @param[in] rx_uart UART used for reception.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_UART_TEST_OK on success, APP_UART_TEST_FAIL otherwise.
 */
static uint8_t app_uart_test_transfer_it(
							UART_HandleTypeDef *tx_uart,
							UART_HandleTypeDef *rx_uart,
							const uint8_t *tx_data,
							uint8_t *rx_data,
							uint16_t length)
{
    uint32_t start_tick;

    if ((tx_uart == NULL) ||
        (rx_uart == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_UART_TEST_FAIL;
    }

    app_uart_tx_done = 0U;
    app_uart_rx_done = 0U;
    app_uart_error = 0U;

    tx_uart->ErrorCode = HAL_UART_ERROR_NONE;
    rx_uart->ErrorCode = HAL_UART_ERROR_NONE;

    __HAL_UART_CLEAR_OREFLAG(rx_uart);
    __HAL_UART_CLEAR_FEFLAG(rx_uart);
    __HAL_UART_CLEAR_NEFLAG(rx_uart);
    __HAL_UART_CLEAR_PEFLAG(rx_uart);

    if (HAL_UART_Receive_IT(rx_uart, rx_data, length) != HAL_OK)
    {
    	APP_LOG("UART FAIL: HAL_UART_Receive_IT failed\r\n");
    	app_uart_test_abort_transfer(tx_uart, rx_uart);
        return APP_UART_TEST_FAIL;
    }

    if (HAL_UART_Transmit_IT(tx_uart, (uint8_t *)tx_data, length) != HAL_OK)
    {
    	 APP_LOG("UART FAIL: HAL_UART_Transmit_IT failed\r\n");
    	app_uart_test_abort_transfer(tx_uart, rx_uart);
        return APP_UART_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_uart_tx_done == 0U) || (app_uart_rx_done == 0U))
    {
        if (app_uart_error != 0U)
        {
        	APP_LOG("UART FAIL: error callback\r\n");
        	app_uart_test_abort_transfer(tx_uart, rx_uart);
            return APP_UART_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_UART_TEST_TIMEOUT_MS)
        {
        	APP_LOG("UART FAIL: timeout\r\n");
        	app_uart_test_abort_transfer(tx_uart, rx_uart);
            return APP_UART_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_UART_TEST_OK;
}

/**
 * @brief Executes UART test using interrupt mode.
 *
 * Sends the command bit pattern from UART4 to UART5 and verifies that
 * UART5 receives the same data.
 *
 * @param[in] command Pointer to UART test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success, APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_uart_test_run_it(const app_protocol_command_t *command)
{
    if (command == NULL)
        return APP_PROTOCOL_TEST_FAIL;

    if ((command->pattern_len == 0U) || (command->pattern_len > APP_UART_TEST_MAX_BUFFER_LEN))
            return APP_PROTOCOL_TEST_FAIL;

    APP_LOG_DEBUG("Running UART IT test %lu\r\n", command->test_id);

#if (APP_LOG_ENABLE_DEBUG == 1U)
    app_debug_print_buffer("Pattern", command->pattern, command->pattern_len);
#endif

    for (uint8_t i = 0U; i < command->iterations; i++)
    {
    	APP_LOG_DEBUG("UART iteration %u\r\n", (uint8_t)(i + 1U));

        if (app_uart_test_transfer_it(&APP_UART_TEST_A_UART,
                                      &APP_UART_TEST_B_UART,
                                      command->pattern,
                                      app_uart_b_rx_buffer,
                                      command->pattern_len) != APP_UART_TEST_OK)
        {
        	APP_LOG("UART iteration %u FAIL 1\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_uart_b_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("UART iteration %u FAIL 2\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));

        if (app_uart_test_transfer_it(&APP_UART_TEST_B_UART,
                                      &APP_UART_TEST_A_UART,
                                      app_uart_b_rx_buffer,
                                      app_uart_a_rx_buffer,
                                      command->pattern_len) != APP_UART_TEST_OK)
        {
        	APP_LOG("UART iteration %u FAIL 3\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_uart_a_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("UART iteration %u FAIL 4\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        APP_LOG_DEBUG("UART iteration %u PASS\r\n", (uint8_t)(i + 1U));
    }

    return APP_PROTOCOL_TEST_SUCCESS;
}

#elif (APP_UART_TEST_MODE == APP_UART_TEST_MODE_DMA)

/**
 * @brief Transfers a data buffer between two UARTs using DMA mode.
 *
 * Starts DMA receive operation first, then starts DMA transmit operation.
 * The function waits until both TX and RX operations are completed
 * or a timeout/error occurs.
 *
 * @param[in] tx_uart UART used for transmission.
 * @param[in] rx_uart UART used for reception.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_UART_TEST_OK on success, APP_UART_TEST_FAIL otherwise.
 */
static uint8_t app_uart_test_transfer_dma(
							UART_HandleTypeDef *tx_uart,
							UART_HandleTypeDef *rx_uart,
							const uint8_t *tx_data,
							uint8_t *rx_data,
							uint16_t length)
{
    uint32_t start_tick;

    if ((tx_uart == NULL) ||
        (rx_uart == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_UART_TEST_FAIL;
    }

    /* Clear any previous unfinished UART operations. */
    (void)HAL_UART_Abort(tx_uart);
    (void)HAL_UART_Abort(rx_uart);

    app_uart_tx_done = 0U;
    app_uart_rx_done = 0U;
    app_uart_error = 0U;

    tx_uart->ErrorCode = HAL_UART_ERROR_NONE;
       rx_uart->ErrorCode = HAL_UART_ERROR_NONE;

   __HAL_UART_CLEAR_OREFLAG(rx_uart);
   __HAL_UART_CLEAR_FEFLAG(rx_uart);
   __HAL_UART_CLEAR_NEFLAG(rx_uart);
   __HAL_UART_CLEAR_PEFLAG(rx_uart);

    if (HAL_UART_Receive_DMA(rx_uart, rx_data, length) != HAL_OK)
    {
        app_uart_test_abort_transfer(tx_uart, rx_uart);
        return APP_UART_TEST_FAIL;
    }

    if (HAL_UART_Transmit_DMA(tx_uart, (uint8_t *)tx_data, length) != HAL_OK)
    {
        app_uart_test_abort_transfer(tx_uart, rx_uart);
        return APP_UART_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_uart_tx_done == 0U) || (app_uart_rx_done == 0U))
    {
        if (app_uart_error != 0U)
        {
            app_uart_test_abort_transfer(tx_uart, rx_uart);
            return APP_UART_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_UART_TEST_TIMEOUT_MS)
        {
            app_uart_test_abort_transfer(tx_uart, rx_uart);
            return APP_UART_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_UART_TEST_OK;
}


/**
 * @brief Executes UART test using DMA mode.
 *
 * @param[in] command Pointer to UART test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success, APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_uart_test_run_dma(const app_protocol_command_t *command)
{
    uint8_t i;

    if (command == NULL)
        return APP_PROTOCOL_TEST_FAIL;

    if ((command->pattern_len == 0U) || (command->pattern_len > APP_UART_TEST_MAX_BUFFER_LEN))
        return APP_PROTOCOL_TEST_FAIL;

    APP_LOG_DEBUG("Running UART DMA test %lu\r\n", command->test_id);

#if (APP_LOG_ENABLE_DEBUG == 1U)
    app_debug_print_buffer("Pattern", command->pattern, command->pattern_len);
#endif

    for (i = 0U; i < command->iterations; i++)
    {
        memset(app_uart_a_rx_buffer, 0, sizeof(app_uart_a_rx_buffer));
        memset(app_uart_b_rx_buffer, 0, sizeof(app_uart_b_rx_buffer));

        if (app_uart_test_transfer_dma(&APP_UART_TEST_A_UART,
                                       &APP_UART_TEST_B_UART,
                                       command->pattern,
                                       app_uart_b_rx_buffer,
                                       command->pattern_len) != APP_UART_TEST_OK)
        {
        	APP_LOG("UART iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_uart_b_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("UART iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_uart_test_transfer_dma(&APP_UART_TEST_B_UART,
                                       &APP_UART_TEST_A_UART,
                                       app_uart_b_rx_buffer,
                                       app_uart_a_rx_buffer,
                                       command->pattern_len) != APP_UART_TEST_OK)
        {
        	APP_LOG("UART iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_uart_a_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("UART iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        APP_LOG_DEBUG("UART iteration %u PASS\r\n", (uint8_t)(i + 1U));
    }

    return APP_PROTOCOL_TEST_SUCCESS;
}

#else

	#error "Invalid UART test mode"

#endif

