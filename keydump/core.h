/*
 *  Copyright (C) 2013 SmikY (smiky2000@hotmail.com)
 * 	main function header file
 */

#ifndef __CORE_H__
#define __CORE_H__

extern int sceHttpStorageOpen(int a0, int a1, int a2);
//extern int sceKernelPowerLock(unsigned int, unsigned int);

/*
 * this function dump 40 bytes the game key for kernel module
 *
 * @param prx 120 bytes buf to count key from FW 6.20
 */
extern int sceResmgr_8E6C62C8(u8* prx);

//int test_sceResmgr_8E6C62C8(u8 *prx);

#endif
