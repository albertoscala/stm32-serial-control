CC = arm-none-eabi-gcc
FLAGS = -Wall -mthumb -mfloat-abi=soft -nostdlib -ffreestanding -specs=nosys.specs
ARCH = -mcpu=cortex-m4
SRC = src/*
BIN = bin/firmware.elf
LINKER = linker.ld

install:
	sudo pacman -Syu
	sudo pacman -S qemu-system-arm arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-gdb

prepare: 
	mkdir -p src include bin

build: prepare
	$(CC) $(ARCH) $(FLAGS) -T $(LINKER) $(SRC) -o $(BIN)

run: build
	qemu-system-arm -M netduinoplus2 -kernel $(BIN) -nographic \
		-serial tcp::4444,server=on,wait=on \
		-serial tcp::8888,server=on,wait=off \
		-serial mon:stdio

clean:
	rm bin/*