#include <assert.h>
#include <stdio.h>
#include "../source/menu_input.h"
int main(void){struct menu_input s={0};menu_input_step(&s,0xc00,1,0,8);assert(!s.visible);menu_input_step(&s,0,1,0,8);menu_input_step(&s,0x6,1,0,8);assert(s.visible==1);menu_input_step(&s,0x6,1,10,8);assert(s.visible==1);menu_input_step(&s,0,1,20,8);menu_input_step(&s,0x40,1,30,8);assert(s.row==1);
menu_input_step(&s,0x40,1,100,8);assert(s.row==1);
menu_input_step(&s,0x40,1,400,8);assert(s.row==2);
menu_input_step(&s,0,1,410,8);menu_input_step(&s,0x10,1,420,8);assert(s.row==1);
menu_input_step(&s,0x2000,1,430,8);assert(s.visible==0);assert(s.drain);
menu_input_step(&s,0x2000,1,440,8);assert(s.drain);
menu_input_step(&s,0,1,450,8);assert(!s.drain);
menu_input_step(&s,0x6,1,460,8);assert(s.visible);
menu_input_step(&s,0,0,470,8);assert(!s.visible && !s.drain);
puts("toggle, navigation/repeat, close/release, disconnect passed");return 0;}
