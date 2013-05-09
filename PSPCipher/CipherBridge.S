	.set noreorder

#include "pspimport.s"

// Build files
// CipherBridge_0000.o CipherBridge_0001.o CipherBridge_0002.o CipherBridge_0003.o 

	IMPORT_START "CipherBridge",0x40090000
	IMPORT_FUNC  "CipherBridge",0x27B49DD6,uprx_decrypt
	IMPORT_FUNC  "CipherBridge",0x018C3EAC,pspDecompress
	IMPORT_FUNC  "CipherBridge",0xBC64788D,test_memlmd_8450109F
	IMPORT_FUNC  "CipherBridge",0xD4AD42A4,kprx_decrypt
	IMPORT_FUNC  "CipherBridge",0xD4FB2269,test_sceResmgr_8E6C62C8
	
