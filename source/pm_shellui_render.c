/*
 * ProsperoMenu PS4 — resident ShellUI renderer module.
 *
 * Original creator: StonedModder. GPL-3.0-or-later.
 * Managed PUI patterns follow the original platform/deck/deck-view.c; geometry
 * and colours come from the ported layout engine (layout/deck-layout-docked.c).
 *
 * RENDERER half of the dashboard overlay (see ShellUI-Renderer-Interface.md).
 * Managed PUI only — no ptrace, no code patching. A separate injector loads this
 * PRX into SceShellUI and calls pm_shellui_tick() each frame on the UI thread.
 *
 * Exports: int pm_shellui_init(void); void pm_shellui_tick(void); void pm_shellui_term(void);
 * It builds the real docked ProsperoMenu: the full style set (fonts/inks/
 * backgrounds), 24 panels + 64 labels, positioned and coloured by
 * pm_deck_layout_docked() from a frame (a built-in demo frame until the PS4
 * coordinator/input feed is wired). module_start resolves only (safe off the UI
 * thread); widget creation happens in tick(), on the UI thread.
 */
#include <stddef.h>
#include <stdint.h>
#ifdef PM_HOST
#include "pm_host_shim.h"          /* host test build: stubs for the libkernel calls below */
#else
#include <orbis/libkernel.h>
#endif
#include "menu_themes.h"
#include "deck-layout-docked.h"   /* PmDeckFrame/Layout, tables, pm_deck_layout_docked (-I layout) */

/* ---- Mono API, link-imported (bound to ShellUI's Mono by the loader) -------- */
extern void* mono_get_root_domain(void);
extern void* mono_thread_attach(void*);
extern void* mono_image_loaded(const char*);
extern void* mono_class_from_name(void*,const char*,const char*);
extern void* mono_class_get_method_from_name(void*,const char*,int);
extern void* mono_class_get_parent(void*);
extern void* mono_object_new(void*,void*);
extern void* mono_runtime_invoke(void*,void*,void**,void**);
extern void* mono_object_unbox(void*);
extern void* mono_string_new(void*,const char*);
extern uint32_t mono_gchandle_new(void*,int);
extern void* mono_gchandle_get_target(uint32_t);
extern void mono_gchandle_free(uint32_t);

void *memset(void *d,int c,size_t n){unsigned char*p=(unsigned char*)d;while(n--)*p++=(unsigned char)c;return d;}
static void scopy(char*d,const char*s,size_t cap){size_t i=0;if(!cap)return;while(i+1<cap&&s[i]){d[i]=s[i];i++;}d[i]=0;}

/* ---- klog ------------------------------------------------------------------- */
struct buf { char *p; size_t cap, len; };
static void b_init(struct buf*b,char*s,size_t c){b->p=s;b->cap=c;b->len=0;if(c)s[0]=0;}
static void b_ch(struct buf*b,char c){if(b->len+1<b->cap){b->p[b->len++]=c;b->p[b->len]=0;}}
static void b_str(struct buf*b,const char*s){if(!s)s="(null)";for(;*s;s++)b_ch(b,*s);}
static void b_dec(struct buf*b,long v){char t[24];unsigned i=0;unsigned long n;
    if(v<0){b_ch(b,'-');n=(unsigned long)(-(v+1))+1;}else n=(unsigned long)v;
    do{t[i++]=(char)('0'+n%10);n/=10;}while(n&&i<sizeof t);while(i)b_ch(b,t[--i]);}
static void klog(const char*s){char l[256];struct buf b;b_init(&b,l,sizeof l);b_str(&b,"PMSH-R ");b_str(&b,s);b_ch(&b,'\n');sceKernelDebugOutText(0,l);}
static void __attribute__((unused)) klogn(const char*s,long n){char l[256];struct buf b;b_init(&b,l,sizeof l);b_str(&b,"PMSH-R ");b_str(&b,s);b_ch(&b,' ');b_dec(&b,n);b_ch(&b,'\n');sceKernelDebugOutText(0,l);}

