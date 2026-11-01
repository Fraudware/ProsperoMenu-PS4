#pragma once
/* BGFT returns ALREADY_INITIALIZED when ShellUI has initialized it first. */
static inline int bgft_init_usable(int rc){return rc==0 || (unsigned)rc==0x80990001u;}
