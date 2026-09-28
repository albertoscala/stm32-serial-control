CC = arm-none-eabi-gcc
FLAGS = -Wall -mthumb -mfloat-abi=soft -nostdlib -ffreestanding -specs=nosys.specs
ARCH = -mcpu=cortex-m4
SRC = src/main.c
BIN = bin/firmware.elf
LINKER = linker.ld


install:
	sudo pacman -Syu
	sudo pacman -S qemu-system-arm arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-gdb

prepare:
	mkdir -p src include bin
	mkdir -p src include src
	mkdir -p src include include

build:
	$(CC) $(ARCH) $(FLAGS) -T $(LINKER) $(SRC) -o $(BIN)

run:
	qemu-system-arm -M netduinoplus2 -kernel $(BIN) -nographic -serial stdio -monitor none