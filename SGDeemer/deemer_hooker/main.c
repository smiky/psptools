/*  ================================================================  *\
 *                                                                    *
 *  >> Savegame Deemer <<                                             *
 *  a tool to read and write back savegames                           *
 *  with the ability to gather the unique game key for FW2.00+ games  *
 *                                                                    *
 *  by ---==> HELLCAT <==---                                          *
 *                                                                    *
 \*  ================================================================  */

//                                                                    //
//  System Hook Module (CFW Plugin)                                   //
//  taking care of system call patching and gathering the gamekey     //
////////////////////////////////////////////////////////////////////////

#include <pspsdk.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <psputility.h>
#include <stdlib.h>
#include <stdio.h>

#include "deemerd.h"

PSP_MODULE_INFO("hcSGDeemerSysHook", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(1024);

#define printf pspDebugScreenPrintf

//////////////////////////////////////
//                                  //
//   Declarations and definitions   //
//                                  //
//////////////////////////////////////

typedef struct SceUtilitySavedataSavenames {
	char name1[20];
	char name2[20];
	char name3[20];
	char name4[20];
	char name5[20];
	char terminator[20];
} SceUtilitySavedataSavenames;

typedef struct SceUtilitySavedataParamEx {
	/*
	 This is almost completely a copy of SceUtilitySavedataParam
	 from psputility_savedata.h, exept for some additions to include
	 the gamekey for 2.00+ games and the pointer to multiple savenames
	 */

	pspUtilityDialogCommon base;
	int mode;
	int unknown12;
	int unknown13;
	char gameName[16];
	char saveName[20];
	SceUtilitySavedataSavenames* saveNames;
	char fileName[16];
	void *dataBuf;
	SceSize dataBufSize;
	SceSize dataSize;
	PspUtilitySavedataSFOParam sfoParam;
	PspUtilitySavedataFileData icon0FileData;
	PspUtilitySavedataFileData icon1FileData;
	PspUtilitySavedataFileData pic1FileData;
	PspUtilitySavedataFileData snd0FileData;
	unsigned char unknown17[4];
	unsigned char unknown18[20];
	unsigned char gamekey[16];
	unsigned char unknown19[20];
} SceUtilitySavedataParamEx;

SceUtilitySavedataParamEx* SDParamBuffer;

///////////////////
//               //
//   Functions   //
//               //
///////////////////

int hcReadFile(char* file, void* buf, int size) {
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		return -1;
	}

	int read = sceIoRead(fd, buf, size);

	if (sceIoClose(fd) < 0)
		return -1;

	return read;
}

int hcWriteFile(char* file, void* buffer, int size) {
	int i;
	int pathlen = 0;
	char path[128];

	// because I'm so lazy, I need folders created "on the fly"
	// this does so, create every folder in the path of the
	// file we are to save.
	// A little bruty forcy, yah, but it does the job :-D
	for (i = 1; i < (strlen(file)); i++) {
		if (strncmp(file + i - 1, "/", 1) == 0) {
			pathlen = i - 1;
			strncpy(path, file, pathlen);
			path[pathlen] = 0;
			sceIoMkdir(path, 0777);
		}
	}

	// now up to the file write....
	SceUID fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if (fd < 0) {
		return -1;
	}

	int written = sceIoWrite(fd, buffer, size);

	if (sceIoClose(fd) < 0)
		return -1;

	return written;
}

void debugIssueError(char* msg) {
	pspDebugScreenInit();
	while (1 == 1) {
		pspDebugScreenSetXY(0, 0);
		printf("%s", msg);
	}
}

void debugIssueMessage(char* msg, int frames) {
	int i = 0;

	pspDebugScreenInit();
	while (i < frames) {
		sceDisplayWaitVblankStart();
		pspDebugScreenSetXY(0, 0);
		printf("%s", msg);
		i++;
	}
}

int hcDeemerCapturedSDParamsCallback(int arg1, int arg2, void *arg)  //void* params)
{
	char s[512];
	SceUtilitySavedataParamEx* SDParams;

	SDParams = (SceUtilitySavedataParamEx*) arg2;
	sprintf(s, "ms0:/PSP/SAVEPLAIN/%s%s/%s.bin", SDParams->gameName, SDParams->saveName, SDParams->gameName);

	hcWriteFile(s, (u8*) arg2, 0x600);

	if ((SDParams->mode == 1) | (SDParams->mode == 5)) {
		sprintf(s, "ms0:/PSP/SAVEPLAIN/%s%s/SDDATA.BIN", SDParams->gameName, SDParams->saveName);
		hcWriteFile(s, SDParams->dataBuf, SDParams->dataBufSize);
		sprintf(s, "ms0:/PSP/SAVEPLAIN/%s%s/SDINFO.BIN", SDParams->gameName, SDParams->saveName);
		hcWriteFile(s, &SDParams->sfoParam, sizeof(PspUtilitySavedataSFOParam));
	}

	if ((SDParams->mode == 0) | (SDParams->mode == 4)) {
		SDParamBuffer = SDParams;
	} else {
		SDParamBuffer = 0;
	}

	return 0;
}

int hcDeemerSavedataGetStatusCallback(int arg1, int arg2, void *arg) {
	int i;
	int r;
	char s[512];
	SceUtilitySavedataParamEx* SDParams;

	// check if we remembered the param struct address
	// 'cause if we did, we're in loading mode, and might want to load something ;-)
	i = (int) SDParamBuffer;
	if (i != 0) {
		SDParams = SDParamBuffer;

		sprintf(s, "ms0:/PSP/SAVEPLAIN/%s%s/SDDATA.BIN", SDParams->gameName, SDParams->saveName);
		r = hcReadFile(s, SDParams->dataBuf, SDParams->dataBufSize);
		sprintf(s, "ms0:/PSP/SAVEPLAIN/%s%s/SDINFO.BIN", SDParams->gameName, SDParams->saveName);
		r = hcReadFile(s, &SDParams->sfoParam, sizeof(PspUtilitySavedataSFOParam));

		SDParamBuffer = 0;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	SceUID mod;
	char s[255];
	int r;
	int SDCB1, SDCB2;

	sceKernelDelayThread(7000000);

	mod = pspSdkLoadStartModule("ms0:/seplugins/deemerd.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (mod < 0) {
		sprintf(s, "Error 0x%08X loading/starting deemerd.prx.\n", mod);
		debugIssueError(s);
	}

	SDCB1 = sceKernelCreateCallback("SaveDataStartCallback", hcDeemerCapturedSDParamsCallback, NULL );
	SDCB2 = sceKernelCreateCallback("SaveDataGetStatusCallback", hcDeemerSavedataGetStatusCallback, NULL );

	hcDeemerDriverSetupCallbackCapturedSDParams(SDCB1, SDCB2);

	r = hcDeemerDriverPatchSavedataInitStart();

	sceKernelSleepThreadCB();

	return 0;
}
