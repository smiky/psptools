/*
 * This file is part of pspcipher.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
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

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman_kernel.h>
#include <pspcrypt.h>
#include <pspdebug.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "psputilsforkernel.h"
#include "systemctrl.h"
#include "../pspcipher.h"

#define MIPS_J_ADDRESS(x) (((u32)((x)) & 0x3FFFFFFF) >> 2)
#define NOP       (0x00000000)
#define JAL_TO(x) (0x0E000000 | MIPS_J_ADDRESS(x))
#define J_TO(x)   (0x08000000 | MIPS_J_ADDRESS(x))
#define LUI(x,y)  (0x3C000000 | ((x & 0x1F) << 0x10) | (y & 0xFFFF))

PSP_MODULE_INFO("CipherBridge", 0x5006, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

int key = 0;

int check_blacklist(u8 *prx, u8 *blacklist, u32 blacklistsize)
{
	u32 i;

	if (blacklistsize / 16 == 0) {
		return 0;
	}

	i = 0;

	while (i < blacklistsize / 16) {
		if (!memcmp(blacklist + i * 16, (prx + 0x140), 0x10)) {
			return 1;
		}

		i++;
	}

	return 0;
}

int kirk7(u8* prx, u32 size, u32 scramble_code, u32 use_polling)
{
	int ret;

	((u32 *) prx)[0] = 5;
	((u32 *) prx)[1] = 0;
	((u32 *) prx)[2] = 0;
	((u32 *) prx)[3] = scramble_code;
	((u32 *) prx)[4] = size;

	if (!use_polling) {
		ret = sceUtilsBufferCopyWithRange (prx, size + 20, prx, size + 20, 7);
	} else {
		ret = sceUtilsBufferCopyByPollingWithRange (prx, size + 20, prx, size + 20, 7);
	}

	return ret;
}

void prx_xor_key_into(u8 *dstbuf, u32 size, u8 *srcbuf, u8 *xor_key)
{
	u32 i;

	i = 0;

	while (i < size) {
		dstbuf[i] = srcbuf[i] ^ xor_key[i];
		++i;
	}
}

void prx_xor_key_large(u8 *buf, u32 size, u8 *xor_key)
{
	u32 i;

	i = 0;

	while (i < size) {
		buf[i] = buf[i] ^ xor_key[i];
		++i;
	}
}

void prx_xor_key(u8 *buf, u32 size, u8 *xor_key1, u8 *xor_key2)
{
	u32 i;

	i =0;
	while (i < size) {
		if (xor_key2 != NULL) {
			buf[i] = buf[i] ^ xor_key2[i&0xf];
		}

		buf[i] = buf[i] ^ xor_key1[i&0xf];
		++i;
	}
}

void prx_xor_key_single(u8 *buf, u32 size, u8 *xor_key)
{
	return prx_xor_key(buf, size, xor_key, NULL);
}

int _uprx_decrypt(user_decryptor *pBlock)
{
	if (pBlock == NULL)
		return -1;

	if (pBlock->prx == NULL || pBlock->newsize == NULL)
		return -2;

	if (pBlock->size < 0x160)
		return -202;

	if ((u32)pBlock->prx & 0x3f)
		return -203;

	if (((0x00220202 >> (((u32)pBlock->prx >> 27) & 0x001F)) & 0x0001) == 0x0000)
		return -204;

	/** buf1 = 0x8fc0 */
	u8 buf1[0x150];
	/** buf2 = 0x91c0 */
	u8 buf2[0x150];
	/** buf3 = 0x9110 */
	u8 buf3[0x90];
	/** buf4 = 0x9340 */
	u8 buf4[0xb4];

	u32 b_0xd4 = 0;

	memset(buf1, 0, sizeof(buf1));
	memset(buf2, 0, sizeof(buf2));
	memset(buf3, 0, sizeof(buf3));
	memset(buf4, 0, sizeof(buf4));

	memcpy(buf1, pBlock->prx, 0x150);

	/** tag mismatched */
	if (memcmp(buf1 + 0xd0, pBlock->tag, 4))
		return -45;

	int ret = -1;

	if (pBlock->type == 3) {
		u8 *p = buf1;
		u32 cnt = 0;

		while (p[0xd4] && cnt < 0x18 ) {
			cnt++;
			p = buf1 + cnt;
		}

		if (p[0xd4] != 0)
			return -17;
	} else if (pBlock->type == 2) {
		u8 *p = buf1;
		u32 cnt = 0;

		while (p[0xd4] && cnt < 0x58 ) {
			cnt++;
			p = buf1 + cnt;
		}

		if (p[0xd4] != 0)
			return -12;
	} else if (pBlock->type == 5) {
		u8 *p = buf1 + 1;
		u32 cnt = 1;

		while (p[0xd4] && cnt < 0x58 ) {
			cnt++;
			p = buf1 + cnt;
		}

		if (p[0xd4] != 0)
			return -13;

		b_0xd4 = buf1[0xd4];
	}