/* ---- Mono state ------------------------------------------------------------- */
static void *g_domain,*g_pui;
static void *C_App,*C_Scene,*C_Container,*C_Widget,*C_Panel,*C_Label,*C_Font,*C_Color;
static int g_failed,g_inited,g_built,g_paintedValid,g_visible,g_sceneMayBeAttached;
static unsigned g_selected;
static uint64_t g_openedAt,g_selectedAt;
static PmDeckMotion g_motion;
static PmDeckLayout g_painted;
static uint32_t h_root,h_scene,h_rootWidget;
static uint64_t millis(void){ return sceKernelGetProcessTime()/1000; }
static int streq(const char*a,const char*b){ size_t i=0; for(;;i++){ if(a[i]!=b[i])return 0; if(!a[i])return 1; } }
static uint32_t h_fonts[PM_DECK_STYLES], h_inks[PM_DECK_INKS], h_bgs[PM_DECK_BACKGROUNDS];
static uint32_t h_panels[PM_DECK_PANELS], h_labels[PM_DECK_LABELS];
static unsigned g_theme_index;

static void *fm(void *klass,const char *name,int argc){
    for(void*k=klass;k;k=mono_class_get_parent(k)){void*m=mono_class_get_method_from_name(k,name,argc);if(m)return m;}
    return 0;
}
static void *inv(void *m,void *o,void **a){ if(g_failed||!m)return 0; void*e=0;void*r=mono_runtime_invoke(m,o,a,&e); if(e){g_failed=1;klog("managed exception");} return r; }
static uint32_t pin(void *klass){void*o=mono_object_new(g_domain,klass);return o?mono_gchandle_new(o,1):0;}
static void *tgt(uint32_t h){return h?mono_gchandle_get_target(h):0;}
static void set_f(void*m,void*o,float v){void*a[]={&v};inv(m,o,a);}
static void set_b(void*m,void*o,uint8_t v){void*a[]={&v};inv(m,o,a);}

static void *M_getInstance,*M_getRootScene,*M_sceneCtor,*M_setOverlay,*M_setFocusable,*M_setDW,*M_setDH,
            *M_getRootWidget,*M_addScene,*M_removeScene,*M_append,*M_setX,*M_setY,
            *M_panelCtor,*M_setW,*M_setH,*M_setVisible,*M_setBg,
            *M_labelCtor,*M_setText,*M_setFont,*M_setTextColor,*M_fitW,*M_fitH,
            *M_fontCtor,*M_colorCtor;

