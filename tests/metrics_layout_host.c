#include <assert.h>
#include <string.h>
#include "deck-layout-docked.h"
int main(void){
 PmDeckFrame f={0};PmDeckLayout l={0};
 f.flags=1;f.count=7;strcpy(f.title,"ProsperoMenu");
 strcpy(f.subtitle,"FW 13.52  |  PS4 Pro");
 strcpy(f.metrics,"CPU 58 C | SoC 59 C\nIP 192.0.2.1 | Up 1d 02h\nStorage 696/871 GiB free");
 for(unsigned i=0;i<7;i++){f.rows[i].flags=1;strcpy(f.rows[i].label,"Action");}
 pm_deck_layout_docked(&f,1,0,&l);
 assert(strstr(l.labels[PM_LABEL_SUBTITLE].text,"FW 13.52"));
 assert(strstr(l.labels[3].text,"CPU 58 C"));
 assert(strstr(l.labels[4].text,"IP 192.0.2.1"));
 assert(strstr(l.labels[5].text,"Storage"));
 assert(strcmp(l.labels[PM_LABEL_ROW_TEXT].text,"Action")==0);
 assert(l.panels[19].visible);
 PmDeckFrame f2={0};PmDeckLayout l2={0};
 f2.flags=1;f2.count=7;strcpy(f2.title,"ProsperoMenu");
 for(unsigned i=0;i<7;i++){f2.rows[i].flags=1;strcpy(f2.rows[i].label,"Action");}
 pm_deck_layout_docked(&f2,1,0,&l2);
 assert(!l2.panels[19].visible);
 assert(l2.labels[PM_LABEL_ROW_TEXT].y < l.labels[PM_LABEL_ROW_TEXT].y);
 return 0;
}
