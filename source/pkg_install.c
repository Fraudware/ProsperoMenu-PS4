/* GPL-3.0-or-later. Local .pkg install via AppInstUtil + Bgft, resolved from the
 * ShellUI sandbox mount. Non-destructive: no auto-uninstall, no delete-after.
 * The caller (marker worker) gates this behind a user confirmation dialog. */
#include "probe_common.h"
#include "pkg_init.h"
#include <orbis/libkernel.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

enum { BGFT_OPT_DISABLE_CDN=0x10000, BGFT_OPT_FORCE_UPDATE=0x8 };

struct bgft_download_param {
    int user_id, entitlement_type;
    const char *id,*content_url,*content_ex_url,*content_name,*icon_path,*sku_id;
    int option;
    const char *playgo_scenario_id,*release_date,*package_type,*package_sub_type;
    unsigned long package_size;
};
struct bgft_download_param_ex { struct bgft_download_param param; unsigned int slot; };
struct bgft_init_params { void *mem; unsigned long size; };

static int (*p_aiu_init)(void);
static int (*p_aiu_tid)(const char*,char*,int*);
static int (*p_bgft_init)(struct bgft_init_params*);
static int (*p_bgft_reg)(struct bgft_download_param_ex*,int*);
static int (*p_bgft_start)(int);
static int resolved, bgft_ready;

static int resolve(void){
    if(resolved) return (p_aiu_init&&p_aiu_tid&&p_bgft_init&&p_bgft_reg&&p_bgft_start)?0:-1;
    resolved=1;
    const char *w=sceKernelGetFsSandboxRandomWord(); if(!w||!w[0]) w="system";
    const char *words[3]={w,"system","system_ex"};
    char path[256]; int hb=-1,ha=-1,r=0;
    for(unsigned i=0;i<3 && hb<0;i++){snprintf(path,sizeof path,"/%s/common/lib/libSceBgft.sprx",words[i]);hb=sceKernelLoadStartModule(path,0,NULL,0,NULL,&r);}
    for(unsigned i=0;i<3 && ha<0;i++){snprintf(path,sizeof path,"/%s/common/lib/libSceAppInstUtil.sprx",words[i]);ha=sceKernelLoadStartModule(path,0,NULL,0,NULL,&r);}
    probe_log("PKG","RESOLVE bgft=%x appinst=%x",hb,ha);
    if(hb<0||ha<0) return -1;
    void *fn;
    fn=NULL; if(sceKernelDlsym(ha,"sceAppInstUtilInitialize",&fn)==0) p_aiu_init=(int(*)(void))fn;
    fn=NULL; if(sceKernelDlsym(ha,"sceAppInstUtilGetTitleIdFromPkg",&fn)==0) p_aiu_tid=(int(*)(const char*,char*,int*))fn;
    fn=NULL; if(sceKernelDlsym(hb,"sceBgftServiceIntInit",&fn)==0) p_bgft_init=(int(*)(struct bgft_init_params*))fn;
    fn=NULL; if(sceKernelDlsym(hb,"sceBgftServiceIntDownloadRegisterTaskByStorageEx",&fn)==0) p_bgft_reg=(int(*)(struct bgft_download_param_ex*,int*))fn;
    fn=NULL; if(sceKernelDlsym(hb,"sceBgftServiceIntDownloadStartTask",&fn)==0) p_bgft_start=(int(*)(int))fn;
    probe_log("PKG","SYMS init=%p tid=%p binit=%p reg=%p start=%p",(void*)p_aiu_init,(void*)p_aiu_tid,(void*)p_bgft_init,(void*)p_bgft_reg,(void*)p_bgft_start);
    return (p_aiu_init&&p_aiu_tid&&p_bgft_init&&p_bgft_reg&&p_bgft_start)?0:-1;
}

/* Returns 0 when the install task is queued; otherwise a nonzero firmware rc
 * (e.g. 0x80990088 == title already installed) or a negative resolve/alloc error.
 * A queued task is reported by the system's own install/notification UI. */
int pkg_install(const char *path){
    if(!path||!path[0]) return -1002;
    if(resolve()!=0) return -1000;
    int aiu_rc=p_aiu_init();
    if(aiu_rc!=0){probe_log("PKG","APPINST_INIT rc=%x",aiu_rc);return aiu_rc;}
    char tid[20]={0}; int is_app=0;
    int rt=p_aiu_tid(path,tid,&is_app);
    probe_log("PKG","TITLEID path=%.200s rc=%x tid=%.16s is_app=%d",path,rt,tid,is_app);
    if(rt!=0) return rt; /* not a readable/valid pkg at this path: abort before any write */
    if(!bgft_ready){
        void *mem=calloc(1,0x100000); if(!mem) return -1001;
        struct bgft_init_params ip={mem,0x100000};
        int ri=p_bgft_init(&ip);
        probe_log("PKG","BGFT_INIT rc=%x",ri);
        if(!bgft_init_usable(ri)){free(mem);return ri;}
        if(ri!=0)free(mem); /* An existing BGFT service does not adopt this buffer. */
        bgft_ready=1; /* BGFT retains the buffer only on a new init. */
    }
    struct bgft_download_param_ex params; memset(&params,0,sizeof params);
    params.param.entitlement_type=5;
    params.param.id="";
    params.param.content_url=path;
    params.param.content_name=strrchr(path,'/')+1;
    params.param.icon_path="";
    params.param.playgo_scenario_id="0";
    params.param.option=BGFT_OPT_DISABLE_CDN|BGFT_OPT_FORCE_UPDATE; /* no delete or uninstall */
    params.slot=0;
    int task=-1;
    int rr=p_bgft_reg(&params,&task);
    probe_log("PKG","REGISTER rc=%x task=%d",rr,task);
    if(rr!=0) return rr; /* e.g. 0x80990088 already installed: reported, never force-uninstalled */
    int rs=p_bgft_start(task);
    probe_log("PKG","START rc=%x task=%d",rs,task);
    return rs;
}
