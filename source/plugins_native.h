#pragma once
#include <stddef.h>
#define PI_MAX 48
struct pi_entry{char label[120];int enabled;};
int plugins_load(struct pi_entry*out,int max);
int plugins_toggle(int index);
