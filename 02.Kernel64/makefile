NASM64 = nasm -f elf64 -g -F dwarf
#   -mno-red-zone                  : SysV의 128바이트 red zone 금지. 인터럽트가
#                                    인터럽트된 컨텍스트의 [rsp-128]을 밟는 것을 막는다.
#                                    현재는 모든 IDT 게이트가 IST를 쓰기 때문에 CPU가
#                                    항상 스택을 바꿔서 우연히 안전할 뿐이다.
#   -fno-stack-protector           : __stack_chk_fail 같은 libc 심볼 참조 방지
#   -fno-asynchronous-unwind-tables : .eh_frame 제거. objcopy가 .eh_frame을 버리므로
#                                    이게 있으면 링커 심볼 _end가 커널 이미지 끝과
#                                    어긋난다(현재 _end=0x207FE8 vs .bss끝=0x206148).
#   -fno-pic -fno-pie              : const 포인터 테이블이 .data.rel.ro로 가버리면
#                                    objcopy의 -j 목록에 없어서 이미지에서 통째로
#                                    사라진다. 절대 주소 코드를 강제한다.
GCC64 = x86_64-pc-linux-gcc.exe -c -g -gdwarf-4 -m64 -ffreestanding \
        -mno-red-zone -fno-stack-protector -fno-asynchronous-unwind-tables \
        -fno-pic -fno-pie -Wall
LD64 = x86_64-pc-linux-ld.exe -melf_x86_64 -T ../elf_x86_64.x -nostdlib -e main -Ttext 0x200000

# 바이너리 이미지 생성을 위한 OBJCOPY 옵션 정의
OBJCOPY64 = x86_64-pc-linux-objcopy -j .text -j .data -j .rodata -j .bss -S -O binary

# 디렉터리 정의
OBJECTDIRECTORY = temp
SOURCEDIRECTORY	= src

################################################################################
# 빌드 항목 및 빌드 방법 설정
################################################################################
# 기본적으로 빌드를 수행할 목록
all: prepare Kernel64.bin

# 오브젝트 파일이 위치할 디렉터리를 생성
prepare:
	mkdir -p $(OBJECTDIRECTORY)


# 커널의 C 소스 파일에 대한 의존성 정보 생성
dep:
	@echo === Make Dependancy File ===
	make -C $(OBJECTDIRECTORY) -f ../makefile InternalDependency
	@echo === Dependancy Search Complete ===

# 디렉터리를 오브젝트 파일 디렉터리로 이동해서 의존성 파일 및 실행 파일을 생성
ExecuteInternalBuild: dep
	make -C $(OBJECTDIRECTORY) -f ../makefile Kernel64.elf

# 커널 이미지를 바이너리 파일로 변환하여 IA-32e 모드 바이너리 생성
Kernel64.bin: ExecuteInternalBuild
	$(OBJCOPY64) $(OBJECTDIRECTORY)/Kernel64.elf $@
		
# 소스 파일을 제외한 나머지 파일 정리	
clean:
	rm -f *.bin
	rm -f $(OBJECTDIRECTORY)/*.*

################################################################################
# Make에 의해 다시 호출되는 부분, Temp 디렉터리를 기준으로 수행됨
################################################################################
# 빌드할 어셈블리어 엔트리 포인트 소스 파일 정의, Temp 디렉터리를 기준으로 설정
ENTRYPOINTSOURCEFILE = ../$(SOURCEDIRECTORY)/EntryPoint.s
ENTRYPOINTOBJECTFILE = EntryPoint.o
# 빌드할 C 소스 파일 정의, Temp 디렉터리를 기준으로 설정
CSOURCEFILES = $(wildcard ../$(SOURCEDIRECTORY)/*.c)
ASSEMBLYSOURCEFILES = $(wildcard ../$(SOURCEDIRECTORY)/*.asm)
COBJECTFILES = $(notdir $(patsubst %.c,%.o,$(CSOURCEFILES)))
ASSEMBLYOBJECTFILES = $(notdir $(patsubst %.asm,%.o,$(ASSEMBLYSOURCEFILES)))

# 어셈블리어 엔트리 포인트 빌드
$(ENTRYPOINTOBJECTFILE): $(ENTRYPOINTSOURCEFILE)
	$(NASM64) -o $@ $<

# .c 파일을 .o 파일로 바꾸는 규칙 정의
%.o: ../$(SOURCEDIRECTORY)/%.c
	$(GCC64) -c $<

# .asm 파일을 .o 파일로 바꾸는 규칙 정의
%.o: ../$(SOURCEDIRECTORY)/%.asm
	$(NASM64) -o $@ $<

# 실제 의존성에 관련된 파일을 생성
InternalDependency:
	$(GCC64) -MM $(CSOURCEFILES) > Dependency.dep

# 실제 커널 이미지를 빌드
Kernel64.elf: $(ENTRYPOINTOBJECTFILE) $(COBJECTFILES) $(ASSEMBLYOBJECTFILES)
	$(LD64) -o $@ $^

# 현재 디릭토리의 파일 중, dependency 파일이 있으면 make에 포함	
ifeq (Dependency.dep, $(wildcard Dependency.dep))
include Dependency.dep
endif
