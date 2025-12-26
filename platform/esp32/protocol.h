/**
 * @file protocol.h
 * @brief EDTSP (ED61 Transport Protocol) Core Definitions
 * 
 * Distributed Device Management and Data Acquisition Protocol
 * for heterogeneous devices (PC, ESP32, Embedded Systems)
 * 
 * Features:
 * - Autonomous leader election (highest SourceID)
 * - Failover mechanism
 * - Platform-agnostic design
 * - Multi-rate sensor streaming
 */

#ifndef EDTSP_PROTOCOL_H
#define EDTSP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PROTOCOL CONSTANTS
// ============================================================================

/** Protocol magic number - identifies EDTSP packets */
#define EDTSP_MAGIC 0xED61

/** Protocol version */
#define EDTSP_VERSION 1

/** Maximum payload size (1 byte length field) */
#define EDTSP_MAX_PAYLOAD 255

/** Default multicast group */
#define EDTSP_MULTICAST_ADDR "239.255.0.1"

/** Default UDP port */
#define EDTSP_PORT 5000

/** Heartbeat interval (milliseconds) */
#define EDTSP_HEARTBEAT_INTERVAL_MS 1000

/** Heartbeat timeout (milliseconds) - consider device dead after this */
#define EDTSP_HEARTBEAT_TIMEOUT_MS 5000

/** Maximum number of devices in network */
#define EDTSP_MAX_DEVICES 256

// ============================================================================
// PACKET TYPES
// ============================================================================

/** Packet type enumeration */
typedef enum {
    EDTSP_TYPE_DISCOVERY  = 1,  /**< Device announcement and presence declaration */
    EDTSP_TYPE_HEARTBEAT  = 2,  /**< Liveness signal + Master/Slave role status */
    EDTSP_TYPE_HANDSHAKE  = 3,  /**< 3-way handshake + Capability mask reporting */
    EDTSP_TYPE_CONFIG     = 4,  /**< Master→Slave configuration (sampling rates) */
    EDTSP_TYPE_DATA       = 5   /**< Sensor data stream */
} EDTSPPacketType;

// ============================================================================
// DEVICE ROLES
// ============================================================================

/** Device role in network */
typedef enum {
    EDTSP_ROLE_UNKNOWN = 0,  /**< Role not yet determined */
    EDTSP_ROLE_SLAVE   = 1,  /**< Slave device (receives config, sends data) */
    EDTSP_ROLE_MASTER  = 2   /**< Master device (highest ID, sends config) */
} EDTSPRole;

// ============================================================================
// INTERFACE TYPES & PRIORITIES
// ============================================================================

/** Physical interface types */
typedef enum {
    EDTSP_IFACE_UNKNOWN = 0,  /**< Unknown interface */
    EDTSP_IFACE_ETH     = 1,  /**< Ethernet (Priority 1 - Primary) */
    EDTSP_IFACE_WIFI    = 2,  /**< WiFi (Priority 2 - Backup) */
    EDTSP_IFACE_5G      = 3   /**< 5G (Priority 3 - Backup) */
} EDTSPInterfaceType;

/** Get interface priority (lower = better) */
static inline uint8_t edtsp_iface_priority(EDTSPInterfaceType iface) {
    switch (iface) {
        case EDTSP_IFACE_ETH:  return 1;
        case EDTSP_IFACE_WIFI: return 2;
        case EDTSP_IFACE_5G:   return 3;
        default:               return 99;
    }
}

// ============================================================================
// CAPABILITY MASK
// ============================================================================

/** Capability bits (sensors and features) */
typedef enum {
    EDTSP_CAP_TEMPERATURE   = (1 << 0),  /**< Temperature sensor */
    EDTSP_CAP_HUMIDITY      = (1 << 1),  /**< Humidity sensor */
    EDTSP_CAP_PRESSURE      = (1 << 2),  /**< Pressure sensor */
    EDTSP_CAP_DISTANCE      = (1 << 3),  /**< Distance/ultrasonic sensor */
    EDTSP_CAP_LIGHT         = (1 << 4),  /**< Light sensor */
    EDTSP_CAP_MOTION        = (1 << 5),  /**< Motion/PIR sensor */
    EDTSP_CAP_GPS           = (1 << 6),  /**< GPS module */
    EDTSP_CAP_ACCELEROMETER = (1 << 7),  /**< Accelerometer */
    EDTSP_CAP_GYROSCOPE     = (1 << 8),  /**< Gyroscope */
    EDTSP_CAP_MAGNETOMETER  = (1 << 9),  /**< Magnetometer */
    EDTSP_CAP_CURRENT       = (1 << 10), /**< Current sensor */
    EDTSP_CAP_VOLTAGE       = (1 << 11), /**< Voltage sensor */
    EDTSP_CAP_GAS           = (1 << 12), /**< Gas sensor */
    EDTSP_CAP_SMOKE         = (1 << 13), /**< Smoke detector */
    EDTSP_CAP_RELAY         = (1 << 14), /**< Relay output */
    EDTSP_CAP_PWM           = (1 << 15)  /**< PWM output */
} EDTSPCapability;

/** Capability mask type (16-bit bitmask) */
typedef uint16_t EDTSPCapabilityMask;

// ============================================================================
// PROTOCOL HEADER (8 bytes, padding-free)
// ============================================================================

#pragma pack(push, 1)

