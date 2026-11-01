#include <assert.h>
#include <stdio.h>
#include "../source/menu_actions.h"
int main(void){struct action_menu a={0};
 assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_GAMES);
 a=(struct action_menu){0};assert(action_cross(&a,1,11)==REQ_NONE && a.page==PAGE_PLUGINS);
 a=(struct action_menu){0};assert(action_cross(&a,2,11)==REQ_NONE && a.page==PAGE_FAN);
 a=(struct action_menu){0};assert(action_cross(&a,2,11)==REQ_NONE && a.page==PAGE_FAN);
 action_back(&a);assert(a.page==PAGE_HOME);
 a=(struct action_menu){0};assert(action_cross(&a,3,11)==REQ_NONE && a.page==PAGE_FILES);
 action_back(&a);assert(a.page==PAGE_HOME);
 a=(struct action_menu){0};assert(action_cross(&a,4,11)==REQ_SETTINGS);
 a=(struct action_menu){0};assert(action_cross(&a,5,11)==REQ_NONE && a.page==PAGE_THEMES);
 assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_PROSPERO_THEMES);
 assert(action_cross(&a,3,11)==REQ_NONE && a.theme_index==3 && a.page==PAGE_PROSPERO_THEMES);
 action_back(&a);assert(a.page==PAGE_THEMES);
 assert(action_cross(&a,1,11)==REQ_PS4_THEMES && a.page==PAGE_THEMES);
 action_back(&a);assert(a.page==PAGE_HOME);
 action_back(&a);assert(a.page==PAGE_HOME);

 a=(struct action_menu){0};assert(action_cross(&a,6,11)==REQ_NONE && a.page==PAGE_POWER);
 assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_CONFIRM);
 assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_POWER);
 assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_CONFIRM && action_cross(&a,1,11)==REQ_REBOOT);
 a=(struct action_menu){0};assert(action_cross(&a,6,11)==REQ_NONE && a.page==PAGE_POWER);
 assert(action_cross(&a,1,11)==REQ_NONE && a.page==PAGE_CONFIRM && action_cross(&a,1,11)==REQ_SHUTDOWN);
 a=(struct action_menu){0};assert(action_cross(&a,6,11)==REQ_NONE);action_back(&a);assert(a.page==PAGE_HOME);
 a=(struct action_menu){0};assert(action_cross(&a,6,11)==REQ_NONE && a.page==PAGE_POWER);action_back(&a);assert(a.page==PAGE_HOME);
 a=(struct action_menu){0};assert(action_cross(&a,0,11)==REQ_NONE);assert(action_cross(&a,1,11)==REQ_NONE && a.page==PAGE_CONFIRM);
 assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_GAMES);
 a=(struct action_menu){0};a.page=PAGE_PLUGINS;assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_PLUGINS);
 a=(struct action_menu){0};a.page=PAGE_FAN;assert(action_cross(&a,3,11)==REQ_NONE && a.page==PAGE_FAN);
 a=(struct action_menu){0};a.page=PAGE_FILES;assert(action_cross(&a,0,11)==REQ_NONE && a.page==PAGE_FILES);
 puts("flat PS4 options: PASS");return 0;}
