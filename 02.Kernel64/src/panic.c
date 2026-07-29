/*
 * panic.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  예외 발생 시 CR2/에러코드/레지스터를 시리얼과 VGA에 덤프하고 정지
 */

#include <stdarg.h>
#include "panic.h"
#include "console.h"
#include "utility.h"
#include "assembly_utils.h"
#include "serial.h"


static const char* const gvpcExceptionName[PANIC_EXCEPTION_CNT] =
{
	"#DE Divide Error",						// 0
	"#DB Debug Exception",					// 1
	"--- NMI Interrupt",					// 2
	"#BP Breakpoint",						// 3
	"#OF Overflow",							// 4
	"#BR BOUND Range Exceeded",				// 5
	"#UD Invalid Opcode",					// 6
	"#NM Device Not Available",				// 7
	"#DF Double Fault",						// 8
	"--- Coprocessor Segment Overrun",		// 9
	"#TS Invalid TSS",						// 10
	"#NP Segment Not Present",				// 11
	"#SS Stack-Segment Fault",				// 12
	"#GP General Protection",				// 13
	"#PF Page Fault",						// 14
	"--- Reserved",							// 15
	"#MF x87 FPU Error",					// 16
	"#AC Alignment Check",					// 17
	"#MC Machine Check",					// 18
	"#XM SIMD Floating-Point",				// 19
	"#VE Virtualization",					// 20
	"#CP Control Protection",				// 21
	"--- Reserved",							// 22
	"--- Reserved",							// 23
	"--- Reserved",							// 24
	"--- Reserved",							// 25
	"--- Reserved",							// 26
	"--- Reserved",							// 27
	"#HV Hypervisor Injection",				// 28
	"#VC VMM Communication",				// 29
	"#SX Security Exception",				// 30
	"--- Reserved"							// 31
};

static int g_iPanicRow = 0;
static BOOL g_bPanicReady = FALSE;
static char gvcPanicLine[512];
static char gvcPanicMsg[512];


// 콘솔 상태에 의존하지 않고 직접 준비한다. 콘솔 초기화 전에도 동작해야 하므로
static void kPanicBeginOutput(void)
{
	CharStruct* poScreen = (CharStruct*)CONSOLE_VIDEO_MEM_ADDR;
	int i;

	if(TRUE == g_bPanicReady) {
		return;
	}
	g_bPanicReady = TRUE;

	kInitializeSerial();
	for(i=0; i<(CONSOLE_WIDTH * CONSOLE_HEIGHT); ++i) {
		poScreen[i].ucChar = ' ';
		poScreen[i].ucAttr = CONSOLE_DEFAULT_TEXT_COLOR;
	}
	g_iPanicRow = 0;
}


static void kPanicPrintLine(const char* pcStr)
{
	kSerialPutString(pcStr);
	kSerialPutString("\n");

	if(g_iPanicRow < CONSOLE_HEIGHT) {
		kPrintStringXY(0, g_iPanicRow, pcStr);
		++g_iPanicRow;
	}
}


static void kPanicPrintf(const char* pcFmt, ...)
{
	va_list ap;

	va_start(ap, pcFmt);
	kVSPrintf(gvcPanicLine, pcFmt, ap);
	va_end(ap);

	kPanicPrintLine(gvcPanicLine);
}


static void kPanicPrintReg2(const char* pcName1, QWORD qwValue1,
		const char* pcName2, QWORD qwValue2)
{
	char vcHex1[17], vcHex2[17];

	kToHexString(qwValue1, vcHex1, 16);
	kToHexString(qwValue2, vcHex2, 16);
	kPanicPrintf(" %s=%s   %s=%s", pcName1, vcHex1, pcName2, vcHex2);
}


