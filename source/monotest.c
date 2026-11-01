/*
 * ProsperoMenu - ShellUI stage P9d: link-imported Mono reachability test.
 *
 * Original Creator: StonedModder.
 * GPL-3.0-or-later.
 *
 * P9c showed the injected PRX can only enumerate its own eboot.bin, so Mono's
 * handle cannot be found by sceKernelGetModuleList/Dlsym. OSM-Made's Orbis
 * Toolbox instead LINK-IMPORTED the Mono functions and let the loader bind them
 * against ShellUI's Mono. This PRX tests that route with the minimum: it
 * link-imports mono_get_root_domain / mono_thread_attach / mono_image_loaded,
 * logs only via sceKernelDebugOutText (never printf/stdio), and calls them with
 * before/after klogs. If this PRX loads and prints a non-null domain, the
 * link-import route works and the full PUI probe can be rebuilt that way.
 * Read-only: attaches a thread for the GC but creates no object, no UI, no patch.
 * See SHELLUI-PLAN.md stage P9c/P9d.
 */
#include <stddef.h>
#include <stdint.h>
#include <orbis/libkernel.h>

/* Link-imported from ShellUI's Mono (resolved by the loader, like Orbis Toolbox). */
extern void *mono_get_root_domain(void);
extern void *mono_thread_attach(void *domain);
extern void  mono_thread_detach(void *thread);
extern void *mono_image_loaded(const char *name);

void *memset(void *d, int c, size_t n){unsigned char*p=(unsigned char*)d;while(n--)*p++=(unsigned char)c;return d;}

struct buf { char *p; size_t cap, len; };
static void b_init(struct buf*b,char*s,size_t c){b->p=s;b->cap=c;b->len=0;if(c)s[0]=0;}
static void b_ch(struct buf*b,char c){if(b->len+1<b->cap){b->p[b->len++]=c;b->p[b->len]=0;}}
static void b_str(struct buf*b,const char*s){if(!s)s="(null)";for(;*s;s++)b_ch(b,*s);}
static void b_hex(struct buf*b,uint64_t v){static const char d[]="0123456789abcdef";
    b_str(b,"0x");int lead=1;for(int s=60;s>=0;s-=4){unsigned x=(unsigned)((v>>s)&15);
    if(x||s==0)lead=0;if(!lead)b_ch(b,d[x]);}}

static void klog(const char *s){
    char line[320];struct buf b;b_init(&b,line,sizeof line);
    b_str(&b,"PMSH9D ");b_str(&b,s);b_ch(&b,'\n');
    sceKernelDebugOutText(0,line);
}
static void klog_ptr(const char *s, void *p){
    char line[320];struct buf b;b_init(&b,line,sizeof line);
    b_str(&b,"PMSH9D ");b_str(&b,s);b_str(&b," = ");b_hex(&b,(uint64_t)(uintptr_t)p);b_ch(&b,'\n');
    sceKernelDebugOutText(0,line);
}

int __attribute__((visibility("hidden"))) module_start(size_t argc, const void *args)
{
    (void)argc;(void)args;
    /* If this line prints, the PRX LOADED with its Mono imports resolved. */
    klog("link-import Mono test: module_start reached");

    klog("before mono_get_root_domain");
    void *dom = mono_get_root_domain();
    klog_ptr("mono_get_root_domain", dom);
    if (!dom) { klog("no domain; stopping"); return 0; }

    klog("before mono_thread_attach");
    void *thr = mono_thread_attach(dom);
    klog_ptr("mono_thread_attach", thr);

    klog("before mono_image_loaded(Sce.PlayStation.PUI)");
    void *pui = mono_image_loaded("Sce.PlayStation.PUI");
    klog_ptr("PUI image", pui);
    if (!pui) {
        klog("before mono_image_loaded(Sce.PlayStation.HighLevel.UI2)");
        pui = mono_image_loaded("Sce.PlayStation.HighLevel.UI2");
        klog_ptr("HighLevel.UI2 image", pui);
    }

    if (thr) { klog("before mono_thread_detach"); mono_thread_detach(thr); }
    klog("done (Mono reachable via link-import)");
    return 0;
}

int __attribute__((visibility("hidden"))) module_stop(size_t argc, const void *args)
{
    (void)argc;(void)args;
    klog("module_stop");
    return 0;
}
