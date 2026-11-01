#pragma once
#include <stddef.h>
struct fb_entry{char name[256];int is_dir;};
int files_list(const char*path,struct fb_entry*out,int max);
