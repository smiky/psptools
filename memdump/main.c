#include	<pspsdk.h>
#include	<pspctrl.h>
#include	<pspiofilemgr.h>
#include	<string.h>

//#include	"kubridge.h"

PSP_MODULE_INFO("memdump_plugin", PSP_MODULE_KERNEL, 0, 0);

/* �}�N����` */
#define	GO 4
#define ERROR -1
#define MAX_THREAD 64
#define	SUSPEND_MODE 0
#define	RESUME_MODE 1

/* �v���g�^�C�v�錾 */
void Get_FirstThreads(void);
void Suspend_Resume_Threads(int mode);
int memdump(const char *path, void *addr, SceSize size);
int set_config(void);
int main_thread(SceSize args, void *argp);
int module_start(SceSize args, void *argp);
int module_stop(SceSize args, void *argp);

/* �O���[�o���ϐ� */
static int first_th[MAX_THREAD];
static int first_count;
static int current_th[MAX_THREAD];
static int current_count;

char UmemDumpPath[256], KmemDumpPath[256];				//�o�̓p�X�p�ϐ�

SceUID UmemStart = 0x08800000, UmemEnd = 0x09FFFFFF;	//���[�U�[�������͍��̂Ƃ��Œ�
SceUID KmemStart, KmemEnd;								//�J�[�l���������A�h���X�p�ϐ�

void Get_FirstThreads(void) {
	// �X���b�h�ꗗ���擾
	sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, first_th, MAX_THREAD, &first_count);
}

void Suspend_Resume_Threads(int mode) {
	int i, n;
	SceUID my_thid;
	SceUID (*Thread_Func)(SceUID) = NULL;

	my_thid = sceKernelGetThreadId();
	Thread_Func = (mode == RESUME_MODE ? sceKernelResumeThread : sceKernelSuspendThread);

	// �X���b�h�ꗗ���擾
	sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, current_th, MAX_THREAD, &current_count);

	for (i = 0; i < current_count; i++) {
		for (n = 0; n < first_count; n++) {
			if (current_th[i] == first_th[n]) {
				current_th[i] = 0;
				n = first_count;
			}
		}

		if (current_th[i] != my_thid)
			Thread_Func(current_th[i]);
	}

	return;
}

int memdump(const char *path, void *addr, SceSize size) {

	SceUID dump_fd;
	Suspend_Resume_Threads(SUSPEND_MODE);		//�X���b�h�ꎞ��~

	if ((dump_fd = sceIoOpen(path, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777)) >= 0) {
		sceIoWrite(dump_fd, addr, size);
		sceIoClose(dump_fd);
		dump_fd = 0;
	} else {
		return ERROR;
	}

	Suspend_Resume_Threads(RESUME_MODE);		//�X���b�h�ĊJ
	return dump_fd;
}

int set_config(void) {

	u8 model = sceKernelGetModel();		//PSP���f�����o

	if (model == GO) {
		strcpy(UmemDumpPath, "ef0:/umemdump.bin");
		strcpy(KmemDumpPath, "ef0:/kmemdump.bin");
		KmemStart = 0x08000000;
		KmemEnd = 0x087FFFFF;
	} else {
		strcpy(UmemDumpPath, "ms0:/umemdump.bin");
		strcpy(KmemDumpPath, "ms0:/kmemdump.bin");
		KmemStart = 0x88000000;
		KmemEnd = 0x887FFFFF;
	}

	return 0;
}

int main_thread(SceSize args, void *argp) {

	SceCtrlData pad;

	//�L�[�ݒ�
	SceUID UmemDumpButtons = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_CROSS;
	SceUID KmemDumpButtons = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_DOWN;

	set_config();

	while (1) {
		sceKernelDelayThread(50000);
		sceCtrlPeekBufferPositive(&pad, 1);

		if ((pad.Buttons & UmemDumpButtons) == UmemDumpButtons) {
			memdump(UmemDumpPath, (void*) UmemStart, UmemEnd - UmemStart + 1);
		} else if ((pad.Buttons & KmemDumpButtons) == KmemDumpButtons) {
			memdump(KmemDumpPath, (void*) KmemStart, KmemEnd - KmemStart + 1);
		}
	}

	return 0;
}

int module_start(SceSize args, void *argp) {

	SceUID thid;

	thid = sceKernelCreateThread("memdump_plugin", main_thread, 32, 0x800, 0, NULL );

	if (thid >= 0) {
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}

int module_stop(SceSize args, void *argp) {
	return 0;
}
