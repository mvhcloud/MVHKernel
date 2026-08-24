#include <stdint.h>
#include "mvh/net.h"

static net_arp_entry_t arp_cache[NET_ARP_CACHE_SIZE];
static net_route_t routes[NET_ROUTE_MAX];

static uint16_t load16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] << 8u) | data[1];
}

static uint32_t load32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | data[3];
}

static void store16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8u);
    data[1] = (uint8_t)value;
}

static void store32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24u);
    data[1] = (uint8_t)(value >> 16u);
    data[2] = (uint8_t)(value >> 8u);
    data[3] = (uint8_t)value;
}

static void copy_bytes(void *target, const void *source, uint32_t size)
{
    uint8_t *out = (uint8_t *)target;
    const uint8_t *in = (const uint8_t *)source;
    uint32_t index;
    for (index = 0u; index < size; index++) out[index] = in[index];
}

static int mac_equal(const net_mac_t *left, const net_mac_t *right)
{
    uint32_t index;
    for (index = 0u; index < 6u; index++)
        if (left->bytes[index] != right->bytes[index]) return 0;
    return 1;
}

uint16_t net_checksum(const void *data, uint32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0u;
    while (size >= 2u) {
        sum += load16(bytes);
        bytes += 2u;
        size -= 2u;
    }
    if (size != 0u) sum += (uint32_t)bytes[0] << 8u;
    while ((sum >> 16u) != 0u) sum = (sum & 0xFFFFu) + (sum >> 16u);
    return (uint16_t)~sum;
}

int net_ethernet_parse(const void *packet, uint32_t size, net_ethernet_frame_t *frame)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    if (packet == 0 || frame == 0 || size < NET_ETHERNET_HEADER) return -1;
    copy_bytes(frame->destination.bytes, bytes, 6u);
    copy_bytes(frame->source.bytes, bytes + 6u, 6u);
    frame->ethertype = load16(bytes + 12u);
    frame->payload = bytes + NET_ETHERNET_HEADER;
    frame->payload_size = size - NET_ETHERNET_HEADER;
    return 0;
}

int net_ethernet_build(void *packet, uint32_t capacity, const net_mac_t *destination,
                       const net_mac_t *source, uint16_t ethertype,
                       const void *payload, uint32_t payload_size)
{
    uint8_t *bytes = (uint8_t *)packet;
    uint32_t total = NET_ETHERNET_HEADER + payload_size;
    if (packet == 0 || destination == 0 || source == 0 ||
        (payload_size != 0u && payload == 0) || capacity < total) return -1;
    copy_bytes(bytes, destination->bytes, 6u);
    copy_bytes(bytes + 6u, source->bytes, 6u);
    store16(bytes + 12u, ethertype);
    if (payload_size != 0u) copy_bytes(bytes + NET_ETHERNET_HEADER, payload, payload_size);
    return (int)total;
}

int net_arp_parse(const void *packet, uint32_t size, net_arp_packet_t *arp)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    if (packet == 0 || arp == 0 || size < NET_ARP_PACKET_SIZE ||
        load16(bytes) != 1u || load16(bytes + 2u) != NET_ETHERTYPE_IPV4 ||
        bytes[4] != 6u || bytes[5] != 4u) return -1;
    arp->operation = load16(bytes + 6u);
    if (arp->operation != 1u && arp->operation != 2u) return -1;
    copy_bytes(arp->sender_mac.bytes, bytes + 8u, 6u);
    arp->sender_ip = load32(bytes + 14u);
    copy_bytes(arp->target_mac.bytes, bytes + 18u, 6u);
    arp->target_ip = load32(bytes + 24u);
    return 0;
}

