#include <assert.h>
#include <stdio.h>
#include "../source/menu_themes.h"
int main(void){assert(pm_theme_parse("3\n",2)==3);assert(pm_theme_parse("8\n",2)==0);assert(pm_theme_parse("bogus",5)==0);assert(pm_theme_parse("7",1)==7);puts("theme persistence parser: PASS");}
