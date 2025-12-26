/**
 * @file leader_election.c
 * @brief EDTSP Leader Election Algorithm
 * 
 * Implements democratic leader election based on highest SourceID
 */

#include "../include/protocol.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// DEVICE TRACKING
// ============================================================================

typedef struct {
    uint32_t device_id;           /**< Device unique ID */
    uint64_t last_heartbeat_ms;   /**< Last received heartbeat timestamp */
    uint8_t  role;                /**< Current role (Master/Slave) */
    bool     active;              /**< Is device active? */
} EDTSPDeviceInfo;

static EDTSPDeviceInfo device_list[EDTSP_MAX_DEVICES];
static uint8_t device_count = 0;
static uint32_t my_device_id = 0;
static uint8_t my_role = EDTSP_ROLE_UNKNOWN;

// Forward declaration
void edtsp_perform_election(void);

// ============================================================================
// INITIALIZATION
// ============================================================================

void edtsp_election_init(uint32_t device_id) {
    my_device_id = device_id;
    device_count = 0;
    memset(device_list, 0, sizeof(device_list));
    my_role = EDTSP_ROLE_UNKNOWN;
}

// ============================================================================
// DEVICE MANAGEMENT
// ============================================================================

static int find_device_index(uint32_t device_id) {
    for (int i = 0; i < device_count; i++) {
        if (device_list[i].device_id == device_id) {
            return i;
        }
    }
    return -1;
}

void edtsp_update_device(uint32_t device_id, uint64_t timestamp_ms, uint8_t role) {
    int idx = find_device_index(device_id);
    
    if (idx == -1) {
        // New device
        if (device_count >= EDTSP_MAX_DEVICES) {
            printf("[ELECTION] WARNING: Device list full!\n");
            return;
        }
        idx = device_count++;
        device_list[idx].device_id = device_id;
        printf("[ELECTION] New device discovered: ID=0x%08X\n", device_id);
    }
    
    device_list[idx].last_heartbeat_ms = timestamp_ms;
    device_list[idx].role = role;
    device_list[idx].active = true;
}

void edtsp_check_timeouts(uint64_t current_time_ms) {
    bool topology_changed = false;
    
    for (int i = 0; i < device_count; i++) {
        if (!device_list[i].active) continue;
        
        uint64_t elapsed = current_time_ms - device_list[i].last_heartbeat_ms;
        if (elapsed > EDTSP_HEARTBEAT_TIMEOUT_MS) {
            printf("[ELECTION] Device timeout: ID=0x%08X (last seen %lu ms ago)\n",
                   device_list[i].device_id, elapsed);
            device_list[i].active = false;
            topology_changed = true;
        }
    }
    
    if (topology_changed) {
        // Trigger re-election
        edtsp_perform_election();
    }
}

// ============================================================================
// LEADER ELECTION ALGORITHM
// ============================================================================

/**
 * Perform leader election
 * 
 * Rule: Highest SourceID becomes Master
 * Democratic, deterministic, no central authority
 */
void edtsp_perform_election(void) {
    uint32_t highest_id = my_device_id;
    uint8_t old_role = my_role;
    
    // Find highest ID among active devices (including self)
    for (int i = 0; i < device_count; i++) {
        if (device_list[i].active && device_list[i].device_id > highest_id) {
            highest_id = device_list[i].device_id;
        }
    }
    
    // Determine new role
    if (highest_id == my_device_id) {
        my_role = EDTSP_ROLE_MASTER;
    } else {
        my_role = EDTSP_ROLE_SLAVE;
    }
    
    // Announce if role changed
    if (old_role != my_role) {
        printf("[ELECTION] *** ROLE CHANGE: %s → %s (My ID: 0x%08X, Master ID: 0x%08X) ***\n",
               edtsp_role_name(old_role), edtsp_role_name(my_role),
               my_device_id, highest_id);
    }
}

EDTSPRole edtsp_get_my_role(void) {
    return my_role;
}

uint8_t edtsp_get_active_device_count(void) {
    uint8_t count = 1; // Include self
    for (int i = 0; i < device_count; i++) {
        if (device_list[i].active) count++;
    }
    return count;
}

bool edtsp_is_master(void) {
    return my_role == EDTSP_ROLE_MASTER;
}

void edtsp_print_device_list(void) {
    printf("\n[ELECTION] === Device List (%d active) ===\n", edtsp_get_active_device_count());
    printf("  Self: ID=0x%08X, Role=%s\n", my_device_id, edtsp_role_name(my_role));
    
    for (int i = 0; i < device_count; i++) {
        if (device_list[i].active) {
            printf("  Device %d: ID=0x%08X, Role=%s\n",
                   i + 1, device_list[i].device_id, edtsp_role_name(device_list[i].role));
        }
    }
    printf("=====================================\n\n");
}