static void kPanicPrintReg3(const char* pcName1, QWORD qwValue1,
		const char* pcName2, QWORD qwValue2, const char* pcName3, QWORD qwValue3)
{
	char vcHex1[17], vcHex2[17], vcHex3[17];

	kToHexString(qwValue1, vcHex1, 16);
	kToHexString(qwValue2, vcHex2, 16);
	kToHexString(qwValue3, vcHex3, 16);
	kPanicPrintf(" %s=%s  %s=%s  %s=%s",
			pcName1, vcHex1, pcName2, vcHex2, pcName3, vcHex3);
}


static void kPanicPrintErrCodeDetail(int iVectorNum, QWORD qwErrCode)
{
	char vcHex[17];
	const char* pcTable;

	switch(iVectorNum) {
		case 14:	// #PF
			kPanicPrintf("   P=%d(%s) W/R=%d(%s) U/S=%d(%s)",
					(int)(qwErrCode & 0x01),
					(qwErrCode & 0x01) ? "Protection" : "Non-Present",
					(int)((qwErrCode >> 1) & 0x01),
					((qwErrCode >> 1) & 0x01) ? "Write" : "Read",
					(int)((qwErrCode >> 2) & 0x01),
					((qwErrCode >> 2) & 0x01) ? "User" : "Supervisor");
			kPanicPrintf("   RSVD=%d I/D=%d(%s)",
					(int)((qwErrCode >> 3) & 0x01),
					(int)((qwErrCode >> 4) & 0x01),
					((qwErrCode >> 4) & 0x01) ? "Instruction Fetch" : "Data Access");
			kToHexString(kReadCR2(), vcHex, 16);
			kPanicPrintf("   Faulting Address(CR2)=0x%s", vcHex);
			break;

		case 10:	// #TS
		case 11:	// #NP
		case 12:	// #SS
		case 13:	// #GP
			if(0 == qwErrCode) {
				kPanicPrintLine("   Selector: 0 (not segment related)");
				break;
			}
			if(0 != (qwErrCode & 0x02)) {
				pcTable = "IDT";
			}
			else if(0 != (qwErrCode & 0x04)) {
				pcTable = "LDT";
			}
			else {
				pcTable = "GDT";
			}
			kPanicPrintf("   EXT=%d Table=%s Index=%d Selector=0x%X",
					(int)(qwErrCode & 0x01), pcTable,
					(int)((qwErrCode >> 3) & 0x1FFF), (DWORD)(qwErrCode & 0xFFFF));
			break;

		default:
			break;
	}
}


// isr.asm이 실제로 add rsp,8 하는 벡터. 21/29/30도 아키텍처상 에러코드가 있지만
// kISRETCException으로 묶여 벡터 20으로 전달되므로 여기서 제외한다
BOOL kIsExceptionHasErrCode(int iVectorNum)
{
	switch(iVectorNum) {
		case 8: case 10: case 11: case 12: case 13: case 14: case 17:
			return TRUE;
		default:
			return FALSE;
	}
}


const char* kGetExceptionName(int iVectorNum)
{
	if((iVectorNum < 0) || (PANIC_EXCEPTION_CNT <= iVectorNum)) {
		return "--- Unknown Vector";
	}
	return gvpcExceptionName[iVectorNum];
}


