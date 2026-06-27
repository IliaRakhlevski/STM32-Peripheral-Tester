#include "udp_client.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include "response_queue.h"

/* Enables or disables debug logging. */
#define APP_DEBUG_LOG_ENABLED 0

#if APP_DEBUG_LOG_ENABLED
#define APP_DEBUG_LOG(...) printf(__VA_ARGS__)
#else
#define APP_DEBUG_LOG(...)
#endif

/* Maximum time recvfrom() blocks before checking
 * whether the RX thread should terminate.
 */
#define UDP_CLIENT_RX_TIMEOUT_SEC 1U

/* UDP receive thread handle */
static pthread_t udp_client_rx_thread;

/* Shared between main thread and RX thread */
static volatile uint8_t udp_client_rx_thread_running = 0U;

/* Socket used by the RX thread */
static int udp_client_rx_sock = -1;

/**
 * @brief UDP receive thread.
 *
 * Continuously receives UDP responses and prints them.
 *
 * @param[in] arg Unused.
 *
 * @return Always NULL.
 */
static void *udp_client_rx_thread_func(void *arg)
{
    app_protocol_response_t response;
    ssize_t received_len;

    (void)arg;

    while (udp_client_rx_thread_running)
    {
        received_len = recvfrom(udp_client_rx_sock,
                                &response,
                                sizeof(response),
                                0,
                                NULL,
                                NULL);

        if (received_len == sizeof(response))
        {
            APP_DEBUG_LOG("Test %lu %s\n", (unsigned long)response.test_id, (response.result == APP_PROTOCOL_TEST_SUCCESS) ? "PASSED" : "FAILED");
            response_record_t record;

            record.response = response;
            record.received_time = time(NULL);

            if (response_queue_push(&record) != RESPONSE_QUEUE_OK)
            {
                printf("Failed to enqueue response\r\n");
            }
        }
        else if (received_len < 0)
        {
            /*
            * Receive timeout expired.
            * No UDP packet is available at the moment.
            * Continue looping so the thread can check the stop flag.
            */
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                continue;

            perror("recvfrom");
            continue;
        }
    }

    return NULL;
}

/**
 * @brief Starts UDP receive thread.
 *
 * Creates a background thread that continuously receives UDP
 * responses from the STM32 server.
 *
 * @param[in] sock UDP socket descriptor.
 *
 * @return UDP_CLIENT_OK on success, UDP_CLIENT_FAIL otherwise.
 */
udp_client_status_t udp_client_start_rx_thread(int sock)
{
    if (sock < 0)
    {
        return UDP_CLIENT_FAIL;
    }

    udp_client_rx_sock = sock;
    udp_client_rx_thread_running = 1U;

    if (pthread_create(&udp_client_rx_thread,
                       NULL,
                       udp_client_rx_thread_func,
                       NULL) != 0)
    {
        udp_client_rx_thread_running = 0U;
        return UDP_CLIENT_FAIL;
    }

    APP_DEBUG_LOG("UDP RX thread started\n");

    return UDP_CLIENT_OK;
}

/**
 * @brief Stops UDP receive thread.
 *
 * Requests the receive thread to terminate and waits for it
 * to exit.
 *
 * @return UDP_CLIENT_OK on success, UDP_CLIENT_FAIL otherwise.
 */
udp_client_status_t udp_client_stop_rx_thread(void)
{
    if (udp_client_rx_thread_running == 0U)
    {
        return UDP_CLIENT_FAIL;
    }

    udp_client_rx_thread_running = 0U;

    pthread_join(udp_client_rx_thread, NULL);

    APP_DEBUG_LOG("UDP RX thread stopped\n");

    return UDP_CLIENT_OK;
}

/**
 * @brief Opens UDP client socket.
 *
 * Creates a UDP socket used for communication with the STM32
 * test server.
 *
 * @return Socket descriptor on success, negative value otherwise.
 */
int udp_client_open(void)
{
    struct timeval timeout;
    timeout.tv_sec = UDP_CLIENT_RX_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    APP_DEBUG_LOG("Opening UDP socket...\n");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        perror("socket");
    }

    /*
    * Prevent recvfrom() from blocking forever.
    * This allows the RX thread to periodically check
    * the stop flag and exit gracefully.
    */
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
    {
        perror("setsockopt");
        close(sock);
        return -1;
    }

    APP_DEBUG_LOG("UDP socket opened: fd=%d\n", sock);

    return sock;
}

/**
 * @brief Sends command to the STM32 test server.
 *
 * Transmits a command packet over UDP.
 *
 * @param[in] sock UDP socket descriptor.
 * @param[in] command Pointer to command to send.
 *
 * @return UDP_CLIENT_OK on success, UDP_CLIENT_FAIL otherwise.
 */
udp_client_status_t udp_client_send_command(int sock, const app_protocol_command_t *command)
{
    struct sockaddr_in addr;
    ssize_t sent_len;

    if (command == NULL)
    {
        return UDP_CLIENT_FAIL;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_CLIENT_SERVER_PORT);

    if (inet_pton(AF_INET, UDP_CLIENT_SERVER_IP, &addr.sin_addr) != 1)
    {
        perror("inet_pton");
        return UDP_CLIENT_FAIL;
    }

    printf("Sending command: test_id=%lu peripheral=%s iterations=%u payload=%u bytes\n",
                                                (unsigned long)command->test_id,
                                                app_protocol_peripheral_to_string(command->peripheral),
                                                (unsigned int)command->iterations,
                                                (unsigned int)command->pattern_len);

    sent_len = sendto(sock,
                      command,
                      sizeof(*command),
                      0,
                      (struct sockaddr *)&addr,
                      sizeof(addr));

    if (sent_len != sizeof(*command))
    {
        perror("sendto");
        return UDP_CLIENT_FAIL;
    }

    APP_DEBUG_LOG("Command sent: %zd bytes\n", sent_len);

    return UDP_CLIENT_OK;
}

/**
 * @brief Closes UDP client socket.
 *
 * Releases all resources associated with the UDP socket.
 *
 * @param[in] sock UDP socket descriptor.
 */
void udp_client_close(int sock)
{
    close(sock);
}