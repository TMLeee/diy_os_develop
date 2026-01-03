all: Bootloader.bin

Bootloader.bin: Bootloader.asm
	nasm -o Bootloader.bin Bootloader.asm

clean:
	rm -f Bootloader.bin