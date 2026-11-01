#include <assert.h>
#include <string.h>
#include "../source/fan_profile.h"
int main(void){unsigned char profile[28]={0},original[28];
 profile[5]=79;profile[9]=8;profile[12]=0x80;memcpy(original,profile,28);
 assert(fan_profile_valid(profile));
 profile[5]=40;assert(fan_profile_valid(profile));   /* a 40 C profile we wrote must read back valid */
 profile[5]=39;assert(!fan_profile_valid(profile));
 profile[5]=79;
 assert(fan_profile_patch(profile,70)==0 && profile[5]==70 && profile[9]==8 && profile[12]==0x80);
 assert(fan_profile_patch(profile,40)==0 && profile[5]==40);
 profile[5]=0;assert(!fan_profile_valid(profile));
 profile[5]=79;assert(fan_profile_patch(profile,39)!=0 && fan_profile_patch(profile,80)!=0);
 assert(fan_profile_patch(original,0)!=0);
 return 0;}
