NASM32 = nasm
GCC32 = x86_64-pc-linux-gcc.exe -c -m32 -ffreestanding
LD32 = x86_64-pc-linux-ld.exe -melf_i386 -T ../elf_i386.x -nostdlib -e main -Ttext 0x10200

OBJCOPY32 = x86_64-pc-linux-objcopy -j .text -j .data -j .rodata -j .bss -S -O binary

OBJECTDIRECTORY = temp
SOURCEDIRECTORY	= src

################################################################################
# 빌드 항목 및 빌드 방법 설정
################################################################################
all: prepare Kernel32.bin

prepare:
	mkdir -p $(OBJECTDIRECTORY)

$(OBJECTDIRECTORY)/EntryPoint.bin: $(SOURCEDIRECTORY)/EntryPoint.s
	$(NASM32) -o $@ $<

# 커널의 C 소스 파일에 대한 의존성 정보 생성
dep:
	@echo === Make Dependancy File ===
	make -C $(OBJECTDIRECTORY) -f ../makefile InternalDependency
	@echo === Dependancy Search Complete ===

# 디렉터리를 오브젝트 파일 디렉터리로 이동해서 의존성 파일 및 실행 파일을 생성
ExecuteInternalBuild: dep
	make -C $(OBJECTDIRECTORY) -f ../makefile Kernel32.elf

# 커널 이미지를 바이너리 파일로 변환
$(OBJECTDIRECTORY)/Kernel32.elf.bin: ExecuteInternalBuild
	$(OBJCOPY32) $(OBJECTDIRECTORY)/Kernel32.elf $@

# 엔트리 포인트와 커널을 합쳐서 보호 모드 바이너리 생성
Kernel32.bin: $(OBJECTDIRECTORY)/EntryPoint.bin $(OBJECTDIRECTORY)/Kernel32.elf.bin
	cat $^ > $@
		
# 소스 파일을 제외한 나머지 파일 정리	
clean:
	rm -f *.bin
	rm -f $(OBJECTDIRECTORY)/*.*


CENTRYPOINTOBJECTFILE = main.o
CSOURCEFILES = $(wildcard ../$(SOURCEDIRECTORY)/*.c)
ASSEMBLYSOURCEFILES = $(wildcard ../$(SOURCEDIRECTORY)/*.asm)
COBJECTFILES = $(subst main.o, , $(notdir $(patsubst %.c,%.o,$(CSOURCEFILES))))
ASSEMBLYOBJECTFILES = $(notdir $(patsubst %.asm,%.o,$(ASSEMBLYSOURCEFILES)))

%.o: ../$(SOURCEDIRECTORY)/%.c
	$(GCC32) -c $<

%.o: ../$(SOURCEDIRECTORY)/%.asm
	$(NASM32) -f elf32 -o $@ $<

InternalDependency:
	$(GCC32) -MM $(CSOURCEFILES) > Dependency.dep

# 실제 커널 이미지를 빌드
Kernel32.elf: $(CENTRYPOINTOBJECTFILE) $(COBJECTFILES) $(ASSEMBLYOBJECTFILES)
	$(LD32) -o $@ $^

# 현재 디릭토리의 파일 중, dependency 파일이 있으면 make에 포함	
ifeq (Dependency.dep, $(wildcard Dependency.dep))
include Dependency.dep
endif
