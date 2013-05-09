/*
 * This file is part of pspcipher.
 *
 * Copyright (C) 2013 SmikY (smiky2000@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <pspkernel.h>
#include <pspsdk.h>
#include <string.h>

#include "../pspcipher.h"

typedef struct SceModule2 {
    struct SceModule2   *next;
    unsigned short      attribute;
    unsigned char       version[2];
    char                modname[27];
    char                terminal;
    unsigned int        unknown1;
    unsigned int        unknown2;
    SceUID              modid;
    unsigned int        unknown3[2];
    u32         mpid_text;  // 0x38
    u32         mpid_data; // 0x3C
    void *              ent_top;
    unsigned int        ent_size;
    void *              stub_top;
    unsigned int        stub_size;
    unsigned int        unknown4[5];
    unsigned int        entry_addr;
    unsigned int        gp_value;
    unsigned int        text_addr;
    unsigned int        text_size;
    unsigned int        data_size;
    unsigned int        bss_size;
    unsigned int        nsegment;
    unsigned int        segmentaddr[4];
    unsigned int        segmentsize[4];
} SceModule2;

void test_sceResmgr_8E6C62C8(u8 *prx, u8 *new_key) {
	memset(new_key, 0, 0x28);

	SceModule2 *mod = (SceModule2 *) sceKernelFindModuleByName("sceMesgLed");
	if (mod == NULL) {
		new_key[0] = -1;
		return;
	}

	u32 k1 = pspSdkGetK1();
	pspSdkSetK1(0);
	sceResmgr_8E6C62C8(prx);
	memcpy(new_key, (void *) (*(&mod->text_addr) + 0x8E00), 0x4);
	memcpy(new_key + 0x4, (void *)(*(&mod->text_addr) + 0x8E04), 0x10);
	memcpy(new_key + 0x14, (void *)(*(&mod->text_addr) + 0x8E14), 0x4);
	memcpy(new_key + 0x18, (void *)(*(&mod->text_addr) + 0x8E18), 0x10);
	pspSdkSetK1(k1);
}