int pm_shellui_init(void){
    if(g_inited) return g_failed?-1:0;
    g_domain=mono_get_root_domain(); if(!g_domain){klog("init: no domain");return -1;}
    /* Register this thread with Mono (GC/TLS) before any reflection. Harmless if
     * already attached (e.g. the UI thread). Never detached (unwind-detach on a
     * foreign thread crashes ShellUI). */
    mono_thread_attach(g_domain);
    g_pui=mono_image_loaded("Sce.PlayStation.PUI"); if(!g_pui){klog("init: no PUI image");return -1;}
    C_App=mono_class_from_name(g_pui,"Sce.PlayStation.PUI","Application");
    C_Scene=mono_class_from_name(g_pui,"Sce.PlayStation.PUI.UI2","Scene");
    C_Container=mono_class_from_name(g_pui,"Sce.PlayStation.PUI.UI2","ContainerScene");
    C_Widget=mono_class_from_name(g_pui,"Sce.PlayStation.PUI.UI2","Widget");
    C_Panel=mono_class_from_name(g_pui,"Sce.PlayStation.PUI.UI2","Panel");
    C_Label=mono_class_from_name(g_pui,"Sce.PlayStation.PUI.UI2","Label");
    C_Font=mono_class_from_name(g_pui,"Sce.PlayStation.PUI.UI2","UIFont");
    C_Color=mono_class_from_name(g_pui,"Sce.PlayStation.PUI","UIColor");
    if(!C_App||!C_Scene||!C_Container||!C_Widget||!C_Panel||!C_Label||!C_Font||!C_Color){klog("init: a class missing");return -1;}
    M_getInstance=fm(C_App,"get_Instance",0); M_getRootScene=fm(C_App,"get_RootScene",0);
    M_sceneCtor=mono_class_get_method_from_name(C_Scene,".ctor",0);
    M_setOverlay=fm(C_Scene,"set_ScreenRenderOverlay",1); M_setFocusable=fm(C_Scene,"set_Focusable",1);
    M_setDW=fm(C_Scene,"set_DesignWidth",1); M_setDH=fm(C_Scene,"set_DesignHeight",1);
    M_getRootWidget=fm(C_Scene,"get_RootWidget",0);
    M_addScene=fm(C_Container,"AddScene",1); M_removeScene=fm(C_Container,"RemoveScene",1);
    M_append=fm(C_Widget,"AppendChild",1); M_setX=fm(C_Widget,"set_X",1); M_setY=fm(C_Widget,"set_Y",1);
    M_panelCtor=mono_class_get_method_from_name(C_Panel,".ctor",0);
    M_setW=fm(C_Panel,"set_Width",1); M_setH=fm(C_Panel,"set_Height",1);
    M_setVisible=fm(C_Panel,"set_Visible",1); M_setBg=fm(C_Panel,"set_BackgroundColor",1);
    M_labelCtor=mono_class_get_method_from_name(C_Label,".ctor",0);
    M_setText=fm(C_Label,"set_Text",1); M_setFont=fm(C_Label,"set_Font",1);
    M_setTextColor=fm(C_Label,"set_TextColor",1);
    M_fitW=fm(C_Label,"set_FitWidthToText",1); M_fitH=fm(C_Label,"set_FitHeightToText",1);
    M_fontCtor=mono_class_get_method_from_name(C_Font,".ctor",3);
    M_colorCtor=mono_class_get_method_from_name(C_Color,".ctor",4);
    if(!M_getInstance||!M_getRootScene||!M_sceneCtor||!M_getRootWidget||!M_addScene||!M_removeScene||
       !M_append||!M_setX||!M_setY||!M_panelCtor||!M_setW||!M_setH||!M_setVisible||!M_setBg||
       !M_labelCtor||!M_setText||!M_setFont||!M_setTextColor||!M_fontCtor||!M_colorCtor){klog("init: a method missing");return -1;}
    g_inited=1; klog("init ok (Mono + PUI resolved)"); return 0;
}

static uint32_t make_color(const float rgba[4]){
    uint32_t h=pin(C_Color); void*o=tgt(h); if(!o)return 0;
    float r=rgba[0],g=rgba[1],b=rgba[2],a=rgba[3]; void*ar[]={&r,&g,&b,&a};
    inv(M_colorCtor,mono_object_unbox(o),ar); return h;
}

/* A minimal valid frame for the pure-presentation layout engine (no network
 * validation needed for pm_deck_layout_docked). Replace with the PS4 coordinator
 * / input feed later; the render path below does not change. */
static PmDeckFrame g_frame;
static PmDeckLayout g_layout;
/* A row: label "text", optional right-aligned value (PM_ROW_VALUE encodes it as
 * "text\0value" in label[]), and flags. */
