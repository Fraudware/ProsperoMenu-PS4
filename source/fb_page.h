#pragma once
#include "menu_actions.h"
/* 28 entries, leaving two navigation slots within the renderer's 30-row cap. */
enum { FB_PAGE_SIZE=28, FB_PREV=-1, FB_NEXT=-2, FB_INVALID=-3 };
static inline unsigned fb_page_rows(unsigned count,unsigned offset){
 if(offset>=count)return count?0:1; /* empty placeholder */
 unsigned entries=count-offset;if(entries>FB_PAGE_SIZE)entries=FB_PAGE_SIZE;
 return entries+(offset>0)+(offset+entries<count);
}
static inline int fb_page_target(unsigned count,unsigned offset,unsigned row){
 if(offset>=count)return FB_INVALID;
 unsigned entries=count-offset;if(entries>FB_PAGE_SIZE)entries=FB_PAGE_SIZE;
 if(offset>0){if(row==0)return FB_PREV;row--;}
 if(row<entries)return (int)(offset+row);
 if(row==entries && offset+entries<count)return FB_NEXT;
 return FB_INVALID;
}
static inline int fb_reset_for_entry(enum action_page from,enum action_page to){
 return from==PAGE_HOME && to==PAGE_FILES;
}
