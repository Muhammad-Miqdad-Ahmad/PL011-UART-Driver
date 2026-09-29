# Bare-metal PL011 UART driver build.
# Targets QEMU's "virt" machine / Renode's cortex-a15+PL011 platform by
# default (matches PL011_config.h's default UART0_BASE/UARTCLK_HZ) -
# override CROSS_COMPILE/CPU for real hardware.
#
# Two independent build worlds live here:
#   * firmware  -> cross-compiled with arm-none-eabi, runs in QEMU/Renode
#                  (all: run: renode: interactive: bin:)
#   * unit tests -> compiled with host g++ via CMake, run on this machine
#                  (test:) - see CMakeLists.txt / test/

CROSS_COMPILE ?= arm-none-eabi-
CC             = $(CROSS_COMPILE)gcc
OBJCOPY        = $(CROSS_COMPILE)objcopy

CPU ?= cortex-a15

SRC_DIR    = src
BUILD_DIR  = build
RENODE_DIR = renode
TEST_DIR   = build-tests

LDSCRIPT = linker.ld

SRCS = $(SRC_DIR)/DRIVER.c $(SRC_DIR)/HAL.c $(SRC_DIR)/application.c $(SRC_DIR)/main.c
OBJS = $(BUILD_DIR)/startup.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.d,$(SRCS))

TARGET = $(BUILD_DIR)/pl011.elf
BIN    = $(BUILD_DIR)/pl011.bin

CFLAGS  = -std=c11 -mcpu=$(CPU) -ffreestanding -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS = -nostdlib -nostartfiles -T $(LDSCRIPT)

QEMU      = qemu-system-arm
QEMUFLAGS = -M virt -cpu $(CPU) -nographic -kernel $(TARGET)

RENODE = renode

.PHONY: all clean run renode renode-gui interactive bin test test-clean clean-all

# --- firmware (cross-compiled) ---

all: $(TARGET)

$(TARGET): $(OBJS) $(LDSCRIPT) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/startup.o: $(SRC_DIR)/startup.s | $(BUILD_DIR)
	$(CC) -mcpu=$(CPU) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

bin: $(BIN)

$(BIN): $(TARGET)
	$(OBJCOPY) -O binary $< $@

run: $(TARGET)
	$(QEMU) $(QEMUFLAGS)

renode: $(TARGET)
	$(RENODE) --disable-gui --console -e "s @$(RENODE_DIR)/renode.resc"

# Interactive UART on TCP port 3456. Run this, then in a SECOND terminal:
#   telnet 127.0.0.1 3456
# Type there and watch each character echo back (RX round-trip).
interactive: $(TARGET)
	$(RENODE) --disable-gui --console -e "s @$(RENODE_DIR)/interactive.resc"

renode-gui: $(TARGET)
	$(RENODE) --console -e "s @$(RENODE_DIR)/renode.resc"

# --- host unit tests (GoogleTest via CMake) ---

# Configure (downloads gtest the first time), build, and run all tests.
# Uses host g++, not arm-none-eabi, and its own build dir so it never
# collides with the firmware in $(BUILD_DIR).
test:
	cmake -S . -B $(TEST_DIR)
	cmake --build $(TEST_DIR)
	ctest --test-dir $(TEST_DIR) --output-on-failure

# --- cleaning ---

clean:
	rm -rf $(BUILD_DIR)

# Wipes the test build too. Kept out of `clean` because it forces gtest to be
# re-downloaded on the next `make test`.
test-clean:
	rm -rf $(TEST_DIR)

clean-all: clean test-clean

-include $(DEPS)
