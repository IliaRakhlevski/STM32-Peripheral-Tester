#include "app_i2c_test.h"
#include "app_config.h"
#include "app_data_compare.h"
#include "app_response_queue.h"
#include "app_debug.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "main.h"
#include "i2c.h"
#include "io_tools.h"
#include <stddef.h>
#include <stdio.h>


/* I2C test queue configuration. */
#define APP_I2C_QUEUE_LENGTH              10U
#define APP_I2C_QUEUE_SEND_WAIT_MS        100U

/* I2C instances used by the loopback test. */
#define APP_I2C_TEST_MASTER_I2C           hi2c1
#define APP_I2C_TEST_SLAVE_I2C            hi2c2

/* Slave address used during testing. */
#define APP_I2C_TEST_SLAVE_ADDR_7BIT      0x22U
#define APP_I2C_TEST_SLAVE_ADDR           (APP_I2C_TEST_SLAVE_ADDR_7BIT << 1U)

#define APP_I2C_TEST_TIMEOUT_MS           1000U
#define APP_I2C_TEST_MAX_BUFFER_LEN       APP_PROTOCOL_MAX_PATTERN_LEN

/* Buffers used to receive data on both I2C interfaces. */
static uint8_t app_i2c_master_rx_buffer[APP_I2C_TEST_MAX_BUFFER_LEN];
static uint8_t app_i2c_slave_rx_buffer[APP_I2C_TEST_MAX_BUFFER_LEN];

/* Transfer completion flags updated from I2C callbacks. */
static volatile uint8_t app_i2c_master_tx_done = 0U;
static volatile uint8_t app_i2c_master_rx_done = 0U;
static volatile uint8_t app_i2c_slave_tx_done = 0U;
static volatile uint8_t app_i2c_slave_rx_done = 0U;
static volatile uint8_t app_i2c_error = 0U;

/* Queue used to receive completed I2C test requests. */
static QueueHandle_t app_i2c_queue_handle = NULL;

static void app_i2c_test_task(void *argument);
static void app_i2c_test_run(const app_protocol_command_t *command);


#if (APP_I2C_TEST_MODE == APP_I2C_TEST_MODE_IT)

static uint8_t app_i2c_test_run_it(const app_protocol_command_t *command);

#elif (APP_I2C_TEST_MODE == APP_I2C_TEST_MODE_DMA)

static uint8_t app_i2c_test_run_dma(const app_protocol_command_t *command);

#else

	#error "Invalid I2C test mode"

#endif


void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == APP_I2C_TEST_MASTER_I2C.Instance)
    {
        app_i2c_master_tx_done = 1U;
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == APP_I2C_TEST_MASTER_I2C.Instance)
    {
        app_i2c_master_rx_done = 1U;
    }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == APP_I2C_TEST_SLAVE_I2C.Instance)
    {
        app_i2c_slave_tx_done = 1U;
    }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == APP_I2C_TEST_SLAVE_I2C.Instance)
    {
        app_i2c_slave_rx_done = 1U;
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    (void)hi2c;
    app_i2c_error = 1U;
}


/**
 * @brief Initializes the I2C test module.
 *
 * Creates the I2C command queue and starts the I2C test task.
 */
void app_i2c_test_init(void)
{
    app_i2c_queue_handle = xQueueCreate(APP_I2C_QUEUE_LENGTH, sizeof(app_protocol_command_t));

    if (app_i2c_queue_handle == NULL)
    {
    	APP_LOG("FATAL: Failed to create I2C test queue\r\n");
        Error_Handler();
    }

    if (xTaskCreate(app_i2c_test_task,
                    "I2CTest",
                    APP_TASK_STACK_SIZE_WORDS,
                    NULL,
                    APP_TASK_PRIORITY,
                    NULL) != pdPASS)
    {
    	APP_LOG("FATAL: Failed to create I2C test task\r\n");
        Error_Handler();
    }
}

/**
 * @brief Sends an I2C test command to the I2C test task.
 *
 * @param[in] command Pointer to command to send.
 *
 * @return APP_I2C_TEST_OK on success, APP_I2C_TEST_FAIL otherwise.
 */
uint8_t app_i2c_test_send_command(const app_protocol_command_t *command)
{
    if ((command == NULL) || (app_i2c_queue_handle == NULL))
    {
        return APP_I2C_TEST_FAIL;
    }

    if (xQueueSend(app_i2c_queue_handle,
                   command,
                   pdMS_TO_TICKS(APP_I2C_QUEUE_SEND_WAIT_MS)) != pdPASS)
    {
        return APP_I2C_TEST_FAIL;
    }

    return APP_I2C_TEST_OK;
}

