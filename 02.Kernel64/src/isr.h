/*
 * isr.h
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_ISR_H_
#define __02_KERNEL64_SRC_ISR_H_

// 함수
// 예외 처리를 위한 ISR
void kISRDivideError(void);
void kISRDebug(void);
void kISRNMI(void);
void kISRBreakPoint(void);
void kISROverFlow(void);
void kISRBoundRangeExceeded(void);
void kISRInvalidOpcode(void);
void kISRDeviceNotAvailable(void);
void kISRDoubleFault(void);
void kISRCoprocessorSegmentOverrun(void);
void kISRInvalidTSS(void);
void kISRSegmentNotPresent(void);
void kISRStackSegmentFault(void);
void kISRGeneralProtection(void);
void kISRPageFault(void);
void kISR15(void);
void kISRFPUError(void);
void kISRAlignmentCheck(void);
void kISRMachineCheck(void);
void kISRSIMDError(void);
void kISRETCException(void);


// 인터럽트 처리를 위한 ISR
void kISRTimer(void);
void kISRKeyboard(void);
void kISRSlavePIC(void);
void kISRSerial2(void);
void kISRSerial1(void);
void kISRParallel2(void);
void kISRFloppy(void);
void kISRParallel1(void);
void kISRRTC(void);
void kISRReserved(void);
void kISRNotUsed1(void);
void kISRNotUsed2(void);
void kISRMouse(void);
void kISRCoprocessor(void);
void kISRHDD1(void);
void kISRHDD2(void);
void kISRETCInterrupt(void);


#endif /* 02_KERNEL64_SRC_ISR_H_ */
