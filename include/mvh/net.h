#ifndef MVH_NET_H
#define MVH_NET_H

#include <stdint.h>

#define NET_ETHERNET_HEADER 14u
#define NET_IPV4_HEADER_MIN 20u
#define NET_ARP_PACKET_SIZE 28u
#define NET_ARP_CACHE_SIZE 32u
#define NET_ROUTE_MAX 16u
#define NET_ETHERTYPE_IPV4 0x0800u
#define NET_ETHERTYPE_ARP 0x0806u
#define NET_PROTOCOL_ICMP 1u
#define NET_PROTOCOL_UDP 17u

typedef struct { uint8_t bytes[6]; } net_mac_t;

typedef struct {
    net_mac_t destination;
    net_mac_t source;
    uint16_t ethertype;
    const uint8_t *payload;
    uint32_t payload_size;
} net_ethernet_frame_t;

typedef struct {
    uint16_t operation;
    net_mac_t sender_mac;
    uint32_t sender_ip;
    net_mac_t target_mac;
    uint32_t target_ip;
} net_arp_packet_t;

typedef struct {
    uint8_t header_size;
    uint8_t dscp_ecn;
    uint16_t total_size;
    uint16_t identification;
    uint16_t fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint32_t source;
    uint32_t destination;
    const uint8_t *payload;
    uint32_t payload_size;
} net_ipv4_packet_t;

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    const uint8_t *payload;
    uint32_t payload_size;
} net_udp_datagram_t;

typedef struct {
    uint8_t valid;
    uint32_t address;
    net_mac_t mac;
    uint64_t expires_at;
} net_arp_entry_t;

typedef struct {
    uint8_t valid;
    uint32_t network;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t metric;
    uint32_t interface_id;
} net_route_t;

uint16_t net_checksum(const void *data, uint32_t size);
int net_ethernet_parse(const void *packet, uint32_t size, net_ethernet_frame_t *frame);
int net_ethernet_build(void *packet, uint32_t capacity, const net_mac_t *destination,
                       const net_mac_t *source, uint16_t ethertype,
                       const void *payload, uint32_t payload_size);
int net_arp_parse(const void *packet, uint32_t size, net_arp_packet_t *arp);
int net_arp_build(void *packet, uint32_t capacity, uint16_t operation,
                  const net_mac_t *sender_mac, uint32_t sender_ip,
                  const net_mac_t *target_mac, uint32_t target_ip);
int net_ipv4_parse(const void *packet, uint32_t size, net_ipv4_packet_t *ipv4);
int net_ipv4_build(void *packet, uint32_t capacity, uint32_t source,
                   uint32_t destination, uint8_t protocol, uint8_t ttl,
                   uint16_t identification, const void *payload, uint32_t payload_size);
int net_udp_parse(const void *packet, uint32_t size, net_udp_datagram_t *udp);
int net_udp_build(void *packet, uint32_t capacity, uint32_t source_ip,
                  uint32_t destination_ip, uint16_t source_port,
                  uint16_t destination_port, const void *payload, uint32_t payload_size);
int net_icmp_echo_reply(void *packet, uint32_t size);
void net_arp_cache_init(void);
void net_arp_cache_expire(uint64_t now);
int net_arp_cache_update(uint32_t address, const net_mac_t *mac, uint64_t expires_at);
int net_arp_cache_lookup(uint32_t address, uint64_t now, net_mac_t *mac);
void net_route_init(void);
int net_route_add(uint32_t network, uint32_t netmask, uint32_t gateway,
                  uint32_t metric, uint32_t interface_id);
int net_route_lookup(uint32_t destination, net_route_t *route);
int net_self_test(void);

#endif
