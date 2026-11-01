#include <assert.h>
#include "../source/fb_page.h"
int main(void){
 assert(fb_page_rows(22,0)==22);
 assert(fb_page_rows(40,0)==29); /* 28 entries + Next */
 assert(fb_page_target(40,0,27)==27);
 assert(fb_page_target(40,0,28)==FB_NEXT);
 assert(fb_page_rows(40,28)==13); /* Previous + 12 entries */
 assert(fb_page_target(40,28,0)==FB_PREV);
 assert(fb_page_target(40,28,1)==28);
 assert(fb_page_target(40,28,12)==39);
 assert(fb_page_target(40,28,13)==FB_INVALID);
 assert(fb_page_rows(60,28)==30); /* Previous + 28 entries + Next */
 assert(fb_page_target(60,28,29)==FB_NEXT);
 assert(fb_page_target(60,56,0)==FB_PREV);
 assert(fb_page_target(60,56,4)==59);
 assert(fb_page_target(60,56,5)==FB_INVALID);
 assert(fb_page_target(1,0,1)==FB_INVALID);
 assert(fb_reset_for_entry(PAGE_HOME,PAGE_FILES));
 assert(!fb_reset_for_entry(PAGE_CONFIRM,PAGE_FILES));
 assert(!fb_reset_for_entry(PAGE_FILES,PAGE_FILES));
 return 0;
}
