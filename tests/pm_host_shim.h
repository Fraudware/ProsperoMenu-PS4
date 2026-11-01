#ifndef PM_HOST_SHIM_H
#define PM_HOST_SHIM_H
#include <stdint.h>
typedef struct { unsigned Size; char VersionString[32]; } OrbisKernelSwVersion;
int sceKernelDebugOutText(int, const char *);
uint64_t sceKernelGetProcessTime(void);
int sceKernelGetSystemSwVersion(OrbisKernelSwVersion *);
int sceKernelIsNeoMode(void);
#endif
