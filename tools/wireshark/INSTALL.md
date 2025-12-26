# Wireshark EDTSP Dissector Installation Guide

## Installation

### Linux

1. **Find Wireshark plugins directory:**
   ```bash
   # Personal plugins folder (recommended):
   mkdir -p ~/.local/lib/wireshark/plugins
   
   # Or system-wide (requires sudo):
   # /usr/lib/x86_64-linux-gnu/wireshark/plugins
   ```

2. **Copy dissector:**
   ```bash
   cp edtsp.lua ~/.local/lib/wireshark/plugins/
   ```

3. **Restart Wireshark** or reload Lua plugins:
   - Menu: `Analyze → Reload Lua Plugins` (Ctrl+Shift+L)

### Windows

1. **Locate plugins folder:**
   ```
   C:\Users\<YourUsername>\AppData\Roaming\Wireshark\plugins
   ```

2. **Copy `edtsp.lua` to that folder**

3. **Restart Wireshark**

### macOS

1. **Plugins folder:**
   ```
   ~/.local/lib/wireshark/plugins
   ```

2. **Copy dissector and restart**

## Usage

### Capture EDTSP Traffic

1. **Start capture** on network interface
2. **Apply filter:**
   ```
   udp.port == 5000
   ```
   or
   ```
   edtsp
   ```

3. **Start EDTSP devices** (PC or ESP32)

4. **Watch packets** being decoded automatically!

### Example Filter Expressions

- All EDTSP packets:
  ```
  edtsp
  ```

- Only HEARTBEAT packets:
  ```
  edtsp.type == 2
  ```

- Packets from specific device:
  ```
  edtsp.source_id == 0x12345678
  ```

- Only MASTER heartbeats:
  ```
  edtsp.type == 2 && edtsp.role == 2
  ```

### Verify Installation

1. Open Wireshark
2. Go to: `Help → About Wireshark → Plugins`
3. Search for `edtsp.lua` in the list
4. If found, dissector is loaded!

## Troubleshooting

### Dissector not loading

- Check Lua syntax errors: `Analyze → Reload Lua Plugins`
- Verify file path is correct
- Check Wireshark console for errors

### Packets not decoded

- Verify magic number is `0xED61`
- Check UDP port is `5000`
- Try manual "Decode As": Right-click packet → `Decode As` → Select `EDTSP`

## Features

The EDTSP dissector automatically decodes:

- **Header**: Magic, Type, Source ID, Payload Length
- **DISCOVERY**: Interface type, device name
- **HEARTBEAT**: Role (Master/Slave), uptime, active devices
- **HANDSHAKE**: Capabilities, target device
- **CONFIG**: Sensor configuration, sampling rates
- **DATA**: Sensor readings with timestamps

Enjoy packet analysis! 📊
