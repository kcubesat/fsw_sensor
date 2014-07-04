/*
  This file is part of UFFS, the Ultra-low-cost Flash File System.

  Copyright (C) 2005-2009 Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

  UFFS is free software; you can redistribute it and/or modify it under
  the GNU Library General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any
  later version.

  UFFS is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  or GNU Library General Public License, as applicable, for more details.

  You should have received a copy of the GNU General Public License
  and GNU Library General Public License along with UFFS; if not, write
  to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301, USA.

  As a special exception, if other files instantiate templates or use
  macros or inline functions from this file, or you compile this file
  and link it with other works to produce a work based on this file,
  this file does not by itself cause the resulting work to be covered
  by the GNU General Public License. However the source code for this
  file must still be made available in accordance with section (3) of
  the GNU General Public License v2.

  This exception does not invalidate any other reasons why a work based
  on this file might be covered by the GNU General Public License.
*/

/* UFFS interface to FreeRTOS synchronisation functions */

#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"

#include <stdio.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

/* Every partition uses 4 semaphores */
#define UFFS_MAX_SEMAPHORES 8

enum uffs_sem_state {
	SEM_FREE,
	SEM_USED,
};

struct uffs_sem {
	xSemaphoreHandle sem;
	enum uffs_sem_state state;
};

/* Semaphore list - state is initialized to 0 = SEM_FREE */
struct uffs_sem semaphores[UFFS_MAX_SEMAPHORES] = {{0}};

int uffs_SemCreate(OSSEM *sem) {
	int i;

	/* We only support binary semaphores */
/*	if (n != 1) {
		printf("UFFS tried to create semaphore with n = %d\r\n", n);
		return -1;
	}*/

	/* Find available semaphore in list */
	for (i = 0; i < UFFS_MAX_SEMAPHORES; i++) {
		if (semaphores[i].state == SEM_FREE) {
			if (!semaphores[i].sem)
				vSemaphoreCreateBinary(semaphores[i].sem);

			if (!semaphores[i].sem) {
				printf("UFFS failed to create FreeRTOS semaphore (i = %d)\r\n", i);
				return -1;
			}

			semaphores[i].state = SEM_USED;
			*sem = (OSSEM *) &semaphores[i];
			return i;
		}
	}

	printf("No available FreeRTOS semaphore for UFFS (i=%d)\r\n", i);
	return -1;
}

int uffs_SemWait(OSSEM sem) {
	struct uffs_sem * semaphore = (struct uffs_sem *) sem;
	if (semaphore == NULL)
		return -1;

	if (xSemaphoreTake(semaphore->sem, portMAX_DELAY) == pdPASS) {
		return 0;
	} else {
		return -1;
	}
}

int uffs_SemSignal(OSSEM sem) {
	struct uffs_sem * semaphore = (struct uffs_sem *) sem;
	if (semaphore == NULL)
		return -1;

	if (xSemaphoreGive(semaphore->sem) == pdPASS) {
		return 0;
	} else {
		return -1;
	}
}

int uffs_SemDelete(OSSEM *sem) {
	struct uffs_sem *semaphore = (struct uffs_sem *) (*sem);
	vQueueDelete(semaphore->sem);
	semaphore->sem = NULL;
	semaphore->state = SEM_FREE;
	return 0;
}

void uffs_CriticalEnter(void) {
	portENTER_CRITICAL();
	return;
}

void uffs_CriticalExit(void) {
	portEXIT_CRITICAL();
	return;
}

int uffs_OSGetTaskId(void) {
	return (int)xTaskGetCurrentTaskHandle();
}

unsigned int uffs_GetCurDateTime(void) {
	return (int)(xTaskGetTickCount()/configTICK_RATE_HZ);
}

/*
int uffs_SemCreate(int n) {
	int i;
*/
	/* We only support binary semaphores */
/*	if (n != 1) {
		printf("UFFS tried to create semaphore with n = %d\r\n", n);
		return -1;
	}
*/
	/* Find available semaphore in list */
/*	for (i = 0; i < UFFS_MAX_SEMAPHORES; i++) {
		if (semaphores[i].state == SEM_FREE) {
			if (!semaphores[i].sem)
				vSemaphoreCreateBinary(semaphores[i].sem);

			if (!semaphores[i].sem) {
				printf("UFFS failed to create FreeRTOS semaphore (i = %d)\r\n", i);
				return -1;
			}

			semaphores[i].state = SEM_USED;
			return i;
		}
	}

	printf("No available FreeRTOS semaphore for UFFS (i=%d)\r\n", i);
	return -1;
}

int uffs_SemWait(int sem) {
	if (sem >= 0 && xSemaphoreTake(semaphores[sem].sem, portMAX_DELAY) == pdPASS) {
		return 0;
	} else {
		return -1;
	}
}

int uffs_SemSignal(int sem) {
	if (sem >= 0 && xSemaphoreGive(semaphores[sem].sem) == pdPASS) {
		return 0;
	} else {
		return -1;
	}
}

int uffs_SemDelete(int *sem) {
	vQueueDelete(semaphores[*sem].sem);
	semaphores[*sem].sem = NULL;
	semaphores[*sem].state = SEM_FREE;
	return 0;
}*/
