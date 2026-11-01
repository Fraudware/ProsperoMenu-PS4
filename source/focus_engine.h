#pragma once
#include <stdint.h>
struct focus_status{int32_t app_id,launch_request;uint8_t type,pad[3];};
struct focus_engine{int owned,self,return_app,last_rc;};
struct focus_api{int(*self)(struct focus_status*);int(*current)(struct focus_status*);int(*set)(int);};
static inline int focus_step(struct focus_engine*s,const struct focus_api*a,int want){
 struct focus_status self={0},current={0};int rc;
 if(want){
  if(s->owned)return 0;
  if((rc=a->self(&self))!=0 || (rc=a->current(&current))!=0){s->last_rc=rc;return -1;}
  if(self.type!=1 || self.app_id<0 || current.app_id<0){s->last_rc=-22;return -1;}
  if(current.app_id==self.app_id){s->last_rc=0;return 0;}
  if(current.type!=5 && current.type!=4){s->last_rc=-16;return -1;}
  s->self=self.app_id;s->return_app=current.app_id;
  rc=a->set(s->self);s->last_rc=rc;if(rc!=0)return -1;s->owned=1;return 0;
 }
 if(!s->owned)return 0;
 if((rc=a->current(&current))!=0){s->last_rc=rc;return -1;}
 /* Restore current app foreground, not a stale app after an app transition. */
 int target=current.app_id>=0 && current.app_id!=s->self?current.app_id:s->return_app;
 rc=a->set(target);s->last_rc=rc;if(rc!=0)return -1;s->owned=0;return 0;
}
