/* GPL-3.0. Native Lnc controller-focus capture, no app injection or code patch. */
#include "probe_common.h"
#include "focus_engine.h"
#include <stdatomic.h>
#include <orbis/libkernel.h>
extern int sceLncUtilGetAppStatus(struct focus_status*);
extern int sceLncUtilGetAppFocusedAppStatus(struct focus_status*);
extern int sceLncUtilSetControllerFocus(int);
_Static_assert(sizeof(struct focus_status)==12,"PS4 13.52 Lnc status IPC ABI");
static OrbisPthread thread;
static atomic_int requested,acknowledged,ready,stop_thread,owned_snapshot,rc_snapshot;
static const struct focus_api api={sceLncUtilGetAppStatus,sceLncUtilGetAppFocusedAppStatus,sceLncUtilSetControllerFocus};
static void *focus_worker(void *unused){
 (void)unused;struct focus_engine engine={0};
 struct focus_status self={0},current={0};int a=api.self(&self),b=api.current(&current);
 probe_log("FOCUS","STATUS self_rc=%x self=%x type=%u current_rc=%x current=%x type=%u",a,self.app_id,self.type,b,current.app_id,current.type);
 atomic_store(&ready,1);
 while(!atomic_load(&stop_thread)){
  int want=atomic_load(&requested),ack=atomic_load(&acknowledged);
  if(want!=ack && !(want && ack<0)){
   int result=focus_step(&engine,&api,want);
   atomic_store(&rc_snapshot,engine.last_rc);atomic_store(&owned_snapshot,engine.owned);
   probe_log("FOCUS","TRANSFER want=%d result=%d native_rc=%x self=%x return_app=%x owned=%d",want,result,engine.last_rc,engine.self,engine.return_app,engine.owned);
   atomic_store(&acknowledged,result==0?want:-1);
  }
  sceKernelUsleep(10000);
 }
 return NULL;
}
int pm_focus_start(void){return scePthreadCreate(&thread,NULL,focus_worker,NULL,"pm_focus_capture");}
int pm_native_input_capture(int capture){
 capture=!!capture;atomic_store(&requested,capture);
 if(!atomic_load(&ready))return 1;
 int ack=atomic_load(&acknowledged);
 if(ack<0 && capture)return -1;
 return ack==capture?0:1;
}
int pm_focus_state(void){return atomic_load(&acknowledged);}
int pm_focus_owned(void){return atomic_load(&owned_snapshot);}
int pm_focus_rc(void){return atomic_load(&rc_snapshot);}
void pm_focus_stop_when_released(void){if(!atomic_load(&owned_snapshot) && !atomic_load(&requested))atomic_store(&stop_thread,1);}
