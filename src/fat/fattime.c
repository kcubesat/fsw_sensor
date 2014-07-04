#include <fat_sd/integer.h>

DWORD get_fattime (void) {

	DWORD res;

	res =  (((DWORD)2010 - 1980) << 25)
			| ((DWORD)1 << 21)
			| ((DWORD)1 << 16)
			| (WORD)(12 << 11)
			| (WORD)(0 << 5)
			| (WORD)(0 >> 1);

	return res;

}

