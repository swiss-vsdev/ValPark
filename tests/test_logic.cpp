#include <cassert>
#include "../esp32/src/logic.h"
int main(){
 assert(discFor(8,10).displayMinutes==510);
 assert(discFor(8,30).displayMinutes==540);
 assert(discFor(23,59).displayMinutes==0);
 assert(utcEpoch(1970,1,1,0,0,0)==0);
 assert(utcEpoch(2026,10,7,15,0,0)==1791385200LL);
 auto f=parseFix("+CGNSSINFO: 2,05,00,00,4613.986500,N,00721.635400,E,191026,093015.0,500,0\r\n");
 assert(f.valid&&fabs(f.lat-46.2331083333)<1e-7);
 assert(!parseFix("+CGNSSINFO: ,,,,,,,,,,,,\r\n").valid);
 assert(httpStatus("+HTTPACTION: 1,200,15\r\n")==200);
 assert(httpStatus("+HTTPACTION:")==-1);
}
