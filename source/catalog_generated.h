#pragma once
struct catalog_item{const char *id,*title;};
static const struct catalog_item catalog[]={
{"APOL00004","Apollo Save Tool"},
{"CUSA00133","BATMAN\u2122: ARKHAM KNIGHT"},
{"PKGI13337","FPKGi"},
{"MANU90003","FPKGi Data"},
{"FRDW00001","Fraudware Overlay Diagnostic"},
{"FRDW00002","Fraudware ProsperoMenu"},
{"ITEM00001","Itemzflow Game Manager"},
{"LAPY20009","PS4-Xplorer 2.0"},
{"NPXS39041","Store"},
{"CUSA00341","Uncharted 4: A Thief\u2019s End\u2122"},
{"VITA00004","VITA4"},
};
#define CATALOG_COUNT (sizeof(catalog)/sizeof(catalog[0]))
static const char *configured_plugins[]={"afr.prx","game_patch.prx","no_share_watermark.prx","prosperomenu_dev.prx"};
#define PLUGIN_COUNT (sizeof(configured_plugins)/sizeof(configured_plugins[0]))
