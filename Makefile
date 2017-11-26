TARGET = synth
DEBUG = 1
OPT = -O1

BUILD_DIR = build

######################################
# source
######################################
C_SOURCES =  \
src/main.c \
src/synth.c \
src/midi.c \
src/notes.c \
src/stm32f4xx_it.c \
src/system_stm32f4xx.c \
stm32/hal/Src/stm32f4xx_hal.c \
stm32/hal/Src/stm32f4xx_hal_cortex.c \
stm32/hal/Src/stm32f4xx_hal_rcc_ex.c \
stm32/hal/Src/stm32f4xx_hal_i2c.c \
stm32/hal/Src/stm32f4xx_hal_i2s.c \
stm32/hal/Src/stm32f4xx_hal_i2s_ex.c \
stm32/hal/Src/stm32f4xx_hal_dma.c \
stm32/hal/Src/stm32f4xx_hal_gpio.c \
stm32/hal/Src/stm32f4xx_hal_pcd.c \
stm32/hal/Src/stm32f4xx_hal_pcd_ex.c \
stm32/hal/Src/stm32f4xx_hal_rcc.c \
stm32/hal/Src/stm32f4xx_hal_uart.c \
stm32/hal/Src/stm32f4xx_hal_usart.c \
stm32/hal/Src/stm32f4xx_hal_spi.c \
stm32/hal/Src/stm32f4xx_ll_tim.c \
stm32/hal/Src/stm32f4xx_ll_usart.c \
board/uart.c \
board/input.c \
board/stm32f401_discovery.c \
board/stm32f401_discovery_audio.c \
board/cs43l22/cs43l22.c \

C_INCLUDES =  \
-Isrc \
-Ilib \
-Istm32 \
-Istm32/cmsis \
-Istm32/hal/Inc \
-Iboard \
-Iboard/cs43l22 \

# ASM sources
ASM_SOURCES = stm32/startup_stm32f401xc.s


#######################################
# binaries
#######################################
BINPATH = /usr/bin/
PREFIX = arm-none-eabi-
CC = $(BINPATH)/$(PREFIX)gcc
AS = $(BINPATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(BINPATH)/$(PREFIX)objcopy
AR = $(BINPATH)/$(PREFIX)ar
SZ = $(BINPATH)/$(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# C defines
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DUSE_FULL_LL_DRIVER \
-DSTM32F401xC

ASFLAGS = $(MCU) $(OPT) -Wall -fdata-sections -ffunction-sections
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -Wdouble-promotion

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)"


#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = STM32F401VCTx_FLASH.ld

# libraries
LIBS = -lc -lm -lnosys
#LIBDIR = lib/libPDMFilter_CM4F_GCC.a
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections  -u _printf_float


OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))



################################################################################

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo $<
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo ASM $<
	@$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo linking
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@
	@arm-none-eabi-objdump -dS $@ > $(BUILD_DIR)/$(TARGET).txt

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

clean:
	-rm -fR .dep $(BUILD_DIR)

-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)

upload: $(BUILD_DIR)/$(TARGET).hex
	st-flash --format ihex write $(BUILD_DIR)/$(TARGET).hex

