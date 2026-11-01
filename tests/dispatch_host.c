#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dispatch_gate.h"
int main(void) {
    unsigned char hook[32]={0xf3,0x0f,0x1e,0xfa,0x51,0x48,0x8b,0x05};
    int32_t disp=0x40; memcpy(hook+8,&disp,4);
    uintptr_t global=0;
    assert(pm_dispatch_global((uintptr_t)hook,hook,&global));
    assert(global==(uintptr_t)hook+12+0x40);
    hook[5]=0;assert(!pm_dispatch_global((uintptr_t)hook,hook,&global));
    uintptr_t object[6]={0xafafafaf00000000ULL,0x20f2d8000ULL,0x1e,0x840032180ULL,0x9a000f0c4ULL,0};
    assert(pm_dispatch_object(object,object[3],object[4]));
    object[0]=0;assert(pm_dispatch_object(object,object[3],object[4]));
    object[0]=0x800000000ULL;assert(pm_dispatch_object(object,object[3],object[4]));
    object[0]=0x616e455f00000000ULL;assert(pm_dispatch_object(object,object[3],object[4]));
    object[2]=0x20;assert(!pm_dispatch_object(object,object[3],object[4]));object[2]=0x1e;
    object[0]=0x1234;assert(!pm_dispatch_object(object,object[3],object[4]));
    object[0]=0;object[3]++;assert(!pm_dispatch_object(object,0x840032180ULL,object[4]));
    puts("dispatch pattern and object identity: PASS");
}
