/*
 * keyboard.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "types.h"
#include "assembly_utils.h"
#include "keyboard.h"

static KeyBoardMag_t gtKeyboardManager = {0,};


BOOL kIsOutputBufferFull(void)
{
	// 상태 레지스터(포트: 0x64)에서 출력 버퍼에 데이터가 있는지 확인
	if (kInPortByte(0x64) & 0x01) {
		return TRUE;
	}
	return FALSE;
}


BOOL kIsInputBufferFull(void)
{
	// 상태 레지스터(포트: 0x64)에서 입력 버퍼에 데이터가 있는지 확인
	if (kInPortByte(0x64) & 0x02) {
		return TRUE;
	}
	return FALSE;
}


BOOL kEnableKeyboard(void)
{
	int i, j;

	// 컨트롤 레지스터(포트: 0x64)에 키보드 활성화 커멘드(0xAE) 전송
	// 키보드 활성화
	kOutPortByte(0x64, 0xAE);

	// 입력 버퍼가 빌 때 까지 대기 후 키보드 활성화 명령 전송
	// 충분히 대기
	for(i=0; i<0xFFFF; ++i) {
		if(FALSE == kIsInputBufferFull()) {
			break;
		}
	}

	// 컨트롤 레지스터(포트: 0x64)에 키보드 활성화 커멘드(0xF4) 전송
	// 키보드 활성화
	kOutPortByte(0x64, 0xF4);

	// ACK 수신 대기
	// 100번 정도 충분히 읽어 ACK 확인
	for(j=0; j<100; ++j) {
		for(i=0; i<0xFFFF; ++i) {
			if(TRUE == kIsOutputBufferFull()) {
				break;
			}
		}

		if(0xFA == kInPortByte(0x60)) {
			return TRUE;
		}
	}

	return FALSE;
}


BYTE kGetKeyboardScanCode(void)
{
	// Output 버퍼에 값이 있을 때 까지 대기
	// 폴링 방식
	while(FALSE == kIsOutputBufferFull()) {

	}

	return kInPortByte(0x60);
}


BOOL kChangeKeyboardLED(BOOL ucCapsLockOn, BOOL bNumLockOn, BOOL bScrollLockOn)
{
	int i, j;

	// 키보드에 LED 명령을 전송하기 전에 전송 가능 할 때 까지 대기
	for(i=0; i<0xFFFF; ++i) {
		// 입력버퍼가 비여있는지 확인
		if(FALSE == kIsInputBufferFull()) {
			break;
		}
	}

	// 출력 버퍼로 LED 변경 명령 전송
	kOutPortByte(0x60, 0xED);
	for(i=0; i<0xFFFF; ++i) {
		// 입력버퍼가 비여있는지 확인
		if(FALSE == kIsInputBufferFull()) {
			break;
		}
	}

	// ACK가 올 때 까지 대기
	for(j=0; j<100; ++j) {
		for(i=0; i<0xFFFF; ++i) {
			if(TRUE == kIsOutputBufferFull()) {
				break;
			}
		}

		// ACK 확인
		if(0xFA == kInPortByte(0x60)) {
			break;
		}
	}
	if(100 <= j) {
		return FALSE;
	}

	// LED 값을 전송
	kOutPortByte(0x60, ((ucCapsLockOn << 2) | (bNumLockOn << 1) | bScrollLockOn));
	for(i=0; i<0xFFFF; ++i) {
		// 입력버퍼가 비여있는지 확인
		if(FALSE == kIsInputBufferFull()) {
			break;
		}
	}

	// ACK가 올 때 까지 대기
	for(j=0; j<100; ++j) {
		for(i=0; i<0xFFFF; ++i) {
			if(TRUE == kIsOutputBufferFull()) {
				break;
			}
		}

		// ACK 확인
		if(0xFA == kInPortByte(0x60)) {
			break;
		}
	}
	if(100 <= j) {
		return FALSE;
	}

	return TRUE;
}


void kEnableA20Gate(void)
{
	BYTE ucOutputPortData;
	int i;

	// 컨트롤 레지스터에 커멘드 전송
	kOutPortByte(0x64, 0xD0);

	// 출력 데이터가 있을 때 까지 대기
	for(i=0; i<0xFFFF; ++i) {
		if(TRUE == kIsOutputBufferFull()) {
			break;
		}
	}

	// 출력 포트에 수신된 값을 읽음
	ucOutputPortData = kInPortByte(0x60);

	// A20 게이트 활성화
	ucOutputPortData |= 0x01;
	for(i=0; i<0xFFFF; ++i) {
		if(FALSE == kIsInputBufferFull()) {
			break;
		}
	}

	// 커멘드 레지스터에 출력 포트 설정 명령 전송
	kOutPortByte(0x64, 0xD1);

	// 입력 버퍼에 A20 활성화 비트 전송
	kOutPortByte(0x60, ucOutputPortData);
}


void kReboot(void)
{
	int i;

	// 출력 데이터가 있을 때 까지 대기
	for(i=0; i<0xFFFF; ++i) {
		if(TRUE == kIsOutputBufferFull()) {
			break;
		}
	}

	// 커멘드 레지스터에 출력 포트 설정 명령 전송
	kOutPortByte(0x64, 0xD1);

	// 리셋 명령 전송
	kOutPortByte(0x60, 0x00);

	while(1);
}


BOOL kIsAlphabetScanCode(BYTE ucScanCode)
{
	// 변환 테이블 값을 읽어 알파벳인지 확인
	if( ('a' <= gtKeyMapTbl[ucScanCode].ucNormalCode) && (gtKeyMapTbl[ucScanCode].ucNormalCode <= 'z')) {
		return TRUE;
	}
	return FALSE;
}


BOOL kIsNumOrSymbolScanCode(BYTE ucScanCode)
{
	// 변환 테이블 값을 읽어 숫자나 범위인지 확인 (스캔 코드 2-53)에서 알파벳이 아니면
	if( (2<=ucScanCode) && (ucScanCode <= 53) && (FALSE == kIsAlphabetScanCode(ucScanCode))) {
		return TRUE;
	}
	return FALSE;
}


BOOL kIsNumPadScanCode(BYTE ucScanCode)
{
	// 넘버패드는 71 - 83에 맵핑됨
	if((71 <= ucScanCode) && (ucScanCode <= 83)) {
		return TRUE;
	}
	return FALSE;
}


BOOL kIsUseCombinedCode(BYTE ucScanCode)
{
	BYTE ucDownScanCode;
	BYTE ucUseCombKey = FALSE;

	ucDownScanCode = ucScanCode & 0x7F;

	// 알파벳은 Shift, CapsLock과 콤보
	if(TRUE == kIsAlphabetScanCode(ucDownScanCode)) {
		// Shift나 CapsLock 중 하나만 눌린 경우 콤보키 적용
		if(gtKeyboardManager.ucShiftDown ^ gtKeyboardManager.ucCapLockOn) {
			ucUseCombKey = TRUE;
		}
		else {
			ucUseCombKey = FALSE;
		}
	}
	// 숫자키는 Shift와 콤보
	else if(TRUE == kIsNumOrSymbolScanCode(ucDownScanCode)) {
		// Shift가 눌린 경우 콤보키 적용
		if(TRUE == gtKeyboardManager.ucShiftDown) {
			ucUseCombKey = TRUE;
		}
		else {
			ucUseCombKey = FALSE;
		}
	}
	// 넘버패드는 Number Lock과 콤보
	else if((TRUE == kIsNumPadScanCode(ucDownScanCode)) && (FALSE == gtKeyboardManager.ucExtCodeIn)) {
		// Number Lock 눌린 경우 콤보키 적용
		if(TRUE == gtKeyboardManager.ucNumLockOn) {
			ucUseCombKey = TRUE;
		}
		else {
			ucUseCombKey = FALSE;
		}
	}

	return ucUseCombKey;
}


void UpdateCombinationKeyStatusAndLED(BYTE ucScanCode)
{
	BOOL ucDown;
	BYTE ucDownScanCode;
	BOOL ucLEDStatusChanged = FALSE;
}


BOOL kConvertScanCodeToASCIICode(BYTE ucScanCode, BYTE* poUcASCIICode, BOOL* poUcFlags)
{

}