static void add_row(const char *text,const char *value,uint32_t flags){
    if(g_frame.count>=PM_DECK_ROWS) return;
    PmDeckRow *r=&g_frame.rows[g_frame.count++];
    size_t n=0; while(n+1<sizeof r->label && text[n]){ r->label[n]=text[n]; n++; } r->label[n]=0;
    if(value){ size_t j=n+1,k=0; while(j+1<sizeof r->label && value[k]){ r->label[j++]=value[k++]; } r->label[j]=0; flags|=PM_ROW_VALUE; }
    r->flags=flags;
}
static int g_home_extra_cheats;
void pm_shellui_home_extra(int cheats){g_home_extra_cheats=cheats?1:0;}
static void demo_frame(void){
    memset(&g_frame,0,sizeof g_frame);g_frame.flags=1;
    scopy(g_frame.title,"ProsperoMenu",sizeof g_frame.title);
    scopy(g_frame.subtitle,"PS4 menu",sizeof g_frame.subtitle);
    scopy(g_frame.footer,"Cross: Select   Circle: Back / Close   L3+R3: Menu",sizeof g_frame.footer);
    add_row("Games & Apps",0,PM_ROW_ENABLED);
    if(g_home_extra_cheats)add_row("Cheats",0,PM_ROW_ENABLED);
    add_row("Plugins",0,PM_ROW_ENABLED);
    add_row("Fan Control",0,PM_ROW_ENABLED);
    add_row("File Browser",0,PM_ROW_ENABLED);
    add_row("GoldHEN Settings",0,PM_ROW_ENABLED);
    add_row("Themes",0,PM_ROW_ENABLED);
    add_row("Power",0,PM_ROW_ENABLED);
}

static void theme_color(int background,unsigned slot,float out[4]){
    const float *base=background?pm_deck_background[slot]:pm_deck_ink[slot];
    for(unsigned i=0;i<4;i++)out[i]=base[i];
    if(g_theme_index==0)return;
    const struct pm_theme_preset *p=&pm_theme_presets[g_theme_index];
    if(!background && slot==PM_INK_TEAL)pm_theme_rgba(p->accent,out);
    if(background){
        if(slot==PM_BG_TEAL||slot==PM_BG_LOGO)pm_theme_rgba(p->accent,out);
        else if(slot==4)pm_theme_rgba(p->fill,out);
        else if(slot==PM_BG_SCROLL)pm_theme_rgba(p->scroll,out);
    }
}
static void recolor(uint32_t h,const float rgba[4]){
    if(!h)return;
    float r=rgba[0],g=rgba[1],b=rgba[2],a=rgba[3];
    void*ar[]={&r,&g,&b,&a};inv(M_colorCtor,mono_object_unbox(tgt(h)),ar);
}
int pm_shellui_set_theme(unsigned index){
    if(index>=PM_THEME_COUNT || !g_inited || g_failed)return -1;
    if(index==g_theme_index)return 0;
    g_theme_index=index;
    if(g_built){
        float color[4];
        for(unsigned i=0;i<PM_DECK_INKS;i++){theme_color(0,i,color);recolor(h_inks[i],color);}
        for(unsigned i=0;i<PM_DECK_BACKGROUNDS;i++){theme_color(1,i,color);recolor(h_bgs[i],color);}
        g_paintedValid=0;
    }
    return g_failed?-1:0;
}
static void styles(void){
    for(unsigned i=0;i<PM_DECK_STYLES;i++){
        h_fonts[i]=pin(C_Font); int size=pm_deck_font_sizes[i],style=0,weight=0;
        void*fa[]={&size,&style,&weight}; inv(M_fontCtor,mono_object_unbox(tgt(h_fonts[i])),fa);
    }
    float color[4];
    for(unsigned i=0;i<PM_DECK_INKS;i++){theme_color(0,i,color);h_inks[i]=make_color(color);}
    for(unsigned i=0;i<PM_DECK_BACKGROUNDS;i++){theme_color(1,i,color);h_bgs[i]=make_color(color);}
}
static void widgets(void){
    for(unsigned i=0;i<PM_DECK_PANELS;i++){
        h_panels[i]=pin(C_Panel); void*p=tgt(h_panels[i]); if(!p){g_failed=1;return;}
        inv(M_panelCtor,p,0); set_b(M_setVisible,p,0);
        void*a[]={p}; inv(M_append,tgt(h_rootWidget),a);
    }
    for(unsigned i=0;i<PM_DECK_LABELS;i++){
        h_labels[i]=pin(C_Label); void*lb=tgt(h_labels[i]); if(!lb){g_failed=1;return;}
        inv(M_labelCtor,lb,0); set_b(M_fitW,lb,1); set_b(M_fitH,lb,1);
        void*a[]={lb}; inv(M_append,tgt(h_rootWidget),a);
    }
}
/* Animated + diffed render: compute the docked layout at time `now` (marquee from
 * the moment the selection changed), apply the slide-in, then push ONLY the
 * widget properties that changed since the last paint (cheap per-frame, like
 * deck-view.c). Returns non-zero while still animating. */