/**
 * @brief I2C test task.
 *
 * Waits for I2C test commands and executes them one by one.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
static void app_i2c_test_task(void *argument)
{
    app_protocol_command_t command;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(app_i2c_queue_handle,
                          &command,
                          portMAX_DELAY) == pdPASS)
        {
        	APP_LOG_DEBUG("I2C task received: test_id=%lu iterations=%u len=%u\r\n",
                   command.test_id,
                   command.iterations,
                   command.pattern_len);

            app_i2c_test_run(&command);
        }
    }
}

/**
 * @brief Executes an I2C test command and sends a response.
 *
 * Selects the configured I2C backend and places the final test result
 * into the response queue.
 *
 * @param[in] command Pointer to I2C test command.
 */
static void app_i2c_test_run(const app_protocol_command_t *command)
{
    app_protocol_response_t response;

    if (command == NULL)
    {
        return;
    }

    response.test_id = command->test_id;

#if (APP_I2C_TEST_MODE == APP_I2C_TEST_MODE_IT)

    response.result = app_i2c_test_run_it(command);

#elif (APP_I2C_TEST_MODE == APP_I2C_TEST_MODE_DMA)

    response.result = app_i2c_test_run_dma(command);

#else

    #error "Invalid I2C test mode"

#endif

    if (app_response_queue_send(&response) != APP_RESPONSE_QUEUE_OK)
    {
    	APP_LOG("Failed to send I2C response\r\n");
    }
}

/**
 * @brief Aborts an active I2C transfer and restores a clean state.
 *
 * Aborts ongoing master/slave transfers, reinitializes both I2C
 * peripherals and clears internal completion and error flags.
 *
 * This function is called after HAL failures, I2C errors or transfer
 * timeouts to ensure that the next test iteration starts from a known
 * state.
 *
 * @param[in] master_i2c I2C handle used as master.
 * @param[in] slave_i2c I2C handle used as slave.
 */
static void app_i2c_test_abort_transfer(I2C_HandleTypeDef *master_i2c, I2C_HandleTypeDef *slave_i2c)
{
    if ((master_i2c == NULL) || (slave_i2c == NULL))
    {
        return;
    }

    (void)HAL_I2C_Master_Abort_IT(master_i2c,
                                  APP_I2C_TEST_SLAVE_ADDR);

    (void)HAL_I2C_DeInit(master_i2c);
    (void)HAL_I2C_Init(master_i2c);

    (void)HAL_I2C_DeInit(slave_i2c);
    (void)HAL_I2C_Init(slave_i2c);

    app_i2c_master_tx_done = 0U;
    app_i2c_master_rx_done = 0U;
    app_i2c_slave_tx_done = 0U;
    app_i2c_slave_rx_done = 0U;
    app_i2c_error = 0U;
}

#if (APP_I2C_TEST_MODE == APP_I2C_TEST_MODE_IT)

/**
 * @brief Transfers data from I2C master to I2C slave using interrupt mode.
 *
 * Starts slave reception, then starts master transmission and waits
 * for both transfer completion callbacks.
 *
 * @param[in] master_i2c I2C handle used as master.
 * @param[in] slave_i2c I2C handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_I2C_TEST_OK on success,
 *         APP_I2C_TEST_FAIL otherwise.
 */
