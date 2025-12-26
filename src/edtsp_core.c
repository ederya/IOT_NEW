/**
 * @file edtsp_core.c
 * @brief EDTSP Core Protocol Implementation
 * 
 * Platform-agnostic packet handling and protocol logic
 */

#include "../include/protocol.h"
#include <string.h>
#include <arpa/inet.h> // For htons/htonl (use platform-specific on embedded)

// ============================================================================
// BYTE ORDER CONVERSION (Network byte order)
// ============================================================================

#ifndef __EMBEDDED__
// PC/Linux: use standard network byte order functions
#define EDTSP_HTONS(x) htons(x)
#define EDTSP_HTONL(x) htonl(x)
#define EDTSP_NTOHS(x) ntohs(x)
#define EDTSP_NTOHL(x) ntohl(x)
#else
// Embedded: manual byte swapping (if needed)
static uint16_t edtsp_swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}
static uint32_t edtsp_swap32(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) | 
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}
#define EDTSP_HTONS(x) edtsp_swap16(x)
#define EDTSP_HTONL(x) edtsp_swap32(x)
#define EDTSP_NTOHS(x) edtsp_swap16(x)
#define EDTSP_NTOHL(x) edtsp_swap32(x)
#endif

// ============================================================================
// HEADER INITIALIZATION
// ============================================================================

void edtsp_init_header(EDTSPHeader *header, uint8_t type, uint32_t source_id, uint8_t payload_len) {
    if (!header) return;
    
    header->magic = EDTSP_HTONS(EDTSP_MAGIC);
    header->type = type;
    header->source_id = EDTSP_HTONL(source_id);
    header->payload_len = payload_len;
}

// ============================================================================
// PACKET BUILDERS
// ============================================================================

void edtsp_build_discovery(EDTSPDiscoveryPacket *pkt, uint32_t source_id, 
                           uint8_t iface_type, const char *device_name) {
    if (!pkt) return;
    
    memset(pkt, 0, sizeof(EDTSPDiscoveryPacket));
    edtsp_init_header(&pkt->header, EDTSP_TYPE_DISCOVERY, source_id, 
                      sizeof(EDTSPDiscoveryPacket) - sizeof(EDTSPHeader));
    
    pkt->interface_type = iface_type;
    pkt->version = EDTSP_VERSION;
    if (device_name) {
        strncpy(pkt->device_name, device_name, sizeof(pkt->device_name) - 1);
    }
}

void edtsp_build_heartbeat(EDTSPHeartbeatPacket *pkt, uint32_t source_id,
                          uint8_t role, uint32_t uptime_ms, uint8_t active_devices) {
    if (!pkt) return;
    
    memset(pkt, 0, sizeof(EDTSPHeartbeatPacket));
    edtsp_init_header(&pkt->header, EDTSP_TYPE_HEARTBEAT, source_id,
                      sizeof(EDTSPHeartbeatPacket) - sizeof(EDTSPHeader));
    
    pkt->role = role;
    pkt->uptime_ms = EDTSP_HTONL(uptime_ms);
    pkt->active_devices = active_devices;
}

void edtsp_build_handshake(EDTSPHandshakePacket *pkt, uint32_t source_id,
                          uint8_t step, uint32_t target_id, 
                          EDTSPCapabilityMask caps, uint8_t iface_type) {
    if (!pkt) return;
    
    memset(pkt, 0, sizeof(EDTSPHandshakePacket));
    edtsp_init_header(&pkt->header, EDTSP_TYPE_HANDSHAKE, source_id,
                      sizeof(EDTSPHandshakePacket) - sizeof(EDTSPHeader));
    
    pkt->handshake_step = step;
    pkt->target_id = EDTSP_HTONL(target_id);
    pkt->capabilities = EDTSP_HTONS(caps);
    pkt->interface_type = iface_type;
}

void edtsp_build_config(EDTSPConfigPacket *pkt, uint32_t source_id,
                       uint32_t target_id, uint8_t sensor_id,
                       uint16_t sampling_rate_ms, uint8_t enable) {
    if (!pkt) return;
    
    memset(pkt, 0, sizeof(EDTSPConfigPacket));
    edtsp_init_header(&pkt->header, EDTSP_TYPE_CONFIG, source_id,
                      sizeof(EDTSPConfigPacket) - sizeof(EDTSPHeader));
    
    pkt->target_id = EDTSP_HTONL(target_id);
    pkt->sensor_id = sensor_id;
    pkt->sampling_rate_ms = EDTSP_HTONS(sampling_rate_ms);
    pkt->enable = enable;
}

void edtsp_build_data(EDTSPDataPacket *pkt, uint32_t source_id,
                     uint8_t sensor_id, uint32_t timestamp_ms,
                     const uint8_t *data, uint8_t data_len) {
    if (!pkt || !data || data_len > 64) return;
    
    memset(pkt, 0, sizeof(EDTSPDataPacket));
    edtsp_init_header(&pkt->header, EDTSP_TYPE_DATA, source_id,
                      sizeof(EDTSPDataPacket) - sizeof(EDTSPHeader));
    
    pkt->sensor_id = sensor_id;
    pkt->timestamp_ms = EDTSP_HTONL(timestamp_ms);
    pkt->data_len = data_len;
    memcpy(pkt->data, data, data_len);
}

// ============================================================================
// PACKET PARSERS (convert from network byte order)
// ============================================================================

bool edtsp_parse_header(EDTSPHeader *header) {
    if (!header) return false;
    
    // Convert to host byte order
    header->magic = EDTSP_NTOHS(header->magic);
    header->source_id = EDTSP_NTOHL(header->source_id);
    
    // Validate
    return edtsp_header_valid(header);
}

void edtsp_parse_heartbeat(EDTSPHeartbeatPacket *pkt) {
    if (!pkt) return;
    pkt->uptime_ms = EDTSP_NTOHL(pkt->uptime_ms);
}

void edtsp_parse_handshake(EDTSPHandshakePacket *pkt) {
    if (!pkt) return;
    pkt->target_id = EDTSP_NTOHL(pkt->target_id);
    pkt->capabilities = EDTSP_NTOHS(pkt->capabilities);
}

void edtsp_parse_config(EDTSPConfigPacket *pkt) {
    if (!pkt) return;
    pkt->target_id = EDTSP_NTOHL(pkt->target_id);
    pkt->sampling_rate_ms = EDTSP_NTOHS(pkt->sampling_rate_ms);
}

void edtsp_parse_data(EDTSPDataPacket *pkt) {
    if (!pkt) return;
    pkt->timestamp_ms = EDTSP_NTOHL(pkt->timestamp_ms);
}
