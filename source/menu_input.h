#pragma once
#include <stdint.h>
struct menu_input { unsigned visible,row; uint32_t previous; uint64_t repeat_at; int drain; };
static inline int menu_input_step(struct menu_input*s,uint32_t buttons,int connected,uint64_t now,unsigned rows){
 unsigned before=s->visible;
 if(!connected){s->visible=0;s->drain=0;s->previous=0;s->repeat_at=0;return before!=0;}
 if(s->drain && !(buttons&0x10ffff))s->drain=0;
 if((buttons&0x6)==0x6 && (s->previous&0x6)!=0x6){s->visible=!s->visible;if(!s->visible)s->drain=1;}
 if(s->visible && (buttons&~s->previous)&0x2000){s->visible=0;s->drain=1;}
 if(s->visible && rows){
  uint32_t direction=buttons&0x50;
  if(direction && direction!=0x50 && ((buttons&~s->previous)&0x50 || now>=s->repeat_at)){
   s->row=direction==0x40?(s->row+1)%rows:(s->row+rows-1)%rows;
   s->repeat_at=now+(((buttons&~s->previous)&0x50)?350:120);
  }
 }
 s->previous=buttons;return before!=s->visible;
}
