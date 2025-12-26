# EDTSP PC Build System

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I./include -D_DEFAULT_SOURCE
LDFLAGS = -pthread

SRC_DIR = src
PLATFORM_DIR = platform/pc
BUILD_DIR = build
TARGET = edtsp_pc

# Source files
CORE_SOURCES = $(SRC_DIR)/edtsp_core.c \
               $(SRC_DIR)/leader_election.c

PLATFORM_SOURCES = $(PLATFORM_DIR)/edtsp_pc.c \
                   $(PLATFORM_DIR)/persistent_id.c

SOURCES = $(CORE_SOURCES) $(PLATFORM_SOURCES)
OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SOURCES)))

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build executable
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Build complete: $(TARGET)"

# Compile core sources
$(BUILD_DIR)/edtsp_core.o: $(SRC_DIR)/edtsp_core.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/leader_election.o: $(SRC_DIR)/leader_election.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile platform sources
$(BUILD_DIR)/edtsp_pc.o: $(PLATFORM_DIR)/edtsp_pc.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/persistent_id.o: $(PLATFORM_DIR)/persistent_id.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Clean complete"

# Run the application
run: $(TARGET)
	./$(TARGET)

# Reset device ID (for testing)
reset-id:
	rm -f /tmp/edtsp_device_id
	@echo "Device ID reset"

.PHONY: all clean run reset-id
