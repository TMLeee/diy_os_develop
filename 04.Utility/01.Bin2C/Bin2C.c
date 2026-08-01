/*
 * Bin2C.c
 *
 *  Created on: 2026. 8. 2.
 *      Author: Macbook_pro
 *
 *  바이너리를 커널 .rodata에 박을 C 배열로 바꾼다.
 *  디스크 드라이버도 파일시스템도 없는 단계에서 유저 프로그램을 커널에
 *  들여보내는 가장 짧은 길이다. 앱이 둘 이상 되면 initrd로 올려야 한다
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char** argv)
{
	FILE* pfIn;
	FILE* pfOut;
	unsigned char vucBuf[4096];
	size_t uiRead, i;
	unsigned long ulTotal = 0;
	const char* pcName;

	if(4 != argc) {
		fprintf(stderr, "usage: %s <input.bin> <output.c> <symbol>\n", argv[0]);
		return 1;
	}
	pcName = argv[3];

	pfIn = fopen(argv[1], "rb");
	if(NULL == pfIn) {
		fprintf(stderr, "cannot open %s\n", argv[1]);
		return 1;
	}

	pfOut = fopen(argv[2], "w");
	if(NULL == pfOut) {
		fprintf(stderr, "cannot create %s\n", argv[2]);
		fclose(pfIn);
		return 1;
	}

	fprintf(pfOut, "// Bin2C가 %s에서 생성했다. 직접 고치지 말 것\n", argv[1]);
	fprintf(pfOut, "#include \"types.h\"\n\n");
	fprintf(pfOut, "const BYTE g_v%s[] = {", pcName);

	while(0 < (uiRead = fread(vucBuf, 1, sizeof(vucBuf), pfIn))) {
		for(i=0; i<uiRead; ++i) {
			if(0 == (ulTotal % 12)) {
				fprintf(pfOut, "\n\t");
			}
			fprintf(pfOut, "0x%02X, ", vucBuf[i]);
			++ulTotal;
		}
	}

	fprintf(pfOut, "\n};\n\n");
	fprintf(pfOut, "const QWORD g_qw%sSize = %lu;\n", pcName, ulTotal);

	fclose(pfIn);
	fclose(pfOut);

	printf("%s -> %s (%lu bytes, symbol g_v%s)\n", argv[1], argv[2], ulTotal, pcName);
	return 0;
}