void kDumpRegisters(QWORD* pqwFrame, int iVectorNum, QWORD qwErrCode, BOOL bHasErrCode)
{
	QWORD qwRIP, qwCS, qwRFLAGS, qwRSP, qwSS;
	int iIretIdx;
	char vcHex[17];
	char vcDS[5], vcES[5], vcFS[5], vcGS[5], vcCS[5], vcSS[5];

	kPanicBeginOutput();
	kPanicPrintLine("=============== KERNEL EXCEPTION DUMP ===============");

	kPanicPrintf(" Vector : %d (0x%X)  %s",
			iVectorNum, (DWORD)iVectorNum, kGetExceptionName(iVectorNum));

	if(TRUE == bHasErrCode) {
		kToHexString(qwErrCode, vcHex, 16);
		kPanicPrintf(" ErrCode: 0x%s", vcHex);
		kPanicPrintErrCodeDetail(iVectorNum, qwErrCode);
	}
	else {
		kPanicPrintLine(" ErrCode: (none for this vector)");
	}

	kPanicPrintReg2("CR0", kReadCR0(), "CR2", kReadCR2());
	kPanicPrintReg2("CR3", kReadCR3(), "CR4", kReadCR4());

	if(NULL == pqwFrame) {
		kPanicPrintLine(" (no saved register frame)");
		return;
	}

	// 에러 코드가 있으면 iretq 프레임이 QWORD 1개만큼 밀린다
	iIretIdx = PANIC_FRAME_RIP_IDX + ((TRUE == bHasErrCode) ? PANIC_ERRCODE_SHIFT : 0);

	qwRIP    = pqwFrame[iIretIdx + 0];
	qwCS     = pqwFrame[iIretIdx + 1];
	qwRFLAGS = pqwFrame[iIretIdx + 2];
	qwRSP    = pqwFrame[iIretIdx + 3];
	qwSS     = pqwFrame[iIretIdx + 4];

	kPanicPrintReg2("RIP", qwRIP,    "CS ", qwCS);
	kPanicPrintReg2("FLG", qwRFLAGS, "RSP", qwRSP);
	kPanicPrintReg2("SS ", qwSS,     "RBP", pqwFrame[PANIC_FRAME_RBP_IDX]);

	kPanicPrintReg3("RAX", pqwFrame[PANIC_FRAME_RAX_IDX],
					"RBX", pqwFrame[PANIC_FRAME_RBX_IDX],
					"RCX", pqwFrame[PANIC_FRAME_RCX_IDX]);
	kPanicPrintReg3("RDX", pqwFrame[PANIC_FRAME_RDX_IDX],
					"RSI", pqwFrame[PANIC_FRAME_RSI_IDX],
					"RDI", pqwFrame[PANIC_FRAME_RDI_IDX]);
	kPanicPrintReg3("R8 ", pqwFrame[PANIC_FRAME_R8_IDX],
					"R9 ", pqwFrame[PANIC_FRAME_R9_IDX],
					"R10", pqwFrame[PANIC_FRAME_R10_IDX]);
	kPanicPrintReg3("R11", pqwFrame[PANIC_FRAME_R11_IDX],
					"R12", pqwFrame[PANIC_FRAME_R12_IDX],
					"R13", pqwFrame[PANIC_FRAME_R13_IDX]);
	kPanicPrintReg2("R14", pqwFrame[PANIC_FRAME_R14_IDX],
					"R15", pqwFrame[PANIC_FRAME_R15_IDX]);

	kToHexString(pqwFrame[PANIC_FRAME_DS_IDX], vcDS, 4);
	kToHexString(pqwFrame[PANIC_FRAME_ES_IDX], vcES, 4);
	kToHexString(pqwFrame[PANIC_FRAME_FS_IDX], vcFS, 4);
	kToHexString(pqwFrame[PANIC_FRAME_GS_IDX], vcGS, 4);
	kToHexString(qwCS, vcCS, 4);
	kToHexString(qwSS, vcSS, 4);
	kPanicPrintf(" DS=%s ES=%s FS=%s GS=%s CS=%s SS=%s",
			vcDS, vcES, vcFS, vcGS, vcCS, vcSS);
}


void kPanic(const char* pcFmt, ...)
{
	va_list ap;

	kDisableInterrupt();
	kPanicBeginOutput();

	va_start(ap, pcFmt);
	kVSPrintf(gvcPanicMsg, pcFmt, ap);
	va_end(ap);

	kPanicPrintLine("*************** KERNEL PANIC ***************");
	kPanicPrintLine(gvcPanicMsg);
	kPanicPrintLine("System Halted.");

	while(1) {
		kDisableInterrupt();
		kHlt();
	}
}
