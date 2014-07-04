
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <fat_sd/ff.h>

int ff_cre_syncobj(BYTE vol, _SYNC_t* sobj) {
	*sobj = xSemaphoreCreateMutex();
	if (*sobj == NULL)
		return 0;
	return 1;
}

int ff_del_syncobj(_SYNC_t sobj) {
	printf("Semaphore delete not implemented\r\n");
	//vSemaphoreDelete(sobj);
	return 1;
}

int ff_req_grant(_SYNC_t sobj) {
	if (xSemaphoreTake(sobj, _FS_TIMEOUT))
		return 1;
	return 0;
}

void ff_rel_grant(_SYNC_t sobj) {
	xSemaphoreGive(sobj);
}
