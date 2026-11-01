#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct storage_values{uint64_t total,available;};
static inline uint64_t storage_u64(const unsigned char*p){uint64_t v;memcpy(&v,p,8);return v;}
static inline int storage_decode(const unsigned char*d,size_t n,struct storage_values*s){
 if(!d||!s||n<56)return -1;
 uint64_t size=storage_u64(d+16),blocks=storage_u64(d+32),free_blocks=storage_u64(d+40),avail=storage_u64(d+48);
 if(size<512||size>1048576||(size&(size-1))||!blocks||blocks>UINT64_MAX/size||free_blocks>blocks)return -1;
 if((int64_t)avail<0)avail=0;
 if(avail>blocks)return -1;
 s->total=size*blocks;s->available=size*avail;return 0;
}
