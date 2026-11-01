/* GPL-3.0. PS4 13.52 ShellUI integrated UI-thread hook candidate.
 * Private-stub UI bridge with native controller polling and balanced PUI capture. */
#include "probe_common.h"
#include "hook_gate.h"
#include "dispatch_gate.h"
#include <orbis/libkernel.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include "menu_input.h"
extern void *mono_get_root_domain(void);
extern void *mono_thread_attach(void *);
extern void *mono_image_loaded(const char *);
extern void *mono_class_from_name(void *,const char *,const char *);
extern void *mono_class_get_method_from_name(void *,const char *,int);
extern void *mono_compile_method(void *);
extern void *mono_method_signature(void *);
extern unsigned mono_signature_get_param_count(void *);
extern unsigned mono_method_get_flags(void *,unsigned *);
extern void *mono_signature_get_return_type(void *);
extern int mono_type_get_type(void *);
extern int pm_shellui_init(void);
extern int pm_shellui_set_visible(int);
extern void pm_shellui_tick(void);
extern void pm_shellui_term(void);
static OrbisPthread worker;
static atomic_int started, stopping, hook_published;
static int worker_created;
static unsigned long long seq;
static struct pm_hook_plan plan;
static uintptr_t original_stub;
static const char *reason="not_scanned";
static char detour_fields[192]="none";
static atomic_ullong ui_ticks;
static atomic_int callback_busy, renderer_state, desired_visible=1, cleanup_requested;
extern void pm_shellui_select(unsigned);
extern unsigned pm_shellui_rows(void);
extern int pm_shellui_capture_input(int);
extern int pm_shellui_input_captured(void);
static struct menu_input menu={.visible=1};
static int pad_handle=-1,owned_pad=-1;static uint64_t retry_pad_at;
static atomic_int pad_status=-1,pad_handle_status=-1,capture_status,selected_status,visible_request=-1;
static atomic_uint buttons_status;
static atomic_ullong input_events;
static int read_buttons(uint32_t *buttons){
    uint64_t now=sceKernelGetProcessTime()/1000;
    if(pad_handle<0 && now>=retry_pad_at){
        int user=-1;int rc=sceUserServiceGetForegroundUser(&user);
        if(rc!=0)rc=sceUserServiceGetInitialUser(&user);
        if(rc==0){
            const int users[]={user,-1,0x10000000};
            const unsigned types[]={0,1,2,3};
            for(unsigned u=0;u<3 && pad_handle<0;++u)
                for(unsigned t=0;t<4 && pad_handle<0;++t){
                    int h=scePadGetHandle(users[u],types[t],0);
                    if(h>=0){pad_handle=h;probe_log("INPUT","PAD_FOUND user=%x type=%u handle=%d",users[u],types[t],h);}
                }
        }
        if(rc==0 && pad_handle<0){
            int h=scePadOpen(user,0,0,NULL);
            probe_log("INPUT","PAD_OPEN user=%x user_rc=%x result=%x",user,rc,h);
            if(h>=0){pad_handle=h;owned_pad=h;}
        }else if(rc!=0)probe_log("INPUT","USER_FAILED result=%x user=%x",rc,user);
        retry_pad_at=now+2000;atomic_store(&pad_handle_status,pad_handle);
    }
    OrbisPadData data={0};
    int rc=pad_handle>=0?scePadReadState(pad_handle,&data):-1;
    atomic_store(&pad_status,rc);
    if(rc!=0 || !data.connected){if(rc!=0){if(owned_pad==pad_handle){scePadClose(owned_pad);owned_pad=-1;}pad_handle=-1;}*buttons=0;return 0;}
    *buttons=data.buttons;return 1;
}
#include "menu_actions_runtime.h"
extern int pm_focus_start(void),pm_focus_state(void),pm_focus_owned(void),pm_focus_rc(void);
extern int pm_shellui_set_theme(unsigned);
static atomic_int theme_to_save=-1,theme_save_result;
static unsigned applied_theme=~0u;
static const char *theme_path="/user/data/GoldHEN/pm_theme.txt";
static void load_theme(void){
    const char *paths[]={"/user/data/GoldHEN/pm_theme.txt","/data/GoldHEN/pm_theme.txt"};
    for(unsigned i=0;i<2;i++){
        int fd=sceKernelOpen(paths[i],0,0);
        if(fd<0)continue;
        char text[4]={0};int n=sceKernelRead(fd,text,3);sceKernelClose(fd);
        if(n>0){actions.theme_index=pm_theme_parse(text,(unsigned)n);theme_path=paths[i];
            probe_log("THEME","LOADED index=%u path=%s",actions.theme_index,theme_path);return;}
    }
    probe_log("THEME","DEFAULT index=0");
}
static void save_theme_if_requested(void){
    int next=atomic_exchange(&theme_to_save,-1);
    if(next<0)return;
    char text[4];snprintf(text,sizeof(text),"%d\n",next);
    int result=probe_write_text(theme_path,text);
    if(!result && theme_path[0]=='/' && theme_path[1]=='u'){
        theme_path="/data/GoldHEN/pm_theme.txt";
        result=probe_write_text(theme_path,text);
    }
    atomic_store(&theme_save_result,result?0:-1);
    probe_log("THEME","SAVE index=%d result=%d path=%s",next,result,theme_path);
}
extern void pm_focus_stop_when_released(void);
static uintptr_t jump_slot;
__attribute__((visibility("hidden"))) uintptr_t pm_ui_continue;
extern void pm_ui_bridge(void);
extern int pm_shellui_state(void);
struct wordpair { uint64_t lo,hi; };
static struct wordpair original_jump, installed_jump;
static int cas_jump(struct wordpair expect,struct wordpair next){
    unsigned char yes;
    __asm__ volatile("lock cmpxchg16b %1; sete %0" : "=q"(yes), "+m"(*(volatile struct wordpair*)jump_slot), "+a"(expect.lo), "+d"(expect.hi) : "b"(next.lo), "c"(next.hi) : "cc","memory");
    return yes;
}
__attribute__((visibility("hidden"))) void pm_ui_frame(void){
    atomic_fetch_add(&ui_ticks,1);
    if(atomic_exchange(&callback_busy,1))return;
    if(atomic_load(&cleanup_requested)){
        pm_shellui_capture_input(0);pm_shellui_term();
        if(pm_shellui_state()==0){
            pm_focus_stop_when_released();
            if(owned_pad>=0){scePadClose(owned_pad);owned_pad=-1;pad_handle=-1;}
            if(cas_jump(installed_jump,original_jump)){atomic_store(&hook_published,0);atomic_store(&renderer_state,0);}
        }
    }else{
        int state=pm_shellui_state();
        if(state==0 && pm_shellui_init()!=0)state=-1;
        if(state>=0 && applied_theme!=actions.theme_index){
            if(pm_shellui_set_theme(actions.theme_index)==0)applied_theme=actions.theme_index;
        }
        uint32_t buttons=0;int connected=read_buttons(&buttons);
        actions.cheats=(unsigned)pm_focus_owned();
        unsigned prev_row=menu.row,prev_visible=menu.visible,prev_theme=actions.theme_index;
        uint32_t pressed=buttons&~menu.previous;
        enum action_page previous_page=actions.page;unsigned previous_offset=actions.offset;
        uint32_t navigation_buttons=buttons;
        if(menu.visible && (pressed&0x2000)){
            if(actions.page==PAGE_FILES){if(!fb_up())actions.page=PAGE_HOME;page_dirty=1;}
            else if(actions.page!=PAGE_HOME)action_back(&actions);
            else{menu.visible=0;menu.drain=1;}
            navigation_buttons&=~0x2000u;
        }
        menu_input_step(&menu,navigation_buttons,connected,sceKernelGetProcessTime()/1000,pm_shellui_rows());
        menu.previous=buttons;
        int requested_page=atomic_exchange(&page_request,-1);
        if(requested_page==PAGE_HOME || requested_page==PAGE_GAMES){actions.page=requested_page;actions.offset=0;menu.visible=1;page_dirty=1;}
        if(menu.visible && state==2 && pm_focus_state()==1 && (pressed&0x4000) && !(buttons&6)){
            if(actions.page==PAGE_FILES){int fr=fb_page_target((unsigned)fb_count,fb_offset,menu.row);
                if(fr==FB_NEXT){fb_offset+=FB_PAGE_SIZE;menu.row=0;}
                else if(fr==FB_PREV){fb_offset-=FB_PAGE_SIZE;menu.row=0;}
                else if(fr>=0){
                    if(!fb_entries[fr].is_dir && pkg_candidate(fb_path,fb_entries[fr].name,install_path,sizeof install_path)){
                        actions.confirm=REQ_INSTALL;actions.return_page=PAGE_FILES;actions.page=PAGE_CONFIRM;
                    }else fb_select((unsigned)fr);
                }
                page_dirty=1;}
            else if(actions.page==PAGE_PLUGINS){if((int)menu.row<pi_count){if(plugins_toggle((int)menu.row)==0)plugins_reload();}page_dirty=1;}
            else if(actions.page==PAGE_FAN){unsigned r=menu.row;static const int preset[]={75,55,40};if(r==0)atomic_store(&fan_request,-3);else if(r==1)atomic_store(&fan_request,-2);else if(r<=4){atomic_store(&fan_request,preset[r-2]);}page_dirty=1;}
            else{enum action_request request=action_cross(&actions,menu.row,CATALOG_COUNT);
                if(schedule_action(request)){menu.visible=0;menu.drain=1;}}
        }
        if(actions.theme_index!=prev_theme){
            atomic_store(&theme_to_save,(int)actions.theme_index);page_dirty=1;
            probe_log("THEME","SELECT index=%u",actions.theme_index);
        }
        if(actions.page!=previous_page || actions.offset!=previous_offset){menu.row=0;page_dirty=1;}
        if(actions.page!=previous_page){
            if(fb_reset_for_entry(previous_page,actions.page)){
                snprintf(fb_path,sizeof fb_path,"/");fb_offset=0;fb_refresh();
            }
            else if(actions.page==PAGE_PLUGINS)plugins_reload();
            else if(actions.page==PAGE_CHEATS)cheats_refresh();
        }
        atomic_store(&page_snapshot,actions.page);
        int request=atomic_exchange(&visible_request,-1);
        if(request>=0){menu.visible=(unsigned)request;if(!request && buttons)menu.drain=1;}
        if(prev_row!=menu.row || prev_visible!=menu.visible){
            atomic_fetch_add(&input_events,1);
            probe_log("INPUT","visible=%u row=%u buttons=%x connected=%d",menu.visible,menu.row,buttons,connected);
        }
        if(state>=0){
            int want_capture=menu.visible || menu.drain;
            int capture_result=pm_shellui_capture_input(want_capture);
            if(capture_result<0){
                menu.visible=0;menu.drain=0;pm_shellui_set_visible(0);pm_shellui_capture_input(0);
            }else{
                int actual_visible=menu.visible && capture_result==0;
                pm_shellui_set_visible(actual_visible);
                pm_shellui_select(menu.row);pm_shellui_tick();state=pm_shellui_state();
                static int last_fan_value=-1,last_fan_rc=-1,last_fan_current=-1,last_fan_cpu=-1,last_fan_soc=-1,last_fan_auto=-1;
                if(actions.page==PAGE_FAN){
                    int fv=atomic_load(&fan_selected),fr=atomic_load(&fan_result),fa=atomic_load(&fan_auto);
                    if(last_fan_value!=fv || last_fan_rc!=fr || last_fan_auto!=fa){last_fan_value=fv;last_fan_rc=fr;last_fan_auto=fa;page_dirty=1;}
                    else{int cur=atomic_load(&fan_current),cpu=atomic_load(&fan_cpu),soc=atomic_load(&fan_soc);
                        if(last_fan_current!=cur || last_fan_cpu!=cpu || last_fan_soc!=soc){char fm[160];fan_metrics_string(fm,sizeof fm);pm_shellui_set_metrics(fm);}
                        last_fan_current=cur;last_fan_cpu=cpu;last_fan_soc=soc;}
                }
                static int last_drawn_page=-1;
                if(state==2 && page_dirty && action_draw()==0){if(last_drawn_page!=(int)actions.page)menu.row=0;last_drawn_page=actions.page;page_dirty=0;pm_shellui_select(menu.row);pm_shellui_tick();}
            }
        }else{menu.visible=0;menu.drain=0;pm_shellui_capture_input(0);}
        atomic_store(&desired_visible,(int)menu.visible);
        atomic_store(&capture_status,pm_shellui_input_captured());
        if(atomic_load(&action_pending)!=REQ_NONE && !menu.visible && !buttons && !pm_shellui_input_captured())
            atomic_store_explicit(&action_ready,1,memory_order_release);
        atomic_store(&selected_status,(int)menu.row);atomic_store(&buttons_status,buttons);
        atomic_store(&rows_snapshot,pm_shellui_rows());
        atomic_store(&renderer_state,state);
    }
    atomic_store(&callback_busy,0);
}
/* Resolve and validate the owner-allocated stub before private-tail publication. */
static void inspect(void) {
    void *domain=mono_get_root_domain();
    if (!domain || !mono_thread_attach(domain)) { reason="mono_attach_failed"; return; }
    void *image=mono_image_loaded("Sce.PlayStation.PUI");
    void *klass=image?mono_class_from_name(image,"Sce.PlayStation.PUI","Application"):NULL;
    void *method=klass?mono_class_get_method_from_name(klass,"Update",0):NULL;
    if (!method) { reason="method_missing"; return; }
    void *sig=mono_method_signature(method);
    unsigned impl=0, flags=mono_method_get_flags(method,&impl);
    int ret=sig?mono_type_get_type(mono_signature_get_return_type(sig)):-1;
    unsigned params=sig?mono_signature_get_param_count(sig):~0u;
    probe_log("LIVE","ABI params=%u flags=0x%x return=%d",params,flags,ret);
    if (!pm_hook_abi_valid(params,1,flags,ret)) { reason="unsupported_abi"; return; }
    void *entry=mono_compile_method(method);
    if (!entry) { reason="no_aot_entry"; return; }
    void *begin=NULL,*end=NULL; int prot=0;
    if (sceKernelQueryMemoryProtection(entry,&begin,&end,&prot)!=0 || !(prot&1) ||
        (uintptr_t)entry<(uintptr_t)begin || (uintptr_t)end-(uintptr_t)entry<16) {
        reason="entry_unmapped";return;
    }
    unsigned char bytes[16];memcpy(bytes,entry,sizeof bytes);
    enum pm_hook_result result=pm_hook_plan_entry(&plan,(uintptr_t)entry,bytes,params,1,ret);
    if (result==PM_HOOK_BAD_ENTRY) { reason="unexpected_entry";return; }
    if (result==PM_HOOK_BAD_ABI) { reason="unsupported_abi";return; }
    uintptr_t hook=plan.prior;
    if (sceKernelQueryMemoryProtection((void *)hook,&begin,&end,&prot)!=0 ||
        (prot&5)!=5 || hook<(uintptr_t)begin || (uintptr_t)end-hook<16) {
        reason="hook_unmapped";return;
    }
    unsigned char prefix[16];memcpy(prefix,(void *)hook,16);
    uintptr_t global=0;
    if (!pm_dispatch_global(hook,prefix,&global) ||
        global<(uintptr_t)begin || (uintptr_t)end-global<8 || !(prot&2) || (global&7)) {
        reason="dispatch_global_unverified";return;
    }
    uintptr_t object=0;memcpy(&object,(void *)global,8);
    void *ob=NULL,*oe=NULL;int op=0;
    if (sceKernelQueryMemoryProtection((void *)object,&ob,&oe,&op)!=0 ||
        (op&3)!=3 || object<(uintptr_t)ob || (uintptr_t)oe-object<64 || (object&7)) {
        reason="detour_object_unverified";return;
    }
    uintptr_t fields[6];memcpy(fields,(void *)object,sizeof fields);
    snprintf(detour_fields,sizeof detour_fields,"%llx,%llx,%llx,%llx,%llx,%llx",(unsigned long long)fields[0],(unsigned long long)fields[1],(unsigned long long)fields[2],(unsigned long long)fields[3],(unsigned long long)fields[4],(unsigned long long)fields[5]);
    probe_log("LIVE","DETOUR_DIAG entry=%p hook=%p global=%p object=%p fields=%llx,%llx,%llx,%llx,%llx,%llx",entry,(void*)hook,(void*)global,(void*)object,(unsigned long long)fields[0],(unsigned long long)fields[1],(unsigned long long)fields[2],(unsigned long long)fields[3],(unsigned long long)fields[4],(unsigned long long)fields[5]);
    if (!pm_dispatch_object(fields,(uintptr_t)entry,hook)) {
        reason="detour_identity_mismatch";return;
    }
    uintptr_t stub=fields[1];
    void *sb=NULL,*se=NULL;int sp=0;
    if (sceKernelQueryMemoryProtection((void *)stub,&sb,&se,&sp)!=0 ||
        (sp&5)!=5 || stub<(uintptr_t)sb || (uintptr_t)se-stub<30) {
        reason="stub_unmapped";return;
    }
    static const unsigned char stub_prefix[]={0x55,0x48,0x8b,0xec,0x48,0x83,0xec,0x60};
    if (memcmp((void *)stub,stub_prefix,sizeof stub_prefix) ||
        memcmp((void *)(stub+16),"\xff\x25\0\0\0\0",6)) {
        reason="stub_signature_mismatch";return;
    }
    uintptr_t back=0;memcpy(&back,(void *)(stub+22),8);
    if (back!=(uintptr_t)entry+16) {reason="stub_return_mismatch";return;}
    original_stub=stub;
    probe_log("LIVE","entry=%p prot=%x outer=%p global=%p object=%p prot=%x stub=%p prot=%x callback=%p",
        entry,prot,(void *)hook,(void *)global,(void *)object,op,(void *)stub,sp,(void *)pm_ui_bridge);
    /* Preserve the owner's first16 stolen bytes and all Detour fields. Only the
     * anonymous executable stub's aligned16 jump-tail is atomically changed.
     * Owner restoration copies the original16 prefix, which remains untouched. */
    if(ret!=1 || (stub&15) || (sp&7)!=7 || fields[2]!=30){reason="private_stub_gate";return;}
    unsigned ax=1,bx,cx,dx;
    __asm__ volatile("cpuid" : "+a"(ax),"=b"(bx),"=c"(cx),"=d"(dx));
    if(!(cx&(1u<<13))){reason="no_cmpxchg16b";return;}
    jump_slot=stub+16;
    memcpy(&original_jump,(void*)jump_slot,16);
    installed_jump=original_jump;
    uintptr_t bridge=(uintptr_t)pm_ui_bridge;
    memcpy((unsigned char*)&installed_jump+6,&bridge,8);
    pm_ui_continue=back;
    if(!cas_jump(original_jump,installed_jump)){reason="stub_changed";return;}
    atomic_store(&hook_published,1);
    reason="private_stub_published";
    probe_log("LIVE","PUMP_INSTALLED stub=%p slot=%p bridge=%p continue=%p code_entry_unchanged=1 owner_prologue_unchanged=1",(void*)stub,(void*)jump_slot,(void*)bridge,(void*)back);

}
static void *run(void *arg) {
    (void)arg;
    struct proc_info own={0}; OrbisKernelSwVersion fw={0}; fw.Size=sizeof fw;
    if (probe_process_report("LIVE",&own)!=0 || strcmp(own.name,"SceShellUI") ||
        strcmp(own.titleid,"NPXS20001") || sceKernelGetSystemSwVersion(&fw)!=0 ||
        fw.Version!=0x13520001) { reason="identity_or_firmware_guard"; return NULL; }
    int user_init=sceUserServiceInitialize(NULL),pad_init=scePadInit();
    probe_log("INPUT","INIT user_service=%x pad=%x",user_init,pad_init);
    int focus_start=pm_focus_start();
    probe_log("FOCUS","WORKER_START result=%x",focus_start);
    if(focus_start!=0){reason="focus_worker_failed";return NULL;}
    load_theme();
    resolve_action_apis();
    tools_log_snapshot();
    {struct pi_entry _pp[PI_MAX]; int _pc=plugins_load(_pp,PI_MAX);
     struct fb_entry _fe[8]; int _fc=files_list("/",_fe,8);
     struct fb_entry _de[8]; int _dc=files_list("/user/data/GoldHEN",_de,8);
     probe_log("STARTUP","plugins=%d root_entries=%d user_data_goldhen_entries=%d",_pc,_fc,_dc);}
    {int current=-1;if(fan_get_threshold(&current)==0){atomic_store(&fan_initial,current);atomic_store(&fan_current,current);}else probe_log("FAN","NO_BASELINE - refuse all writes until AC power reset");}
    inspect();
    while (!atomic_load(&stopping)) {
        int fan_next=atomic_exchange(&fan_request,-1);
        static unsigned fan_cycle;static int auto_thr=62,last_auto_thr=-1;
        if(fan_next==-3){atomic_store(&fan_auto,1);atomic_store(&fan_selected,-1);atomic_store(&fan_choice,0);auto_thr=62;last_auto_thr=-1;fan_cycle=6;/* apply first auto step next loop, not 14s later */}
        else if(fan_next!=-1){
            atomic_store(&fan_auto,0);
            int choice=fan_next==-2?1:fan_next==75?2:fan_next==55?3:fan_next==40?4:-1;
            int target=fan_next==-2?(sceKernelIsNeoMode()?79:75):fan_next;
            int rc=target>=40 && atomic_load(&fan_initial)>=40?fan_set_threshold(target):-4;
            atomic_store(&fan_result,rc);
            atomic_store(&fan_selected,rc==0?target:-1);
            atomic_store(&fan_choice,rc==0?choice:-1);
            fan_cycle=0;
        }
        if(atomic_load(&fan_auto) && atomic_load(&fan_initial)>=40 && ++fan_cycle>=7){
            fan_cycle=0;
            int cpu=atomic_load(&fan_cpu);
            if(cpu>0){
                auto_thr=fan_auto_step(auto_thr,cpu,65);
                if(auto_thr!=last_auto_thr){
                    int rc=fan_set_threshold(auto_thr);
                    atomic_store(&fan_result,rc);
                    if(rc==0)last_auto_thr=auto_thr;
                    else{atomic_store(&fan_auto,0);atomic_store(&fan_choice,-1);}
                }
            }
        }else if(atomic_load(&fan_selected)>=0 && ++fan_cycle>=7){
            fan_cycle=0;
            atomic_store(&fan_result,fan_set_threshold(atomic_load(&fan_selected)));
            if(atomic_load(&fan_result)!=0){atomic_store(&fan_selected,-1);atomic_store(&fan_choice,-1);}
        }
        {int current=-1;if(fan_get_threshold(&current)==0)atomic_store(&fan_current,current);
         int cpu=-1,soc=-1;if(tools_cpu_temp_c(&cpu)==0)atomic_store(&fan_cpu,cpu);
         if(tools_soc_temp_c(&soc)==0)atomic_store(&fan_soc,soc);}
        execute_pending_action();
        save_theme_if_requested();
        struct process_identity id={0};
        if (probe_find_process("SceShellUI",&id) && id.pid==own.pid) {
            char text[1024];
            char command[32]={0};int cmd=sceKernelOpen("/Temp/" PROBE_STAGE "/command.txt",0,0);
            if(cmd>=0){int n=sceKernelRead(cmd,command,sizeof(command)-1);sceKernelClose(cmd);if(n>0){
                if(!strncmp(command,"hide",4))atomic_store(&visible_request,0);
                if(!strncmp(command,"show",4))atomic_store(&visible_request,1);
                if(!strncmp(command,"stop",4))atomic_store(&cleanup_requested,1);
                if(!strncmp(command,"page-games",10))atomic_store(&page_request,PAGE_GAMES);
                if(!strncmp(command,"page-home",9))atomic_store(&page_request,PAGE_HOME);
                sceKernelUnlink("/Temp/" PROBE_STAGE "/command.txt");
            }}
            unsigned long long ticks=atomic_load(&ui_ticks);
            int rs=atomic_load(&renderer_state);
            const char *state=rs<0?"renderer_failed":rs==2?"rendering":ticks?"ui_pump":"waiting";
            snprintf(text,sizeof text,"PM_SHELLUI_HOOK %s build=%s pid=%d cookie=%llx start_s=%lld start_us=%lld state=%s retained=1 seq=%llu hook=%s reason=%s ui_ticks=%llu renderer=%d visible=%d code_entry_writes=0 pad=%d pad_rc=%d buttons=%x selected=%d capture=%d input_events=%llu page=%d rows=%u action=%d action_rc=%x dispatched=%llu games=%u native_capture=%d focus_owned=%d focus_rc=%x theme=%u theme_save_rc=%d fan_initial=%d fan_current=%d fan_selected=%d fan_rc=%d detour_fields=%s\n",
                PROBE_VERSION,PROBE_BUILD,id.pid,(unsigned long long)id.cookie,
                id.start_seconds,id.start_microseconds,state,++seq,
                atomic_load(&hook_published)?"published":"none",reason,ticks,rs,atomic_load(&desired_visible),atomic_load(&pad_handle_status),atomic_load(&pad_status),atomic_load(&buttons_status),atomic_load(&selected_status),atomic_load(&capture_status),atomic_load(&input_events),atomic_load(&page_snapshot),atomic_load(&rows_snapshot),atomic_load(&last_action),atomic_load(&last_result),atomic_load(&actions_dispatched),(unsigned)CATALOG_COUNT,pm_focus_state(),pm_focus_owned(),pm_focus_rc(),actions.theme_index,atomic_load(&theme_save_result),atomic_load(&fan_initial),atomic_load(&fan_current),atomic_load(&fan_selected),atomic_load(&fan_result),detour_fields);
            probe_write_text("/Temp/" PROBE_STAGE "/status.txt",text);
        }
        sceKernelUsleep(2000000);
    }
    return NULL;
}
PROBE_EXPORT int module_start(size_t argc,const void *args) {
    (void)argc;(void)args;
    if (atomic_exchange(&started,1)) return 0;
    int rc=scePthreadCreate(&worker,NULL,run,NULL,"pm_ui_live");
    worker_created=rc==0; return rc;
}
PROBE_EXPORT int module_stop(size_t argc,const void *args) {
    (void)argc;(void)args;
    /* Never detach a live hook: foreign threads could still execute module code.
     * No hook is currently possible, so the worker is the only live activity. */
    if (original_stub) return -1;
    atomic_store(&stopping,1);
    if (worker_created) scePthreadJoin(worker,NULL);
    return 0;
}
