/*
 * hello.c
 *
 *  Created on: 2026. 8. 2.
 *      Author: Macbook_pro
 *
 *  커널 밖에서 따로 빌드되는 첫 유저 프로그램. 커널 헤더를 하나도 쓰지 않는다 -
 *  int 0x80 규약만 맞으면 되고, 그게 곧 유저/커널 경계가 실재한다는 뜻이다
 */


#define SYS_WRITE	1
#define SYS_FORK	57
#define SYS_GETPID	39
#define SYS_UPTIME	201


static long syscall3(long lNum, long lA1, long lA2, long lA3)
{
	long lRet;

	// 리눅스 x86-64 규약 그대로. 인터럽트 게이트라 커널이 모든 레지스터를
	// 복원해 주지만, 규약을 지키는 쪽이 나중에 syscall 명령으로 바꾸기 쉽다
	__asm__ __volatile__(
			"int $0x80"
			: "=a"(lRet)
			: "a"(lNum), "D"(lA1), "S"(lA2), "d"(lA3)
			: "rcx", "r11", "memory");
	return lRet;
}


static unsigned long kStrLen(const char* pcStr)
{
	unsigned long i = 0;

	while('\0' != pcStr[i]) {
		++i;
	}
	return i;
}


static void kWrite(const char* pcStr)
{
	syscall3(SYS_WRITE, 1, (long)pcStr, (long)kStrLen(pcStr));
}


// 값이 커널에서 온 것인지 확인할 수 있게 16진수로 찍는다
static void kWriteHex(const char* pcLabel, unsigned long qwValue)
{
	static const char vcDigit[] = "0123456789ABCDEF";
	char vcBuf[19];
	int i;

	vcBuf[0] = '0';
	vcBuf[1] = 'x';
	for(i=0; i<16; ++i) {
		vcBuf[2 + i] = vcDigit[(qwValue >> ((15 - i) * 4)) & 0x0F];
	}
	vcBuf[18] = '\0';

	kWrite(pcLabel);
	kWrite(vcBuf);
	kWrite("\n");
}


// .data와 .bss가 제대로 적재됐는지 보려면 두 종류 다 있어야 한다.
// .data는 파일에서 복사돼 와야 하고, .bss는 0으로 채워져야 한다
static char gvcGreeting[] = "hello from a real ELF\n";
static unsigned long gqwZeroed[64];


void _start(void)
{
	unsigned long i, qwSum = 0;

	kWrite(gvcGreeting);

	// .bss는 커널이 0으로 준 것이어야 한다. 아니면 앞서 쓰던 쓰레기가 보인다
	for(i=0; i<64; ++i) {
		qwSum += gqwZeroed[i];
	}
	kWrite((0 == qwSum) ? "bss is zeroed\n" : "BSS NOT ZEROED\n");

	// .bss에 써 보고 되읽는다. 쓰기 가능해야 한다
	for(i=0; i<64; ++i) {
		gqwZeroed[i] = i + 1;
	}
	qwSum = 0;
	for(i=0; i<64; ++i) {
		qwSum += gqwZeroed[i];
	}
	kWrite((2080 == qwSum) ? "bss is writable\n" : "BSS NOT WRITABLE\n");

	kWriteHex("pid=", (unsigned long)syscall3(SYS_GETPID, 0, 0, 0));

	// 유저에서 끝낼 방법이 아직 없다. 셸이 걷어낸다
	while(1) {
	}
}
