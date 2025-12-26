#!/bin/bash
# EDTSP 3-Device Test Launcher
# Launches 2 PC instances for testing

echo "=========================================="
echo "  EDTSP 3-Device Test"
echo "=========================================="
echo ""
echo "This script will launch 2 PC instances."
echo "You should also start your ESP32 device separately."
echo ""
echo "Press Ctrl+C to stop all instances."
echo ""

# Build the application first
echo "[BUILD] Compiling EDTSP..."
make clean && make

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "[BUILD] Build successful!"
echo ""

# Reset device IDs to force new random IDs
make reset-id

echo ""
echo "=========================================="
echo "  Starting Devices..."
echo "=========================================="
echo ""

# Launch instance 1 in new terminal
gnome-terminal --title="EDTSP Device 1" -- bash -c "
    rm -f /tmp/edtsp_device_id
    echo '=== EDTSP DEVICE 1 ==='
    ./edtsp_pc
    read -p 'Press Enter to close...'
" &

sleep 1

# Launch instance 2 in new terminal
gnome-terminal --title="EDTSP Device 2" -- bash -c "
    rm -f /tmp/edtsp_device_id
    echo '=== EDTSP DEVICE 2 ==='
    ./edtsp_pc
    read -p 'Press Enter to close...'
" &

echo ""
echo "✓ Device 1 started"
echo "✓ Device 2 started"
echo ""
echo "Now flash and start your ESP32 (Device 3)"
echo ""
echo "Expected behavior:"
echo "  1. All 3 devices discover each other"
echo "  2. Device with highest ID becomes Master"
echo "  3. Heartbeats exchanged every second"
echo "  4. Device list printed every 5 seconds"
echo ""
echo "To test failover:"
echo "  - Close the Master device terminal"
echo "  - Watch other devices re-elect new Master"
echo ""
echo "Press Ctrl+C to exit"
echo ""

# Wait for user interrupt
trap 'echo ""; echo "Stopping..."; exit 0' INT
wait
