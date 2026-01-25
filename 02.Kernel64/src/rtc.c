/*
 * rtc.c
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */


#include "rtc.h"


void kReadRTCTime(BYTE* poHour, BYTE* poMinute, BYTE* poSecond)
{
	BYTE ucData;

	// Read Hour
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_HOUR);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poHour = RTC_BCD_TO_BINARY(ucData);

	// Read Minute
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_MINUTE);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poMinute = RTC_BCD_TO_BINARY(ucData);

	// Read Second
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_SECOND);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poSecond = RTC_BCD_TO_BINARY(ucData);
}


void kReadRTCData(WORD* poYear, BYTE* poMonth, BYTE* poDayOfMonth, BYTE* poDayOfWeek)
{
	BYTE ucData;

	// Read Year
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_YEAR);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poYear = RTC_BCD_TO_BINARY(ucData) + 2000;

	// Read Month
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_MONTH);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poMonth = RTC_BCD_TO_BINARY(ucData);

	// Read Day of Month
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_DAY_OF_MONTH);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poDayOfMonth = RTC_BCD_TO_BINARY(ucData);

	// Read Day of Week
	kOutPortByte(RTC_CMOS_ADDR, RTC_ADDR_DAY_OF_WEEK);
	ucData = kInPortByte(RTC_CMOS_DATA);
	*poDayOfWeek = RTC_BCD_TO_BINARY(ucData);
}


char* kConvDayOfWeekToString(BYTE ucDayOfWeek)
{
	static char* strDayOfWeek[8] = {
			"Error", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
	};

	if(7 < ucDayOfWeek) {
		return strDayOfWeek[0];
	}

	return strDayOfWeek[ucDayOfWeek];
}
