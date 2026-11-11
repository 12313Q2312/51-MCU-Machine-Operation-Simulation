# firmware/Makefile — 51 MCU / STM32 固件构建
#
# 用法:
#   make PLATFORM=8051        # 编译 51 固件 (默认, 需要 SDCC)
#   make PLATFORM=stm32       # 编译 STM32 固件 (需要 arm-none-eabi-gcc)
#   make PLATFORM=8051 flash  # 编译并烧录 (需要 stcgal + USB-TTL)
#   make clean                # 清理构建产物

# ---- 平台选择 ----
PLATFORM ?= 8051
.DEFAULT_GOAL := all

# ---- 源文件 ----
SRCS = main.c protocol.c led_matrix.c queue.c

# ---- 输出目录 ----
BUILD_DIR = build
TARGET    = $(BUILD_DIR)/firmware

# ================================================================
# 51 MCU (SDCC)
# ================================================================
ifeq ($(PLATFORM),8051)
CC       = sdcc
CFLAGS   = -DMCU_8051 -mmcs51 --opt-code-size --std-c99
OBJS     = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.rel))
HEX      = $(TARGET).ihx
BIN      = $(TARGET).hex

$(BUILD_DIR)/%.rel: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(HEX): $(OBJS)
	$(CC) $(CFLAGS) -o $(HEX) $(OBJS)

# 将 .ihx 转为 .hex (用于烧录工具)
$(BIN): $(HEX)
	@echo "Generating $(BIN)..."
	packihx $(HEX) > $(BIN)

all: $(HEX) $(BIN)
	@echo ""
	@echo "========================================"
	@echo "  Build complete: 51 MCU firmware"
	@echo "  HEX:  $(HEX)"
	@echo "  BIN:  $(BIN)"
	@echo "========================================"

# ---- 烧录 (51 MCU: STC-ISP / stcgal) ----
flash: $(BIN)
	@echo ""
	@echo "========================================"
	@echo "  准备烧录到 51 MCU..."
	@echo "  请确保:"
	@echo "    1. USB-TTL 已连接 (TX→MCU RX, RX→MCU TX)"
	@echo "    2. MCU 已断电"
	@echo "========================================"
	@echo ""
	@echo "  推荐烧录工具:"
	@echo "    stcgal:  stcgal -P $(PORT) -b 115200 $(BIN)"
	@echo "    STC-ISP: 打开 STC-ISP.exe, 加载 $(BIN), 点击下载后给 MCU 上电"
	@echo ""
	@echo "  如果用 stcgal (自动烧录):"
	@echo "    stcgal -P $(PORT) -b 115200 $(BIN)"
	@echo "========================================"

# ================================================================
# STM32 (arm-none-eabi-gcc)
# ================================================================
else ifeq ($(PLATFORM),stm32)
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
CFLAGS   = -DMCU_STM32 -DSTM32F103xB -mcpu=cortex-m3 -mthumb -O2 -std=c99
LDFLAGS  = -T STM32F103C8Tx_FLASH.ld -nostartfiles -lc -lnosys
OBJS     = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
ELF      = $(TARGET).elf
BIN      = $(TARGET).bin

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(ELF) $(OBJS)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BIN)

all: $(ELF) $(BIN)
	@echo ""
	@echo "========================================"
	@echo "  Build complete: STM32 firmware"
	@echo "  ELF:  $(ELF)"
	@echo "  BIN:  $(BIN)"
	@echo "========================================"

flash: $(BIN)
	@echo ""
	@echo "  烧录到 STM32:"
	@echo "    st-flash write $(BIN) 0x08000000"
	@echo "    # 或使用 STM32CubeProgrammer"
	@echo "========================================"

# ================================================================
else
$(error Unknown PLATFORM: $(PLATFORM). Use PLATFORM=8051 or PLATFORM=stm32)
endif

# ---- 通用 ----
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned."

.PHONY: all flash clean
