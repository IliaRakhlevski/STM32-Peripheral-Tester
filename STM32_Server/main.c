#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include "app_protocol.h"
#include "udp_client.h"
#include "command_generator.h"
#include "response_queue.h"
#include "response_writer.h"
#include "result_database.h"

/* Minimum and maximum delay between generated commands */
#define COMMAND_GENERATOR_MIN_DELAY_SEC  1U
#define COMMAND_GENERATOR_MAX_DELAY_SEC  5U

/* Time to wait for pending responses before shutting down. */
#define APPLICATION_STOP_DELAY_SEC       20U

/* Application running flag controlled by the signal handler. */
static volatile sig_atomic_t app_running = 1;

/**
 * @brief Handles Ctrl+C (SIGINT).
 *
 * Requests graceful application shutdown.
 *
 * @param[in] signal Signal number.
 */
static void app_signal_handler(int signal)
{
    (void)signal;

    app_running = 0;
}

int main(void)
{
    int sock;
    uint8_t delay_sec = COMMAND_GENERATOR_MAX_DELAY_SEC;
    app_protocol_command_t command = {0};

    signal(SIGINT, app_signal_handler);

    printf("STM32 Server started\n");

    /* Create UDP client socket. */
    sock = udp_client_open();

    /* Initialize response queue. */
    if (response_queue_init() != RESPONSE_QUEUE_OK)
    {
        printf("Failed to initialize response queue\n");
        udp_client_close(sock);

        return EXIT_FAILURE;
    }

    /* Open the test results database. */
    if (result_database_open("test_results.db") != RESULT_DATABASE_OK)
    {
        printf("Failed to open database\n");

        response_queue_shutdown();
        udp_client_close(sock);

        return EXIT_FAILURE;
    }

    /* Start the response writer thread. */
    if (response_writer_start() != RESPONSE_WRITER_OK)
    {
        printf("Failed to start response writer\n");

        result_database_close();
        response_queue_shutdown();
        udp_client_close(sock);

        return EXIT_FAILURE;
    }

    /* Start the UDP receive thread. */
    if (udp_client_start_rx_thread(sock) != UDP_CLIENT_OK)
    {
        printf("Failed to start UDP RX thread\n");

        if (response_writer_stop() != RESPONSE_WRITER_OK)
        {
            printf("Failed to stop response writer\n");
        }

        result_database_close();
        response_queue_shutdown();
        udp_client_close(sock);

        return EXIT_FAILURE;
    }

    /* Initialize the test command generator. */
    command_generator_init();

    /* Main application loop. */
    while (app_running)
    {
        /* Generate the next test command. */
        command_generator_build_next(&command);

        /* Send the command to the target. */
        if (udp_client_send_command(sock, &command) == UDP_CLIENT_OK)
        {
            if (result_database_insert_command(&command) != RESULT_DATABASE_OK)
            {
                printf("Failed to insert command into database\n");
            }
        }
        else
        {
            printf("Failed to send command\n");
        }

        /* Wait before sending the next command. */
        delay_sec = command_generator_random_range(COMMAND_GENERATOR_MIN_DELAY_SEC, COMMAND_GENERATOR_MAX_DELAY_SEC);

        sleep(delay_sec);
    }

    printf("\nStopping application...\n");
    printf("Waiting for pending responses...\n");

    /* Wait before stopping the RX thread to allow pending test
    * responses to arrive from the target.
    */
    sleep(APPLICATION_STOP_DELAY_SEC);

    /* Stop the UDP receive thread. */
    if (udp_client_stop_rx_thread() != UDP_CLIENT_OK)
    {
        printf("Failed to stop UDP RX thread\n");
    }

    /* Shut down the response queue and stop the response writer. */
    response_queue_shutdown();

    /* Stop the response writer thread after queue shutdown. */
    if (response_writer_stop() != RESPONSE_WRITER_OK)
    {
        printf("Failed to stop response writer thread\n");
    }

    /* Close the UDP socket. */
    udp_client_close(sock);
  
    /* Print the final test execution summary. */
    if (result_database_print_summary() != RESULT_DATABASE_OK)
    {
        printf("Failed to print database summary\n");
    }

    /* Close the test results database. */
    result_database_close();

    printf("\nApplication stopped\n");

    return EXIT_SUCCESS;
}

