######################################
# target
######################################
TARGET = Demo


######################################
# building variables
######################################
# debug build?
DEBUG = 0
# optimization
OPT = -Og

# Build path
BUILD_DIR = build

######################################
# source
######################################
# C sources
C_SOURCES =  \
src/main.c \
src/uart.c \
src/audio_play.c \
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
board/stm32f401_discovery.c \
board/stm32f401_discovery_audio.c \
board/cs43l22/cs43l22.c \

# C includes
C_INCLUDES =  \
-Isrc \
-Ilib \
-Istm32 \
-Istm32/cmsis \
-Istm32/hal/Inc \
-Iboard \
-Iboard/cs43l22 \

#-IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \

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
# cpu
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# macros for gcc
# AS defines
AS_DEFS =

# C defines
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DSTM32F401xC

# AS includes
AS_INCLUDES =

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

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
LIBDIR = lib/libPDMFilter_CM4F_GCC.a
LDFLAGS = $(MCU) -specs=nano.specs -u _printf_float -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

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

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

#######################################
# clean up
#######################################
clean:
	-rm -fR .dep $(BUILD_DIR)

#######################################
# dependencies
#######################################
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)

#######################################
# upload
#######################################

audio:
	st-flash --format binary write art_of_gard_128K.bin 0x8020000

upload: $(BUILD_DIR)/$(TARGET).hex
	st-flash --format ihex write $(BUILD_DIR)/$(TARGET).hex

# *** EOF ***