int net_arp_build(void *packet, uint32_t capacity, uint16_t operation,
                  const net_mac_t *sender_mac, uint32_t sender_ip,
                  const net_mac_t *target_mac, uint32_t target_ip)
{
    uint8_t *bytes = (uint8_t *)packet;
    if (packet == 0 || sender_mac == 0 || target_mac == 0 ||
        capacity < NET_ARP_PACKET_SIZE || (operation != 1u && operation != 2u)) return -1;
    store16(bytes, 1u);
    store16(bytes + 2u, NET_ETHERTYPE_IPV4);
    bytes[4] = 6u;
    bytes[5] = 4u;
    store16(bytes + 6u, operation);
    copy_bytes(bytes + 8u, sender_mac->bytes, 6u);
    store32(bytes + 14u, sender_ip);
    copy_bytes(bytes + 18u, target_mac->bytes, 6u);
    store32(bytes + 24u, target_ip);
    return NET_ARP_PACKET_SIZE;
}

int net_ipv4_parse(const void *packet, uint32_t size, net_ipv4_packet_t *ipv4)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    uint32_t header_size;
    uint32_t total;
    if (packet == 0 || ipv4 == 0 || size < NET_IPV4_HEADER_MIN ||
        (bytes[0] >> 4u) != 4u || (bytes[0] & 0x0Fu) < 5u) return -1;
    header_size = (uint32_t)(bytes[0] & 0x0Fu) * 4u;
    total = load16(bytes + 2u);
    if (header_size > size || total < header_size || total > size ||
        net_checksum(bytes, header_size) != 0u) return -1;
    ipv4->header_size = (uint8_t)header_size;
    ipv4->dscp_ecn = bytes[1];
    ipv4->total_size = (uint16_t)total;
    ipv4->identification = load16(bytes + 4u);
    ipv4->fragment = load16(bytes + 6u);
    ipv4->ttl = bytes[8];
    ipv4->protocol = bytes[9];
    ipv4->source = load32(bytes + 12u);
    ipv4->destination = load32(bytes + 16u);
    ipv4->payload = bytes + header_size;
    ipv4->payload_size = total - header_size;
    return 0;
}

int net_ipv4_build(void *packet, uint32_t capacity, uint32_t source,
                   uint32_t destination, uint8_t protocol, uint8_t ttl,
                   uint16_t identification, const void *payload, uint32_t payload_size)
{
    uint8_t *bytes = (uint8_t *)packet;
    uint32_t total = NET_IPV4_HEADER_MIN + payload_size;
    uint32_t index;
    if (packet == 0 || (payload_size != 0u && payload == 0) || total > 65535u ||
        capacity < total || ttl == 0u) return -1;
    for (index = 0u; index < NET_IPV4_HEADER_MIN; index++) bytes[index] = 0u;
    bytes[0] = 0x45u;
    store16(bytes + 2u, (uint16_t)total);
    store16(bytes + 4u, identification);
    store16(bytes + 6u, 0x4000u);
    bytes[8] = ttl;
    bytes[9] = protocol;
    store32(bytes + 12u, source);
    store32(bytes + 16u, destination);
    store16(bytes + 10u, net_checksum(bytes, NET_IPV4_HEADER_MIN));
    if (payload_size != 0u) copy_bytes(bytes + NET_IPV4_HEADER_MIN, payload, payload_size);
    return (int)total;
}

static uint16_t udp_checksum(uint32_t source_ip, uint32_t destination_ip,
                             const uint8_t *udp, uint32_t size)
{
    uint32_t sum = (source_ip >> 16u) + (source_ip & 0xFFFFu) +
                   (destination_ip >> 16u) + (destination_ip & 0xFFFFu) +
                   NET_PROTOCOL_UDP + size;
    uint32_t offset;
    for (offset = 0u; offset + 1u < size; offset += 2u) sum += load16(udp + offset);
    if ((size & 1u) != 0u) sum += (uint32_t)udp[size - 1u] << 8u;
    while ((sum >> 16u) != 0u) sum = (sum & 0xFFFFu) + (sum >> 16u);
    return (uint16_t)~sum;
}

int net_udp_parse(const void *packet, uint32_t size, net_udp_datagram_t *udp)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    uint32_t length;
    if (packet == 0 || udp == 0 || size < 8u) return -1;
    length = load16(bytes + 4u);
    if (length < 8u || length > size) return -1;
    udp->source_port = load16(bytes);
    udp->destination_port = load16(bytes + 2u);
    udp->payload = bytes + 8u;
    udp->payload_size = length - 8u;
    return 0;
}

