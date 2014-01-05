#include "net.h"

#include "hw.h"
#include "ip.h"
#include "tcp.h"

#include <memory.h>
#include <stdlib.h>

session_t *net_init(const char *interface, const uint8_t src_ip[], uint16_t port, protocol_t protocol)
{
    session_t *s = malloc(sizeof(session_t));
    if(s == 0)
        return 0;

    s->session_id = hw_init(interface);
    if(s->session_id == -1)
    {
        free(s);
        return 0;
    }

    if(hw_if_addr(s->session_id, interface, s->src_addr) == -1)
    {
        net_free(s);
        return 0;
    }

    memcpy(s->src_ip, src_ip, IP_ADDR_LEN);
    s->port = port;
    
    switch(protocol)
    {
        case TCP:
            s->protocol = IP_PROTOCOL_TCP;
            break;
        case UDP:
            s->protocol = IP_PROTOCOL_UDP;
            break;
        default:
            net_free(s);
            return 0;
    }

    return s;
}

int net_free(session_t *session)
{
    const int err = hw_free(session->session_id);
    free(session);
    return err;
}

size_t net_send(session_t *session, const uint8_t data[], size_t data_len)
{
    switch(session->protocol)
    {
        case TCP:
            return tcp_send(session, data, data_len);
        case UDP:
            return 0;
        default:
            return 0;
    }
}

size_t net_recv(session_t *session, uint8_t buffer[], size_t buffer_len)
{
    switch(session->protocol)
    {
        case TCP:
            return tcp_recv(session, buffer, buffer_len);
        case UDP:
            return 0;
        default:
            return 0;
    }
}