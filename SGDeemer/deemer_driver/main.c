/*  ================================================================  *\
 *                                                                    *
 *  >> Savegame Deemer <<                                             *
 *  a tool to read and write back savegames                           *
 *  with the ability to gather the unique game key for FW2.00+ games  *
 *                                                                    *
 *  by ---==> HELLCAT <==---                                          *
 *                                                                    *
\*  ================================================================  */

//                                                                    //
//  Driver Module                                                     //
//  holding all kernel mode functions                                 //
////////////////////////////////////////////////////////////////////////


#include <pspsdk.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspsyscon.h>
#include <psputilsforkernel.h>
#include <string.h>
#include <stdlib.h>


PSP_MODULE_INFO("hcSGDeemer_driver", 0x1006, 1, 0);
PSP_MAIN_THREAD_ATTR(0);



//////////////////////////////////////
//                                  //
//   Declarations and definitions   //
//                                  //
//////////////////////////////////////


struct SyscallHeader 
{ 
  void *unk; 
  unsigned int basenum; 
  unsigned int topnum; 
  unsigned int size; 
};

int hcDeemerDriverCapturedSDParamsCallback;
int hcDeemerSDGetStatusCallback;


////////////////////////////
//                        //
//   Internal functions   //
//                        //
////////////////////////////


// I guess everyone knowing to use this snippet knows where it comes from ;-)))
u32 pspFindProc(const char* szMod, const char* szLib, u32 nid)
{
  struct SceLibraryEntryTable *entry;
	SceModule *pMod;
	void *entTab;
	int entLen;

	pMod = sceKernelFindModuleByName(szMod);

	if (!pMod)
	{
		// NOP //
		return 0;
	}
	
	int i = 0;

	entTab = pMod->ent_top;
	entLen = pMod->ent_size;
	
	while(i < entLen)
  {
		int count;
		int total;
		unsigned int *vars;

		entry = (struct SceLibraryEntryTable *) (entTab + i);

    if(entry->libname && !strcmp(entry->libname, szLib))
		{
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;

			if(entry->stubcount > 0)
			{
				for(count = 0; count < entry->stubcount; count++)
				{
					if (vars[count] == nid)
					{
				    return vars[count+total];
				  }					
				}
			}
		}

		i += (entry->len * 4);
	}

	return 0;
}

void* pspGetSysCallAddr(u32 addr) 
{ 
  struct SyscallHeader *head; 
  u32 *syscalls; 
  void **ptr; 
  int size; 
  int i; 

  asm( 
    "cfc0 %0, $12\n" 
    : "=r"(ptr) 
  ); 

  if(!ptr) 
  { 
    return NULL; 
  } 

  head = (struct SyscallHeader *) *ptr; 
  syscalls = (u32*) (*ptr + 0x10); 
  size = (head->size - 0x10);

  for(i = 0; i < size; i++) 
  { 
    if(syscalls[i] == addr) 
    { 
      return &syscalls[i]; 
    } 
  } 

  return NULL; 
}

void* pspPatchProcCall(u32 *addr, void *func) 
{
  if(!addr) 
  { 
    return NULL; 
  } 
  *addr = (u32) func; 
  sceKernelDcacheWritebackInvalidateRange(addr, sizeof(addr)); 
  sceKernelIcacheInvalidateRange(addr, sizeof(addr)); 

  return addr; 
}


int (*oUtilitySavedataInitStart)(SceUtilitySavedataParam* params);
// our own hook-a-boo function
int patchedUtilitySavedataInitStart(SceUtilitySavedataParam* params)
{
	sceKernelNotifyCallback(hcDeemerDriverCapturedSDParamsCallback, (int)params);
	
	// give the callback some time to become idle....
	sceKernelDelayThread(3000000);
	
  return oUtilitySavedataInitStart(params);
}

int (*oUtilitySavedataGetStatus)(void);
int patchedUtilitySavedataGetStatus(void)
{
  int r;
  
  r = oUtilitySavedataGetStatus();
  
  if( r == 3 )
  {
    sceKernelNotifyCallback(hcDeemerSDGetStatusCallback, 0);
  
    // give the callback some time to become idle....
	  sceKernelDelayThread(3000000);
  }
  
  return r;
}


////////////////////////////
//                        //
//   Exported functions   //
//                        //
////////////////////////////


int module_start(SceSize args, void *argp)
{
  return 0;
}

int module_stop()
{
  return 0;
}


int hcDeemerDriverPatchSavedataInitStart(void)
{
	void* oProcAddr;
	
  oProcAddr = (void*)pspFindProc("sceUtility_Driver", "sceUtility", 0x8874DBE0);
  pspPatchProcCall(pspGetSysCallAddr((u32)oProcAddr), &patchedUtilitySavedataGetStatus);
  oUtilitySavedataGetStatus = (void*)oProcAddr;
  
  oProcAddr = (void*)pspFindProc("sceUtility_Driver", "sceUtility", 0x50C4CD57);
  pspPatchProcCall(pspGetSysCallAddr((u32)oProcAddr), &patchedUtilitySavedataInitStart);
  oUtilitySavedataInitStart = (void*)oProcAddr;
  
  return (int)oProcAddr;
}


void hcDeemerDriverSetupCallbackCapturedSDParams(int CallbackID1, int CallbackID2)
{
  hcDeemerDriverCapturedSDParamsCallback = CallbackID1;
  hcDeemerSDGetStatusCallback = CallbackID2;
}
