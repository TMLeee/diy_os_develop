/*
 * elf.h
 *
 *  Created on: 2026. 8. 2.
 *      Author: Macbook_pro
 *
 *  ELF64 실행 파일 적재
 */

#ifndef __ELF_H__
#define __ELF_H__

#include "types.h"
#include "mm_struct.h"


#define ELF_MAGIC0		0x7F
#define ELF_MAGIC1		'E'
#define ELF_MAGIC2		'L'
#define ELF_MAGIC3		'F'

#define ELFCLASS64		2
#define ELFDATA2LSB		1
#define ET_EXEC			2
#define EM_X86_64		62

#define PT_LOAD			1

// p_flags
#define PF_X			0x1
#define PF_W			0x2
#define PF_R			0x4


#pragma pack (push, 1)

typedef struct kElf64Ehdr {
	BYTE	vucIdent[16];
	WORD	wType;
	WORD	wMachine;
	DWORD	dwVersion;
	QWORD	qwEntry;
	QWORD	qwPhoff;
	QWORD	qwShoff;
	DWORD	dwFlags;
	WORD	wEhsize;
	WORD	wPhentsize;
	WORD	wPhnum;
	WORD	wShentsize;
	WORD	wShnum;
	WORD	wShstrndx;
}Elf64Ehdr_t;

typedef struct kElf64Phdr {
	DWORD	dwType;
	DWORD	dwFlags;
	QWORD	qwOffset;
	QWORD	qwVaddr;
	QWORD	qwPaddr;
	QWORD	qwFilesz;
	QWORD	qwMemsz;
	QWORD	qwAlign;
}Elf64Phdr_t;

#pragma pack (pop)


// 이미지가 이 커널에서 실행 가능한 형태인지
BOOL kElfIsValid(const BYTE* pbImage, QWORD qwSize);

// poMm에 PT_LOAD 세그먼트를 올리고 진입점을 돌려준다. 실패하면 0
QWORD kElfLoad(mm_t* poMm, const BYTE* pbImage, QWORD qwSize);

#endif
