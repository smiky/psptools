#include <pspdebug.h>
#include <pspsdk.h>
#include <psploadexec.h>

PSP_MODULE_INFO("keydump", PSP_MODULE_USER, 1, 0);

#define printk pspDebugScreenPrintf

extern int SysMemUserForUser_D8DE5C1E();
extern int sceResmgr_8E6C62C8(void *buf);

int main(void) {
	pspDebugScreenInit();
	printk("key dump (c) SmikY 5.50Prome-v4\n");
	unsigned char key[120];

	if (SysMemUserForUser_D8DE5C1E())
		return 0;

	printk("key is: 0x%08X\n", sceResmgr_8E6C62C8(key));
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

	printk("Exiting...\n");

	sceKernelDelayThread(1000 * 1000);
	sceKernelExitGame();
	sceKernelExitDeleteThread(0);

	return 0;
}
