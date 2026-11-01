#include <assert.h>
#include <stdio.h>
#include "../source/focus_engine.h"
static int target,sets,report_self;static int self(struct focus_status*s){*s=(struct focus_status){.app_id=42,.type=1};return 0;}
static int current(struct focus_status*s){*s=(struct focus_status){.app_id=report_self?42:99,.type=report_self?1:5};return 0;}
static int set(int app){target=app;sets++;return 0;}
int main(void){struct focus_engine s={0};const struct focus_api a={self,current,set};assert(focus_step(&s,&a,1)==0);assert(s.owned && target==42 && sets==1);assert(focus_step(&s,&a,1)==0 && sets==1);report_self=1;assert(focus_step(&s,&a,0)==0);assert(!s.owned && target==99 && sets==2);puts("native acquire/release ownership passed");return 0;}
