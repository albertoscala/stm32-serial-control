CC = arm-none-eabi-gcc
FLAGS = -Wall -mthumb -mfloat-abi=soft -nostdlib -ffreestanding -specs=nosys.specs
ARCH = -mcpu=cortex-m4
SRC = src/*
BIN = bin/firmware.elf
LINKER = linker.ld

install_arch:
	sudo pacman -Syu
	sudo pacman -S qemu-system-arm arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-gdb

install_rocky:
	sudo dnf -y update
	sudo dnf -y install epel-release
	sudo dnf config-manager --set-enabled crb
	sudo dnf -y install arm-none-eabi-gcc-cs arm-none-eabi-newlib arm-none-eabi-binutils-cs gdb

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