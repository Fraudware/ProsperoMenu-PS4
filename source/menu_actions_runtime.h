#pragma once
#include "menu_actions.h"
#include "menu_themes.h"
#include "fan_display.h"
#include "catalog_generated.h"
#include "files_native.h"
#include "pkg_path.h"
#include "fb_page.h"
#include "plugins_native.h"
#include <string.h>
#include <orbis/SystemService.h>
#include <orbis/ShellUIUtil.h>
#include <Syscall.h>
extern int pm_shellui_page(const char*,const char*,const char*const*,const char*const*,unsigned);
extern int pm_shellui_page_metrics(const char*,const char*,const char*,const char*const*,const char*const*,unsigned);
extern int pm_shellui_home(const char*,const char*);
extern int pm_shellui_home_metrics(const char*,const char*);
extern int tools_cpu_temp_c(int*);
extern int tools_soc_temp_c(int*);
extern int tools_ip(char*,size_t);
extern int tools_uptime(char*,size_t);
extern int tools_storage(const char*,char*,size_t);
extern void tools_log_snapshot(void);
extern int fan_get_threshold(int*);
extern int fan_set_threshold(int);
extern int pm_shellui_set_metrics(const char*);
extern int pkg_install(const char*);
static char install_path[1024];
extern int pm_focus_owned(void);
extern void pm_shellui_home_extra(int);
static struct action_menu actions;
#define FB_MAX 1024
static char fb_path[1024]="/";
static struct fb_entry fb_entries[FB_MAX];
static int fb_count;
static unsigned fb_offset;
static void fb_refresh(void){fb_count=files_list(fb_path,fb_entries,FB_MAX);if(fb_count<0)fb_count=0;}
static void fb_select(unsigned row){
    if((int)row<fb_count && fb_entries[row].is_dir){
        char np[1024];
        if(fb_path[0]=='/'&&fb_path[1]==0)snprintf(np,sizeof np,"/%s",fb_entries[row].name);
        else snprintf(np,sizeof np,"%s/%s",fb_path,fb_entries[row].name);
        snprintf(fb_path,sizeof fb_path,"%s",np);fb_offset=0;fb_refresh();
    }
}
static int fb_up(void){
    if(fb_path[0]=='/'&&fb_path[1]==0)return 0;
    size_t l=strlen(fb_path); if(l>1&&fb_path[l-1]=='/')fb_path[--l]=0;
    char*s=strrchr(fb_path,'/'); if(!s||s==fb_path){fb_path[0]='/';fb_path[1]=0;}else *s=0;
    fb_offset=0;fb_refresh(); return 1;
}
static struct pi_entry pi_entries[PI_MAX];
static int pi_count;
static void plugins_reload(void){pi_count=plugins_load(pi_entries,PI_MAX);if(pi_count<0)pi_count=0;}
#define CHEAT_MAX 48
static struct fb_entry cheat_entries[CHEAT_MAX]; static int cheat_count;
static void cheats_refresh(void){
    cheat_count=0;
    static const char*dirs[]={"/user/data/GoldHEN/cheats/json","/user/data/GoldHEN/cheats/shn","/user/data/GoldHEN/cheats/mc4"};
    for(unsigned d=0;d<3;d++){
        struct fb_entry tmp[CHEAT_MAX]; int n=files_list(dirs[d],tmp,CHEAT_MAX);
        for(int i=0;i<n && cheat_count<CHEAT_MAX;i++) if(!tmp[i].is_dir) cheat_entries[cheat_count++]=tmp[i];
    }
}
static int page_dirty=1;
static atomic_int fan_request=-1,fan_selected=-1,fan_result=-1,fan_initial=-1,fan_current=-1,fan_cpu=-1,fan_soc=-1,fan_auto=0,fan_choice=-1;
static char home_metrics[192],home_heading[64];
static void home_metrics_refresh(void){
    /* The home page omits console metrics. */
    home_heading[0]=0; home_metrics[0]=0;
}
static atomic_int page_snapshot,page_request=-1,action_pending,action_ready,last_action,last_result;
static unsigned request_game;
static int (*launch_uri)(const char*,OrbisShellUIUtilLaunchByUriParam*);
static int (*initialize_uri)(void);
static int (*launch_app)(const char*,const char*[],LncAppParam*);
static void resolve_action_apis(void){
    launch_app=sceLncUtilLaunchApp;
    char random_path[256];const char *random_word=sceKernelGetFsSandboxRandomWord();
    if(!random_word)random_word="";
    snprintf(random_path,sizeof(random_path),"/%s/common/lib/libSceShellUIUtil.sprx",random_word);
    const char *paths[]={random_path,"/system/common/lib/libSceShellUIUtil.sprx","/system_ex/common/lib/libSceShellUIUtil.sprx"};
    for(unsigned i=0;i<3 && !launch_uri;i++){
        int result=0;int h=sceKernelLoadStartModule(paths[i],0,NULL,0,NULL,&result);
        probe_log("ACTION","SETTINGS_LIBRARY path=%s handle=%x start=%x",paths[i],h,result);
        if(h>=0){void *fn=NULL;
            if(sceKernelDlsym(h,"sceShellUIUtilLaunchByUri",&fn)==0)launch_uri=(int(*)(const char*,OrbisShellUIUtilLaunchByUriParam*))fn;
            fn=NULL;if(sceKernelDlsym(h,"sceShellUIUtilInitialize",&fn)==0)initialize_uri=(int(*)(void))fn;
        }
    }
    probe_log("ACTION","API_LINKED launch_uri=%p initialize_uri=%p launch_app=%p",(void*)launch_uri,(void*)initialize_uri,(void*)launch_app);
}
static atomic_ullong actions_dispatched;
static atomic_uint rows_snapshot;
static const char *request_name(enum action_request r){
    switch(r){case REQ_SETTINGS:return "GoldHEN settings";case REQ_PS4_THEMES:return "PS4 Themes";case REQ_LAUNCH:return "Launch app";case REQ_REBOOT:return "Reboot PS4";case REQ_SHUTDOWN:return "Shutdown PS4";case REQ_INSTALL:return "Install package";default:return "None";}
}
static void fan_metrics_string(char *out,size_t cap){
    int initial=atomic_load(&fan_initial);
    if(initial<40)snprintf(out,cap,"ICC profile invalid - AC power reset required");
    else fan_display(out,cap,atomic_load(&fan_cpu),atomic_load(&fan_soc),atomic_load(&fan_current),initial,atomic_load(&fan_result));
}
static int action_draw(void){
    if(actions.page==PAGE_HOME){pm_shellui_home_extra((int)actions.cheats);home_metrics_refresh();return pm_shellui_home(home_heading,home_metrics);}
    const char *labels[32]={0},*values[32]={0};unsigned count=0;
    char subtitle[160],fan_metrics[160]={0};const char *title="ProsperoMenu";
    snprintf(subtitle,sizeof(subtitle),"Cross: Select | Circle: Back / Close");
    if(actions.page==PAGE_GAMES){
        title="Installed Games & Apps";
        snprintf(subtitle,sizeof(subtitle),"%u installed | catalog captured at deployment",(unsigned)CATALOG_COUNT);
        for(unsigned i=actions.offset;i<CATALOG_COUNT && i<actions.offset+20;i++){
            labels[count]=catalog[i].title;values[count++]=catalog[i].id;
        }
        if(actions.offset+20<CATALOG_COUNT)labels[count++]="Next page";
    }else if(actions.page==PAGE_TOOLS){
        title="Console Tools";labels[count++]="Console information";
        labels[count++]="Configured plugins (read only)";
        labels[count++]="GoldHEN settings";
    }else if(actions.page==PAGE_THEMES){
        title="Themes";
        snprintf(subtitle,sizeof(subtitle),"Prospero colors or the PS4's system themes");
        labels[count++]="Prospero Themes";
        labels[count++]="PS4 Themes (system settings)";
    }else if(actions.page==PAGE_POWER){
        title="Power";
        snprintf(subtitle,sizeof(subtitle),"Jailbreak and resident menu end on power action");
        labels[count++]="Reboot PS4";labels[count++]="Shutdown PS4";
    }else if(actions.page==PAGE_PROSPERO_THEMES){
        title="Prospero Themes";
        snprintf(subtitle,sizeof(subtitle),"Choose an overlay palette | applies immediately");
        for(unsigned i=0;i<PM_THEME_COUNT;i++){
            labels[count]=pm_theme_presets[i].name;
            values[count++]=i==actions.theme_index?"Current":NULL;
        }
    }else if(actions.page==PAGE_PLUGINS){
        title="Plugins";
        snprintf(subtitle,sizeof(subtitle),"Cross toggles ON/OFF | applies on next game launch");
        if(pi_count==0)labels[count++]="(no plugins in plugins.ini)";
        for(int i=0;i<pi_count && count<30;i++){labels[count]=pi_entries[i].label;values[count++]=pi_entries[i].enabled?"ON":"OFF";}
    }else if(actions.page==PAGE_FILES){
        title="File Browser";
        snprintf(subtitle,sizeof(subtitle),"%.110s | %u-%u / %d%s",fb_path,
                 fb_count?(unsigned)fb_offset+1:0,
                 fb_count?(unsigned)fb_offset+FB_PAGE_SIZE<(unsigned)fb_count?fb_offset+FB_PAGE_SIZE:(unsigned)fb_count:0,
                 fb_count,fb_count==FB_MAX?" (listing limit)":"");
        if(fb_count==0)labels[count++]="(empty or unreadable)";
        else for(unsigned row=0;row<fb_page_rows((unsigned)fb_count,fb_offset);row++){
            int entry=fb_page_target((unsigned)fb_count,fb_offset,row);
            if(entry==FB_PREV)labels[count++]="Previous page";
            else if(entry==FB_NEXT)labels[count++]="Next page";
            else if(entry>=0){labels[count]=fb_entries[entry].name;values[count++]=fb_entries[entry].is_dir?"<dir>":NULL;}
        }
    }else if(actions.page==PAGE_CHEATS){
        title="Cheats";
        snprintf(subtitle,sizeof(subtitle),"Long-press SHARE in-game for GoldHEN's cheat menu");
        if(cheat_count==0)labels[count++]="(no cheat files in GoldHEN/cheats)";
        for(int i=0;i<cheat_count && count<30;i++)labels[count++]=cheat_entries[i].name;
    }else if(actions.page==PAGE_FAN){
        title="Fan Control";
        int au=atomic_load(&fan_auto),fc=atomic_load(&fan_choice);
        snprintf(subtitle,sizeof subtitle,"Fan-on temp = ramp-up trigger; Auto adjusts trigger");
        fan_metrics_string(fan_metrics,sizeof fan_metrics);
        labels[count]="Auto ~65 C (experimental)";values[count++]=au?"On":NULL;
        labels[count]="Default";values[count++]=fc==1?"On":NULL;
        labels[count]="Low (fan on at 75 C)";values[count++]=fc==2?"On":NULL;
        labels[count]="Medium (fan on at 55 C)";values[count++]=fc==3?"On":NULL;
        labels[count]="High (fan on at 40 C)";values[count++]=fc==4?"On":NULL;
    }else if(actions.page==PAGE_CONFIRM){
        title="Confirm Action";
        if(actions.confirm==REQ_LAUNCH && actions.game_index<CATALOG_COUNT)
            snprintf(subtitle,sizeof(subtitle),"Launch %.125s?",catalog[actions.game_index].title);
        else if(actions.confirm==REQ_INSTALL)snprintf(subtitle,sizeof(subtitle),"Install/update (may replace existing title)? %.90s",install_path);
        else snprintf(subtitle,sizeof(subtitle),"%s? Jailbreak and resident menu will be lost.",request_name(actions.confirm));
        labels[count++]="Cancel (safe default)";labels[count++]=request_name(actions.confirm);
    }
    return actions.page==PAGE_FAN?pm_shellui_page_metrics(title,subtitle,fan_metrics,labels,values,count):pm_shellui_page(title,subtitle,labels,values,count);
}
static int schedule_action(enum action_request request){
    if(request==REQ_CLOSE)return 1;
    if(request==REQ_NONE)return 0;
    if(atomic_load(&action_pending)!=REQ_NONE)return 0;
    request_game=actions.game_index;
    atomic_store_explicit(&action_pending,request,memory_order_release);
    probe_log("ACTION","CONFIRMED request=%s game_index=%u",request_name(request),request_game);
    return 1;
}
static void execute_pending_action(void){
    if(!atomic_load_explicit(&action_ready,memory_order_acquire))return;
    enum action_request request=atomic_load_explicit(&action_pending,memory_order_acquire);
    if(request==REQ_NONE)return;
    int rc=-1;
    if(request==REQ_SETTINGS || request==REQ_PS4_THEMES){
        int user=-1;rc=sceUserServiceGetForegroundUser(&user);
        if(rc==0 && launch_uri){
            OrbisShellUIUtilLaunchByUriParam param={.size=sizeof(param),.userId=(uint32_t)user};
            if(initialize_uri)initialize_uri();
            if(request==REQ_PS4_THEMES){
                /* Native PS4 theme selector; if this firmware rejects the deep link,
                   open Sony's Settings so the user can choose Themes there. */
                rc=launch_uri("pssettings:play?mode=settings&function=theme",&param);
                probe_log("ACTION","PS4_THEME_DEEPLINK rc=%x",rc);
                if(rc!=0)rc=launch_uri("pssettings:play?mode=settings",&param);
            }else rc=launch_uri("pssettings:play?mode=settings&function=goldhen_menu",&param);
        }else if(rc==0)rc=-38;
    }else if(request==REQ_LAUNCH && request_game<CATALOG_COUNT){
        int user=-1;rc=sceUserServiceGetForegroundUser(&user);
        if(rc==0 && launch_app){
            LncAppParam param={.size=sizeof(param),.user_id=(uint32_t)user,.app_opt=0,.crash_report=0,.LaunchAppCheck_flag=LaunchApp_None};
            const char *argv[]={NULL};rc=launch_app(catalog[request_game].id,argv,&param);
        }else if(rc==0)rc=-38;
    }else if(request==REQ_INSTALL){
        rc=pkg_install(install_path);
        if(rc==0)probe_notify("ProsperoMenu: package install task queued; check PS4 Notifications.");
        else{char notice[112];snprintf(notice,sizeof notice,"ProsperoMenu: package install failed (0x%08x)",(unsigned)rc);probe_notify(notice);}
    }else if(request==REQ_REBOOT){
        probe_log("ACTION","POWER_REQUEST reboot user_confirmed=1 input_released=1");
        sceKernelReboot();rc=0;
    }else if(request==REQ_SHUTDOWN){
        /* Same orderly shutdown sequence as the pinned PS4 payload SDK. */
        void *event=NULL;rc=sceKernelOpenEventFlag(&event,"SceSysCoreReboot");
        if(rc==0){
            rc=sceKernelSetEventFlag(event,0x4000);sceKernelCloseEventFlag(event);
            if(rc==0){probe_log("ACTION","POWER_REQUEST shutdown user_confirmed=1 input_released=1");rc=orbis_syscall(37,1,30);}
        }
    }
    atomic_store(&last_action,request);atomic_store(&last_result,rc);
    atomic_fetch_add(&actions_dispatched,1);
    probe_log("ACTION","API_RETURN action=%s rc=%x (return is not visual acceptance)",request_name(request),rc);
    atomic_store(&action_ready,0);atomic_store(&action_pending,REQ_NONE);
}
