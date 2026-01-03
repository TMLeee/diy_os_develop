/*
 * main.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "types.h"

void kPrintString(int x, int y, const char* str);

void main(void)
{
	kPrintString(0, 10, "IA-32e Mode Kernel Start............[ OK ]");

	while(1);
}


void kPrintString(int x, int y, const char* str)
{
	CharStruct* poScreen = (CharStruct*) 0xB8000;
	int i;

	poScreen += (y * 80) + x;
	for(i = 0; str[i] != 0; ++i) {
		poScreen[i].ucChar = str[i];
	}
}
