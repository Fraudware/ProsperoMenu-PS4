/* Behavioral harness: compile the real renderer and layout; fake only Mono/kernel. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pm_host_shim.h"
int pm_shellui_init(void);
void pm_shellui_tick(void), pm_shellui_term(void), pm_shellui_select(unsigned);
int pm_shellui_set_visible(int);
int pm_shellui_set_theme(unsigned);
int pm_shellui_capture_input(int);
unsigned pm_shellui_rows(void);
int pm_shellui_page(const char*,const char*,const char*const*,const char*const*,unsigned);
struct object { const char *kind; int visible; } objects[2048];
static struct object app={"Application",1}, root={"ContainerScene",1}, rw={"Widget",1};
static void *handles[4096];
static unsigned allocated, next_handle, live, freed, adds, removes, polls;
static int delay=3, fail_append, fail_remove, fail_add, fail_ctor;
static void *added;
static unsigned suspends,resumes;
static int foreground_app_blocked,native_pending;
static float purple_r,purple_g,purple_b;static unsigned color_writes;
int pm_native_input_capture(int capture){if(native_pending)return 1;foreground_app_blocked=!!capture;return 0;}
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
int sceKernelDebugOutText(int n,const char*s){(void)n;fputs(s,stdout);return 0;}
uint64_t sceKernelGetProcessTime(void){static uint64_t t;t+=16000;return t;}
int sceKernelGetSystemSwVersion(OrbisKernelSwVersion*s){strcpy(s->VersionString,"host");return 0;}
int sceKernelIsNeoMode(void){return 0;}
void *mono_get_root_domain(void){return &app;}
void *mono_thread_attach(void*p){return p;}
void *mono_image_loaded(const char*s){(void)s;return &app;}
void *mono_class_from_name(void*i,const char*n,const char*c){(void)i;(void)n;return (void*)c;}
void *mono_class_get_parent(void*k){(void)k;return NULL;}
void *mono_class_get_method_from_name(void*k,const char*n,int a){(void)k;(void)a;return (void*)n;}
void *mono_object_new(void*d,void*k){(void)d;CHECK(allocated<2048);objects[allocated]=(struct object){k,1};return &objects[allocated++];}
void *mono_object_unbox(void*p){CHECK(p);return p;}
void *mono_string_new(void*d,const char*s){(void)d;return (void*)s;}
uint32_t mono_gchandle_new(void*p,int pinned){CHECK(p&&pinned);CHECK(next_handle+1<4096);handles[++next_handle]=p;live++;return next_handle;}
void *mono_gchandle_get_target(uint32_t h){CHECK(h<=next_handle);return handles[h];}
void mono_gchandle_free(uint32_t h){CHECK(handles[h]);handles[h]=NULL;live--;freed++;}
void *mono_runtime_invoke(void*m,void*o,void**a,void**e){
    const char*n=m; *e=NULL;
    if(!strcmp(n,"SuspendInputEvents"))suspends++;
    if(!strcmp(n,"ResumeInputEvents"))resumes++;
    if(!strcmp(n,"get_Instance"))return &app;
    if(!strcmp(n,"get_RootScene"))return &root;
    if(!strcmp(n,"get_RootWidget")){polls++;return polls<=(unsigned)delay?NULL:&rw;}
    if(!strcmp(n,"AddScene")){adds++;CHECK(o==&root);added=a[0];if(fail_add)*e=&app;}
    if(!strcmp(n,"RemoveScene")){removes++;CHECK(o==&root);CHECK(a[0]==added);if(fail_remove)*e=&app;else added=NULL;}
    if(!strcmp(n,"AppendChild")&&fail_append)*e=&app;
    if(!strcmp(n,".ctor")&&o&&((struct object*)o)->kind&& !strcmp(((struct object*)o)->kind,"UIColor")){
        float r=*(float*)a[0],g=*(float*)a[1],b=*(float*)a[2];
        if(r>0.68f&&r<0.70f && g>0.48f&&g<0.50f && b>0.99f){purple_r=r;purple_g=g;purple_b=b;}color_writes++;
    }
    if(!strcmp(n,".ctor")&&fail_ctor)*e=&app;
    if(!strcmp(n,"set_Visible")){CHECK(o);((struct object*)o)->visible=*(uint8_t*)a[0];}
    return NULL;
}
int main(int argc,char**argv){
    CHECK(argc==2);CHECK(pm_shellui_init()==0);
    if(strcmp(argv[1],"visibility"))CHECK(pm_shellui_set_visible(1)==0);
    if(!strcmp(argv[1],"theme")){
        delay=0;pm_shellui_tick();CHECK(adds==1);
        unsigned before=color_writes;
        CHECK(pm_shellui_set_theme(3)==0);
        CHECK(color_writes>before);
        CHECK(purple_r>0.68f&&purple_g>0.48f&&purple_b>0.99f);
        pm_shellui_tick();CHECK(adds==1);
        pm_shellui_term();CHECK(live==0);
    }else if(!strcmp(argv[1],"pending-release")){
        CHECK(pm_shellui_capture_input(1)==0);CHECK(suspends==1);
        native_pending=1;CHECK(pm_shellui_capture_input(0)==1);CHECK(resumes==0);
        CHECK(pm_shellui_capture_input(0)==1);CHECK(resumes==0);
        native_pending=0;CHECK(pm_shellui_capture_input(0)==0);CHECK(resumes==1);
    }else if(!strcmp(argv[1],"exclusive-app")){
        CHECK(pm_shellui_capture_input(1)==0);
        CHECK(suspends==1); /* Dashboard cancellation succeeds. */
        CHECK(foreground_app_blocked); /* Foreground app must also be blocked. */
        CHECK(pm_shellui_capture_input(0)==0);CHECK(!foreground_app_blocked);
    }else if(!strcmp(argv[1],"page")){
        delay=0;pm_shellui_tick();CHECK(adds==1);
        const char *labels[]={"Cancel","Confirm"};
        CHECK(pm_shellui_page("Confirm","Explicit confirmation",labels,NULL,2)==0);
        CHECK(pm_shellui_rows()==2);pm_shellui_tick();CHECK(adds==1);
        pm_shellui_term();CHECK(live==0);
    }else if(!strcmp(argv[1],"capture")){
        CHECK(pm_shellui_capture_input(1)==0);CHECK(suspends==1);
        CHECK(pm_shellui_capture_input(1)==0);CHECK(suspends==1);
        CHECK(pm_shellui_capture_input(0)==0);CHECK(resumes==1);
        CHECK(pm_shellui_capture_input(0)==0);CHECK(resumes==1);
    }else if(!strcmp(argv[1],"visibility")){
        for(int i=0;i<5;i++)pm_shellui_tick();CHECK(adds==0);CHECK(live==0);
        CHECK(pm_shellui_set_visible(1)==0);
        delay=0;pm_shellui_tick();CHECK(adds==1);CHECK(rw.visible==1);
        unsigned saved=live;CHECK(pm_shellui_set_visible(0)==0);CHECK(rw.visible==0);
        for(int i=0;i<5;i++)pm_shellui_tick();CHECK(live==saved);CHECK(adds==1);
        pm_shellui_select(3);CHECK(pm_shellui_set_visible(1)==0);pm_shellui_tick();
        CHECK(rw.visible==1);CHECK(adds==1);pm_shellui_term();CHECK(live==0);
        CHECK(pm_shellui_set_visible(1)==-1);
    }else if(!strcmp(argv[1],"retry")){
        for(int i=0;i<8;i++)pm_shellui_tick();
        CHECK(polls==4);CHECK(adds==1);CHECK(allocated>2);
        pm_shellui_term();CHECK(removes==1);CHECK(live==0);
    }else if(!strcmp(argv[1],"failure-cleanup")){
        delay=0;fail_append=1;pm_shellui_tick();CHECK(adds==1);
        pm_shellui_term();CHECK(removes==1);CHECK(added==NULL);CHECK(live==0);
    }else if(!strcmp(argv[1],"remove-retry")){
        delay=0;pm_shellui_tick();unsigned saved=live;CHECK(saved>2);
        fail_remove=1;pm_shellui_term();CHECK(removes==1);CHECK(live==saved);CHECK(freed==0);CHECK(added);CHECK(pm_shellui_init()==-1);
        CHECK(pm_shellui_set_visible(1)==-1);
        pm_shellui_tick();CHECK(adds==1);
        fail_remove=0;pm_shellui_term();CHECK(removes==2);CHECK(live==0);CHECK(!added);
        pm_shellui_term();CHECK(removes==2);
        CHECK(pm_shellui_init()==0);CHECK(pm_shellui_set_visible(1)==0);pm_shellui_tick();CHECK(adds==2);
        pm_shellui_term();CHECK(live==0);
    }else if(!strcmp(argv[1],"ctor-failure")){
        fail_ctor=1;pm_shellui_tick();CHECK(adds==0);
        pm_shellui_term();CHECK(removes==0);CHECK(live==0);
    }else if(!strcmp(argv[1],"add-exception")){
        fail_add=1;pm_shellui_tick();CHECK(adds==1);
        unsigned saved=live;pm_shellui_tick();CHECK(adds==1);CHECK(live==saved);
        pm_shellui_term();CHECK(removes==1);CHECK(live==0);CHECK(!added);
    }else if(!strcmp(argv[1],"pending-term")){
        delay=100;pm_shellui_tick();CHECK(adds==1);CHECK(live==2);
        pm_shellui_term();CHECK(removes==1);CHECK(live==0);
    }else return 2;
    printf("PASS %s\n",argv[1]);return 0;
}