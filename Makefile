# ============================================================
#  MiniOS - Lightweight Embedded RTOS
#  Makefile for ARM Cortex-M3 (STM32F103)
# ============================================================

# Host OS helpers (Windows cmd.exe vs POSIX shell tools)
ifeq ($(OS),Windows_NT)
define MAKE_DIR
if not exist "$(subst /,\\,$(1))" mkdir "$(subst /,\\,$(1))"
endef
define REMOVE_DIR
if exist "$(subst /,\\,$(1))" (attrib -R /S /D "$(subst /,\\,$(1))\\*" 2>NUL & rmdir /S /Q "$(subst /,\\,$(1))")
endef
else
define MAKE_DIR
mkdir -p $(1)
endef
define REMOVE_DIR
rm -rf $(1)
endef
endif

# Toolchain
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
LD      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE    = $(PREFIX)size

# Target
TARGET  = minios

# Build directory
BUILD_DIR = build

# MCU flags (Cortex-M3)
CPU     = -mcpu=cortex-m3
FPU     =
FLOAT-ABI =
MCU     = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# Sources
C_SOURCES =  \
	kernel/heap4.c \
	kernel/task.c \
	kernel/scheduler.c \
	kernel/kernel.c \
	kernel/queue.c \
	kernel/semaphore.c \
	kernel/mutex.c \
	kernel/timer.c \
	kernel/eventgroup.c \
	kernel/mempool.c \
	kernel/mailbox.c \
	kernel/tickless.c \
	kernel/notify.c \
	kernel/stats.c \
	kernel/sysinfo.c \
	kernel/trace.c \
	kernel/watchdog.c \
	port/port.c \
	app/main.c

ASM_SOURCES = \
	port/startup_stm32f103.s

# Include paths
C_INCLUDES =  \
	-Iconfig \
	-Icommon \
	-Iinclude \
	-Ikernel \
	-Iport

# Compiler flags
CFLAGS  = $(MCU) $(C_INCLUDES) -Wall -Wextra -Werror
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -g -O2 -std=c11
CFLAGS += -DSTM32F103xB

# Assembler flags
ASFLAGS = $(MCU) -Wall -fdata-sections -ffunction-sections

# Linker script
LDSCRIPT = port/stm32f103.ld

# Linker flags
LDFLAGS = $(MCU) -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
LDFLAGS += -nostartfiles -lc -lm -lnosys

# Object files
OBJECTS  = $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.s=.o))

# Create build directories
OBJECTS_DIRS = $(sort $(dir $(OBJECTS)))

# ============================================================
#  Build Rules
# ============================================================

.PHONY: all clean size info

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin size

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@$(call MAKE_DIR,$(dir $@))
	$(CC) -c $(CFLAGS) $< -o $@

# Assemble ASM sources
$(BUILD_DIR)/%.o: %.s
	@$(call MAKE_DIR,$(dir $@))
	$(AS) -c $(ASFLAGS) $< -o $@

# Link
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(OBJDUMP) -D -S $@ > $(BUILD_DIR)/$(TARGET).lst

# Generate hex file
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# Generate binary file
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary -S $< $@

# Print size info
size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

# Clean build artifacts
clean:
	@$(call REMOVE_DIR,$(BUILD_DIR))

# Project info
info:
	@echo "Target:  $(TARGET)"
	@echo "MCU:     STM32F103 (Cortex-M3)"
	@echo "Sources: $(C_SOURCES)"
	@echo "ASM:     $(ASM_SOURCES)"

# Flash (using OpenOCD)
flash: $(BUILD_DIR)/$(TARGET).bin
	openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg \
		-c "program $< 0x08000000 verify reset exit"
