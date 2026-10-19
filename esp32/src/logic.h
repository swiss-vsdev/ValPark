#pragma once
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
struct Disc { int displayMinutes; }; 
// Trait de demi-heure strictement suivant l'arrivee. L'heure affichee reste figee.
inline Disc discFor(int h,int m) { return {((h*60+m)/30+1)*30%1440}; }
inline int64_t utcEpoch(int y,int m,int d,int h,int mi,int s) {
 y-=m<=2; int era=(y>=0?y:y-399)/400; unsigned yo=y-era*400;
 unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
 return ((int64_t)era*146097+yo*365+yo/4-yo/100+doy-719468)*86400+h*3600+mi*60+s;
}
struct Fix { double lat=0,lon=0; int64_t epoch=0; bool valid=false; };
// Format CGNSSINFO A76xx attendu : mode,GPS,GLONASS,BEIDOU,lat,N/S,lon,E/W,date,time,...
inline Fix parseFix(const char* text) {
 Fix f; const char* p=strstr(text,"+CGNSSINFO:"); if(!p)return f;
 p+=11; char b[300]; snprintf(b,sizeof b,"%s",p); char* end=strpbrk(b,"\r\n");if(end)*end=0;
 char* a[20];int n=0;char* q=b;
 do { a[n++]=q;char* c=strchr(q,',');if(!c)break;*c=0;q=c+1; }while(n<20);
 if(n<10||!a[4][0]||!a[6][0]||strlen(a[8])!=6||strlen(a[9])<6)return f;
 if((a[5][0]!='N'&&a[5][0]!='S')||(a[7][0]!='E'&&a[7][0]!='W'))return f;
 double la=atof(a[4]),lo=atof(a[6]);int ld=la/100,od=lo/100;
 f.lat=ld+(la-ld*100)/60;f.lon=od+(lo-od*100)/60;
 if(a[5][0]=='S')f.lat=-f.lat;if(a[7][0]=='W')f.lon=-f.lon;
 int d,m,y,h,mi,s;if(sscanf(a[8],"%2d%2d%2d",&d,&m,&y)!=3||sscanf(a[9],"%2d%2d%2d",&h,&mi,&s)!=3)return Fix{};
 if(y<24||m<1||m>12||d<1||d>31||h>23||mi>59||s>60||fabs(f.lat)>90||fabs(f.lon)>180)return Fix{};
 f.epoch=utcEpoch(2000+y,m,d,h,mi,s);f.valid=true;return f;
}
inline int httpStatus(const char* s) {
 const char* p=strstr(s,"+HTTPACTION:");int method,code,len;
 return p&&sscanf(p+12," %d,%d,%d",&method,&code,&len)==3?code:-1;
}
