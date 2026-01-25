/*
 * rtc.h
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_RTC_H_
#define __02_KERNEL64_SRC_RTC_H_

#include "types.h"

// I/O Port
#define RTC_CMOS_ADDR			0x70
#define RTC_CMOS_DATA			0x71

// CMOS Memory Address
#define RTC_ADDR_SECOND			0x00
#define RTC_ADDR_MINUTE			0x02
#define RTC_ADDR_HOUR			0x04
#define RTC_ADDR_DAY_OF_WEEK	0x06
#define RTC_ADDR_DAY_OF_MONTH	0x07
#define RTC_ADDR_MONTH			0x08
#define RTC_ADDR_YEAR			0x09

// BCD to Binary Macro
#define RTC_BCD_TO_BINARY(X)	((((X) >> 4) * 10) + ((X) & 0x0F))


void kReadRTCTime(BYTE* poHour, BYTE* poMinute, BYTE* poSecond);
void kReadRTCData(WORD* poYear, BYTE* poMonth, BYTE* poDayOfMonth, BYTE* poDayOfWeek);
char* kConvDayOfWeekToString(BYTE ucDayOfWeek);



#endif /* 02_KERNEL64_SRC_RTC_H_ */
