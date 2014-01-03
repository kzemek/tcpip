#include "ip.h"

#include "common.h"
#include "ethernet.h"

#include <arpa/inet.h>

#include <memory.h>

typedef union PACKED ip_packet
{
    union PACKED
    {
        struct PACKED
        {
            uint32_t flow; // 4 bits version, 8 bits traffic class, 20 bits flow id
            uint16_t payload_length;
            uint8_t  next_header;
            uint8_t  hop_limit;
            uint8_t  src_ip[IP_ADDR_LEN];
            uint8_t  dst_ip[IP_ADDR_LEN];
            uint8_t  data[IP_DATA_MAX_LEN];
        };

        uint8_t version; // The first 4 bits are the version
    };

    uint8_t buffer[IP_PACKET_MAX_LEN];

} ip_packet_t;

static int ip_to_hw(const uint8_t ip_addr[], uint8_t hw_addr[])
{
    static uint8_t addr[] = { 0x33, 0x33, 0x0, 0x0, 0x0, 0x0 };
    memcpy(hw_addr, addr, ETH_ADDR_LEN);
    return 0;
}

size_t ip_send(session_t *session, const uint8_t dst_ip[], uint8_t protocol,
               const uint8_t data[], size_t data_len)
{
    // We can send maximum of IP_DATA_MAX_LEN
    data_len = MIN(data_len, IP_DATA_MAX_LEN);

    // Prepare the packet
    ip_packet_t packet;
    packet.flow = htonl(0);
    packet.version = 0x60;
    packet.payload_length = htons(data_len);
    packet.next_header = protocol;
    packet.hop_limit = 64;
    memcpy(packet.src_ip, session->src_ip, IP_ADDR_LEN);
    memcpy(packet.dst_ip, dst_ip, IP_ADDR_LEN);
    memcpy(packet.data, data, data_len);

    uint8_t dst_hw_addr[ETH_ADDR_LEN];
    if(ip_to_hw(dst_ip, dst_hw_addr) != 0)
        return 0;

    size_t data_left = data_len;
    size_t sent = 0;
    do
    {
        // We always send the header
        const size_t packet_len = IP_HEADER_LEN + data_left;
        sent = eth_send(session, dst_hw_addr, packet.buffer, packet_len);

        const size_t data_sent = sent - IP_HEADER_LEN;
        data_left -= data_sent;

        // Move unsent data to the beginning of packet's data
        if(data_sent)
            memmove(packet.data, packet.data + sent, data_left);

    } while(sent != 0 && data_left != 0);

    return data_left == 0 ? data_len : 0;
}

size_t ip_recv(session_t *session, uint8_t data[])
{
    ip_packet_t packet;

    const size_t packet_len = eth_recv(session, packet.buffer);
    if(packet_len == 0)
        return 0;

    const size_t data_len = packet_len - IP_HEADER_LEN;
    memcpy(data, packet.data, data_len);

    return data_len;
}

#define PSEUDO_PACKET_HEADER_LEN (2 * IP_ADDR_LEN + 8)

typedef union PACKED pseudo_packet
{
    struct PACKED
    {
        uint8_t src_ip[IP_ADDR_LEN];
        uint8_t dst_ip[IP_ADDR_LEN];
        uint32_t upper_layer_packet_len;
        uint8_t zeros[3];
        uint8_t next_header;
        uint8_t data[IP_DATA_MAX_LEN];
    };

    uint16_t buffer[(PSEUDO_PACKET_HEADER_LEN + IP_DATA_MAX_LEN)/2];

} pseudo_packet_t;

static uint16_t add_with_carry(uint16_t acc, uint16_t val)
{
    // The condition in ternary op checks for overflow without casting.
    return (acc + val) + (acc > UINT16_MAX - val ? 1 : 0);
}

/* @TODO Copying the whole packet data in order to calculate the checksum is far
 * from the most efficient implementation, but it's one of the most convenient
 * ones. */
uint16_t ip_chksum(session_t *session, const uint8_t dst_ip[], uint8_t protocol,
                   uint8_t data[], size_t data_len)
{
    pseudo_packet_t packet;
    memcpy(packet.src_ip, session->src_ip, IP_ADDR_LEN);
    memcpy(packet.dst_ip, dst_ip, IP_ADDR_LEN);
    packet.upper_layer_packet_len = htonl(data_len);
    memset(packet.zeros, 0, sizeof(packet.zeros));
    packet.next_header = protocol;
    memcpy(packet.data, data, data_len);

    uint32_t acc = 0;
    for(int i = 0; i < (PSEUDO_PACKET_HEADER_LEN + data_len)/2; ++i)
        acc = add_with_carry(acc, packet.buffer[i]);

    return (~acc == 0 ? acc : ~acc);
}