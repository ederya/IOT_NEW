# EDTSP - Distributed Device Management Protocol

**EDTSP (ED61 Transport Protocol)** is a distributed device management and data acquisition protocol for heterogeneous devices including PC, ESP32, and embedded systems.

## 🎯 Features

- **Autonomous Leader Election**: Democratic, highest SourceID becomes Master
- **Failover Mechanism**: Automatic re-election on Master failure (seconds)
- **Platform Agnostic**: Works on PC/Linux, ESP32 (Arduino), any embedded system
- **Multi-Rate Streaming**: Different sampling rates per sensor (10ms to 10s+)
- **Interface Prioritization**: Ethernet > WiFi > 5G automatic selection
- **Persistent Device IDs**: True random IDs stored in NVS (ESP32) or filesystem (PC)
- **Wireshark Support**: Full packet dissector for protocol analysis

## 📦 Protocol Specifications

### Header (8 bytes, padding-free)

```c
Magic:       0xED61 (2 bytes)
Type:        1-5 (1 byte)
SourceID:    Unique device ID (4 bytes)
PayloadLen:  0-255 (1 byte)
```

### Packet Types

1. **DISCOVERY**: Device announcement and presence
2. **HEARTBEAT**: Liveness signal + role status (Master/Slave)
3. **HANDSHAKE**: 3-way handshake + capability exchange
4. **CONFIG**: Master → Slave sensor configuration
5. **DATA**: Sensor data stream with timestamp

### Leader Election Algorithm

- **Rule**: Highest `SourceID` = Master
- **Deterministic**: Always same result for same devices
- **No central server**: Fully distributed
- **Automatic failover**: Re-elect on timeout (5 seconds)

## 🛠️ Building & Running

### PC/Linux

```bash
# Build
make

# Run single instance
./edtsp_pc

# Test with 3 devices (2 PC + ESP32)
./tests/test_3_devices.sh
```

### ESP32 (Arduino IDE)

1. **Open sketch**: `platform/esp32/edtsp_esp32.ino`
2. **Edit WiFi credentials** (lines 23-24):
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```
3. **Include protocol.h**: Copy `platform/esp32/protocol.h` to sketch folder
4. **Upload** to ESP32
5. **Monitor** Serial output (115200 baud)

## 📊 Wireshark Analysis

### Install Dissector

```bash
# Copy to Wireshark plugins folder
mkdir -p ~/.local/lib/wireshark/plugins
cp tools/wireshark/edtsp.lua ~/.local/lib/wireshark/plugins/

# Reload Lua plugins in Wireshark
# Menu: Analyze → Reload Lua Plugins
```

### Capture Examples

```bash
# Filter EDTSP packets
edtsp

# Only heartbeats
edtsp.type == 2

# Only Master heartbeats
edtsp.type == 2 && edtsp.role == 2

# Specific device
edtsp.source_id == 0x12345678
```

See [`tools/wireshark/INSTALL.md`](tools/wireshark/INSTALL.md) for details.

## 🧪 Testing Scenarios

### 3-Device Network Test

1. **Launch 2 PC instances**:
   ```bash
   ./tests/test_3_devices.sh
   ```

2. **Flash ESP32** and power on

3. **Observe**:
   - Discovery phase (all devices announce)
   - Leader election (highest ID wins)
   - Heartbeat exchange (every 1 second)
   - Device list updates (every 5 seconds)

### Failover Test

1. **Start 3 devices** (2 PC + ESP32)
2. **Identify Master** (check serial/terminal output)
3. **Kill Master** (Close terminal or power off)
4. **Observe re-election** (within 5 seconds)
5. **Verify new Master** elected

## 📁 Project Structure

```
IOT_NEW/
├── include/
│   └── protocol.h              # Core protocol definitions
├── src/
│   ├── edtsp_core.c            # Packet handling
│   └── leader_election.c       # Election algorithm
├── platform/
│   ├── esp32/
│   │   ├── edtsp_esp32.ino     # Arduino sketch
│   │   └── protocol.h          # Protocol header (copy)
│   └── pc/
│       ├── edtsp_pc.c          # PC application
│       └── persistent_id.c     # ID storage
├── tools/
│   └── wireshark/
│       ├── edtsp.lua           # Wireshark dissector
│       └── INSTALL.md          # Installation guide
├── tests/
│   └── test_3_devices.sh       # 3-device test launcher
├── Makefile                    # Build system
└── README.md                   # This file
```

## 🔧 Configuration

### Network Settings

Default multicast group: `239.255.0.1:5000`

Change in `include/protocol.h`:
```c
#define EDTSP_MULTICAST_ADDR "239.255.0.1"
#define EDTSP_PORT 5000
```

### Timing Parameters

```c
#define EDTSP_HEARTBEAT_INTERVAL_MS 1000  // Send heartbeat every 1s
#define EDTSP_HEARTBEAT_TIMEOUT_MS  5000  // Consider dead after 5s
```

### Device Capabilities

Edit in ESP32 sketch (`edtsp_esp32.ino`):
```cpp
const EDTSPCapabilityMask MY_CAPABILITIES = 
    EDTSP_CAP_TEMPERATURE | 
    EDTSP_CAP_HUMIDITY | 
    EDTSP_CAP_DISTANCE;
```

Available capabilities (16-bit bitmask):
- Temperature, Humidity, Pressure
- Distance, Light, Motion
- GPS, Accelerometer, Gyroscope, Magnetometer
- Current, Voltage, Gas, Smoke
- Relay, PWM outputs

## 🐛 Troubleshooting

### PC: "Failed to join multicast group"

- Check firewall allows UDP port 5000
- Verify network interface supports multicast
- Try running with sudo (testing only)

### ESP32: "WiFi connection failed"

- Double-check SSID and password
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check WiFi signal strength

### Devices not discovering each other

- Verify all on same network/subnet
- Check multicast is enabled on router
- Use Wireshark to confirm packets sent

### Wireshark not decoding packets

- Verify dissector installed: `Help → About → Plugins`
- Check magic number is `0xED61` in packet
- Manual decode: Right-click → `Decode As` → `EDTSP`

## 📝 Development Roadmap

- [x] Core protocol implementation
- [x] Leader election algorithm
- [x] PC/Linux support
- [x] ESP32 Arduino support
- [x] Wireshark dissector
- [ ] CONFIG packet implementation
- [ ] DATA packet streaming
- [ ] Multi-sensor sampling
- [ ] TLS/DTLS encryption
- [ ] QoS and reliability layer
- [ ] Web dashboard

## 🤝 Contributing

This is a demonstration/educational project. Feel free to fork and extend!

## 📄 License

MIT License - See project for details.

## 👤 Author

Created for IOT distributed systems research and development.

---

**Protocol Name**: EDTSP (ED61 Transport Protocol)  
**Magic Number**: `0xED61`  
**Version**: 1.0  
**Status**: Beta - Tested with 3 devices (2 PC + ESP32)
