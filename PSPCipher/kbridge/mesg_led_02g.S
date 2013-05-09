	.set noreorder

#include "pspstub.s"

	STUB_START "sceResmgr",0x40090011,0x00010005
	STUB_FUNC  0x8E6C62C8,sceResmgr_8E6C62C8
	STUB_END

	STUB_START "sceResmgr_driver",0x00090011,0x00010005
	STUB_FUNC  0x8E6C62C8,sceResmgr_driver_8E6C62C8
	STUB_END
