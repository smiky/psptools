	.set noreorder

#include "pspimport.s"

	IMPORT_START "sceResmgr_driver",0x00090000
  	IMPORT_FUNC  "sceResmgr_driver",0x8E6C62C8,sceResmgr_driver_8E6C62C8

	IMPORT_START "sceResmgr",0x40090000
	IMPORT_FUNC  "sceResmgr",0x8E6C62C8,sceResmgr_8E6C62C8
