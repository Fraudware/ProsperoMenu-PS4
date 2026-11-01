/* Console telemetry and explicitly requested fan threshold setter. GPL-3.0-or-later. */
#include "probe_common.h"
#include "storage_values.h"
#include <stdatomic.h>
#include <orbis/libkernel.h>
#include <stdio.h>
#include <string.h>
#include <Syscall.h>
#include <sys/syscall.h>

#ifndef SYS_clock_gettime
#define SYS_clock_gettime 232
#endif
#define PM_CLOCK_MONOTONIC 4

/* temps are linked directly against libkernel (exported by the toolchain) */
extern int sceKernelGetCpuTemperature(int*);
extern int sceKernelGetSocSensorTemperature(int,int*);
static int (*p_netctl_init)(void);
static int (*p_netctl_getinfo)(int,void*);
static int resolved;

static void resolve_once(void){
    if(resolved)return; resolved=1;
    int r=0;
    int nh=sceKernelLoadStartModule("libSceNetCtl.sprx",0,NULL,0,NULL,&r);
    if(nh>=0){void*fn=NULL;
        if(sceKernelDlsym(nh,"sceNetCtlInit",&fn)==0)p_netctl_init=(int(*)(void))fn;
        fn=NULL; if(sceKernelDlsym(nh,"sceNetCtlGetInfo",&fn)==0)p_netctl_getinfo=(int(*)(int,void*))fn;
    }
    if(p_netctl_init)p_netctl_init();
    probe_log("TOOLS","RESOLVE netctl_getinfo=%p",(void*)p_netctl_getinfo);
}

/* CPU temperature in Celsius; returns 0 on success */
int tools_cpu_temp_c(int*out){
    int t=0; int rc=sceKernelGetCpuTemperature(&t);
    if(rc)return -2;
    *out=t; return 0;
}
/* SoC sensor 0 temperature in Celsius; returns 0 on success */
int tools_soc_temp_c(int*out){
    int t=0; int rc=sceKernelGetSocSensorTemperature(0,&t);
    if(rc)return -2;
    *out=t; return 0;
}

/* IPv4 address string (code 14); returns 0 on success */
int tools_ip(char*out,size_t cap){
    resolve_once();
    if(!p_netctl_getinfo)return -1;
    /* SceNetCtlInfo union is large; use a generous buffer */
    unsigned char info[544] __attribute__((aligned(16)))={0};
    int rc=p_netctl_getinfo(14,info); /* SCE_NET_CTL_INFO_IP_ADDRESS */
    if(rc)return -2;
    snprintf(out,cap,"%.15s",(const char*)info);
    return 0;
}

/* System uptime (monotonic) formatted hh:mm:ss; returns 0 on success */
int tools_uptime(char*out,size_t cap){
    long ts[2]={0,0}; /* {tv_sec, tv_nsec} */
    int rc=orbis_syscall(SYS_clock_gettime,PM_CLOCK_MONOTONIC,ts);
    if(rc)return -1;
    unsigned long long s=(unsigned long long)ts[0];
    snprintf(out,cap,"%llud %02lluh %02llum",s/86400,(s%86400)/3600,(s%3600)/60);
    return 0;
}

/* Free/total for a mount path; returns 0 on success */
int tools_storage(const char*path,char*out,size_t cap){
    unsigned char raw[4096] __attribute__((aligned(16)))={0};
    int rc=orbis_syscall(SYS_statfs,path,raw);
    struct storage_values v={0};
    if(rc==0 && storage_decode(raw,sizeof(raw),&v)==0){
        uint64_t g=1ULL<<30;
        snprintf(out,cap,"%llu.%llu/%llu.%llu GiB free",
            (unsigned long long)(v.available/g),(unsigned long long)((v.available%g)*10/g),
            (unsigned long long)(v.total/g),(unsigned long long)((v.total%g)*10/g));
        return 0;
    }
    snprintf(out,cap,"unavailable (0x%x)",rc);
    return -1;
}

/* One-shot telemetry snapshot to klog, for verification. */
void tools_log_snapshot(void){
    int temp=-1; char ip[32]="n/a",up[48]="n/a",st[64]="n/a";
    int soc=-1;
    int tr=tools_cpu_temp_c(&temp); int sr=tools_soc_temp_c(&soc);
    tools_ip(ip,sizeof ip); tools_uptime(up,sizeof up); tools_storage("/user",st,sizeof st);
    probe_log("TOOLS","SNAPSHOT cpu_rc=%d cpu=%dC soc_rc=%d soc=%dC ip=%s uptime=%s user_storage=%s",tr,temp,sr,soc,ip,up,st);
}

/* ICC fan profile. GET is 0xC01C8F08, SET is 0xC01C8F07.
 * Refuse writes until GET returns a plausible current threshold. */
#include "fan_profile.h"
#define ICC_FAN_PATH "/dev/icc_fan"
#define ICC_FAN_GET 0xC01C8F08u
#define ICC_FAN_SET 0xC01C8F07u
#ifndef SYS_ioctl
#define SYS_ioctl 54
#endif
int fan_get_threshold(int*out){
    unsigned char d[28]={0};
    int fd=sceKernelOpen(ICC_FAN_PATH,0,0);if(fd<0)return -1;
    int rc=orbis_syscall(SYS_ioctl,fd,(long)ICC_FAN_GET,d);sceKernelClose(fd);
    if(rc || !fan_profile_valid(d)){
        probe_log("FAN","GET invalid rc=%d threshold=%u",rc,(unsigned)d[5]);return -2;
    }
    *out=(int)d[5];return 0;
}
int fan_set_threshold(int target){
    unsigned char d[28]={0},check[28]={0};
    if(target<40 || target>79)return -3;
    int fd=sceKernelOpen(ICC_FAN_PATH,0,0);if(fd<0)return -1;
    int rc=orbis_syscall(SYS_ioctl,fd,(long)ICC_FAN_GET,d);
    int old=d[5];
    if(rc || fan_profile_patch(d,target)!=0){sceKernelClose(fd);probe_log("FAN","REFUSED invalid current=%d get_rc=%d",old,rc);return -4;}
    rc=orbis_syscall(SYS_ioctl,fd,(long)ICC_FAN_SET,d);
    int verify=rc?rc:orbis_syscall(SYS_ioctl,fd,(long)ICC_FAN_GET,check);
    sceKernelClose(fd);
    probe_log("FAN","WRITE old=%d target=%d set_rc=%d get_rc=%d verified=%u",old,target,rc,verify,(unsigned)check[5]);
    return rc || verify || check[5]!=(unsigned)target?-5:0;
}
