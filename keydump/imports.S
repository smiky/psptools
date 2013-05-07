	.set noreorder

#include "pspstub.s"

	STUB_START "UtilsForUser",0x40000011,0x00020005
	STUB_FUNC  0x920F104A,sceKernelIcacheInvalidateAll
	STUB_FUNC  0xB435DEC5,sceKernelDcacheWritebackInvalidateAll
	STUB_END

	STUB_START "sceHttpStorage",0x40090011,0x00010005
	STUB_FUNC  0x700AAD44,sceHttpStorageOpen
	STUB_END

	STUB_START "sceResmgr",0x40090011,0x00010005
	STUB_FUNC  0x8E6C62C8,sceResmgr_8E6C62C8
	STUB_END

	STUB_START "sceResmgr_driver",0x00090011,0x00010005
	STUB_FUNC  0x8E6C62C8,sceResmgr_driver_8E6C62C8
	STUB_END
