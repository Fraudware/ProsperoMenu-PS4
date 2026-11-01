#pragma once
/* Never send a zero or unknown ICC profile to the PS4 fan controller. */
static inline int fan_profile_valid(const unsigned char d[28]){
    return d[5]>=40 && d[5]<=85 && d[9]==8;
}
static inline int fan_profile_patch(unsigned char d[28],int target){
    if(!fan_profile_valid(d) || target<40 || target>79)return -1;
    d[5]=(unsigned char)target;return 0;
}