int net_udp_build(void *packet, uint32_t capacity, uint32_t source_ip,
                  uint32_t destination_ip, uint16_t source_port,
                  uint16_t destination_port, const void *payload, uint32_t payload_size)
{
    uint8_t *bytes = (uint8_t *)packet;
    uint32_t total = 8u + payload_size;
    uint16_t checksum;
    if (packet == 0 || (payload_size != 0u && payload == 0) ||
        total > 65535u || capacity < total || source_port == 0u || destination_port == 0u)
        return -1;
    store16(bytes, source_port);
    store16(bytes + 2u, destination_port);
    store16(bytes + 4u, (uint16_t)total);
    store16(bytes + 6u, 0u);
    if (payload_size != 0u) copy_bytes(bytes + 8u, payload, payload_size);
    checksum = udp_checksum(source_ip, destination_ip, bytes, total);
    store16(bytes + 6u, checksum == 0u ? 0xFFFFu : checksum);
    return (int)total;
}

int net_icmp_echo_reply(void *packet, uint32_t size)
{
    uint8_t *bytes = (uint8_t *)packet;
    if (packet == 0 || size < 8u || bytes[0] != 8u || bytes[1] != 0u ||
        net_checksum(bytes, size) != 0u) return -1;
    bytes[0] = 0u;
    bytes[2] = 0u;
    bytes[3] = 0u;
    store16(bytes + 2u, net_checksum(bytes, size));
    return 0;
}

void net_arp_cache_init(void)
{
    uint32_t index;
    for (index = 0u; index < NET_ARP_CACHE_SIZE; index++) arp_cache[index].valid = 0u;
}

void net_arp_cache_expire(uint64_t now)
{
    uint32_t index;
    for (index = 0u; index < NET_ARP_CACHE_SIZE; index++)
        if (arp_cache[index].valid != 0u && arp_cache[index].expires_at <= now)
            arp_cache[index].valid = 0u;
}

int net_arp_cache_update(uint32_t address, const net_mac_t *mac, uint64_t expires_at)
{
    uint32_t index;
    uint32_t slot = NET_ARP_CACHE_SIZE;
    if (address == 0u || mac == 0 || expires_at == 0u) return -1;
    for (index = 0u; index < NET_ARP_CACHE_SIZE; index++) {
        if (arp_cache[index].valid != 0u && arp_cache[index].address == address) {
            slot = index;
            break;
        }
        if (slot == NET_ARP_CACHE_SIZE && arp_cache[index].valid == 0u) slot = index;
    }
    if (slot == NET_ARP_CACHE_SIZE) slot = address % NET_ARP_CACHE_SIZE;
    arp_cache[slot].valid = 1u;
    arp_cache[slot].address = address;
    arp_cache[slot].mac = *mac;
    arp_cache[slot].expires_at = expires_at;
    return 0;
}

int net_arp_cache_lookup(uint32_t address, uint64_t now, net_mac_t *mac)
{
    uint32_t index;
    if (mac == 0) return -1;
    for (index = 0u; index < NET_ARP_CACHE_SIZE; index++) {
        if (arp_cache[index].valid != 0u && arp_cache[index].address == address) {
            if (arp_cache[index].expires_at <= now) {
                arp_cache[index].valid = 0u;
                return -1;
            }
            *mac = arp_cache[index].mac;
            return 0;
        }
    }
    return -1;
}

void net_route_init(void)
{
    uint32_t index;
    for (index = 0u; index < NET_ROUTE_MAX; index++) routes[index].valid = 0u;
}

int net_route_add(uint32_t network, uint32_t netmask, uint32_t gateway,
                  uint32_t metric, uint32_t interface_id)
{
    uint32_t index;
    if ((network & ~netmask) != 0u) return -1;
    for (index = 0u; index < NET_ROUTE_MAX; index++) {
        if (routes[index].valid == 0u) {
            routes[index].valid = 1u;
            routes[index].network = network;
            routes[index].netmask = netmask;
            routes[index].gateway = gateway;
            routes[index].metric = metric;
            routes[index].interface_id = interface_id;
            return 0;
        }
    }
    return -1;
}

