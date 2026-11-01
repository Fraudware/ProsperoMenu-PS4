/* Read + safe toggle of /data/GoldHEN/plugins.ini. GPL-3.0-or-later.
 * Toggle only comments/uncomments a plugin line; the file is rewritten atomically. */
#include "plugins_native.h"
#include "probe_common.h"
#include <orbis/libkernel.h>
#include <string.h>
#include <stdio.h>
#define PI_BUF 16384
static const char*pi_path(void){
    int fd=sceKernelOpen("/user/data/GoldHEN/plugins.ini",0,0);
    if(fd>=0){sceKernelClose(fd);return "/user/data/GoldHEN/plugins.ini";}
    return "/data/GoldHEN/plugins.ini";
}
static int read_all(char*buf,int cap){
    int fd=sceKernelOpen(pi_path(),0,0); if(fd<0)return -1;
    int n=sceKernelRead(fd,buf,cap-1); sceKernelClose(fd); if(n<0)return -1; buf[n]=0; return n;
}
int plugins_load(struct pi_entry*out,int max){
    static char buf[PI_BUF]; if(read_all(buf,sizeof buf)<0)return -1;
    int count=0; char section[32]="default"; char*line=buf;
    while(*line && count<max){
        char*eol=strchr(line,'\n'); int len=eol?(int)(eol-line):(int)strlen(line);
        char tmp[256]; int c=len<255?len:255; memcpy(tmp,line,c); tmp[c]=0;
        if(c>0&&tmp[c-1]=='\r')tmp[--c]=0;
        char*p=tmp; while(*p==' '||*p=='\t')p++;
        if(*p=='['){char*rb=strchr(p,']'); if(rb){int sl=(int)(rb-p-1); if(sl>0){int k=sl<31?sl:31;memcpy(section,p+1,k);section[k]=0;}}}
        else if(strstr(p,".prx")){
            int enabled=1; char*q=p;
            if(*q==';'){enabled=0;q++;while(*q==' '||*q=='\t')q++;}
            char*bn=strrchr(q,'/'); bn=bn?bn+1:q;
            char name[80]; int k=0; while(bn[k]&&bn[k]!=' '&&bn[k]!='\r'&&k<79){name[k]=bn[k];k++;} name[k]=0;
            snprintf(out[count].label,sizeof out[count].label,"[%.20s] %.50s",section,name);
            out[count].enabled=enabled; count++;
        }
        if(!eol)break; line=eol+1;
    }
    probe_log("PLUGINS","LOAD count=%d",count); return count;
}
int plugins_toggle(int index){
    static char buf[PI_BUF]; if(read_all(buf,sizeof buf)<0)return -1;
    static char outb[PI_BUF+512]; int oi=0,pcount=0; char*line=buf;
    while(*line){
        char*eol=strchr(line,'\n'); int len=eol?(int)(eol-line):(int)strlen(line);
        char tmp[256]; int c=len<255?len:255; memcpy(tmp,line,c); tmp[c]=0;
        int cr=(c>0&&tmp[c-1]=='\r'); if(cr)tmp[c-1]=0;
        char*p=tmp; while(*p==' '||*p=='\t')p++;
        int is_plugin=(*p!='[')&&strstr(p,".prx")!=NULL;
        const char*nl=cr?"\r\n":(eol?"\n":"");
        if(is_plugin && pcount==index){
            char reb[300];
            if(*p==';'){char*q=p+1; while(*q==' '||*q=='\t')q++; snprintf(reb,sizeof reb,"%s",q);}
            else snprintf(reb,sizeof reb,"; %s",p);
            oi+=snprintf(outb+oi,sizeof outb-oi,"%s%s",reb,nl); pcount++;
        }else{
            if(is_plugin)pcount++;
            oi+=snprintf(outb+oi,sizeof outb-oi,"%s%s",tmp,nl);
        }
        if(!eol)break; line=eol+1;
    }
    int rc=probe_write_text(pi_path(),outb);
    probe_log("PLUGINS","TOGGLE index=%d rc=%d bytes=%d",index,rc?0:-1,oi);
    return rc?0:-1;
}
