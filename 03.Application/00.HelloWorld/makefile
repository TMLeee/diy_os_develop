# 유저 프로그램. 커널과 같은 크로스 툴체인을 쓰지만 -mcmodel=small 이고
# 커널 헤더를 하나도 참조하지 않는다
GCC = x86_64-pc-linux-gcc.exe -c -m64 -ffreestanding -mno-red-zone \
      -fno-stack-protector -fno-asynchronous-unwind-tables -fno-pic -fno-pie \
      -mcmodel=small -O1 -Wall
LD  = x86_64-pc-linux-ld.exe -melf_x86_64 -T hello.ld -nostdlib -e _start

all: hello.elf

hello.o: hello.c
	$(GCC) -o $@ $<

hello.elf: hello.o hello.ld
	$(LD) -o $@ hello.o

clean:
	rm -f hello.o hello.elf
