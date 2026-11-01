/* Read-only directory listing. GPL-3.0-or-later. No writes, no file content reads. */
#include "files_native.h"
#include "probe_common.h"
#include <orbis/libkernel.h>
#include <string.h>
#include <stdint.h>
extern int32_t sceKernelGetdents(int32_t,char*,int);
int files_list(const char*path,struct fb_entry*out,int max){
    int fd=sceKernelOpen(path,0,0);
    if(fd<0){probe_log("FILES","OPEN_FAIL path=%.120s fd=%d",path,fd);return -1;}
    char buf[8192];int count,n=0;
    while((count=sceKernelGetdents(fd,buf,sizeof buf))>0 && n<max){
        for(int off=0;off+8<=count && n<max;){
            uint16_t reclen;memcpy(&reclen,buf+off+4,sizeof reclen);
            unsigned char type=(unsigned char)buf[off+6];
            unsigned namelen=(unsigned char)buf[off+7];
            if(reclen<9||off+reclen>count||namelen>=(unsigned)(reclen-8))break;
            const char*nm=buf+off+8;
            int dot=(namelen==1&&nm[0]=='.')||(namelen==2&&nm[0]=='.'&&nm[1]=='.');
            if(!dot){int c=namelen<255?(int)namelen:255;memcpy(out[n].name,nm,c);out[n].name[c]=0;out[n].is_dir=(type==4);n++;}
            off+=reclen;
        }
    }
    sceKernelClose(fd);
    probe_log("FILES","LIST path=%.120s entries=%d",path,n);
    return n;
}
