#pragma once
#include <stddef.h>
#include <stdio.h>
static inline int fan_auto_step(int thr,int cpu,int target){
 /* Nudge the ICC ramp-up trigger to hold the console near TARGET.
  * Hot -> lower trigger (fan engages sooner); cool -> raise it (quieter). */
 if(cpu>0){ if(cpu>target+2 && thr>45)thr-=3; else if(cpu<target-3 && thr<70)thr+=2; }
 if(thr<45)thr=45; if(thr>70)thr=70; return thr;
}
static inline void fan_display(char *out,size_t cap,int cpu,int soc,int icc,int startup,int result){
 char a[16],b[16],c[16];
 if(cpu>=0)snprintf(a,sizeof a,"%d C",cpu);else snprintf(a,sizeof a,"n/a");
 if(soc>=0)snprintf(b,sizeof b,"%d C",soc);else snprintf(b,sizeof b,"n/a");
 if(icc>=0)snprintf(c,sizeof c,"%d C",icc);else snprintf(c,sizeof c,"n/a");
 const char *fs=(cpu<0||icc<0)?"unknown":(icc-cpu>=10)?"low/idle":(icc-cpu>0)?"ramping":"high";
 snprintf(out,cap,"CPU %s | SoC %s | ICC trigger %s | start %d C\nFan: %s (est. from temp vs trigger; PS4 has no RPM sensor) | %s",a,b,c,startup,fs,
          result==-1?"ready":result==0?"ICC readback OK":"ICC write failed");
}