//label38:
	if (pBlock->blacklist != NULL && pBlock->blacklistsize != 0) {
		ret = check_blacklist(buf1, pBlock->blacklist, pBlock->blacklistsize);

		if (ret == 1)
			return -305;
	}

	u32 elf_size_comp =  *(u32*)(buf1+0xb0);
//	printf("elf_size_comp: %d\n", elf_size_comp);
	*pBlock->newsize = elf_size_comp;

	if (pBlock->size - 50 < elf_size_comp)
		return -206;

	if (pBlock->type >= 6) {
		memcpy(buf2 + 14, pBlock->key, 0x90);
	} else {
		int i;

		for (i=0; i<9; i++)
		{
			memcpy(buf2 + 0x14 + (i << 4), pBlock->key, 0x10);
			buf2[0x14+ (i<<4)] = i;
		}
	}

	if ((ret = kirk7(buf2, 0x90, pBlock->code, pBlock->use_polling)) < 0) {
		return ret;
	}

	if (pBlock->type == 3 || pBlock->type == 5) {
		if (pBlock->xor_key2 != NULL) {
			prx_xor_key_single(buf2, 0x90, pBlock->xor_key2);
		}
	}

	memcpy(buf3, buf2, 0x90);

	if (pBlock->type == 3) {
		memcpy(buf2, buf1 + 0xec, 0x40);
		memset(buf2 + 0x40, 0, 0x50);
		buf2[0x60] = 0x03;
		buf2[0x70] = 0x50;

		memcpy(buf2 + 0x90, buf1 + 0x80, 0x30);
		memcpy(buf2 + 0xc0, buf1 + 0xc0, 0x10);
		memcpy(buf2 + 0xd0, buf1 + 0x12c, 0x10);

		prx_xor_key(buf2+144, 0x50, pBlock->xor_key1, pBlock->xor_key2);
		ret = sceUtilsBufferCopyWithRange(buf4, 0xb4, buf2, 0X150, 3);

		if (ret != 0) {
			return -14;
		}
		
		memcpy(buf2, buf1 + 0xd0, 4);
		memset(buf2 + 4, 0, 0x58);
		memcpy(buf2 + 0x5c, buf1 + 0x140, 0x10);
		memcpy(buf2 + 0x6c, buf1 + 0x12c, 0x14);
		memcpy(buf2 + 0x6c, buf4, 0x10);
		memcpy(buf2 + 0x80, buf4, 0x30);
		memcpy(buf2 + 0xd0, buf4 + 0x30, 0x10);
		memcpy(buf2 + 0xc0, buf1 + 0xb0, 0x10);
		memcpy(buf2 + 0xd0, buf1, 0x80);
	} else if (pBlock->type == 5) {
		memcpy(buf2 + 0x14, buf1 + 0x80, 0x30);
		memcpy(buf2 + 0x44, buf1 + 0xc0, 0x10);
		memcpy(buf2 + 0x54, buf1 + 0x12c, 0x10);
		prx_xor_key(buf2+20, 0x50, pBlock->xor_key1, pBlock->xor_key2);
		ret = kirk7 (buf2, 0x50, pBlock->code, pBlock->use_polling);

		if (ret != 0) {
			return -11;
		}

		memcpy(buf4, buf2, 0x50);
		memcpy(buf2, buf1 + 0xd0, 0x4);
		memset(buf2 + 4, 0, 0x58);
		memcpy(buf2 + 0x5c, buf1 + 0x140, 0x10);
		memcpy(buf2 + 0x6c, buf1 + 0x12c, 0x14);
		memcpy(buf2 + 0x6c, buf4 + 0x40, 0x10);
		memcpy(buf2 + 0x80, buf4, 0x30);
		memcpy(buf2 + 0xb0, buf4 + 0x30, 0x10);
		memcpy(buf2 + 0xc0, buf1 + 0xb0, 0x10);
		memcpy(buf2 + 0xd0, buf1, 0x80);
	} else {
		if (pBlock->type != 2 && pBlock->type != 4) {
			memcpy(buf2, buf1 + 0xd0, 0x80);
			memcpy(buf2 + 0x80, buf1 + 0x80, 0x50);
			memcpy(buf2 + 0xd0, buf1, 0x80);
		} else {
			memcpy(buf2       , buf1 +  0xd0, 0x5C);
			memcpy(buf2 + 0x5c, buf1 + 0x140, 0x10);
			memcpy(buf2 + 0x6c, buf1 + 0x12c, 0x14);
			memcpy(buf2 + 0x80, buf1 +  0x80, 0x30);
			memcpy(buf2 + 0xb0, buf1 +  0xc0, 0x10);
			memcpy(buf2 + 0xc0, buf1 +  0xb0, 0x10);
			memcpy(buf2 + 0xd0, buf1        , 0x80);
		}
	}

