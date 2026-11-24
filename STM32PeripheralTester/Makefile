# Command-line build of the STM32PeripheralTester firmware (STM32F756ZG, Cortex-M7).
#
# Uses the same settings as the STM32CubeIDE "Debug" configuration (.cproject).
# STM32CubeIDE does not use this file: it keeps building with its own makefiles
# in Debug/, so the project opens and builds in the IDE exactly as before.
#
# Requirements: GNU Arm Embedded Toolchain (arm-none-eabi-gcc) and make.
# Usage:
#   make          - build build/STM32PeripheralTester.elf, .hex and .bin
#   make clean    - remove the build directory

TARGET    := STM32PeripheralTester
BUILD_DIR := build

PREFIX  := arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

# ---- MCU (from .cproject) ----
MCU := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

DEFS := -DDEBUG -DUSE_HAL_DRIVER -DSTM32F756xx

LWIP := Middlewares/Third_Party/LwIP
INC_DIRS := \
	Core/Inc \
	App/Inc \
	App/Src \
	LWIP/App \
	LWIP/Target \
	$(LWIP)/src/include \
	$(LWIP)/system \
	$(LWIP)/system/arch \
	$(LWIP)/src/include/netif/ppp \
	$(LWIP)/src/include/lwip \
	$(LWIP)/src/include/lwip/apps \
	$(LWIP)/src/include/lwip/priv \
	$(LWIP)/src/include/lwip/prot \
	$(LWIP)/src/include/netif \
	$(LWIP)/src/include/compat/posix \
	$(LWIP)/src/include/compat/posix/arpa \
	$(LWIP)/src/include/compat/posix/net \
	$(LWIP)/src/include/compat/posix/sys \
	$(LWIP)/src/include/compat/stdc \
	Drivers/STM32F7xx_HAL_Driver/Inc \
	Drivers/STM32F7xx_HAL_Driver/Inc/Legacy \
	Drivers/BSP/Components/lan8742 \
	Drivers/CMSIS/Device/ST/STM32F7xx/Include \
	Drivers/CMSIS/Include \
	Middlewares/Third_Party/FreeRTOS/Source/include \
	Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 \
	Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
INCS := $(addprefix -I,$(INC_DIRS))

# ---- Sources ----
# Like STM32CubeIDE: every .c / .s file inside the project's source folders.
SRC_DIRS := App Core Drivers LWIP Middlewares
C_SRCS   := $(shell find $(SRC_DIRS) -name '*.c')
ASM_SRCS := $(shell find $(SRC_DIRS) -name '*.s')

LDSCRIPT := STM32F756ZGTX_FLASH.ld

# ---- Flags (STM32CubeIDE defaults for Debug) ----
CFLAGS := $(MCU) $(DEFS) $(INCS) \
	-std=gnu11 -g3 -O0 -Wall \
	-ffunction-sections -fdata-sections \
	--specs=nano.specs

ASFLAGS := $(MCU) $(DEFS) -g3 -x assembler-with-cpp --specs=nano.specs

LDFLAGS := $(MCU) -T$(LDSCRIPT) \
	--specs=nosys.specs --specs=nano.specs \
	-Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--gc-sections -static \
	-Wl,--start-group -lc -lm -Wl,--end-group

# ---- Objects ----
OBJS := \
	$(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o)) \
	$(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.s=.o))

# ---- Rules ----
.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
