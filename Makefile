CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft

CFLAGS  = $(CPU_FLAGS)
CFLAGS += -Wall -Wextra
CFLAGS += -O0 -g
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -nostdlib
CFLAGS += -ffreestanding

LDFLAGS  = $(CPU_FLAGS)
LDFLAGS += -T lm3s6965.ld
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=output.map
LDFLAGS += -nostdlib

INCLUDES  = -Isrc
INCLUDES += -Ifreertos/include
INCLUDES += -Ifreertos/portable/GCC/ARM_CM3

SRCS  = src/startup.c
SRCS += src/main.c
SRCS += src/mem.c
SRCS += freertos/tasks.c
SRCS += freertos/queue.c
SRCS += freertos/list.c
SRCS += freertos/timers.c
SRCS += freertos/event_groups.c
SRCS += freertos/portable/GCC/ARM_CM3/port.c
SRCS += freertos/portable/MemMang/heap_4.c

TARGET  = freertos-demo
ELF     = $(TARGET).elf
BIN     = $(TARGET).bin

all: $(ELF) size

$(ELF): $(SRCS) lm3s6965.ld
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) $(LDFLAGS) -o $@
	$(OBJCOPY) -O binary $@ $(BIN)

size: $(ELF)
	@echo ""
	@echo "--- Bellek Kullanimi ---"
	$(SIZE) $(ELF)

run: $(ELF)
	qemu-system-arm \
		-machine lm3s6965evb \
		-kernel $(ELF) \
		-nographic \
		-monitor null

debug: $(ELF)
	qemu-system-arm \
		-machine lm3s6965evb \
		-kernel $(ELF) \
		-nographic \
		-monitor null \
		-S -gdb tcp::1234

clean:
	rm -f $(ELF) $(BIN) output.map

.PHONY: all size run debug clean