static uint8_t app_i2c_test_master_to_slave_it(
								I2C_HandleTypeDef *master_i2c,
								I2C_HandleTypeDef *slave_i2c,
								const uint8_t *tx_data,
								uint8_t *rx_data,
								uint8_t length)
{
    uint32_t start_tick;

    if ((tx_data == NULL) || (rx_data == NULL) || (length == 0U))
    {
        return APP_I2C_TEST_FAIL;
    }

    app_i2c_master_tx_done = 0U;
    app_i2c_slave_rx_done = 0U;
    app_i2c_error = 0U;

    if (HAL_I2C_Slave_Receive_IT(&APP_I2C_TEST_SLAVE_I2C,
                                 rx_data,
                                 length) != HAL_OK)
    {
    	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    if (HAL_I2C_Master_Transmit_IT(&APP_I2C_TEST_MASTER_I2C,
                                   APP_I2C_TEST_SLAVE_ADDR,
                                   (uint8_t *)tx_data,
                                   length) != HAL_OK)
    {
    	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_i2c_master_tx_done == 0U) ||
           (app_i2c_slave_rx_done == 0U))
    {
        if (app_i2c_error != 0U)
        {
        	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_I2C_TEST_TIMEOUT_MS)
        {
        	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_I2C_TEST_OK;
}

/**
 * @brief Transfers data from I2C slave to I2C master using interrupt mode.
 *
 * Starts slave transmission, then starts master reception and waits
 * for both transfer completion callbacks.
 *
 * @param[in] master_i2c I2C handle used as master.
 * @param[in] slave_i2c I2C handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_I2C_TEST_OK on success,
 *         APP_I2C_TEST_FAIL otherwise.
 */
static uint8_t app_i2c_test_slave_to_master_it(
									I2C_HandleTypeDef *master_i2c,
									I2C_HandleTypeDef *slave_i2c,
									const uint8_t *tx_data,
									uint8_t *rx_data,
									uint8_t length)
{
    uint32_t start_tick;

    if ((tx_data == NULL) || (rx_data == NULL) || (length == 0U))
    {
        return APP_I2C_TEST_FAIL;
    }

    app_i2c_master_rx_done = 0U;
    app_i2c_slave_tx_done = 0U;
    app_i2c_error = 0U;

    if (HAL_I2C_Slave_Transmit_IT(&APP_I2C_TEST_SLAVE_I2C,
                                  (uint8_t *)tx_data,
                                  length) != HAL_OK)
    {
    	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    if (HAL_I2C_Master_Receive_IT(&APP_I2C_TEST_MASTER_I2C,
                                  APP_I2C_TEST_SLAVE_ADDR,
                                  rx_data,
                                  length) != HAL_OK)
    {
    	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_i2c_master_rx_done == 0U) ||
           (app_i2c_slave_tx_done == 0U))
    {
        if (app_i2c_error != 0U)
        {
        	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_I2C_TEST_TIMEOUT_MS)
        {
        	app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_I2C_TEST_OK;
}

/**
 * @brief Executes I2C test using interrupt mode.
 *
 * @param[in] command Pointer to I2C test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success, APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_i2c_test_run_it(const app_protocol_command_t *command)
{
    if (command == NULL)
        return APP_PROTOCOL_TEST_FAIL;

    APP_LOG_DEBUG("Running I2C IT test %lu\r\n", command->test_id);

#if (APP_LOG_ENABLE_DEBUG == 1U)
    app_debug_print_buffer("Pattern", command->pattern, command->pattern_len);
#endif

    for (uint8_t i = 0U; i < command->iterations; i++)
	{
    	APP_LOG_DEBUG("I2C iteration %u\r\n", (uint8_t)(i + 1U));

		/*
		 * Master -> Slave
		 */
		if (app_i2c_test_master_to_slave_it(
				&APP_I2C_TEST_MASTER_I2C,
				&APP_I2C_TEST_SLAVE_I2C,
				command->pattern,
				app_i2c_slave_rx_buffer,
				command->pattern_len) != APP_I2C_TEST_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		if (app_data_compare(command->pattern,
							 app_i2c_slave_rx_buffer,
							 command->pattern_len) != APP_DATA_COMPARE_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		/*
		 * Slave -> Master
		 */
		if (app_i2c_test_slave_to_master_it(
				&APP_I2C_TEST_MASTER_I2C,
				&APP_I2C_TEST_SLAVE_I2C,
				app_i2c_slave_rx_buffer,
				app_i2c_master_rx_buffer,
				command->pattern_len) != APP_I2C_TEST_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		if (app_data_compare(command->pattern,
							 app_i2c_master_rx_buffer,
							 command->pattern_len) != APP_DATA_COMPARE_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		APP_LOG_DEBUG("I2C iteration %u PASS\r\n", (uint8_t)(i + 1U));
	}

    return APP_PROTOCOL_TEST_SUCCESS;
}

#elif (APP_I2C_TEST_MODE == APP_I2C_TEST_MODE_DMA)

/**
 * @brief Transfers data from I2C master to I2C slave using DMA mode.
 *
 * Starts slave DMA reception, then starts master DMA transmission and
 * waits for both transfer completion callbacks.
 *
 * @param[in] master_i2c I2C handle used as master.
 * @param[in] slave_i2c I2C handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_I2C_TEST_OK on success,
 *         APP_I2C_TEST_FAIL otherwise.
 */
static uint8_t app_i2c_test_master_to_slave_dma(
    I2C_HandleTypeDef *master_i2c,
    I2C_HandleTypeDef *slave_i2c,
    const uint8_t *tx_data,
    uint8_t *rx_data,
    uint8_t length)
{
    uint32_t start_tick;

    if ((master_i2c == NULL) ||
        (slave_i2c == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_I2C_TEST_FAIL;
    }

    app_i2c_master_tx_done = 0U;
    app_i2c_slave_rx_done = 0U;
    app_i2c_error = 0U;

    if (HAL_I2C_Slave_Receive_DMA(slave_i2c, rx_data, length) != HAL_OK)
    {
        app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    if (HAL_I2C_Master_Transmit_DMA(master_i2c,
                                    APP_I2C_TEST_SLAVE_ADDR,
                                    (uint8_t *)tx_data,
                                    length) != HAL_OK)
    {
        app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_i2c_master_tx_done == 0U) ||
           (app_i2c_slave_rx_done == 0U))
    {
        if (app_i2c_error != 0U)
        {
            app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_I2C_TEST_TIMEOUT_MS)
        {
            app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_I2C_TEST_OK;
}

/**
 * @brief Transfers data from I2C slave to I2C master using DMA mode.
 *
 * Starts slave DMA transmission, then starts master DMA reception and
 * waits for both transfer completion callbacks.
 *
 * @param[in] master_i2c I2C handle used as master.
 * @param[in] slave_i2c I2C handle used as slave.
 * @param[in] tx_data Pointer to transmit buffer.
 * @param[out] rx_data Pointer to receive buffer.
 * @param[in] length Number of bytes to transfer.
 *
 * @return APP_I2C_TEST_OK on success,
 *         APP_I2C_TEST_FAIL otherwise.
 */
static uint8_t app_i2c_test_slave_to_master_dma(
    I2C_HandleTypeDef *master_i2c,
    I2C_HandleTypeDef *slave_i2c,
    const uint8_t *tx_data,
    uint8_t *rx_data,
    uint8_t length)
{
    uint32_t start_tick;

    if ((master_i2c == NULL) ||
        (slave_i2c == NULL) ||
        (tx_data == NULL) ||
        (rx_data == NULL) ||
        (length == 0U))
    {
        return APP_I2C_TEST_FAIL;
    }

    app_i2c_master_rx_done = 0U;
    app_i2c_slave_tx_done = 0U;
    app_i2c_error = 0U;

    if (HAL_I2C_Slave_Transmit_DMA(slave_i2c,
                                   (uint8_t *)tx_data,
                                   length) != HAL_OK)
    {
        app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    if (HAL_I2C_Master_Receive_DMA(master_i2c,
                                   APP_I2C_TEST_SLAVE_ADDR,
                                   rx_data,
                                   length) != HAL_OK)
    {
        app_i2c_test_abort_transfer(master_i2c, slave_i2c);
        return APP_I2C_TEST_FAIL;
    }

    start_tick = HAL_GetTick();

    while ((app_i2c_master_rx_done == 0U) ||
           (app_i2c_slave_tx_done == 0U))
    {
        if (app_i2c_error != 0U)
        {
            app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        if ((HAL_GetTick() - start_tick) > APP_I2C_TEST_TIMEOUT_MS)
        {
            app_i2c_test_abort_transfer(master_i2c, slave_i2c);
            return APP_I2C_TEST_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return APP_I2C_TEST_OK;
}

/**
 * @brief Executes I2C test using DMA mode.
 *
 * @param[in] command Pointer to I2C test command.
 *
 * @return APP_PROTOCOL_TEST_SUCCESS on success, APP_PROTOCOL_TEST_FAIL otherwise.
 */
static uint8_t app_i2c_test_run_dma(const app_protocol_command_t *command)
{
    if (command == NULL)
        return APP_PROTOCOL_TEST_FAIL;

    APP_LOG_DEBUG("Running I2C DMA test %lu\r\n", command->test_id);

    vTaskDelay(pdMS_TO_TICKS(2U));
#if (APP_LOG_ENABLE_DEBUG == 1U)
    app_debug_print_buffer("Pattern", command->pattern, command->pattern_len);
#endif

	for (uint8_t i = 0U; i < command->iterations; i++)
	{
		APP_LOG_DEBUG("I2C iteration %u\r\n", (uint8_t)(i + 1U));

		/*
		 * Master -> Slave
		 */
		if (app_i2c_test_master_to_slave_dma(
				&APP_I2C_TEST_MASTER_I2C,
				&APP_I2C_TEST_SLAVE_I2C,
				command->pattern,
				app_i2c_slave_rx_buffer,
				command->pattern_len) != APP_I2C_TEST_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		if (app_data_compare(command->pattern,
							 app_i2c_slave_rx_buffer,
							 command->pattern_len) != APP_DATA_COMPARE_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		/*
		 * Slave -> Master
		 */
		if (app_i2c_test_slave_to_master_dma(
				&APP_I2C_TEST_MASTER_I2C,
				&APP_I2C_TEST_SLAVE_I2C,
				app_i2c_slave_rx_buffer,
				app_i2c_master_rx_buffer,
				command->pattern_len) != APP_I2C_TEST_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		if (app_data_compare(command->pattern,
							 app_i2c_master_rx_buffer,
							 command->pattern_len) != APP_DATA_COMPARE_OK)
		{
			APP_LOG("I2C iteration %u FAIL\r\n", (uint8_t)(i + 1U));
			return APP_PROTOCOL_TEST_FAIL;
		}

		APP_LOG_DEBUG("I2C iteration %u PASS\r\n", (uint8_t)(i + 1U));
	}

    return APP_PROTOCOL_TEST_SUCCESS;
}

#else

	#error "Invalid I2C test mode"

#endif