/**
 * EDTSP Protocol Header (8 bytes)
 * 
 * All multi-byte fields are in network byte order (big-endian)
 */
typedef struct {
    uint16_t magic;        /**< Protocol identifier: 0xED61 */
    uint8_t  type;         /**< Packet type (1-5) */
    uint32_t source_id;    /**< Unique device identifier (random, persistent) */
    uint8_t  payload_len;  /**< Payload size in bytes (0-255) */
} EDTSPHeader;

#pragma pack(pop)

/** Compile-time assertion: header must be exactly 8 bytes */
_Static_assert(sizeof(EDTSPHeader) == 8, "EDTSPHeader must be 8 bytes");

// ============================================================================
// PACKET STRUCTURES
// ============================================================================

#pragma pack(push, 1)

/**
 * Type 1: DISCOVERY Packet
 * 
 * Sent by devices when joining network or periodically
 * Announces presence and interface type
 */
typedef struct {
    EDTSPHeader header;              /**< Standard header */
    uint8_t     interface_type;      /**< EDTSPInterfaceType */
    uint8_t     version;             /**< Protocol version */
    char        device_name[32];     /**< Human-readable device name */
} EDTSPDiscoveryPacket;

/**
 * Type 2: HEARTBEAT Packet
 * 
 * Periodic liveness signal
 * Declares current role and uptime
 */
typedef struct {
    EDTSPHeader header;              /**< Standard header */
    uint8_t     role;                /**< EDTSPRole (Master/Slave) */
    uint32_t    uptime_ms;           /**< Device uptime in milliseconds */
    uint8_t     active_devices;      /**< Number of known active devices */
} EDTSPHeartbeatPacket;

/**
 * Type 3: HANDSHAKE Packet (ACK/Capability Report)
 * 
 * Three-way handshake and capability exchange
 * Slave reports sensors and features to Master
 */
typedef struct {
    EDTSPHeader          header;         /**< Standard header */
    uint8_t              handshake_step; /**< Handshake phase (1=SYN, 2=SYN-ACK, 3=ACK) */
    uint32_t             target_id;      /**< Target device ID (for handshake) */
    EDTSPCapabilityMask  capabilities;   /**< Available sensors/features (16-bit mask) */
    uint8_t              interface_type; /**< Current active interface */
} EDTSPHandshakePacket;

/**
 * Type 4: CONFIG Packet
 * 
 * Master sends configuration to Slave
 * Specifies which sensors to sample and at what rate
 */
typedef struct {
    EDTSPHeader header;              /**< Standard header */
    uint32_t    target_id;           /**< Target Slave device ID */
    uint8_t     sensor_id;           /**< Sensor to configure (capability bit index) */
    uint16_t    sampling_rate_ms;    /**< Sampling interval in milliseconds */
    uint8_t     enable;              /**< 1=enable, 0=disable */
} EDTSPConfigPacket;

/**
 * Type 5: DATA Packet
 * 
 * Slave sends sensor data to Master
 * Contains raw sensor reading with timestamp
 */
typedef struct {
    EDTSPHeader header;              /**< Standard header */
    uint8_t     sensor_id;           /**< Sensor ID (capability bit index) */
    uint32_t    timestamp_ms;        /**< Timestamp in milliseconds */
    uint8_t     data_len;            /**< Length of sensor data */
    uint8_t     data[64];            /**< Raw sensor data (flexible, max 64 bytes) */
} EDTSPDataPacket;

#pragma pack(pop)

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Validate packet header
 * 
 * @param header Pointer to header
 * @return true if valid, false otherwise
 */
static inline bool edtsp_header_valid(const EDTSPHeader *header) {
    if (!header) return false;
    if (header->magic != EDTSP_MAGIC) return false;
    if (header->type < EDTSP_TYPE_DISCOVERY || header->type > EDTSP_TYPE_DATA) return false;
    return true;
}

/**
 * Get packet type name (for debugging)
 * 
 * @param type Packet type
 * @return String name
 */
static inline const char* edtsp_type_name(uint8_t type) {
    switch (type) {
        case EDTSP_TYPE_DISCOVERY: return "DISCOVERY";
        case EDTSP_TYPE_HEARTBEAT: return "HEARTBEAT";
        case EDTSP_TYPE_HANDSHAKE: return "HANDSHAKE";
        case EDTSP_TYPE_CONFIG:    return "CONFIG";
        case EDTSP_TYPE_DATA:      return "DATA";
        default:                   return "UNKNOWN";
    }
}

/**
 * Get role name (for debugging)
 * 
 * @param role Device role
 * @return String name
 */
static inline const char* edtsp_role_name(uint8_t role) {
    switch (role) {
        case EDTSP_ROLE_MASTER:  return "MASTER";
        case EDTSP_ROLE_SLAVE:   return "SLAVE";
        case EDTSP_ROLE_UNKNOWN: return "UNKNOWN";
        default:                 return "INVALID";
    }
}

/**
 * Get interface name (for debugging)
 * 
 * @param iface Interface type
 * @return String name
 */
static inline const char* edtsp_iface_name(uint8_t iface) {
    switch (iface) {
        case EDTSP_IFACE_ETH:  return "ETHERNET";
        case EDTSP_IFACE_WIFI: return "WIFI";
        case EDTSP_IFACE_5G:   return "5G";
        default:               return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif // EDTSP_PROTOCOL_H
