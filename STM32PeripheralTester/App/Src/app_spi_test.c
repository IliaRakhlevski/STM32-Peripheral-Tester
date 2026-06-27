#include "app_spi_test.h"
#include "app_config.h"
#include "app_data_compare.h"
#include "app_response_queue.h"
#include "app_debug.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "main.h"
#include "spi.h"
#include "io_tools.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>


/* SPI test queue configuration. */
#define APP_SPI_QUEUE_LENGTH             10U
#define APP_SPI_QUEUE_SEND_WAIT_MS       100U

/* SPI instances used by the loopback test. */
#define APP_SPI_TEST_MASTER_SPI          hspi1
#define APP_SPI_TEST_SLAVE_SPI           hspi2

#define APP_SPI_TEST_TIMEOUT_MS          1000U
#define APP_SPI_TEST_MAX_BUFFER_LEN      APP_PROTOCOL_MAX_PATTERN_LEN

/* Buffers used to receive data on both SPI interfaces. */
static uint8_t app_spi_master_rx_buffer[APP_SPI_TEST_MAX_BUFFER_LEN];
static uint8_t app_spi_slave_rx_buffer[APP_SPI_TEST_MAX_BUFFER_LEN];

/* Dummy transmit buffers required to generate the SPI clock. */
static uint8_t app_spi_master_dummy_tx_buffer[APP_SPI_TEST_MAX_BUFFER_LEN];
static uint8_t app_spi_slave_dummy_tx_buffer[APP_SPI_TEST_MAX_BUFFER_LEN];

/* Dummy receive buffers used while transmitting dummy data. */
static uint8_t app_spi_master_dummy_rx_buffer[APP_SPI_TEST_MAX_BUFFER_LEN];
static uint8_t app_spi_slave_dummy_rx_buffer[APP_SPI_TEST_MAX_BUFFER_LEN];

/* Transfer completion flags updated from SPI callbacks. */
static volatile uint8_t app_spi_master_done = 0U;
static volatile uint8_t app_spi_slave_done = 0U;
static volatile uint8_t app_spi_error = 0U;

/* Queue used to receive completed SPI test requests. */
static QueueHandle_t app_spi_queue_handle = NULL;

static void app_spi_test_task(void *argument);
static void app_spi_test_run(const app_protocol_command_t *command);


#if (APP_SPI_TEST_MODE == APP_SPI_TEST_MODE_IT)

static uint8_t app_spi_test_run_it(const app_protocol_command_t *command);

#elif (APP_SPI_TEST_MODE == APP_SPI_TEST_MODE_DMA)

static uint8_t app_spi_test_run_dma(const app_protocol_command_t *command);

#else

#error "Invalid SPI test mode"

#endif


void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == APP_SPI_TEST_MASTER_SPI.Instance)
    {
        app_spi_master_done = 1U;
    }
    else if (hspi->Instance == APP_SPI_TEST_SLAVE_SPI.Instance)
    {
        app_spi_slave_done = 1U;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
    app_spi_error = 1U;
}

/**
 * @brief Initializes the SPI test module.
 *
 * Creates the SPI command queue and starts the SPI test task.
 */
