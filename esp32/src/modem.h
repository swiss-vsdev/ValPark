#pragma once
#include <Arduino.h>
#include "config.h"
#include "logic.h"
// Couche AT de prototype : sequence et support HTTPS a verifier sur le firmware A7670E exact.
class Cellular {
 public:
 String receive(const char* marker,uint32_t timeout) {
  String r;uint32_t t=millis();
  while(millis()-t<timeout){
   while(Serial1.available())r+=(char)Serial1.read();
   // On attend la fin d'une ligne, pas seulement le prefixe d'une reponse AT.
   if(r.indexOf(marker)>=0&&r.endsWith("\r\n"))return r;
   if(r.indexOf("ERROR")>=0&&r.endsWith("\r\n"))return r;
   delay(5);
  }return r;
 }
 String command(const String& c,uint32_t timeout=3000,const char* marker="OK") {
  while(Serial1.available())Serial1.read();Serial1.print(c);Serial1.print("\r\n");return receive(marker,timeout);
 }
 bool on() {
  pinMode(MODEM_POWER,OUTPUT);digitalWrite(MODEM_POWER,HIGH);
  Serial1.begin(115200,SERIAL_8N1,MODEM_RX,MODEM_TX);
  pinMode(MODEM_KEY,OUTPUT);digitalWrite(MODEM_KEY,LOW);delay(100);
  digitalWrite(MODEM_KEY,HIGH);delay(1100);digitalWrite(MODEM_KEY,LOW);
  bool ready=false;for(int i=0;i<20;i++){if(command("AT").indexOf("OK")>=0){ready=true;break;}delay(500);}
  if(!ready)return false;command("ATE0");
  String pin=command("AT+CPIN?");
  if(pin.indexOf("SIM PIN")>=0&&strlen(SIM_PIN))command(String("AT+CPIN=\"")+SIM_PIN+"\"");
  command(String("AT+CGDCONT=1,\"IP\",\"")+APN+"\"");return true;
 }
 bool network(uint32_t timeout=90000) {
  uint32_t t=millis();while(millis()-t<timeout){
   String r=command("AT+CEREG?");int n=-1,s=-1;int p=r.indexOf("+CEREG:");
   if(p>=0&&sscanf(r.c_str()+p+7," %d,%d",&n,&s)==2&&(s==1||s==5))return true;
   delay(1500);
  }return false;
 }
 Fix gps(uint32_t timeout=120000) {
  command("AT+CGNSSPWR=1",8000);uint32_t t=millis();Fix f;
  while(millis()-t<timeout){f=parseFix(command("AT+CGNSSINFO").c_str());if(f.valid)break;delay(1500);}
  command("AT+CGNSSPWR=0");return f;
 }
 int post(const char* path,const String& body) {
  command("AT+HTTPTERM");if(command("AT+HTTPINIT").indexOf("OK")<0)return -1;
  String url=String(SERVER_BASE)+path;
  if(command(String("AT+HTTPPARA=\"URL\",\"")+url+"\"").indexOf("OK")<0)return -1;
  command("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  String r=command(String("AT+HTTPDATA=")+body.length()+",15000",5000,"DOWNLOAD");
  if(r.indexOf("DOWNLOAD")<0)return -1;
  Serial1.print(body);if(receive("OK",16000).indexOf("OK")<0)return -1;
  r=command("AT+HTTPACTION=1",65000,"+HTTPACTION:");
  // Une reponse synchrone OK peut preceder l'URC, receive attend bien HTTPACTION.
  int result=httpStatus(r.c_str());command("AT+HTTPTERM");return result;
 }
 void off(){command("AT+CGNSSPWR=0");command("AT+CPOF",6000);Serial1.end();digitalWrite(MODEM_POWER,LOW);}
};
