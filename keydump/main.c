#include <pspctrl.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psploadexec.h>
#include <pspsysmem.h>
#include <string.h>

PSP_MODULE_INFO("KeyDumper", PSP_MODULE_USER, 1, 0);

#define printk pspDebugScreenPrintf

extern int sceResmgr_8E6C62C8(u8 *buf, u32 size, u32* newsize);
/*
extern int sceKernelLoadModule_620(const char *path, int flags, SceKernelLMOption *option);
extern int sceKernelStartModule_620(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
extern int sceKernelUnloadModule_620(SceUID modid);

 extern int SysMemUserForUser_D8DE5C1E(void);

 extern int sceResmgr_driver_8E6C62C8();
 extern int sceResmgr_driver_9DC14891();
 extern int sceResmgr_8E6C62C8(void *buf);
 extern int sceResmgr_9DC14891(void *buf);*/

/*
static int load_opnssmp(const char *path, u32 tag) {
	SceUID modid;
	int opnssmp_type, ret;
	char opnssmp_path[128], *p;

	opnssmp_type = (tag >> 8) & 0xFF;
	strcpy(opnssmp_path, path);
	p = strrchr(opnssmp_path, '/');

	if (p != NULL ) {
		p[1] = '\0';
	} else {
		opnssmp_path[0] = '\0';
	}

	strcat(opnssmp_path, "OPNSSMP.BIN");
	modid = sceKernelLoadModule_620(opnssmp_path, 0, NULL );

	if (modid < 0) {
		printk("%s: load %s -> 0x%08X\n", __func__, opnssmp_path, modid);

		return modid;
	}

	ret = sceKernelStartModule_620(modid, 4, &opnssmp_type, 0, NULL );

	if (ret < 0) {
		printk("%s: start module -> 0x%08X\n", __func__, modid);
		sceKernelUnloadModule_620(modid);

		return ret;
	}

	return modid;
}
*/

int main(int argc, char *argv[]) {
	pspDebugScreenInit();
	printk("KeyDumper (c) SmikY 6.20 Pro-C2\nPSP Kernel Version: 0x%08X\n", sceKernelDevkitVersion());

	/*if (SysMemUserForUser_D8DE5C1E())
	 return 0;*/

	//printk("key is: 0x%08X\n", sceResmgr_8E6C62C8(key));
	/*
	 if (argv[0] == 0x00000014) {
	 printk("key 1 is: 0x%08X\n", sceResmgr_8E6C62C8(0x00000470));
	 } else if (argv[0] == 0x00000015) {
	 printk("key 2 is: 0x%08X\n", sceResmgr_8E6C62C8(0x00000300));
	 } else if (argv[0] == 0x00000017) {
	 printk("key 2 is: 0x%08X\n", sceResmgr_8E6C62C8(0x000003F0));
	 } else {
	 printk("key 3 is: 0x%08X\n", sceResmgr_8E6C62C8(0x000003F0));
	 }
	 */

	/*int ret = load_opnssmp("ms0:", 0xD91613F0);
	printk("opnssmp.bin modid is: 0x%08X\n", ret);*/
	printf("\nPress X to exit\n");

	while (1) {
		SceCtrlData pad;
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CROSS)
			break;
		sceKernelDelayThread(10000);
	}

	sceKernelExitGame();

	return 0;
}
