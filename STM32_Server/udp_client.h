#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <stdint.h>
#include "app_protocol.h"

#define UDP_CLIENT_SERVER_IP   "192.168.137.2"
#define UDP_CLIENT_SERVER_PORT 5000U

/**
 * @brief UDP client operation status.
 */
typedef enum
{
    UDP_CLIENT_OK = 0U, /**< Operation completed successfully. */
    UDP_CLIENT_FAIL     /**< Operation failed. */
} udp_client_status_t;

udp_client_status_t udp_client_start_rx_thread(int sock);
udp_client_status_t udp_client_stop_rx_thread(void);

int udp_client_open(void);
udp_client_status_t udp_client_send_command(int sock, const app_protocol_command_t *command);
//udp_client_status_t udp_client_receive_response(int sock, app_protocol_response_t *response);
void udp_client_close(int sock);

#endif