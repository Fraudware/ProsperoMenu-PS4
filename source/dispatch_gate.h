#pragma once
#include <stdint.h>
#include <string.h>
static inline int pm_dispatch_global(uintptr_t hook,const unsigned char *code,uintptr_t *out) {
    const unsigned char prefix[]={0xf3,0x0f,0x1e,0xfa,0x51,0x48,0x8b,0x05};
    if (memcmp(code,prefix,sizeof prefix)) return 0;
    int32_t displacement;memcpy(&displacement,code+8,4);
    if (displacement>=0) {if (hook>UINTPTR_MAX-12-(uintptr_t)displacement)return 0;}
    else {if (hook+12<(uintptr_t)(-(int64_t)displacement))return 0;}
    *out=(uintptr_t)((int64_t)hook+12+displacement);return 1;
}
static inline int pm_dispatch_object(const uintptr_t *obj,uintptr_t entry,uintptr_t hook) {
    return (obj[0]==0xafafafaf00000000ULL || obj[0]==0 || obj[0]==0x800000000ULL || obj[0]==0x616e455f00000000ULL) && obj[1] && obj[2]==0x1e &&
           obj[3]==entry && obj[4]==hook;
}
