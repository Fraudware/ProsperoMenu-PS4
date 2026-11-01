#include <assert.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdio.h>
#include "hook_gate.h"
int main(void) {
    alignas(16) unsigned char jump[16] = {0xff,0x25,0,0,0,0,0xc4,0xf0,0x00,0x9a,0,0,0,0,0x90,0x90};
    struct pm_hook_plan plan;
    assert(pm_hook_plan_entry(&plan, (uintptr_t)jump, jump, 0, 1, 1) == PM_HOOK_NO_PRIVATE_ROUTE);
    assert(pm_hook_plan_entry(&plan, (uintptr_t)jump, jump, 0, 1, 1) == PM_HOOK_NO_PRIVATE_ROUTE);
    assert(pm_hook_plan_entry(&plan, (uintptr_t)jump+1, jump, 0, 1, 1) != PM_HOOK_READY);
    jump[0]=0x90;
    assert(pm_hook_plan_entry(&plan, (uintptr_t)jump, jump, 0, 1, 1) != PM_HOOK_READY);
    assert(pm_hook_abi_valid(0,1,0,1));
    assert(pm_hook_abi_valid(0,1,0,2));
    assert(!pm_hook_abi_valid(1,1,0,1));
    assert(!pm_hook_abi_valid(0,1,0x10,1));
    assert(!pm_hook_abi_valid(0,1,0,8));
    puts("hook gate: fail-closed and ABI checks passed");
    return 0;
}
