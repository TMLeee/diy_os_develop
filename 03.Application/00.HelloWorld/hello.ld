/* 유저 프로그램의 링크 주소. 커널은 저주소 절반 전체를 비워 두었다 */
ENTRY(_start)

SECTIONS
{
	. = 0x400000;

	.text : ALIGN(0x1000) { *(.text*) }
	.rodata : ALIGN(0x1000) { *(.rodata*) }
	.data : ALIGN(0x1000) { *(.data*) }
	.bss : ALIGN(0x1000) { *(.bss*) *(COMMON) }

	/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
