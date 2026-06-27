#ifndef APP_NET
#define APP_NET

#include "app_protocol.h"
#include <stdint.h>

#define APP_NET_UDP_OK    0U
#define APP_NET_UDP_FAIL  1U

void app_net_init(void);
void app_net_poll(void);
uint8_t app_net_udp_send_response(const app_protocol_response_t *response);

#endif
