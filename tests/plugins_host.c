#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
/* in-memory plugins.ini backing the stubbed syscalls */
static char g_file[16384];
static int g_len;
static char g_written[16400];
/* stubs for the PS4 APIs plugins_native.c uses */
static int rd_pos;
int sceKernelOpen(const char*p,int a,int b){(void)p;(void)a;(void)b;rd_pos=0;return 3;}
long sceKernelRead(int fd,void*buf,long n){(void)fd;int left=g_len-rd_pos;if(left<=0)return 0;int c=n<left?(int)n:left;memcpy(buf,g_file+rd_pos,c);rd_pos+=c;return c;}
int sceKernelClose(int fd){(void)fd;return 0;}
int probe_write_text(const char*path,const char*text){(void)path;snprintf(g_written,sizeof g_written,"%s",text);/*apply back*/snprintf(g_file,sizeof g_file,"%s",text);g_len=(int)strlen(g_file);return 1;}
void probe_log(const char*a,const char*b,...){(void)a;(void)b;}
#define main plugins_main_unused
#include "../source/plugins_native.c"
#undef main
static void setfile(const char*s){snprintf(g_file,sizeof g_file,"%s",s);g_len=(int)strlen(g_file);}
int main(void){
 const char*orig="; comment\n\n[default]\n/data/GoldHEN/plugins/prosperomenu_dev.prx\n/data/GoldHEN/plugins/game_patch.prx\n\n[CUSA00001]\n/data/GoldHEN/plugins/afr.prx\n/data/GoldHEN/plugins/no_share_watermark.prx\n";
 setfile(orig);
 struct pi_entry e[48];int n=plugins_load(e,48);
 assert(n==4);
 assert(e[0].enabled==1 && strstr(e[0].label,"prosperomenu_dev.prx"));
 assert(strstr(e[0].label,"[default]"));
 assert(strstr(e[2].label,"[CUSA00001]") && strstr(e[2].label,"afr.prx"));
 /* disable index 1 (game_patch) */
 assert(plugins_toggle(1)==0);
 n=plugins_load(e,48);assert(n==4);
 assert(e[0].enabled==1 && e[1].enabled==0);
 assert(strstr(g_file,"; /data/GoldHEN/plugins/game_patch.prx"));
 /* other lines untouched */
 assert(strstr(g_file,"/data/GoldHEN/plugins/prosperomenu_dev.prx\n"));
 assert(strstr(g_file,"/data/GoldHEN/plugins/afr.prx"));
 /* re-enable index 1 restores */
 assert(plugins_toggle(1)==0);
 n=plugins_load(e,48);assert(e[1].enabled==1);
 assert(!strstr(g_file,"; /data/GoldHEN/plugins/game_patch.prx"));
 assert(strstr(g_file,"/data/GoldHEN/plugins/game_patch.prx"));
 /* section headers preserved */
 assert(strstr(g_file,"[default]") && strstr(g_file,"[CUSA00001]"));
 puts("plugins toggle host: PASS");
 return 0;
}
