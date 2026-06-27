#include "response_writer.h"
#include "result_database.h"
#include "response_queue.h"
#include "app_protocol.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

/* Response writer thread. */
static pthread_t response_writer_thread;

/* Indicates whether the response writer thread is running. */
static uint8_t response_writer_running = 0U;

/**
 * @brief Response writer thread function.
 *
 * Reads response records from the response queue and prints
 * test results to stdout.
 *
 * @param[in] arg Thread argument. Not used.
 *
 * @return Always NULL.
 */
static void *response_writer_thread_func(void *arg)
{
    response_record_t record;

    (void)arg;

    while (response_writer_running != 0U)
    {
        if (response_queue_pop(&record) != RESPONSE_QUEUE_OK)
        {
            continue;
        }

        printf("Test %lu %s\n", (unsigned long)record.response.test_id,
               (record.response.result == APP_PROTOCOL_TEST_SUCCESS) ? "PASSED" : "FAILED");

        if (result_database_update_response(&record) != RESULT_DATABASE_OK)
        {
            printf("Failed to update response in database\n");
        }
    }

    return NULL;
}

/**
 * @brief Starts response writer thread.
 *
 * Creates a background thread that reads response records from
 * the response queue and writes them to the selected output.
 *
 * @return RESPONSE_WRITER_OK on success, RESPONSE_WRITER_FAIL otherwise.
 */
response_writer_status_t response_writer_start(void)
{
    response_writer_running = 1U;

    if (pthread_create(&response_writer_thread,
                       NULL,
                       response_writer_thread_func,
                       NULL) != 0)
    {
        response_writer_running = 0U;
        return RESPONSE_WRITER_FAIL;
    }

    return RESPONSE_WRITER_OK;
}

/**
 * @brief Stops response writer thread.
 *
 * Requests response writer thread shutdown and waits for it
 * to finish.
 *
 * @return RESPONSE_WRITER_OK on success, RESPONSE_WRITER_FAIL otherwise.
 */
response_writer_status_t response_writer_stop(void)
{
    response_writer_running = 0U;

    pthread_join(response_writer_thread, NULL);

    return RESPONSE_WRITER_OK;
}