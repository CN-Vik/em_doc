>arm-none-eabi-gcc -c -mcpu=cortex-m3 -mthumb   -DUSE_HAL_DRIVER -DSTM32F103xB -ICore/Inc -IDrivers/STM32F1xx_HAL_Driver/Inc -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy -IDrivers/CMSIS/Device/ST/STM32F1xx/Include -IDrivers/CMSIS/Include -Og -Wall -fdata-sections -ffunction-sections -g -gdwarf-2 -MMD -MP -MF"build/stm32f1xx_hal_msp.d" -Wa,-a,-ad,-alms=build/stm32f1xx_hal_msp.lst Core/Src/stm32f1xx_hal_msp.c -o build/stm32f1xx_hal_msp.o   



这个命令是使用 `arm-none-eabi-gcc` 编译器来编译一个针对 STM32F103 系列 Cortex-M3 微控制器的源文件 `stm32f1xx_hal_msp.c`。下面是对命令中各部分的详细解析：

### 1. 编译器
- `arm-none-eabi-gcc`：这是用于 ARM 架构的 GCC 编译器，专门用于嵌入式系统开发。`none-eabi` 表示没有操作系统（裸机）的嵌入式应用二进制接口（Embedded Application Binary Interface）。

### 2. 编译选项
- `-c`：只编译源文件，不进行链接。生成的目标文件（`.o` 文件）将用于后续的链接步骤。
- `-mcpu=cortex-m3`：指定目标 CPU 为 Cortex-M3。这会启用与 Cortex-M3 架构相关的特定优化和指令集。
- `-mthumb`：生成 Thumb 指令集的代码。Thumb 是 ARM 的一种指令集，通常用于嵌入式系统，因为它生成的代码更紧凑。

### 3. 预处理器定义
- `-DUSE_HAL_DRIVER`：定义一个宏 `USE_HAL_DRIVER`，通常在 STM32 HAL 库中使用，表示使用 HAL 驱动。
- `-DSTM32F103xB`：定义一个宏 `STM32F103xB`，表示目标芯片是 STM32F103 系列的某个型号（如 STM32F103C8T6）。

### 4. 包含路径
- `-ICore/Inc`：将 `Core/Inc` 目录添加到头文件搜索路径中。
- `-IDrivers/STM32F1xx_HAL_Driver/Inc`：将 `Drivers/STM32F1xx_HAL_Driver/Inc` 目录添加到头文件搜索路径中。
- `-IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy`：将 `Drivers/STM32F1xx_HAL_Driver/Inc/Legacy` 目录添加到头文件搜索路径中。
- `-IDrivers/CMSIS/Device/ST/STM32F1xx/Include`：将 `Drivers/CMSIS/Device/ST/STM32F1xx/Include` 目录添加到头文件搜索路径中。
- `-IDrivers/CMSIS/Include`：将 `Drivers/CMSIS/Include` 目录添加到头文件搜索路径中。

### 5. 优化选项
- `-Og`：启用优化，但优化级别较低，适合调试。`-Og` 会保留调试信息，并且不会进行过于激进的优化，以便于调试。

### 6. 警告选项
- `-Wall`：启用所有常见的警告信息。这有助于在编译时发现潜在的问题。

### 7. 代码生成选项
- `-fdata-sections`：将每个数据项（如全局变量）放在单独的段中。这有助于链接器进行无用代码消除（dead code elimination）。
- `-ffunction-sections`：将每个函数放在单独的段中。同样，这有助于链接器进行无用代码消除。

### 8. 调试信息
- `-g`：生成调试信息。
- `-gdwarf-2`：生成 DWARF 2 格式的调试信息。DWARF 是一种调试信息格式，通常用于嵌入式系统。

### 9. 依赖生成
- `-MMD`：生成依赖文件（`.d` 文件），用于记录源文件所依赖的头文件。
- `-MP`：为每个依赖项生成一个伪目标（phony target），以防止在没有找到头文件时出现错误。
- `-MF"build/stm32f1xx_hal_msp.d"`：指定生成的依赖文件的名称和路径。

### 10. 汇编列表
- `-Wa,-a,-ad,-alms=build/stm32f1xx_hal_msp.lst`：生成汇编列表文件（`.lst` 文件），其中包含生成的汇编代码和源代码的对应关系。`-a` 选项表示生成汇编列表，`-ad` 表示禁用调试指令，`-alms` 指定输出文件。

### 11. 输入文件
- `Core/Src/stm32f1xx_hal_msp.c`：要编译的源文件。

### 12. 输出文件
- `-o build/stm32f1xx_hal_msp.o`：指定输出的目标文件（`.o` 文件）的名称和路径。

### 总结
这个命令的目的是将 `stm32f1xx_hal_msp.c` 文件编译为目标文件 `stm32f1xx_hal_msp.o`，同时生成调试信息、依赖文件和汇编列表文件。编译选项和路径设置都是为了确保代码能够正确地针对 STM32F103 系列 Cortex-M3 微控制器进行编译和调试。

----

# 简化后的makefile脚本

```c
# 工具链
CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-ld
OBJCOPY := arm-none-eabi-objcopy
OBJDUMP := arm-none-eabi-objdump
SIZE := arm-none-eabi-size

# 目标名称
TARGET := build/main

# 源文件与目标文件
SRCS := \
  Core/Src/main.c \
  Core/Src/stm32f1xx_hal_msp.c

OBJS := $(SRCS:%.c=build/%.o)
DEPS := $(OBJS:.o=.d)

# 编译器选项
CFLAGS := -mcpu=cortex-m3 -mthumb -DUSE_HAL_DRIVER -DSTM32F103xB
CFLAGS += -Og -Wall -fdata-sections -ffunction-sections -g -gdwarf-2
CFLAGS += -I Core/Inc \
          -I Drivers/STM32F1xx_HAL_Driver/Inc \
          -I Drivers/STM32F1xx_HAL_Driver/Inc/Legacy \
          -I Drivers/CMSIS/Device/ST/STM32F1xx/Include \
          -I Drivers/CMSIS/Include

# 链接器选项
LDFLAGS := -T STM32F103C8T6.ld -mcpu=cortex-m3 -mthumb -Wl,--gc-sections

# 默认目标
all: $(TARGET).elf

# 链接目标文件生成 ELF
$(TARGET).elf: $(OBJS)
	@echo "Linking $@"
	@$(CC) $(LDFLAGS) -o $@ $^
	@$(SIZE) $@

# 编译 C 文件
build/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CC) -c $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -o $@ $<

# 清理
clean:
	@rm -rf build

# 依赖文件包含
-include $(DEPS)

```
