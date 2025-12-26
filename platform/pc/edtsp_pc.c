/**
 * @file edtsp_pc.c
 * @brief EDTSP PC/Linux Implementation
 * 
 * Standalone application for PC/Linux devices
 */

#include "../../include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

// External functions from other modules
extern uint32_t edtsp_get_device_id(void);
extern void edtsp_init_header(EDTSPHeader *header, uint8_t type, uint32_t source_id, uint8_t payload_len);
extern void edtsp_build_discovery(EDTSPDiscoveryPacket *pkt, uint32_t source_id, uint8_t iface_type, const char *device_name);
extern void edtsp_build_heartbeat(EDTSPHeartbeatPacket *pkt, uint32_t source_id, uint8_t role, uint32_t uptime_ms, uint8_t active_devices);
extern bool edtsp_parse_header(EDTSPHeader *header);
extern void edtsp_parse_heartbeat(EDTSPHeartbeatPacket *pkt);
extern void edtsp_election_init(uint32_t device_id);
extern void edtsp_update_device(uint32_t device_id, uint64_t timestamp_ms, uint8_t role);
extern void edtsp_check_timeouts(uint64_t current_time_ms);
extern void edtsp_perform_election(void);
extern EDTSPRole edtsp_get_my_role(void);
extern uint8_t edtsp_get_active_device_count(void);
extern void edtsp_print_device_list(void);

// Global state
static int udp_socket = -1;
static uint32_t my_id = 0;
static uint64_t start_time_ms = 0;
static volatile bool running = true;

// ============================================================================
// UTILITIES
// ============================================================================

uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

void signal_handler(int sig) {
    (void)sig;
    running = false;
    printf("\n[MAIN] Shutting down...\n");
}

// ============================================================================
// NETWORK SETUP
// ============================================================================

bool setup_udp_socket(void) {
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    int reuse = 1;
    
    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("[NETWORK] Failed to create socket");
        return false;
    }
    
    // Allow address reuse (for multiple instances)
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[NETWORK] Failed to set SO_REUSEADDR");
    }
    
    // Bind to multicast port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(EDTSP_PORT);
    
    if (bind(udp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[NETWORK] Failed to bind socket");
        close(udp_socket);
        return false;
    }
    
    // Join multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(EDTSP_MULTICAST_ADDR);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("[NETWORK] Failed to join multicast group");
        close(udp_socket);
        return false;
    }
    
    printf("[NETWORK] Listening on %s:%d\n", EDTSP_MULTICAST_ADDR, EDTSP_PORT);
    return true;
}

