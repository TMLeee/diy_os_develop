/*
 * keyboard.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "types.h"
#include "assembly_utils.h"
#include "keyboard.h"
#include "queue.h"
#include "utility.h"

static KeyBoardMag_t gtKeyboardManager = {0,};
static QUEUE gtKeyQueue;
static KeyData_t gtKeyQueueBuff[KEY_MAX_QUEUE_SIZE];


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


BOOL kWaitForACKAndPutOtherScanCode(void)
{
	int i, j;
	BYTE ucData;
	BOOL bResult = FALSE;

	for(j=0; j<100; ++j) {
		for(i=0; i<0xFFFF; ++i) {
			if(TRUE == kIsOutputBufferFull()) {
				break;
			}
		}

		ucData = kInPortByte(0x60);
		if(0xFA == ucData) {
			bResult = TRUE;
			break;
		}
		else {
			kConvertScanCodeAndPutQueue(ucData);
		}
	}
	return bResult;
}


BOOL kEnableKeyboard(void)
{
	int i, j;
	BOOL bPreINT;
	BOOL bResult;

	// 인터럽트 상태 저장 및 비활성화
	bPreINT = kSetInterruptFlag(FALSE);

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
	bResult = kWaitForACKAndPutOtherScanCode();

	// 인터럽트 상태 복원
	kSetInterruptFlag(bPreINT);

	return bResult;
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
	BOOL bPreINT;
	BOOL bResult;
	BYTE ucData;

	// 인터럽트 상태 저장 및 비활성화
	bPreINT = kSetInterruptFlag(FALSE);

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

	// ACK 수신 대기
	bResult = kWaitForACKAndPutOtherScanCode();

	// LED 값을 전송
	kOutPortByte(0x60, ((ucCapsLockOn << 2) | (bNumLockOn << 1) | bScrollLockOn));
	for(i=0; i<0xFFFF; ++i) {
		// 입력버퍼가 비여있는지 확인
		if(FALSE == kIsInputBufferFull()) {
			break;
		}
	}

	// ACK 수신 대기
	bResult = kWaitForACKAndPutOtherScanCode();

	// 인터럽트 상태 복원
	kSetInterruptFlag(bPreINT);

	return bResult;
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
	if( (2 <= ucScanCode) && (ucScanCode <= 53) && (FALSE == kIsAlphabetScanCode(ucScanCode))) {
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

	// 키 눌림, 떨어짐 여부 확인
	if(ucScanCode & 0x80) {
		ucDown = FALSE;
		ucDownScanCode = ucScanCode & 0x7F;
	}
	else {
		ucDown = TRUE;
		ucDownScanCode = ucScanCode;
	}

	// 조합 키 검색
	// Shift
	if((42 == ucDownScanCode) || (54 == ucDownScanCode)) {
		gtKeyboardManager.ucShiftDown = ucDown;
	}
	// Caps Lock
	else if((58 == ucDownScanCode) && (TRUE == ucDown)) {
		gtKeyboardManager.ucCapLockOn ^= TRUE;
		ucLEDStatusChanged = TRUE;
	}
	// Number Lock
	else if((69 == ucDownScanCode) && (TRUE == ucDown)) {
		gtKeyboardManager.ucNumLockOn ^= TRUE;
		ucLEDStatusChanged = TRUE;
	}
	// Scroll Lock
	else if((70 == ucDownScanCode) && (TRUE == ucDown)) {
		gtKeyboardManager.ucScrollLockOn ^= TRUE;
		ucLEDStatusChanged = TRUE;
	}


	// LED 상태 변경이 필요한 경우
	if(TRUE == ucLEDStatusChanged) {
		kChangeKeyboardLED(gtKeyboardManager.ucCapLockOn, gtKeyboardManager.ucNumLockOn, gtKeyboardManager.ucScrollLockOn);
	}
}


BOOL kConvertScanCodeToASCIICode(BYTE ucScanCode, BYTE* poUcASCIICode, BOOL* poUcFlags)
{
	BOOL ucUseCombKey;

	// 이전에 Pause 키를 수신하였다면
	if(0 < gtKeyboardManager.iSkipCountForPause) {
		--gtKeyboardManager.iSkipCountForPause;
		return FALSE;
	}

	// Pause 키는 별도 처리
	if(0xE1 == ucScanCode) {
		*poUcASCIICode = KEY_PAUSE;
		*poUcFlags = KEY_FLAG_DOWN;
		gtKeyboardManager.iSkipCountForPause = KEY_SKIP_CNT_FOR_PAUSE;
		return TRUE;
	}
	// 확장키 코드인 경우
	else if(0xE0 == ucScanCode) {
		gtKeyboardManager.ucExtCodeIn = TRUE;
		return FALSE;
	}

	// 조합키 반환 여부 확인
	ucUseCombKey = kIsUseCombinedCode(ucScanCode);

	// 키 값 지정
	if(TRUE == ucUseCombKey) {
		*poUcASCIICode = gtKeyMapTbl[ucScanCode & 0x7F].ucCombCode;
	}
	else {
		*poUcASCIICode = gtKeyMapTbl[ucScanCode & 0x7F].ucNormalCode;
	}

	// 확장 키 여부 확인
	if(TRUE == gtKeyboardManager.ucExtCodeIn) {
		*poUcFlags = KET_FLAG_EXTENDED_KEY;
		gtKeyboardManager.ucExtCodeIn = FALSE;
	}
	else {
		*poUcFlags = 0;
	}

	// 눌림 상태 갱신
	if((ucScanCode & 0x80) == 0) {
		*poUcFlags |= KEY_FLAG_DOWN;
	}

	// 조합키 눌림 상태 갱신
	UpdateCombinationKeyStatusAndLED(ucScanCode);
	return TRUE;
}


BOOL kInitializeKeyboard(void)
{
	// Key Queue 초기화
	kInitializeQueue(&gtKeyQueue, gtKeyQueueBuff, KEY_MAX_QUEUE_SIZE, sizeof(KeyData_t));

	// 키보드 활성화
	return kEnableKeyboard();
}


BOOL kConvertScanCodeAndPutQueue(BYTE ucScanCode)
{
	KeyData_t tData;
	BOOL bResult = FALSE;
	BOOL bPreINT;

	// 스캔 코드 복사
	tData.ucScanCode	= ucScanCode;


	// ASCII로 변환 후 큐에 삽입
	if(TRUE == kConvertScanCodeToASCIICode(ucScanCode, &(tData.ucASCIICode), &(tData.ucFlags))) {

		// 인터럽트 상태 저장 및 비활성화
		bPreINT = kSetInterruptFlag(FALSE);

		bResult = kPutQueue(&gtKeyQueue, &tData);

		// 인터럽트 상태 복원
		kSetInterruptFlag(bPreINT);
	}

	return bResult;
}


BOOL kGetKeyFromKeyQueue(KeyData_t* poData)
{
	BOOL bResult;
	BOOL bPreINT;

	// 큐가 비였는지 확인
	if(TRUE == kIsQueueEmpty(&gtKeyQueue)) {
		return FALSE;
	}

	// 인터럽트 상태 저장 및 비활성화
	bPreINT = kSetInterruptFlag(FALSE);

	bResult = kGetQueue(&gtKeyQueue, poData);

	// 인터럽트 상태 복원
	kSetInterruptFlag(bPreINT);

	return bResult;
}
