// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#define PM_THEME_COUNT 8
struct pm_theme_preset{const char *name;uint32_t accent,fill,scroll;};
/* RGBA8 presets adapted from StonedModder Prospero-Menu v1.1.0 settings.mjs. */
static const struct pm_theme_preset pm_theme_presets[PM_THEME_COUNT]={
 {"Tesla teal (default)",0x00FFDDFFu,0x0B0B0BFFu,0x555555FFu},
 {"PlayStation blue",0x0070F0FFu,0x061226FFu,0x2A4D7AFFu},
 {"Light blue",0x6EC6FFFFu,0x0A1620FFu,0x3C6E8FFFu},
 {"Purple",0xB07CFFFFu,0x150B24FFu,0x5B4380FFu},
 {"Crimson",0xFF3B5CFFu,0x22080DFFu,0x7A2A38FFu},
 {"Amber",0xFFB020FFu,0x231806FFu,0x7A5A20FFu},
 {"Emerald",0x2EE59DFFu,0x07201AFFu,0x2A6B52FFu},
 {"Monochrome",0xFFFFFFFFu,0x111111FFu,0x777777FFu}
};
static inline void pm_theme_rgba(uint32_t rgba,float out[4]){
 out[0]=((rgba>>24)&255)/255.f;out[1]=((rgba>>16)&255)/255.f;
 out[2]=((rgba>>8)&255)/255.f;out[3]=(rgba&255)/255.f;
}
static inline unsigned pm_theme_parse(const char *text,unsigned length){
 if(!length || length>2 || text[0]<'0' || text[0]>='0'+PM_THEME_COUNT)return 0;
 if(length==2 && text[1]!='\n')return 0;
 return (unsigned)(text[0]-'0');
}
