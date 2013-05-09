#ifndef DECRYPTBLOCK_H
#define DECRYPTBLOCK_H

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

typedef struct _user_decryptor {
	u32 *tag; // key tag addr
	u8 *key;  // 16 bytes key
	u32 code; // scramble code
	u8 *prx;  // prx addr
	u32 size; // prx size
	u32 *newsize; // pointer of prx new size after decryption
	u32 use_polling; // use sceUtilsBufferCopyByPollingWithRange when 1 is set, pass 0
	u8 *blacklist; // module blacklist, pass NULL
	u32 blacklistsize; // module blacklist size in byte, pass 0
	u32 type; // prx type 2 for game, 5 for game patch etc, look up loadcore.prx if you are unsure
	u8 *xor_key1; // optional xor key, when decrypting prx type 3/5 this key is essential, otherwise can be NULL
   	u8 *xor_key2; // optional xor key, when decrypting DRMed module this key is essential, otherwise can be NULL
} user_decryptor;

typedef struct _kernel_decryptor {
	u8 *prx;  // prx addr
	u32 size; // prx size
	u32 *newsize; // pointer of prx new size after decryption
	u32 use_polling; // use sceUtilsBufferCopyByPollingWithRange when 1 is set, pass 0
} kernel_decryptor;

/**
 * Decrypt user PRX module such as game, game-patch etc.
 * It has the same behavior with sub_000000e0 in mesg_led_02g.prx from FW 6.20
 */
extern int uprx_decrypt(user_decryptor *p);

/**
 * Decrypt kernel PRX module
 * It has the same behavior with sub_00000134 in memlmd_02g.prx from FW 6.20
 */
extern int kprx_decrypt(kernel_decryptor *pBlock);

/**
 * every sony module has an enique 16 bytes ID in offset 0x140.
 * this function check the ID and return 1 if matched
 *
 * @note modules such as memlmd_01g.prx and memlmd_02g.prx share same 16 bytes ID
 * @return 1 if an ID in blacklst is matched, 0 if not
 */
extern int check_blacklist(u8 *prx, u8 *blacklist, u32 blacklistsize);

/**
 * kirk engine
 * It has the same behavior with sub_00000000 in mesg_led_02g.prx from FW 6.20
 */
extern int kirk7(u8* prx, u32 size, u32 scramble_code, u32 use_polling);

extern void prx_xor_key_into(u8 *dstbuf, u32 size, u8 *srcbuf, u8 *xor_key);

/**
 * this function initalizes key seed to decrypt kernel PRX
 *
 * @param unk always pass 0
 * @param hash_addr hash address, PSP store 32 bytes in (u8*)0xbfc00200 as hash seeder
 * @note haven't tested
 */
void memlmd_8450109F(u32 unk, u8 *hash_addr);

/**
 * this function dump 32 bytes the real decryption key for kernel module
 *
 * @param result 32 bytes buf to store key
 * @note haven't tested, currently I use xored key from FW 6.20
 */
void test_memlmd_8450109F(u8 *result);

/**
 * game key calculate function
 * If the game tag is new then this function will base on opnssmp.bin calculate new key and
 * replace old key address(module sceMesgLed textaddr + 0x8E00) value.
 */
extern int sceResmgr_8E6C62C8(u8 *prx);

/**
 * this function dump 40 bytes the new decryption key for game module
 *
 * @param prx 120 bytes buf from opnssmp.bin
 * @param new_key is 40 bytes empty buf save return new key
 */
void test_sceResmgr_8E6C62C8(u8 *prx, u8 *new_key);
#endif
