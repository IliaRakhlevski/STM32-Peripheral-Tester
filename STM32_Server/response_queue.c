#include "response_queue.h"
#include <pthread.h>
#include <string.h>


/* Circular buffer containing queued response records. */
static response_record_t response_queue_buffer[RESPONSE_QUEUE_CAPACITY];

/* Circular buffer indices and current number of queued responses. */
static uint32_t response_queue_head = 0U;
static uint32_t response_queue_tail = 0U;
static uint32_t response_queue_count = 0U;

/* Indicates that queue shutdown has been requested. */
static uint8_t response_queue_shutdown_requested = 0U;

/* Synchronization objects protecting access to the queue. */
static pthread_mutex_t response_queue_mutex;
static pthread_cond_t response_queue_not_empty;


/**
 * @brief Initializes response queue.
 *
 * Initializes internal queue state and synchronization primitives.
 *
 * @return RESPONSE_QUEUE_OK on success, RESPONSE_QUEUE_FAIL otherwise.
 */
response_queue_status_t response_queue_init(void)
{
    memset(response_queue_buffer, 0, sizeof(response_queue_buffer));

    response_queue_head = 0U;
    response_queue_tail = 0U;
    response_queue_count = 0U;
    response_queue_shutdown_requested = 0U;

    if (pthread_mutex_init(&response_queue_mutex, NULL) != 0)
    {
        return RESPONSE_QUEUE_FAIL;
    }

    if (pthread_cond_init(&response_queue_not_empty, NULL) != 0)
    {
        pthread_mutex_destroy(&response_queue_mutex);
        return RESPONSE_QUEUE_FAIL;
    }

    return RESPONSE_QUEUE_OK;
}

/**
 * @brief Pushes response record into queue.
 *
 * Adds a received response record to the queue.
 *
 * @param[in] record Pointer to response record to store.
 *
 * @return RESPONSE_QUEUE_OK on success, RESPONSE_QUEUE_FAIL otherwise.
 */
response_queue_status_t response_queue_push(const response_record_t *record)
{
    if (record == NULL)
    {
        return RESPONSE_QUEUE_FAIL;
    }

    pthread_mutex_lock(&response_queue_mutex);

    if (response_queue_count >= RESPONSE_QUEUE_CAPACITY)
    {
        pthread_mutex_unlock(&response_queue_mutex);
        return RESPONSE_QUEUE_FAIL;
    }

    response_queue_buffer[response_queue_tail] = *record;
    response_queue_tail++;

    if (response_queue_tail >= RESPONSE_QUEUE_CAPACITY)
    {
        response_queue_tail = 0U;
    }

    response_queue_count++;

    pthread_cond_signal(&response_queue_not_empty);
    pthread_mutex_unlock(&response_queue_mutex);

    return RESPONSE_QUEUE_OK;
}

/**
 * @brief Pops response record from queue.
 *
 * Blocks until a response record is available or queue shutdown is requested.
 *
 * @param[out] record Pointer to destination record.
 *
 * @return RESPONSE_QUEUE_OK on success, RESPONSE_QUEUE_FAIL otherwise.
 */
response_queue_status_t response_queue_pop(response_record_t *record)
{
    if (record == NULL)
    {
        return RESPONSE_QUEUE_FAIL;
    }

    pthread_mutex_lock(&response_queue_mutex);

    while ((response_queue_count == 0U) &&
           (response_queue_shutdown_requested == 0U))
    {
        pthread_cond_wait(&response_queue_not_empty, &response_queue_mutex);
    }

    if ((response_queue_count == 0U) &&
        (response_queue_shutdown_requested != 0U))
    {
        pthread_mutex_unlock(&response_queue_mutex);
        return RESPONSE_QUEUE_FAIL;
    }

    *record = response_queue_buffer[response_queue_head];
    response_queue_head++;

    if (response_queue_head >= RESPONSE_QUEUE_CAPACITY)
    {
        response_queue_head = 0U;
    }

    response_queue_count--;

    pthread_mutex_unlock(&response_queue_mutex);

    return RESPONSE_QUEUE_OK;
}

/**
 * @brief Requests response queue shutdown.
 *
 * Wakes any thread blocked on response_queue_pop().
 */
void response_queue_shutdown(void)
{
    pthread_mutex_lock(&response_queue_mutex);

    response_queue_shutdown_requested = 1U;

    pthread_cond_signal(&response_queue_not_empty);

    pthread_mutex_unlock(&response_queue_mutex);
}
