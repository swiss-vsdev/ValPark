#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <time.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include "config.h"
#include "logic.h"
#include "zones.h"
#include "modem.h"

GxEPD2_3C<GxEPD2_290c,GxEPD2_290c::HEIGHT> screen(GxEPD2_290c(EPD_CS,EPD_DC,EPD_RST,EPD_BUSY));
Preferences prefs; Cellular modem; WebServer web(80);DNSServer dns;
String deviceId,apPassword,csrf;
bool registered=false,setupMode=false;
uint32_t portalStarted=0;
RTC_DATA_ATTR bool parked=false;
RTC_DATA_ATTR double lastLat=0,lastLon=0;
RTC_DATA_ATTR int savedLed=0;
RTC_DATA_ATTR bool pending=false;
RTC_DATA_ATTR char pendingBody[640]={0};
// LED RGB : 0 eteinte, 1 verte, 2 rouge, 3 orange. Cathode commune a GND.
void led(int mode){savedLed=mode;digitalWrite(LED_R,mode==2||mode==3);digitalWrite(LED_G,mode==1||mode==3);digitalWrite(LED_B,LOW);}
void text(const String& a,const String& b="",const String& c="",const String& d="") {
 screen.setRotation(1);screen.setFullWindow();screen.firstPage();
 do{screen.fillScreen(GxEPD_WHITE);screen.setTextColor(GxEPD_BLACK);screen.setFont(nullptr);
 screen.setTextSize(2);screen.setCursor(5,12);screen.println(a);screen.println(b);screen.println(c);screen.println(d);
 }while(screen.nextPage());screen.hibernate();
}
void regWrite(uint8_t r,uint8_t value){Wire.beginTransmission(0x68);Wire.write(r);Wire.write(value);Wire.endTransmission();}
bool accel(float v[3]) {
 Wire.beginTransmission(0x68);Wire.write(0x3B);if(Wire.endTransmission(false)!=0)return false;
 if(Wire.requestFrom(0x68,6)!=6)return false;
 for(int i=0;i<3;i++){int h=Wire.read(),l=Wire.read();v[i]=(int16_t)((h<<8)|l)/16384.f;}return true;
}
void mpuActive(){regWrite(0x6B,0);regWrite(0x6C,7);regWrite(0x1A,3);regWrite(0x1C,0);regWrite(0x38,0);delay(50);}
void sleepNow() {
 // Configuration detecteur de mouvement MPU6050 a valider sur le module physique.
 regWrite(0x6B,0);regWrite(0x1A,1);regWrite(0x1C,1);regWrite(0x1F,20);regWrite(0x20,5);
 regWrite(0x37,0x20);regWrite(0x38,0x40);
 Wire.beginTransmission(0x68);Wire.write(0x3A);Wire.endTransmission(false);Wire.requestFrom(0x68,1);if(Wire.available())Wire.read();
 for(int pin:{LED_R,LED_G,LED_B,MODEM_POWER})gpio_hold_en((gpio_num_t)pin);
 gpio_deep_sleep_hold_en();
 esp_sleep_enable_ext0_wakeup((gpio_num_t)MPU_INT,1);
 // Reessai d'un evenement en attente ; reveil periodique aussi apres inscription echouee.
 if(pending||!registered)esp_sleep_enable_timer_wakeup(5ULL*60*1000000);
 esp_deep_sleep_start();
}
bool sendBody(const char* endpoint,const String& body) {
 bool ok=false;if(modem.on()&&modem.network()){int status=modem.post(endpoint,body);ok=status>=200&&status<300;}
 modem.off();return ok;
}
void addAuth(JsonDocument& doc){doc["token"]=DEVICE_TOKEN;doc["device"]=deviceId;}
void savePending(){prefs.putString("pending",pendingBody);}
bool retryPending(){
 if(!pending)return true;
 if(sendBody("/api/event",String(pendingBody))){pending=false;pendingBody[0]=0;prefs.remove("pending");return true;}
 return false;
}
void queueEvent(bool isParked,const Fix& f,const Zone* z,int64_t arrival) {
 // Ne jamais ecraser un evenement non livre : reessayer avant de creer le suivant.
 if(pending&&!retryPending()){led(2);text("ERREUR","Evenement en attente");return;}
 StaticJsonDocument<640> doc;addAuth(doc);
 char eid[50];snprintf(eid,sizeof eid,"%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
 doc["event_id"]=eid;doc["state"]=isParked?"PARKED":"DEPART";
 doc["lat"]=f.lat;doc["lon"]=f.lon;doc["arrival_epoch"]=arrival;doc["zone"]=z?z->name:"TEST";
 doc["disc"]=nullptr;
 if(isParked&&z&&z->blue){time_t epoch=arrival;tm lt{};localtime_r(&epoch,&lt);Disc d=discFor(lt.tm_hour,lt.tm_min);
 char hh[8];snprintf(hh,sizeof hh,"%02d:%02d",d.displayMinutes/60,d.displayMinutes%60);doc["disc"]=hh;text("DISQUE",hh,"Heure d'arrivee");}
 serializeJson(doc,pendingBody,sizeof pendingBody);pending=true;savePending();
 bool ok=retryPending();led(ok?(isParked?1:0):2);
 if(!isParked)text("LIBRE",ok?"Depart recu":"Depart en attente");
}
String page(const String& message="") {
 String html="<!doctype html><html lang=fr><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Configuration Parking</title>";
 html+="<style>body{font:18px sans-serif;max-width:460px;margin:2em auto;padding:1em}input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0}</style><h1>Configuration</h1><p>Module : "+deviceId+"</p><p>"+message+"</p>";
 html+="<p>PoC : utiliser uniquement des identifiants ParkingPay fictifs. Aucune validation ParkingPay ni paiement n'est realise.</p>";
 html+="<form method=post action=/setup><input type=hidden name=csrf value='"+csrf+"'><label>Identifiant ParkingPay<input name=user maxlength=64 autocomplete=off required></label>";
 html+="<label>Mot de passe ParkingPay<input type=password name=pass maxlength=64 autocomplete=new-password required></label>";
 html+="<label><input type=checkbox name=consent value=yes required>J'accepte l'envoi au VPS configure</label><button>Enregistrer</button></form></html>";return html;
}
void startPortal() {
 setupMode=true;led(3);WiFi.mode(WIFI_AP);String ssid="Parking-"+deviceId.substring(deviceId.length()-6);
 WiFi.softAP(ssid.c_str(),apPassword.c_str(),1,false,1);dns.start(53,"*",WiFi.softAPIP());
 char nonce[40];snprintf(nonce,sizeof nonce,"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());csrf=nonce;
 web.on("/",HTTP_GET,[]{web.sendHeader("Cache-Control","no-store");web.send(200,"text/html; charset=utf-8",page());});
 web.on("/setup",HTTP_POST,[]{
  if(web.arg("csrf")!=csrf||web.arg("consent")!="yes"){web.send(403,"text/plain","Refuse");return;}
  String user=web.arg("user"),pw=web.arg("pass");
  if(user.length()<1||user.length()>64||pw.length()<1||pw.length()>64){web.send(400,"text/plain","1 a 64 caracteres");return;}
  if(!ALLOW_CREDENTIAL_UPLOAD){web.send(409,"text/html; charset=utf-8",page("Envoi bloque : validation TLS a terminer. Mettre ALLOW_CREDENTIAL_UPLOAD=1 uniquement avec un compte fictif pour tester."));return;}
  StaticJsonDocument<512> doc;addAuth(doc);doc["pp_user"]=user;doc["pp_pass"]=pw;String body;serializeJson(doc,body);
  // Pas de stockage des identifiants dans la flash du module.
  bool ok=sendBody("/api/register",body);body="";pw="";
  if(ok){prefs.putBool("registered",true);web.send(200,"text/plain","Enregistrement recu par le VPS (pas de validation ParkingPay). Redemarrage.");delay(1000);ESP.restart();}
  else{led(2);web.send(502,"text/html; charset=utf-8",page("VPS injoignable : rien n'a ete conserve sur le module."));}
 });
 web.onNotFound([]{web.sendHeader("Location","http://192.168.4.1/",true);web.send(302,"text/plain","");});web.begin();
 portalStarted=millis();text(ssid,"Code: "+apPassword,"192.168.4.1",deviceId);
}
void setup(){
 Serial.begin(115200);
 gpio_deep_sleep_hold_dis();for(int pin:{LED_R,LED_G,LED_B,MODEM_POWER}){gpio_hold_dis((gpio_num_t)pin);pinMode(pin,OUTPUT);}
 digitalWrite(MODEM_POWER,LOW);led(savedLed);
 prefs.begin("parking",false);
 char id[24];snprintf(id,sizeof id,"PK-%012llX",(unsigned long long)(ESP.getEfuseMac()&0xffffffffffffULL));deviceId=id;
 apPassword=prefs.getString("ap_pw","");if(apPassword.length()<12){char pw[17];snprintf(pw,sizeof pw,"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());apPassword=pw;prefs.putString("ap_pw",apPassword);}
 registered=prefs.getBool("registered",false);
 String queued=prefs.getString("pending","");if(queued.length()){snprintf(pendingBody,sizeof pendingBody,"%s",queued.c_str());pending=true;}
 setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
 Wire.begin(MPU_SDA,MPU_SCL);mpuActive();
 SPI.begin(EPD_SCK,-1,EPD_MOSI,EPD_CS);screen.init(0,true,2,false);
 pinMode(0,INPUT_PULLUP);bool resetRequested=true;
 // BOOT maintenu 3 s au demarrage apres sortie du bootloader : retour au setup.
 for(int i=0;i<30;i++){if(digitalRead(0)!=LOW){resetRequested=false;break;}delay(100);}
 if(resetRequested){prefs.putBool("registered",false);registered=false;}
 if(!registered){startPortal();return;}
 if(pending){bool ok=retryPending();led(ok?(parked?1:0):2);}
 // Un reveil timer ne signifie jamais un depart.
 if(parked&&esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER)sleepNow();
 if(parked){
  float a[3],b[3];int changes=0;if(accel(a))for(int i=0;i<100;i++){delay(50);if(accel(b)){float d=0;for(int k=0;k<3;k++){d+=fabs(b[k]-a[k]);a[k]=b[k];}if(d>0.06)changes++;}}
  if(changes<15)sleepNow();
  Fix f;f.valid=true;f.lat=lastLat;f.lon=lastLon;queueEvent(false,f,nullptr,0);parked=false;
 }
}
void loop(){
 if(setupMode){dns.processNextRequest();web.handleClient();if(millis()-portalStarted>900000){web.stop();dns.stop();WiFi.mode(WIFI_OFF);sleepNow();}delay(5);return;}
 static bool initialized=false,checked=false;static float previous[3];static uint32_t since=0;
 float a[3];if(!accel(a)){led(2);delay(100);return;}
 if(!initialized){for(int k=0;k<3;k++)previous[k]=a[k];initialized=true;since=millis();}
 float delta=0;for(int k=0;k<3;k++){delta+=fabs(a[k]-previous[k]);previous[k]=a[k];}
 if(delta>0.06){since=millis();checked=false;}
 if(!checked&&millis()-since>=15000){
  checked=true;uint32_t checkTime=millis();Fix f;if(modem.on())f=modem.gps();modem.off();
  if(!f.valid){led(2);return;}
  // Estime l'heure du debut de l'arret, pas celle de l'obtention tardive du fix.
  int64_t arrival=f.epoch-(millis()-since)/1000;
  (void)checkTime;const Zone* zone=locate(f);
  if(zone||TEST_ANY_ZONE){
   static const Zone test={0,0,0,0,"TEST",true};if(!zone)zone=&test;
   queueEvent(true,f,zone,arrival);parked=true;lastLat=f.lat;lastLon=f.lon;
   delay(10000);sleepNow();
  }
 }
 // Hors zone : pas de nouvelle verification tant qu'un nouveau mouvement n'est pas detecte.
 if(checked&&millis()-since>180000)sleepNow();
 delay(50);
}
