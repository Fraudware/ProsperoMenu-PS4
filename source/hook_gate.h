#pragma once
/* Never publish to a file-backed AOT mapping merely because mprotect succeeds.
 * A verified private 16 KiB remapping procedure has not been established on PS4.
 * This gate deliberately has no READY path until that procedure is proven. */
#include <stdint.h>
#include <string.h>
enum pm_hook_result { PM_HOOK_READY=0, PM_HOOK_BAD_ENTRY=1, PM_HOOK_BAD_ABI=2,
                      PM_HOOK_NO_PRIVATE_ROUTE=3 };
struct pm_hook_plan { uintptr_t entry, prior; unsigned char original[16]; int returns_bool; };
static inline int pm_hook_abi_valid(unsigned params, int instance, unsigned flags, int type) {
    return params==0 && instance && !(flags&0x10) && (type==1 || type==2);
}
static inline enum pm_hook_result pm_hook_plan_entry(struct pm_hook_plan *p,
        uintptr_t entry, const unsigned char bytes[16], unsigned params,
        int instance, int return_type) {
    if ((entry&15) || ((entry&0x3fff)>0x3ff0) ||
        bytes[0]!=0xff || bytes[1]!=0x25 ||
        bytes[2] || bytes[3] || bytes[4] || bytes[5]) return PM_HOOK_BAD_ENTRY;
    if (!pm_hook_abi_valid(params,instance,0,return_type)) return PM_HOOK_BAD_ABI;
    memcpy(p->original,bytes,16); p->entry=entry;
    memcpy(&p->prior,bytes+6,sizeof p->prior); p->returns_bool=return_type==2;
    if (!p->prior || p->prior==entry) return PM_HOOK_BAD_ENTRY;
    /* No verified process-private page publication: DO NOT write entry or +6.
     * An aligned cmpxchg16b could atomically swap this 16-byte window only AFTER
     * a process-private executable page and safe rollback are demonstrated. */
    return PM_HOOK_NO_PRIVATE_ROUTE;
}