static int render(uint64_t now){
    uint64_t marqueeMs = now>=g_selectedAt ? now-g_selectedAt : 0;
    pm_deck_layout_docked_at(&g_frame,1,g_selected,&g_layout,marqueeMs);
    int moving = pm_deck_motion_docked(&g_motion,&g_layout,g_selected,now);
    int first = !g_paintedValid;
    for(unsigned i=0;i<PM_DECK_PANELS;i++){
        const PmDeckPanel*p=&g_layout.panels[i]; const PmDeckPanel*q=&g_painted.panels[i];
        void*w=tgt(h_panels[i]); if(!w)continue;
        if(first||p->x!=q->x) set_f(M_setX,w,p->x);
        if(first||p->y!=q->y) set_f(M_setY,w,p->y);
        if(first||p->width!=q->width) set_f(M_setW,w,p->width);
        if(first||p->height!=q->height) set_f(M_setH,w,p->height);
        if((first||p->ink!=q->ink)&&p->ink<PM_DECK_BACKGROUNDS&&h_bgs[p->ink]){ void*a[]={mono_object_unbox(tgt(h_bgs[p->ink]))}; inv(M_setBg,w,a); }
        if(first||p->visible!=q->visible) set_b(M_setVisible,w,(uint8_t)(p->visible?1:0));
    }
    for(unsigned i=0;i<PM_DECK_LABELS;i++){
        const PmDeckText*t=&g_layout.labels[i]; const PmDeckText*u=&g_painted.labels[i];
        void*w=tgt(h_labels[i]); if(!w)continue;
        if(first||t->x!=u->x) set_f(M_setX,w,t->x);
        if(first||t->y!=u->y) set_f(M_setY,w,t->y);
        if((first||t->font!=u->font)&&t->font<PM_DECK_STYLES&&h_fonts[t->font]){ void*a[]={mono_object_unbox(tgt(h_fonts[t->font]))}; inv(M_setFont,w,a); }
        if((first||t->ink!=u->ink)&&t->ink<PM_DECK_INKS&&h_inks[t->ink]){ void*a[]={mono_object_unbox(tgt(h_inks[t->ink]))}; inv(M_setTextColor,w,a); }
        if(first||!streq(t->text,u->text)){ void*sa[]={mono_string_new(g_domain,t->text)}; inv(M_setText,w,sa); }
    }
    g_painted=g_layout; g_paintedValid=1;
    return moving || g_layout.marquee;
}

static void build(uint64_t now){
    if(!h_scene){
        void*app=inv(M_getInstance,0,0); if(!app){klog("build: get_Instance null");g_failed=1;return;}
        void*root=inv(M_getRootScene,app,0); if(!root){klog("build: RootScene null");g_failed=1;return;}
        h_root=mono_gchandle_new(root,1); if(!h_root){g_failed=1;return;}
        h_scene=pin(C_Scene); void*scene=tgt(h_scene); if(!scene){g_failed=1;return;}
        inv(M_sceneCtor,scene,0);
        set_b(M_setOverlay,scene,1); set_b(M_setFocusable,scene,0);
        set_f(M_setDW,scene,1920.0f); set_f(M_setDH,scene,1080.0f);
        if(g_failed)return;
        /* AddScene may attach and then throw: retain pins until removal succeeds. */
        g_sceneMayBeAttached=1;
        { void*a[]={scene}; inv(M_addScene,tgt(h_root),a); }
        if(g_failed)return;
    }
    void*scene=tgt(h_scene);
    void*rw=inv(M_getRootWidget,scene,0); if(!rw){klog("build: RootWidget not ready; retry");return;}
    h_rootWidget=mono_gchandle_new(rw,1);
    set_b(M_setVisible,rw,0); /* Keep children hidden until the first paint completes. */
    demo_frame();
    styles();
    widgets();
    g_openedAt=g_selectedAt=now; g_motion.active=0; g_paintedValid=0;
    render(now);
    if(g_failed){klog("build: failed");return;}
    set_b(M_setVisible,rw,(uint8_t)g_visible);
    if(g_failed)return;
    g_built=1; klog("build ok (docked menu: styles + 24 panels + 64 labels)");
}