//label159:
	if (pBlock->type == 1)
	{
		memcpy(buf4 + 0x14, buf2 + 0x10, 0xa0);
		ret = kirk7(buf4, 0xa0, pBlock->code, pBlock->use_polling);

		if (ret < 0) {
			return -15;
		}

		memcpy(buf2 + 0x10, buf4, 0xa0);
	} else {
		if (pBlock->type < 6) {
			memcpy(buf4 + 0x14, buf2 + 0x5c, 0x60);
		}

		if (pBlock->type == 3 || pBlock->type == 5) {
			prx_xor_key_single(buf4 + 20, 0x60, pBlock->xor_key1);
		}

		if (kirk7(buf4, 0x60, pBlock->code, pBlock->use_polling) < 0) {
			return -5;
		}

		memcpy(buf2 + 0x5c, buf4, 0x60);
	}

//label183:
	if (pBlock->type >= 6) {
		memcpy(buf4, buf2 + 0x4, 0x14);
		*((u32*)buf2) = 0x14c;
		memcpy(buf2 + 4, buf3, 0x14);
	} else {
		memcpy(buf4, buf2 + 0x6c, 0x14);

		if (pBlock->type == 4) {
			memmove(buf2 + 0x18, buf2, 0x67);
		} else {
			memcpy(buf2+0x70, buf2+0x5C, 0x10);
			memset(buf2+0x18, 0, 0x58);

			if ( b_0xd4 == 0x80 ) {
				buf2[0x18] = 0x80;
			}
		}

		memcpy(buf2+0x04, buf2, 0x04);
		*((u32*)buf2) = 0x014C;
		memcpy(buf2+0x08, buf3, 0x10);	
	}

	if (pBlock->use_polling == 0) {
		ret = sceUtilsBufferCopyWithRange (buf2, 0x150, buf2, 0x150, 0xB);
	} else {
		ret = sceUtilsBufferCopyByPollingWithRange (buf2, 0x150, buf2, 0x150, 0xB);
	}

	if (ret != 0) {
		return -6;
	}

	if (memcmp(buf2, buf4, 0x14))
	{
		return -8;
	}

	if (pBlock->type >= 6) {
		prx_xor_key_large(buf2+0x40, 0x70, buf3+0x14);

		if (kirk7(buf2 + 0x2c, 0x70, pBlock->code, pBlock->use_polling) < 0) {
			return -16;
		}

		prx_xor_key_into(pBlock->prx+64, 0x70, buf2+44, buf3+32);
		memcpy(pBlock->prx+176, buf2 + 0xb0, 0xa0);

//	label256
	} else {
		prx_xor_key_large(buf2 + 128, 0x40, buf3 + 16);

		if (kirk7(buf2 + 108, 0x40, pBlock->code, pBlock->use_polling) < 0) {
			return -7;
		}

		prx_xor_key_into(pBlock->prx + 64, 0x40, buf2 + 108, buf3 + 80);
		memset(pBlock->prx+128, 0, 0x30);
		((u8*)pBlock->prx)[160] = 1;
		memcpy(pBlock->prx+176, buf2+0xc0, 0x10);
		memset(pBlock->prx+192, 0, 0x10);
		memcpy(pBlock->prx+208, buf2+0xd0, 0x80);
	}

	if (b_0xd4 == 0x80) {
		if (((u8*)pBlock->prx)[1424])
			return -302;

		((u8*)pBlock->prx)[1424] |= 0x80;
	}

	// The real decryption
	if (sceUtilsBufferCopyWithRange(pBlock->prx, pBlock->size, pBlock->prx+0x40, pBlock->size-0x40, 0x1) != 0)
	{
		return -9;
	}

	if (elf_size_comp < 0x150)
	{
		// Fill with 0
		memset(pBlock->prx+elf_size_comp, 0, 0x150-elf_size_comp);		
	}

	return 0;
}

