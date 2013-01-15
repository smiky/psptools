.set noreorder

#include "pspstub.s"

STUB_START "hcSGDeemer",0x40090000,0x00020005
STUB_FUNC 0x3229BCFE,hcDeemerDriverPatchSavedataInitStart
STUB_FUNC 0x86F611CE,hcDeemerDriverSetupCallbackCapturedSDParams
STUB_END
