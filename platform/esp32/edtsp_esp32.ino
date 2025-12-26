/**
 * @file edtsp_esp32.ino
 * @brief EDTSP ESP32 Arduino Implementation
 * 
 * ESP32 device implementation with NVS-based persistent ID
 * 
 * Required Libraries:
 * - WiFi (built-in)
 * - WiFiUdp (built-in)
 * - Preferences (built-in)
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>

// Include protocol header (place protocol.h in same folder or library)
extern "C" {
    #include "../../include/protocol.h"
}

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi credentials - CHANGE THESE!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Device configuration
const char* DEVICE_NAME = "ESP32-EDTSP";

// Capabilities (sensors available on this device)
const EDTSPCapabilityMask MY_CAPABILITIES = 
    EDTSP_CAP_TEMPERATURE | 
    EDTSP_CAP_HUMIDITY | 
    EDTSP_CAP_DISTANCE;

// ============================================================================
// GLOBAL STATE
// ============================================================================

WiFiUDP udp;
Preferences preferences;
IPAddress multicast_ip;

uint32_t my_device_id = 0;
uint8_t my_role = EDTSP_ROLE_UNKNOWN;
unsigned long start_time_ms = 0;

// Simple device tracking
struct DeviceInfo {
    uint32_t id;
    unsigned long last_seen_ms;
    uint8_t role;
    bool active;
};

#define MAX_TRACKED_DEVICES 16
DeviceInfo tracked_devices[MAX_TRACKED_DEVICES];
int device_count = 0;

// ============================================================================
// PERSISTENT ID MANAGEMENT (NVS)
// ============================================================================

uint32_t get_or_create_device_id() {
    preferences.begin("edtsp", false); // Read-write mode
    
    uint32_t id = preferences.getUInt("device_id", 0);
    
    if (id == 0) {
        // Generate new random ID
        id = esp_random();
        if (id == 0) id = 1; // Ensure non-zero
        
        preferences.putUInt("device_id", id);
        Serial.printf("[ID] Generated new ID: 0x%08X\n", id);
    } else {
        Serial.printf("[ID] Loaded persistent ID: 0x%08X\n", id);
    }
    
    preferences.end();
    return id;
}

// ============================================================================
// NETWORK SETUP
// ============================================================================

bool setup_wifi() {
    Serial.print("[WIFI] Connecting to ");
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WIFI] Connection failed!");
        return false;
    }
    
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
    
    return true;
}

bool setup_multicast() {
    multicast_ip.fromString(EDTSP_MULTICAST_ADDR);
    
    if (udp.beginMulticast(multicast_ip, EDTSP_PORT)) {
        Serial.printf("[NETWORK] Joined multicast %s:%d\n", 
                     EDTSP_MULTICAST_ADDR, EDTSP_PORT);
        return true;
    }
    
    Serial.println("[NETWORK] Failed to join multicast!");
    return false;
}

// ============================================================================
// PACKET SEND/RECEIVE
// ============================================================================

void send_packet(const void* data, size_t len) {
    udp.beginPacket(multicast_ip, EDTSP_PORT);
    udp.write((const uint8_t*)data, len);
    udp.endPacket();
}

void send_discovery() {
    EDTSPDiscoveryPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    // Fill header (manual - simplified for ESP32)
    pkt.header.magic = htons(EDTSP_MAGIC);
    pkt.header.type = EDTSP_TYPE_DISCOVERY;
    pkt.header.source_id = htonl(my_device_id);
    pkt.header.payload_len = sizeof(pkt) - sizeof(EDTSPHeader);
    
    pkt.interface_type = EDTSP_IFACE_WIFI;
    pkt.version = EDTSP_VERSION;
    strncpy(pkt.device_name, DEVICE_NAME, sizeof(pkt.device_name) - 1);
    
    send_packet(&pkt, sizeof(pkt));
    Serial.println("[TX] DISCOVERY sent");
}

void send_heartbeat() {
    EDTSPHeartbeatPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    
    pkt.header.magic = htons(EDTSP_MAGIC);
    pkt.header.type = EDTSP_TYPE_HEARTBEAT;
    pkt.header.source_id = htonl(my_device_id);
    pkt.header.payload_len = sizeof(pkt) - sizeof(EDTSPHeader);
    
    pkt.role = my_role;
    pkt.uptime_ms = htonl(millis() - start_time_ms);
    pkt.active_devices = get_active_device_count();
    
    send_packet(&pkt, sizeof(pkt));
    Serial.printf("[TX] HEARTBEAT: Role=%s\n", edtsp_role_name(my_role));
}

// ============================================================================
// LEADER ELECTION (Simplified)
// ============================================================================

void update_device(uint32_t id, uint8_t role) {
    // Find existing device
    for (int i = 0; i < device_count; i++) {
        if (tracked_devices[i].id == id) {
            tracked_devices[i].last_seen_ms = millis();
            tracked_devices[i].role = role;
            tracked_devices[i].active = true;
            return;
        }
    }
    
    // Add new device
    if (device_count < MAX_TRACKED_DEVICES) {
        tracked_devices[device_count].id = id;
        tracked_devices[device_count].last_seen_ms = millis();
        tracked_devices[device_count].role = role;
        tracked_devices[device_count].active = true;
        device_count++;
        Serial.printf("[ELECTION] New device: 0x%08X\n", id);
    }
}

void check_timeouts() {
    unsigned long now = millis();
    bool changed = false;
    
    for (int i = 0; i < device_count; i++) {
        if (tracked_devices[i].active) {
            if (now - tracked_devices[i].last_seen_ms > EDTSP_HEARTBEAT_TIMEOUT_MS) {
                Serial.printf("[ELECTION] Timeout: 0x%08X\n", tracked_devices[i].id);
                tracked_devices[i].active = false;
                changed = true;
            }
        }
    }
    
    if (changed) {
        perform_election();
    }
}

void perform_election() {
    uint32_t highest_id = my_device_id;
    uint8_t old_role = my_role;
    
    // Find highest ID
    for (int i = 0; i < device_count; i++) {
        if (tracked_devices[i].active && tracked_devices[i].id > highest_id) {
            highest_id = tracked_devices[i].id;
        }
    }
    
    // Set role
    my_role = (highest_id == my_device_id) ? EDTSP_ROLE_MASTER : EDTSP_ROLE_SLAVE;
    
    if (old_role != my_role) {
        Serial.printf("\n*** ROLE CHANGE: %s -> %s ***\n", 
                     edtsp_role_name(old_role), edtsp_role_name(my_role));
        Serial.printf("*** My ID: 0x%08X, Master ID: 0x%08X ***\n\n", 
                     my_device_id, highest_id);
    }
}

uint8_t get_active_device_count() {
    uint8_t count = 1; // Self
    for (int i = 0; i < device_count; i++) {
        if (tracked_devices[i].active) count++;
    }
    return count;
}

// ============================================================================
// PACKET HANDLERS
// ============================================================================

void handle_discovery(EDTSPDiscoveryPacket* pkt) {
    Serial.printf("[RX] DISCOVERY from 0x%08X: %s (%s)\n",
                 pkt->header.source_id, pkt->device_name,
                 edtsp_iface_name(pkt->interface_type));
    
    update_device(pkt->header.source_id, EDTSP_ROLE_UNKNOWN);
    perform_election();
}

void handle_heartbeat(EDTSPHeartbeatPacket* pkt) {
    uint32_t uptime = ntohl(pkt->uptime_ms);
    
    Serial.printf("[RX] HEARTBEAT from 0x%08X: Role=%s, Uptime=%u ms\n",
                 pkt->header.source_id, edtsp_role_name(pkt->role), uptime);
    
    update_device(pkt->header.source_id, pkt->role);
    perform_election();
}

void receive_packets() {
    int packet_size = udp.parsePacket();
    if (packet_size == 0) return;
    
    uint8_t buffer[512];
    int len = udp.read(buffer, sizeof(buffer));
    
    if (len < sizeof(EDTSPHeader)) return;
    
    // Parse header
    EDTSPHeader* header = (EDTSPHeader*)buffer;
    uint16_t magic = ntohs(header->magic);
    uint32_t source_id = ntohl(header->source_id);
    
    // Validate
    if (magic != EDTSP_MAGIC) return;
    if (source_id == my_device_id) return; // Ignore own packets
    
    // Update header with host byte order
    header->magic = magic;
    header->source_id = source_id;
    
    // Dispatch by type
    switch (header->type) {
        case EDTSP_TYPE_DISCOVERY:
            if (len >= sizeof(EDTSPDiscoveryPacket)) {
                handle_discovery((EDTSPDiscoveryPacket*)buffer);
            }
            break;
            
        case EDTSP_TYPE_HEARTBEAT:
            if (len >= sizeof(EDTSPHeartbeatPacket)) {
                handle_heartbeat((EDTSPHeartbeatPacket*)buffer);
            }
            break;
            
        default:
            Serial.printf("[RX] Packet type %s from 0x%08X\n",
                         edtsp_type_name(header->type), source_id);
            break;
    }
}

// ============================================================================
// ARDUINO SETUP & LOOP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  EDTSP ESP32 Implementation");
    Serial.println("========================================\n");
    
    // Get or create device ID
    my_device_id = get_or_create_device_id();
    start_time_ms = millis();
    
    // Connect to WiFi
    if (!setup_wifi()) {
        Serial.println("FATAL: WiFi connection failed!");
        while(1) delay(1000);
    }
    
    // Setup multicast
    if (!setup_multicast()) {
        Serial.println("FATAL: Multicast setup failed!");
        while(1) delay(1000);
    }
    
    // Send initial discovery
    send_discovery();
    
    Serial.println("\n[MAIN] Starting main loop...\n");
}

unsigned long last_heartbeat = 0;
unsigned long last_timeout_check = 0;
unsigned long last_status = 0;

void loop() {
    unsigned long now = millis();
    
    // Send heartbeat every second
    if (now - last_heartbeat >= EDTSP_HEARTBEAT_INTERVAL_MS) {
        send_heartbeat();
        last_heartbeat = now;
    }
    
    // Check timeouts every second
    if (now - last_timeout_check >= 1000) {
        check_timeouts();
        last_timeout_check = now;
    }
    
    // Print status every 5 seconds
    if (now - last_status >= 5000) {
        Serial.printf("\n[STATUS] Role: %s | Devices: %d | Uptime: %lu s\n\n",
                     edtsp_role_name(my_role), 
                     get_active_device_count(),
                     (now - start_time_ms) / 1000);
        last_status = now;
    }
    
    // Receive packets
    receive_packets();
    
    delay(10); // Small delay to prevent watchdog issues
}