int uprx_decrypt(user_decryptor *pBlock)
{
	int ret;
	u32 k1 = pspSdkGetK1();


	pspSdkSetK1(0);
	ret = sceKernelExtendKernelStack(0x2000, (void *)_uprx_decrypt, pBlock);
	pspSdkSetK1(k1);

	return ret;
}

////////// Decompression //////////

int pspIsCompressed(u8 *buf)
{
	int k1 = pspSdkSetK1(0);
	int res = 0;

	if (buf[0] == 0x1F && buf[1] == 0x8B)
		res = 1;
	else if (memcmp(buf, "2RLZ", 4) == 0)
		res = 1;

	pspSdkSetK1(k1);
	return res;
}


int decompress_kle(void *outbuf, u32 outcapacity, void *inbuf, void *unk)
{
	int (* decompress)(void *, u32, void *, void *);
	
	u32 *mod = (u32 *)sceKernelFindModuleByName("sceLoadExec");
	u32 text_addr = *(mod+27);
	decompress = (void *)(text_addr+0);

	return decompress(outbuf, outcapacity, inbuf, unk);
}

static int _pspDecompress(u32 *arg)
{
	int retsize;
	u8 *inbuf = (u8 *)arg[0];
	u8 *outbuf = (u8 *)arg[1];
	u32 outcapacity = arg[2];
	
	if (inbuf[0] == 0x1F && inbuf[1] == 0x8B) 
	{
		retsize = sceKernelGzipDecompress(outbuf, outcapacity, inbuf, NULL);
	}
	else if (memcmp(inbuf, "2RLZ", 4) == 0) 
	{
		int (*lzrc)(void *outbuf, u32 outcapacity, void *inbuf, void *unk) = NULL;
		
		if (sceKernelDevkitVersion() >= 0x03080000)
		{
			
			u32 *mod = (u32 *)sceKernelFindModuleByName("sceNp9660_driver");
			if (!mod)
				return -1;

			u32 *code = (u32 *)mod[27];

			int i;
			
			for (i = 0; i < 0x8000; i++)
			{
				if (code[i] == 0x27bdf4f0 && code[i+20] == 0x34018080)
				{
					lzrc = (void *)&code[i];
					break;
				} 
			}

			if (i == 0x8000)
				return -2;
			//lzrc = lzrc_;
		}
		else
		{
			lzrc = (void *)sctrlHENFindFunction("sceSystemMemoryManager", "UtilsForKernel", 0x7DD07271);

		}
		
		retsize = lzrc(outbuf, outcapacity, inbuf+4, NULL);
	}
	else if (memcmp(inbuf, "KL4E", 4) == 0)
	{
		extern int UtilsForKernel_6C6887EE(void *, u32, void *, void *);

		retsize = UtilsForKernel_6C6887EE(outbuf, outcapacity, inbuf+4, NULL);
	}
	else if (memcmp(inbuf, "KL3E", 4) == 0) 
	{
		retsize = decompress_kle(outbuf, outcapacity, inbuf+4, NULL);
	}
	else
	{
		retsize = -1;
	}

	return retsize;
}

int pspDecompress(const u8 *inbuf, u8 *outbuf, u32 outcapacity)
{
	int k1 = pspSdkSetK1(0);
	u32 arg[3];

	arg[0] = (u32)inbuf;
	arg[1] = (u32)outbuf;
	arg[2] = outcapacity;
	
	int res = sceKernelExtendKernelStack(0x2000, (void *)_pspDecompress, arg);
	
	pspSdkSetK1(k1);
	return res;
}

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

int module_start(SceSize args, void *argp)
{
	return 0;
}

int module_stop(void)
{
	return 0;
}