bool send_packet(const void *data, size_t len) {
    struct sockaddr_in dest_addr;
    
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(EDTSP_MULTICAST_ADDR);
    dest_addr.sin_port = htons(EDTSP_PORT);
    
    ssize_t sent = sendto(udp_socket, data, len, 0, 
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (sent < 0) {
        perror("[NETWORK] Send failed");
        return false;
    }
    
    return true;
}

// ============================================================================
// PACKET HANDLERS
// ============================================================================

void handle_discovery(EDTSPDiscoveryPacket *pkt) {
    printf("[RX] DISCOVERY from 0x%08X: %s (%s)\n",
           pkt->header.source_id, pkt->device_name,
           edtsp_iface_name(pkt->interface_type));
    
    uint64_t now = get_time_ms();
    edtsp_update_device(pkt->header.source_id, now, EDTSP_ROLE_UNKNOWN);
    edtsp_perform_election();
}

void handle_heartbeat(EDTSPHeartbeatPacket *pkt) {
    edtsp_parse_heartbeat(pkt);
    
    printf("[RX] HEARTBEAT from 0x%08X: Role=%s, Uptime=%u ms, Devices=%u\n",
           pkt->header.source_id, edtsp_role_name(pkt->role),
           pkt->uptime_ms, pkt->active_devices);
    
    uint64_t now = get_time_ms();
    edtsp_update_device(pkt->header.source_id, now, pkt->role);
    edtsp_perform_election();
}

void receive_packets(void) {
    uint8_t buffer[512];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    // Set socket to non-blocking
    struct timeval tv = {0, 100000}; // 100ms timeout
    setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    ssize_t bytes = recvfrom(udp_socket, buffer, sizeof(buffer), 0,
                            (struct sockaddr*)&sender_addr, &addr_len);
    
    if (bytes < sizeof(EDTSPHeader)) return;
    
    EDTSPHeader *header = (EDTSPHeader*)buffer;
    
    // Parse header (convert from network byte order)
    EDTSPHeader header_copy = *header;
    if (!edtsp_parse_header(&header_copy)) {
        return; // Invalid packet
    }
    
    // Ignore own packets
    if (header_copy.source_id == my_id) return;
    
    // Dispatch by type
    switch (header_copy.type) {
        case EDTSP_TYPE_DISCOVERY:
            if (bytes >= sizeof(EDTSPDiscoveryPacket)) {
                EDTSPDiscoveryPacket *pkt = (EDTSPDiscoveryPacket*)buffer;
                pkt->header = header_copy;
                handle_discovery(pkt);
            }
            break;
            
        case EDTSP_TYPE_HEARTBEAT:
            if (bytes >= sizeof(EDTSPHeartbeatPacket)) {
                EDTSPHeartbeatPacket *pkt = (EDTSPHeartbeatPacket*)buffer;
                pkt->header = header_copy;
                handle_heartbeat(pkt);
            }
            break;
            
        default:
            printf("[RX] Packet type %s from 0x%08X (not yet handled)\n",
                   edtsp_type_name(header_copy.type), header_copy.source_id);
            break;
    }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void send_discovery(void) {
    EDTSPDiscoveryPacket pkt;
    char hostname[64];
    gethostname(hostname, sizeof(hostname));
    
    edtsp_build_discovery(&pkt, my_id, EDTSP_IFACE_ETH, hostname);
    send_packet(&pkt, sizeof(pkt));
    
    printf("[TX] DISCOVERY sent\n");
}

void send_heartbeat(void) {
    EDTSPHeartbeatPacket pkt;
    uint32_t uptime = (uint32_t)(get_time_ms() - start_time_ms);
    
    edtsp_build_heartbeat(&pkt, my_id, edtsp_get_my_role(), uptime, edtsp_get_active_device_count());
    send_packet(&pkt, sizeof(pkt));
    
    printf("[TX] HEARTBEAT sent: Role=%s\n", edtsp_role_name(edtsp_get_my_role()));
}

int main(int argc, char **argv) {
    (void)argc; // Unused
    (void)argv; // Unused
    
    printf("========================================\n");
    printf("  EDTSP PC Implementation\n");
    printf("========================================\n\n");
    
    // Handle signals
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize device ID
    my_id = edtsp_get_device_id();
    start_time_ms = get_time_ms();
    
    // Initialize election
    edtsp_election_init(my_id);
    
    // Setup network
    if (!setup_udp_socket()) {
        fprintf(stderr, "Failed to setup network!\n");
        return 1;
    }
    
    // Send initial discovery
    send_discovery();
    
    // Main loop
    uint64_t last_heartbeat = 0;
    uint64_t last_timeout_check = 0;
    uint64_t last_status_print = 0;
    
    printf("\n[MAIN] Starting main loop...\n\n");
    
    while (running) {
        uint64_t now = get_time_ms();
        
        // Send heartbeat every 1 second
        if (now - last_heartbeat >= EDTSP_HEARTBEAT_INTERVAL_MS) {
            send_heartbeat();
            last_heartbeat = now;
        }
        
        // Check timeouts every 1 second
        if (now - last_timeout_check >= 1000) {
            edtsp_check_timeouts(now);
            last_timeout_check = now;
        }
        
        // Print status every 5 seconds
        if (now - last_status_print >= 5000) {
            edtsp_print_device_list();
            last_status_print = now;
        }
        
        // Receive packets
        receive_packets();
    }
    
    // Cleanup
    if (udp_socket >= 0) {
        close(udp_socket);
    }
    
    printf("\n[MAIN] Goodbye!\n");
    return 0;
}
