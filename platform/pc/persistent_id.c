/**
 * @file persistent_id.c
 * @brief Persistent Device ID Management (PC/Linux)
 * 
 * Generates and stores unique device ID in filesystem
 */

#include "../include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ID_FILE_PATH "/tmp/edtsp_device_id"

/**
 * Generate true random device ID
 */
static uint32_t generate_random_id(void) {
    uint32_t id = 0;
    FILE *urandom = fopen("/dev/urandom", "rb");
    
    if (urandom) {
        fread(&id, sizeof(id), 1, urandom);
        fclose(urandom);
    } else {
        // Fallback: use time-based seed
        srand(time(NULL) ^ getpid());
        id = (uint32_t)rand() * (uint32_t)rand();
    }
    
    // Ensure ID is never 0
    if (id == 0) id = 1;
    
    return id;
}

/**
 * Get or create persistent device ID
 * 
 * @return Unique device ID (persistent across reboots)
 */
uint32_t edtsp_get_device_id(void) {
    FILE *fp;
    uint32_t device_id = 0;
    
    // Try to read existing ID
    fp = fopen(ID_FILE_PATH, "rb");
    if (fp) {
        size_t read = fread(&device_id, sizeof(device_id), 1, fp);
        fclose(fp);
        
        if (read == 1 && device_id != 0) {
            printf("[ID] Loaded persistent ID: 0x%08X\n", device_id);
            return device_id;
        }
    }
    
    // Generate new ID
    device_id = generate_random_id();
    printf("[ID] Generated new random ID: 0x%08X\n", device_id);
    
    // Save to file
    fp = fopen(ID_FILE_PATH, "wb");
    if (fp) {
        fwrite(&device_id, sizeof(device_id), 1, fp);
        fclose(fp);
        printf("[ID] Saved ID to: %s\n", ID_FILE_PATH);
    } else {
        fprintf(stderr, "[ID] WARNING: Could not save ID to file!\n");
    }
    
    return device_id;
}

/**
 * Reset device ID (for testing)
 */
void edtsp_reset_device_id(void) {
    remove(ID_FILE_PATH);
    printf("[ID] Device ID reset (file deleted)\n");
}
