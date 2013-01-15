/*
 *  Header include for Savegame-Deemer Driver
 *
 *  deemerd.h
 *  by ---==> HELLCAT <==---
 *
 * part of "Savegame-Deemer"
 *
 */

/**
 *  patch sceUtilitySavedataInitStart() to an own function
 *  for dumping the passed params structure and relaying it
 *  to the original function after that....
 *
 *  returns:
 *   - pointer (casted to int) of the original syscall
 *     or 0 on error
 *
 *  note:
 *   this also patches sceUtilitySavedataGetStatus!
 */
int hcDeemerDriverPatchSavedataInitStart(void);


/**
 *  setup callback triggered when savedata param struct
 *  has been captured
 *
 *  parameters:
 *   - CallbackID1: ID of the callbackthread to notify for SDInitStart
 *   - CallbackID2: ID of the callbackthread to notify for SDGetStatus
 */
void hcDeemerDriverSetupCallbackCapturedSDParams(int CallbackID1, int CallbackID2);
