#include "app_net.h"
#include "netif.h"
#include "lwip.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "io_tools.h"
#include "app_command_queue.h"
#include <string.h>
#include <stdio.h>



/* UDP port used by the application test server. */
#define APP_NET_UDP_PORT  5000U

extern struct netif gnetif;

/* Current Ethernet link state. */
static uint8_t last_link_state = 0xFF;

/* Active UDP control block. */
static struct udp_pcb *app_udp_pcb = NULL;

/* Remote end point of the last received command. */
static ip_addr_t app_udp_remote_addr;
static uint16_t app_udp_remote_port = 0U;
static uint8_t app_udp_remote_valid = 0U;


/**
 * @brief Processes a received UDP packet.
 *
 * This callback is invoked by lwIP when a UDP packet arrives on the
 * configured port.
 *
 * The current implementation sends the received packet back to the
 * sender (UDP echo) and frees the associated pbuf.
 *
 * @param arg User-supplied callback argument.
 * @param pcb UDP protocol control block.
 * @param p Pointer to the received packet buffer.
 * @param addr Source IP address.
 * @param port Source UDP port.
 */
static void app_net_udp_rx_callback(void *arg,
                                    struct udp_pcb *pcb,
                                    struct pbuf *p,
                                    const ip_addr_t *addr,
                                    u16_t port)
{
	app_protocol_command_t command;

    (void)arg;

    if (p == NULL)
    {
        return;
    }

    ip_addr_copy(app_udp_remote_addr, *addr);
	app_udp_remote_port = port;
	app_udp_remote_valid = 1U;

	APP_LOG_DEBUG("UDP received: len=%u\r\n", (unsigned int)p->tot_len);

    if (p->tot_len != sizeof(app_protocol_command_t))
    {
    	APP_LOG_DEBUG("UDP invalid command size: len=%u expected=%u\r\n",
								(unsigned int)p->tot_len,
								(unsigned int)sizeof(app_protocol_command_t));

        pbuf_free(p);
        return;
    }

    if (pbuf_copy_partial(p, &command, sizeof(command), 0) != sizeof(command))
    {
        APP_LOG("UDP command copy failed\r\n");

        pbuf_free(p);
        return;
    }

    if (app_command_queue_send(&command) != APP_COMMAND_QUEUE_OK)
    {
        APP_LOG("UDP command queue send failed\r\n");
    }

    APP_LOG_DEBUG("UDP command queued: test_id=%lu peripheral=%u\r\n", command.test_id, (unsigned int)command.peripheral);

    pbuf_free(p);
}

/**
 * @brief Initializes the UDP server used by the test application.
 *
 * Creates a UDP PCB, binds it to the configured port and registers
 * the receive callback.
 *
 * This function must be called after MX_LWIP_Init().
 */
static void app_net_udp_init(void)
{
    err_t err;

    app_udp_pcb = udp_new();

    if (app_udp_pcb == NULL)
    {
        APP_LOG("UDP init failed: udp_new\r\n");
        return;
    }

    err = udp_bind(app_udp_pcb, IP_ADDR_ANY, APP_NET_UDP_PORT);

    if (err != ERR_OK)
    {
        APP_LOG("UDP bind failed: %d\r\n", err);
        udp_remove(app_udp_pcb);
        app_udp_pcb = NULL;
        return;
    }

    udp_recv(app_udp_pcb, app_net_udp_rx_callback, NULL);

    APP_LOG_DEBUG("UDP listening on port %u\r\n", (unsigned int)APP_NET_UDP_PORT);
}

/**
 * @brief Initializes application-level Ethernet/link monitoring.
 *
 * This function must be called after MX_LWIP_Init().
 */
void app_net_init(void)
{
    last_link_state = netif_is_link_up(&gnetif);
    app_net_udp_init();
}

/**
 * @brief Periodically checks Ethernet link state and updates netif state.
 *
 * Should be called from a FreeRTOS task with a reasonable delay.
 */
void app_net_poll(void)
{
    uint8_t link_now = netif_is_link_up(&gnetif);

    /* Process LINK UP/LINK DOWN transitions. */
    if (link_now != last_link_state)
    {
        last_link_state = link_now;

        if (link_now)
        {
        	APP_LOG("ETH LINK UP\r\n");

            /* Turn off link indication LED. */
            HAL_GPIO_WritePin(LD2_GPIO_Port,LD2_Pin, GPIO_PIN_RESET);
            netif_set_up(&gnetif);
        }
        else
        {
        	APP_LOG("ETH LINK DOWN\r\n");
            netif_set_down(&gnetif);

        }
    }

    /* Blink blue LED while the Ethernet link is down. */
    if (!link_now)
	{
    	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
	}
}

/**
 * @brief Sends a response to the last UDP command sender.
 *
 * @param[in] response Pointer to response structure.
 *
 * @return APP_NET_OK on success, APP_NET_FAIL otherwise.
 */
uint8_t app_net_udp_send_response(const app_protocol_response_t *response)
{
	struct pbuf *pbuf;
	err_t err;

	if ((response == NULL) ||
		(app_udp_pcb == NULL) ||
		(app_udp_remote_valid == 0U))
	{
		return APP_NET_UDP_FAIL;
	}

	pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(app_protocol_response_t), PBUF_RAM);

	if (pbuf == NULL)
	{
		APP_LOG("UDP response pbuf allocation failed\r\n");
		return APP_NET_UDP_FAIL;
	}

	memcpy(pbuf->payload, response, sizeof(app_protocol_response_t));

	err = udp_sendto(app_udp_pcb,
					 pbuf,
					 &app_udp_remote_addr,
					 app_udp_remote_port);

	pbuf_free(pbuf);

	if (err != ERR_OK)
	{
		APP_LOG("UDP response send failed: err=%d\r\n", (int)err);
		return APP_NET_UDP_FAIL;
	}

	APP_LOG_DEBUG("UDP response sent: test_id=%lu\r\n", response->test_id);
	return APP_NET_UDP_OK;
}