/* UI-thread API: starts hidden; 0 means accepted, -1 means unavailable/failed.
 * Hiding retains the single scene for reuse; term is the explicit teardown. */
int pm_shellui_set_visible(int visible){
    if(!g_inited||g_failed)return -1;
    g_visible=visible?1:0;
    if(h_rootWidget)set_b(M_setVisible,tgt(h_rootWidget),(uint8_t)g_visible);
    return g_failed?-1:0;
}

void pm_shellui_tick(void){
    if(!g_inited||g_failed||!g_visible) return;
    uint64_t now=millis();
    if(!g_built){ build(now); return; }
    render(now);   /* diffed: issues managed calls only while something is changing/animating */
}

/* Injector's input path calls this (on the UI thread, same as tick) to move the
 * highlighted row; restarts the marquee clock. Redraw happens on the next tick. */
void pm_shellui_select(unsigned row){
    if(g_frame.count && row<g_frame.count && row!=g_selected){ g_selected=row; g_selectedAt=millis(); }
}

int pm_shellui_capture_input(int);
void pm_shellui_term(void){
    if(pm_shellui_capture_input(0)!=0){klog("term: input release pending");return;}
    if(!g_inited) return;
    if(g_sceneMayBeAttached){
        /* Cleanup must run even after an earlier managed exception. */
        void*scene=tgt(h_scene); void*root=tgt(h_root); void*e=0;
        if(!M_removeScene||!scene||!root){
            g_failed=1;klog("term: removal unavailable; handles retained");return;
        }
        void*a[]={scene};
        mono_runtime_invoke(M_removeScene,root,a,&e);
        if(e){g_failed=1;klog("term: RemoveScene failed; handles retained");return;}
        g_sceneMayBeAttached=0;
    }
    for(unsigned i=0;i<PM_DECK_PANELS;i++) if(h_panels[i]){mono_gchandle_free(h_panels[i]);h_panels[i]=0;}
    for(unsigned i=0;i<PM_DECK_LABELS;i++) if(h_labels[i]){mono_gchandle_free(h_labels[i]);h_labels[i]=0;}
    for(unsigned i=0;i<PM_DECK_STYLES;i++) if(h_fonts[i]){mono_gchandle_free(h_fonts[i]);h_fonts[i]=0;}
    for(unsigned i=0;i<PM_DECK_INKS;i++) if(h_inks[i]){mono_gchandle_free(h_inks[i]);h_inks[i]=0;}
    for(unsigned i=0;i<PM_DECK_BACKGROUNDS;i++) if(h_bgs[i]){mono_gchandle_free(h_bgs[i]);h_bgs[i]=0;}
    if(h_rootWidget){mono_gchandle_free(h_rootWidget);h_rootWidget=0;}
    if(h_scene){mono_gchandle_free(h_scene);h_scene=0;}
    if(h_root){mono_gchandle_free(h_root);h_root=0;}
    g_built=g_failed=g_inited=g_paintedValid=g_visible=0;
    g_selected=0; g_openedAt=g_selectedAt=0; g_motion.active=0;
    klog("term (overlay removed)");
}

/* module_start does NO Mono work: it runs on a transient loader thread, and doing
 * managed calls there and returning finalizes the module on a foreign Mono thread
 * (crash). The injector drives init/tick/term on the UI thread. (init-resolution
 * is already proven on-device: "PMSH-R init ok" in the smoke-test klog.) */
