all: Bootloader Kernel32 Utility Application Kernel64 Disk.img

Bootloader:
	@echo 
	@echo ============== Build Boot Loader ===============
	@echo 
	
	make -C 00.Bootloader

	@echo 
	@echo =============== Build Complete ===============
	@echo 
	
Kernel32:
	@echo 
	@echo ============== Build Kernel32 ===============
	@echo 
	
	make -C 01.Kernel32

	@echo 
	@echo =============== Build Complete ===============
	@echo 
	
# 유저 프로그램을 빌드해서 커널 .rodata에 박는다. 디스크 드라이버도 FS도 없어서
# 이게 유저 바이너리를 커널에 들여보내는 유일한 길이다.
# Kernel64보다 먼저 돌아야 user_app.c가 컴파일 대상에 들어간다
Application:
	@echo 
	@echo ============= Build Application ==============
	@echo 
	
	make -C 03.Application/00.HelloWorld
	04.Utility/01.Bin2C/Bin2C.exe 03.Application/00.HelloWorld/hello.elf \
			02.Kernel64/src/user_app.c HelloApp
	
	@echo 
	@echo =============== Build Complete ===============
	@echo 

Kernel64:
	@echo 
	@echo ============== Build Kernel64 ===============
	@echo 
	
	make -C 02.Kernel64

	@echo 
	@echo =============== Build Complete ===============
	@echo 

Disk.img: 00.Bootloader/Bootloader.bin 01.Kernel32/Kernel32.bin 02.Kernel64/Kernel64.bin
	@echo 
	@echo =========== Disk Image Build Start ===========
	@echo 

	./ImageMaker.exe $^

	@echo
	@echo === Pad image to standard 1.44MB floppy 1474560 bytes ===
	@echo === modern QEMU derives floppy geometry from image size ===
	truncate -s 1474560 Disk.img

	@echo
	@echo ============= All Build Complete =============
	@echo
	
Utility:
	@echo 
	@echo =========== Utility Build Start ===========
	@echo 

	make -C 04.Utility

	@echo 
	@echo =========== Utility Build Complete ===========
	@echo 

clean:
	make -C 00.Bootloader clean
	make -C 01.Kernel32 clean
	make -C 02.Kernel64 clean
	make -C 04.Utility clean
	rm -f Disk.img