static uint32_t mask_bits(uint32_t mask)
{
    uint32_t count = 0u;
    while (mask != 0u) {
        count += mask & 1u;
        mask >>= 1u;
    }
    return count;
}

int net_route_lookup(uint32_t destination, net_route_t *route)
{
    uint32_t index;
    uint32_t best = NET_ROUTE_MAX;
    uint32_t best_bits = 0u;
    for (index = 0u; index < NET_ROUTE_MAX; index++) {
        uint32_t bits;
        if (routes[index].valid == 0u ||
            (destination & routes[index].netmask) != routes[index].network) continue;
        bits = mask_bits(routes[index].netmask);
        if (best == NET_ROUTE_MAX || bits > best_bits ||
            (bits == best_bits && routes[index].metric < routes[best].metric)) {
            best = index;
            best_bits = bits;
        }
    }
    if (best == NET_ROUTE_MAX || route == 0) return -1;
    *route = routes[best];
    return 0;
}

int net_self_test(void)
{
    uint8_t packet[128];
    uint8_t payload[4] = {'m', 'v', 'h', '!'};
    net_mac_t a = {{0x02u, 0u, 0u, 0u, 0u, 1u}};
    net_mac_t b = {{0x02u, 0u, 0u, 0u, 0u, 2u}};
    net_mac_t result_mac;
    net_arp_packet_t arp;
    net_ipv4_packet_t ipv4;
    net_udp_datagram_t udp;
    net_route_t route;
    int udp_size;
    int ip_size;
    if (net_arp_build(packet, sizeof(packet), 1u, &a, 0xC0A8010Au,
                      &b, 0xC0A80101u) != NET_ARP_PACKET_SIZE ||
        net_arp_parse(packet, NET_ARP_PACKET_SIZE, &arp) != 0 ||
        arp.operation != 1u || arp.sender_ip != 0xC0A8010Au || !mac_equal(&arp.sender_mac, &a))
        return -1;
    udp_size = net_udp_build(packet + 32u, sizeof(packet) - 32u,
                             0xC0A8010Au, 0xC0A80101u, 1234u, 4321u,
                             payload, sizeof(payload));
    if (udp_size != 12 || net_udp_parse(packet + 32u, (uint32_t)udp_size, &udp) != 0 ||
        udp.source_port != 1234u || udp.destination_port != 4321u || udp.payload_size != 4u)
        return -1;
    ip_size = net_ipv4_build(packet, sizeof(packet), 0xC0A8010Au, 0xC0A80101u,
                             NET_PROTOCOL_UDP, 64u, 7u, packet + 32u, (uint32_t)udp_size);
    if (ip_size != 32 || net_ipv4_parse(packet, (uint32_t)ip_size, &ipv4) != 0 ||
        ipv4.protocol != NET_PROTOCOL_UDP || ipv4.payload_size != 12u) return -1;
    packet[10] ^= 1u;
    if (net_ipv4_parse(packet, (uint32_t)ip_size, &ipv4) == 0) return -1;
    packet[10] ^= 1u;
    net_arp_cache_init();
    if (net_arp_cache_update(0xC0A80101u, &b, 100u) != 0 ||
        net_arp_cache_lookup(0xC0A80101u, 99u, &result_mac) != 0 ||
        !mac_equal(&result_mac, &b) ||
        net_arp_cache_lookup(0xC0A80101u, 100u, &result_mac) == 0) return -1;
    net_route_init();
    if (net_route_add(0u, 0u, 0xC0A80101u, 100u, 1u) != 0 ||
        net_route_add(0xC0A80100u, 0xFFFFFF00u, 0u, 10u, 2u) != 0 ||
        net_route_lookup(0xC0A80144u, &route) != 0 || route.interface_id != 2u ||
        net_route_lookup(0x08080808u, &route) != 0 || route.gateway != 0xC0A80101u)
        return -1;
    return 0;
}
