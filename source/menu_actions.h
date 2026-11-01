#pragma once
#include <stdint.h>
enum action_page{PAGE_HOME,PAGE_GAMES,PAGE_TOOLS,PAGE_PLUGINS,PAGE_INFO,PAGE_CONFIRM,PAGE_THEMES,PAGE_PROSPERO_THEMES,PAGE_POWER,PAGE_FAN,PAGE_FILES,PAGE_CHEATS};
enum action_request{REQ_NONE,REQ_CLOSE,REQ_SETTINGS,REQ_PS4_THEMES,REQ_LAUNCH,REQ_REBOOT,REQ_SHUTDOWN,REQ_INSTALL};
struct action_menu{enum action_page page,return_page;enum action_request confirm;unsigned game_index,offset,theme_index,cheats;};
static inline enum action_request action_cross(struct action_menu*a,unsigned row,unsigned games){
 if(a->page==PAGE_CONFIRM){if(row==0){a->page=a->return_page;return REQ_NONE;}if(row==1)return a->confirm;return REQ_NONE;}
 if(a->page==PAGE_HOME){
  if(a->cheats){ if(row==1){a->page=PAGE_CHEATS;return REQ_NONE;} if(row>1)row--; }
  if(row==0)a->page=PAGE_GAMES;
  else if(row==1)a->page=PAGE_PLUGINS;
  else if(row==2)a->page=PAGE_FAN;
  else if(row==3)a->page=PAGE_FILES;
  else if(row==4)return REQ_SETTINGS;
  else if(row==5)a->page=PAGE_THEMES;
  else if(row==6)a->page=PAGE_POWER;
 }else if(a->page==PAGE_THEMES){
  if(row==0)a->page=PAGE_PROSPERO_THEMES;
  else if(row==1)return REQ_PS4_THEMES;
 }else if(a->page==PAGE_POWER){
  if(row==0||row==1){a->return_page=PAGE_POWER;a->confirm=row==0?REQ_REBOOT:REQ_SHUTDOWN;a->page=PAGE_CONFIRM;}
 }else if(a->page==PAGE_PROSPERO_THEMES){
  if(row<8)a->theme_index=row;
 }
 else if(a->page==PAGE_GAMES && a->offset<=games && row<games-a->offset && row<20){
  a->game_index=a->offset+row;a->confirm=REQ_LAUNCH;a->return_page=PAGE_GAMES;a->page=PAGE_CONFIRM;
 }else if(a->page==PAGE_GAMES){
  unsigned left=games>a->offset?games-a->offset:0,n=left<20?left:20;
  if(left>20 && row==n)a->offset+=20;else{a->offset=0;a->page=PAGE_HOME;}
 }else if(a->page==PAGE_INFO)a->page=PAGE_HOME;
 else if(a->page==PAGE_PLUGINS||a->page==PAGE_FAN||a->page==PAGE_FILES||a->page==PAGE_CHEATS){/* handled in runtime */}
 return REQ_NONE;
}
static inline void action_back(struct action_menu*a){
 if(a->page==PAGE_CONFIRM)a->page=a->return_page;
 else if(a->page==PAGE_PROSPERO_THEMES)a->page=PAGE_THEMES;
 else if(a->page==PAGE_POWER)a->page=PAGE_HOME;
 else{a->page=PAGE_HOME;a->offset=0;}
}
