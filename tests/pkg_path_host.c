#include <assert.h>
#include <string.h>
#include "../source/pkg_path.h"
int main(void){
 char out[64];
 assert(pkg_candidate("/user/data", "a.PKG", out, sizeof out)==1);
 assert(strcmp(out,"/user/data/a.PKG")==0);
 assert(pkg_candidate("/", "game.pkg", out, sizeof out)==1);
 assert(strcmp(out,"/game.pkg")==0);
 assert(pkg_candidate("/mnt/usb0", "readme.txt", out, sizeof out)==0);
 assert(pkg_candidate("/mnt/usb0", "../bad.pkg", out, sizeof out)==0);
 assert(pkg_candidate("/mnt/usb0", "a/b.pkg", out, sizeof out)==0);
 char small[8];assert(pkg_candidate("/user/data", "game.pkg", small, sizeof small)==0);
 assert(pkg_candidate("/user/data", "g.pkg", out, 0)==0);
 return 0;
}