void app_spi_test_init(void)
{
    app_spi_queue_handle = xQueueCreate(
        APP_SPI_QUEUE_LENGTH,
        sizeof(app_protocol_command_t));

    if (app_spi_queue_handle == NULL)
    {
    	APP_LOG("FATAL: Failed to create SPI test queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_spi_test_task,
                    "SPITest",
                    APP_TASK_STACK_SIZE_WORDS,
                    NULL,
                    APP_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
    	APP_LOG("FATAL: Failed to create SPI test task\r\n");
        Error_Handler();
    }
}


/**
 * @brief Sends an SPI test command to the SPI test task.
 *
 * @param[in] command Pointer to command to send.
 *
 * @return APP_SPI_TEST_OK on success, APP_SPI_TEST_FAIL otherwise.
 */
uint8_t app_spi_test_send_command(const app_protocol_command_t *command)
{
    if ((command == NULL) || (app_spi_queue_handle == NULL))
    {
        return APP_SPI_TEST_FAIL;
    }

    if (xQueueSend(app_spi_queue_handle,
                   command,
                   pdMS_TO_TICKS(APP_SPI_QUEUE_SEND_WAIT_MS)) != pdPASS)
    {
        return APP_SPI_TEST_FAIL;
    }

    return APP_SPI_TEST_OK;
}

/**
 * @brief SPI test task.
 *
 * Waits for SPI test commands and executes them one by one.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
static void app_spi_test_task(void *argument)
{
    app_protocol_command_t command;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(app_spi_queue_handle,
                          &command,
                          portMAX_DELAY) == pdPASS)
        {
        	APP_LOG_DEBUG("SPI task received: test_id=%lu iterations=%u len=%u\r\n",
						   command.test_id,
						   command.iterations,
						   command.pattern_len);

            app_spi_test_run(&command);
        }
    }
}

/**
 * @brief Executes an SPI test command and sends a response.
 *
 * Selects the configured SPI backend and places the final test result
 * into the response queue.
 *
 * @param[in] command Pointer to SPI test command.
 */
static void app_spi_test_run(const app_protocol_command_t *command)
{
    app_protocol_response_t response;

    if (command == NULL)
    {
        return;
    }

    response.test_id = command->test_id;

#if (APP_SPI_TEST_MODE == APP_SPI_TEST_MODE_IT)

    response.result = app_spi_test_run_it(command);

#elif (APP_SPI_TEST_MODE == APP_SPI_TEST_MODE_DMA)

    response.result = app_spi_test_run_dma(command);

#else

    #error "Invalid SPI test mode"

#endif

    if (app_response_queue_send(&response) != APP_RESPONSE_QUEUE_OK)
    {
    	APP_LOG("Failed to send SPI response\r\n");
    }
}

/**
 * @brief Aborts an active SPI transfer and restores a clean state.
 *
 * Aborts ongoing master/slave transfers, reinitializes both SPI
 * peripherals and clears internal completion and error flags.
 *
 * This function is called after HAL failures, SPI errors or transfer
 * timeouts to ensure that the next test iteration starts from a known
 * state.
 *
 * @param[in] master_spi SPI handle used as master.
 * @param[in] slave_spi SPI handle used as slave.
 */
static void app_spi_test_abort_transfer(SPI_HandleTypeDef *master_spi, SPI_HandleTypeDef *slave_spi)
{
	/*
	 * SPI recovery notes:
	 *
	 * MOSI/MISO disconnects are typically recovered automatically by
	 * aborting and reinitializing both SPI instances.
	 *
	 * If SCK or NSS is disconnected during an active transfer,
	 * one side may remain waiting for a clock or frame completion event.
	 * Recovery depends on the exact timing of the disconnect and may
	 * require restoring the physical connection before communication
	 * can resume.
	 */

    if ((master_spi == NULL) || (slave_spi == NULL))
    {
        return;
    }

    (void)HAL_SPI_Abort(master_spi);
    (void)HAL_SPI_Abort(slave_spi);

    (void)HAL_SPI_DeInit(slave_spi);
    (void)HAL_SPI_DeInit(master_spi);

    vTaskDelay(pdMS_TO_TICKS(2U));

    (void)HAL_SPI_Init(slave_spi);
    (void)HAL_SPI_Init(master_spi);

    app_spi_master_done = 0U;
    app_spi_slave_done = 0U;
    app_spi_error = 0U;
}


#if (APP_SPI_TEST_MODE == APP_SPI_TEST_MODE_IT)

/**
 * @brief Transfers data from SPI master to SPI slave using interrupt mode.
 *
 * Starts slave full-duplex transfer first, then starts master full-duplex
 * transfer and waits for both transfer completion callbacks.
 *
 * @param[in] master_spi SPI handle used as master.
 * @param[in] slave_spi SPI handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_SPI_TEST_OK on success,
 *         APP_SPI_TEST_FAIL otherwise.
 */
static uint8_t app_spi_test_master_to_slave_it(
                                SPI_HandleTypeDef *master_spi,
                                SPI_HandleTypeDef *slave_spi,
                                const uint8_t *tx_data,
                                uint8_t *rx_data,
                                uint8_t length)
{
    uint32_t start_tick;

    if ((master_spi == NULL) ||
        (slave_spi == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_SPI_TEST_FAIL;
    }

    app_spi_master_done = 0U;
    app_spi_slave_done = 0U;
    app_spi_error = 0U;

    memset(app_spi_slave_dummy_tx_buffer, 0xFF, length);
    memset(app_spi_master_dummy_rx_buffer, 0x00, length);

    if (HAL_SPI_TransmitReceive_IT(slave_spi,
                                   app_spi_slave_dummy_tx_buffer,
                                   rx_data,
                                   length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    if (HAL_SPI_TransmitReceive_IT(master_spi,
                                   (uint8_t *)tx_data,
                                   app_spi_master_dummy_rx_buffer,
                                   length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_spi_master_done == 0U) ||
           (app_spi_slave_done == 0U))
    {
        if (app_spi_error != 0U)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_SPI_TEST_TIMEOUT_MS)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_SPI_TEST_OK;
}

/**
 * @brief Transfers data from SPI slave to SPI master using interrupt mode.
 *
 * Starts slave full-duplex transfer first, then starts master full-duplex
 * transfer with dummy bytes to generate the SPI clock.
 *
 * @param[in] master_spi SPI handle used as master.
 * @param[in] slave_spi SPI handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_SPI_TEST_OK on success,
 *         APP_SPI_TEST_FAIL otherwise.
 */
static uint8_t app_spi_test_slave_to_master_it(
                                SPI_HandleTypeDef *master_spi,
                                SPI_HandleTypeDef *slave_spi,
                                const uint8_t *tx_data,
                                uint8_t *rx_data,
                                uint8_t length)
{
    uint32_t start_tick;

    if ((master_spi == NULL) ||
        (slave_spi == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_SPI_TEST_FAIL;
    }

    app_spi_master_done = 0U;
    app_spi_slave_done = 0U;
    app_spi_error = 0U;

    memset(app_spi_master_dummy_tx_buffer, 0xFF, length);
    memset(app_spi_slave_dummy_rx_buffer, 0x00, length);

    if (HAL_SPI_TransmitReceive_IT(slave_spi,
                                   (uint8_t *)tx_data,
                                   app_spi_slave_dummy_rx_buffer,
                                   length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    if (HAL_SPI_TransmitReceive_IT(master_spi,
                                   app_spi_master_dummy_tx_buffer,
                                   rx_data,
                                   length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_spi_master_done == 0U) ||
           (app_spi_slave_done == 0U))
    {
        if (app_spi_error != 0U)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_SPI_TEST_TIMEOUT_MS)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_SPI_TEST_OK;
}

/**
 * @brief Executes SPI test using interrupt mode.
 *
 * @param[in] command Pointer to SPI test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success,
 *         APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_spi_test_run_it(const app_protocol_command_t *command)
{
    if (command == NULL)
    {
        return APP_PROTOCOL_TEST_FAIL;
    }

    APP_LOG_DEBUG("Running SPI IT test %lu\r\n", command->test_id);

#if (APP_LOG_ENABLE_DEBUG == 1U)
    app_debug_print_buffer("Pattern", command->pattern, command->pattern_len);
#endif

    for (uint8_t i = 0U; i < command->iterations; i++)
    {
    	APP_LOG_DEBUG("SPI iteration %u\r\n", (uint8_t)(i + 1U));

        /*
         * Master -> Slave
         */
        if (app_spi_test_master_to_slave_it(
                &APP_SPI_TEST_MASTER_SPI,
                &APP_SPI_TEST_SLAVE_SPI,
                command->pattern,
                app_spi_slave_rx_buffer,
                command->pattern_len) != APP_SPI_TEST_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_spi_slave_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        /*
         * Slave -> Master
         */
        if (app_spi_test_slave_to_master_it(
                &APP_SPI_TEST_MASTER_SPI,
                &APP_SPI_TEST_SLAVE_SPI,
                app_spi_slave_rx_buffer,
                app_spi_master_rx_buffer,
                command->pattern_len) != APP_SPI_TEST_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_spi_master_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        APP_LOG_DEBUG("SPI iteration %u PASS\r\n", (uint8_t)(i + 1U));
    }

    return APP_PROTOCOL_TEST_SUCCESS;
}

#elif (APP_SPI_TEST_MODE == APP_SPI_TEST_MODE_DMA)

/**
 * @brief Transfers data from SPI master to SPI slave using DMA mode.
 *
 * Starts slave full-duplex DMA transfer first, then starts master
 * full-duplex DMA transfer and waits for both completion callbacks.
 *
 * @param[in] master_spi SPI handle used as master.
 * @param[in] slave_spi SPI handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_SPI_TEST_OK on success,
 *         APP_SPI_TEST_FAIL otherwise.
 */
static uint8_t app_spi_test_master_to_slave_dma(
                                SPI_HandleTypeDef *master_spi,
                                SPI_HandleTypeDef *slave_spi,
                                const uint8_t *tx_data,
                                uint8_t *rx_data,
                                uint8_t length)
{
    uint32_t start_tick;

    if ((master_spi == NULL) ||
        (slave_spi == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_SPI_TEST_FAIL;
    }

    app_spi_master_done = 0U;
    app_spi_slave_done = 0U;
    app_spi_error = 0U;

    memset(app_spi_slave_dummy_tx_buffer, 0xFF, length);
    memset(app_spi_master_dummy_rx_buffer, 0x00, length);

    if (HAL_SPI_TransmitReceive_DMA(slave_spi,
                                    app_spi_slave_dummy_tx_buffer,
                                    rx_data,
                                    length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(1U));

    if (HAL_SPI_TransmitReceive_DMA(master_spi,
                                    (uint8_t *)tx_data,
                                    app_spi_master_dummy_rx_buffer,
                                    length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_spi_master_done == 0U) ||
           (app_spi_slave_done == 0U))
    {
        if (app_spi_error != 0U)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_SPI_TEST_TIMEOUT_MS)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_SPI_TEST_OK;
}

/**
 * @brief Transfers data from SPI slave to SPI master using DMA mode.
 *
 * Starts slave full-duplex DMA transfer first, then starts master
 * full-duplex DMA transfer with dummy bytes to generate the SPI clock.
 *
 * @param[in] master_spi SPI handle used as master.
 * @param[in] slave_spi SPI handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_SPI_TEST_OK on success,
 *         APP_SPI_TEST_FAIL otherwise.
 */
static uint8_t app_spi_test_slave_to_master_dma(
                                SPI_HandleTypeDef *master_spi,
                                SPI_HandleTypeDef *slave_spi,
                                const uint8_t *tx_data,
                                uint8_t *rx_data,
                                uint8_t length)
{
    uint32_t start_tick;

    if ((master_spi == NULL) ||
        (slave_spi == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_SPI_TEST_FAIL;
    }

    app_spi_master_done = 0U;
    app_spi_slave_done = 0U;
    app_spi_error = 0U;

    memset(app_spi_master_dummy_tx_buffer, 0xFF, length);
    memset(app_spi_slave_dummy_rx_buffer, 0x00, length);

    if (HAL_SPI_TransmitReceive_DMA(slave_spi,
                                    (uint8_t *)tx_data,
                                    app_spi_slave_dummy_rx_buffer,
                                    length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(1U));

    if (HAL_SPI_TransmitReceive_DMA(master_spi,
                                    app_spi_master_dummy_tx_buffer,
                                    rx_data,
                                    length) != HAL_OK)
    {
        app_spi_test_abort_transfer(master_spi, slave_spi);
        return APP_SPI_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_spi_master_done == 0U) ||
           (app_spi_slave_done == 0U))
    {
        if (app_spi_error != 0U)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_SPI_TEST_TIMEOUT_MS)
        {
            app_spi_test_abort_transfer(master_spi, slave_spi);
            return APP_SPI_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_SPI_TEST_OK;
}

/**
 * @brief Executes SPI test using DMA mode.
 *
 * @param[in] command Pointer to SPI test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success,
 *         APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_spi_test_run_dma(const app_protocol_command_t *command)
{
    if (command == NULL)
    {
        return APP_PROTOCOL_TEST_FAIL;
    }

    APP_LOG_DEBUG("Running SPI DMA test %lu\r\n", command->test_id);

#if (APP_LOG_ENABLE_DEBUG == 1U)
    app_debug_print_buffer("Pattern", command->pattern, command->pattern_len);
#endif

    for (uint8_t i = 0U; i < command->iterations; i++)
    {
    	APP_LOG_DEBUG("SPI iteration %u\r\n", (uint8_t)(i + 1U));

        /*
         * Master -> Slave
         */
        if (app_spi_test_master_to_slave_dma(
                &APP_SPI_TEST_MASTER_SPI,
                &APP_SPI_TEST_SLAVE_SPI,
                command->pattern,
                app_spi_slave_rx_buffer,
                command->pattern_len) != APP_SPI_TEST_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_spi_slave_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        /*
         * Slave -> Master
         */
        if (app_spi_test_slave_to_master_dma(
                &APP_SPI_TEST_MASTER_SPI,
                &APP_SPI_TEST_SLAVE_SPI,
                app_spi_slave_rx_buffer,
                app_spi_master_rx_buffer,
                command->pattern_len) != APP_SPI_TEST_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        if (app_data_compare(command->pattern,
                             app_spi_master_rx_buffer,
                             command->pattern_len) != APP_DATA_COMPARE_OK)
        {
        	APP_LOG("SPI iteration %u FAIL\r\n", (uint8_t)(i + 1U));
            return APP_PROTOCOL_TEST_FAIL;
        }

        APP_LOG_DEBUG("SPI iteration %u PASS\r\n", (uint8_t)(i + 1U));
    }

    return APP_PROTOCOL_TEST_SUCCESS;
}


#else

#error "Invalid SPI test mode"

#endif
