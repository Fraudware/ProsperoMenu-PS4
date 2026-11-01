#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/fan_display.h"
int main(void){
 char s[160];
 /* auto controller: hot lowers trigger, cool raises it, clamped 45..70 */
 assert(fan_auto_step(62,70,65)==59);
 assert(fan_auto_step(62,55,65)==64);
 assert(fan_auto_step(46,80,65)==45);
 assert(fan_auto_step(70,50,65)==70);
 assert(fan_auto_step(62,-1,65)==62);
 fan_display(s,sizeof s,58,59,70,70,0);
 assert(strstr(s,"CPU 58 C") && strstr(s,"ICC trigger 70 C") && strstr(s,"Fan: low/idle") && !strstr(s,"RPM unavailable"));
 fan_display(s,sizeof s,85,86,70,79,0); assert(strstr(s,"Fan: high"));
 fan_display(s,sizeof s,66,66,70,79,0); assert(strstr(s,"Fan: ramping"));
 puts("fan display+auto: PASS");
 return 0;
}
