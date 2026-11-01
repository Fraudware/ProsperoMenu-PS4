#include <assert.h>
#include "../source/pkg_init.h"
int main(void){
 assert(bgft_init_usable(0));
 assert(bgft_init_usable((int)0x80990001u));
 assert(!bgft_init_usable((int)0x80990086u));
 assert(!bgft_init_usable(-1));
 return 0;
}
