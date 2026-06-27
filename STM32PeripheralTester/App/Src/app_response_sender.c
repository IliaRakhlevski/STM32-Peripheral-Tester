#include "app_response_sender.h"
#include "app_response_queue.h"
#include "app_protocol.h"
#include "app_net.h"
#include "FreeRTOS.h"
#include "io_tools.h"
#include <stdio.h>


/**
 * @brief Sends test responses back to the PC.
 *
 * Currently this task only prints responses to UART.
 * Later it will send the same responses over UDP.
 *
 * @param[in] argument FreeRTOS task argument. Not used.
 */
void app_response_sender_task(void *argument)
{
    app_protocol_response_t response;

    (void)argument;

    for (;;)
    {
        if (app_response_queue_receive(&response, portMAX_DELAY) != APP_RESPONSE_QUEUE_OK)
        {
        	APP_LOG("Response sender receive failed\r\n");
            continue;
        }

        APP_LOG("Response: test_id=%lu result=%s\r\n", response.test_id, (response.result == APP_PROTOCOL_TEST_SUCCESS) ? "PASS" : "FAIL");

        if (app_net_udp_send_response(&response) != APP_NET_UDP_OK)
        {
            APP_LOG("UDP response send failed\r\n");
        }
    }
}
