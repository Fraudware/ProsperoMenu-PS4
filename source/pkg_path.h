#pragma once
/* Build a selected absolute package path only if no component is truncated. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static inline int pkg_candidate(const char *dir,const char *name,char *out,size_t cap){
 if(!dir||dir[0]!='/'||!name||!out||!cap||strchr(name,'/')||strchr(name,'\\')||strstr(name,".."))return 0;
 size_t len=strlen(name);
 if(len<=4 || name[len-4]!='.' ||
    (name[len-3]!='p'&&name[len-3]!='P') ||
    (name[len-2]!='k'&&name[len-2]!='K') ||
    (name[len-1]!='g'&&name[len-1]!='G'))return 0;
 int n=dir[1]?snprintf(out,cap,"%s/%s",dir,name):snprintf(out,cap,"/%s",name);
 return n>=0 && (size_t)n<cap;
}