int __attribute__((visibility("hidden"))) pm_renderer_module_start(size_t argc, const void *args){
    (void)argc;(void)args;
    klog("renderer module loaded, resident; waiting for injector to call init/tick on the UI thread");
    return 0;
}
int __attribute__((visibility("hidden"))) pm_renderer_module_stop(size_t argc, const void *args){
    (void)argc;(void)args; klog("renderer module_stop"); return 0;
}
/* Read-only lifecycle evidence, called on UI thread or after atomic snapshot. */
int pm_shellui_state(void){return g_failed?-1:!g_inited?0:g_built?2:1;}

static void *M_suspendInput,*M_resumeInput,*M_cancelKeys;
static int input_owned;
extern int pm_native_input_capture(int);
int pm_shellui_capture_input(int capture){
    capture=!!capture;
    int native_rc=pm_native_input_capture(capture);
    if(native_rc<0)return -1;
    if(!capture && native_rc>0)return 1;
    if(capture==input_owned)return native_rc;
    if(!g_inited)return capture?-1:0;
    if(!M_suspendInput)M_suspendInput=fm(C_App,"SuspendInputEvents",0);
    if(!M_resumeInput)M_resumeInput=fm(C_App,"ResumeInputEvents",0);
    if(!M_cancelKeys)M_cancelKeys=fm(C_App,"CancelKeyEvents",0);
    void *method=capture?M_suspendInput:M_resumeInput;
    if(!method)return -1;
    void *exc=0,*app=mono_runtime_invoke(M_getInstance,0,0,&exc);
    if(exc||!app)return -1;
    if(M_cancelKeys){mono_runtime_invoke(M_cancelKeys,app,0,&exc);if(exc)return -1;}
    mono_runtime_invoke(method,app,0,&exc);if(exc)return -1;
    input_owned=capture;klog(capture?"dashboard input captured":"dashboard input restored");return native_rc;
}
int pm_shellui_input_captured(void){return input_owned;}
unsigned pm_shellui_rows(void){return g_frame.count;}


int pm_shellui_page(const char *title,const char *subtitle,const char *const *labels,const char *const *values,unsigned count){
    if(!g_built || !labels || count>PM_DECK_ROWS)return -1;
    memset(&g_frame,0,sizeof(g_frame));g_frame.flags=1;
    scopy(g_frame.title,title,sizeof(g_frame.title));scopy(g_frame.subtitle,subtitle,sizeof(g_frame.subtitle));
    scopy(g_frame.footer,"Cross: Select   Circle: Back   L3+R3: Menu",sizeof(g_frame.footer));
    for(unsigned i=0;i<count;i++)add_row(labels[i],values?values[i]:NULL,PM_ROW_ENABLED);
    g_selected=0;g_selectedAt=millis();g_motion.active=0;g_paintedValid=0;return 0;
}
int pm_shellui_page_metrics(const char *title,const char *subtitle,const char *metrics,const char *const *labels,const char *const *values,unsigned count){
    int rc=pm_shellui_page(title,subtitle,labels,values,count);
    if(rc==0)scopy(g_frame.metrics,metrics,sizeof g_frame.metrics);
    return rc;
}
int pm_shellui_set_metrics(const char *metrics){if(!g_built)return -1;scopy(g_frame.metrics,metrics,sizeof g_frame.metrics);g_paintedValid=0;return 0;}
int pm_shellui_home(const char *heading,const char *metrics){if(!g_built)return -1;demo_frame();scopy(g_frame.subtitle,heading,sizeof g_frame.subtitle);scopy(g_frame.metrics,metrics,sizeof g_frame.metrics);g_selected=0;g_selectedAt=millis();g_motion.active=0;g_paintedValid=0;return 0;}
int pm_shellui_home_metrics(const char *heading,const char *metrics){if(!g_built)return -1;scopy(g_frame.subtitle,heading,sizeof g_frame.subtitle);scopy(g_frame.metrics,metrics,sizeof g_frame.metrics);g_paintedValid=0;return 0;}
