#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <time.h>
#include <esp_sleep.h>
#include <RadioLib.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <TinyGPSPlus.h>
#include "config.h"
#include "parking_logic.h"

// ---------- Peripheriques ----------
SPIClass epdSPI(FSPI);
SPIClass radioSPI(HSPI);
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
SX1262 radio = new Module(PIN_RADIO_CS, PIN_RADIO_DIO1, PIN_RADIO_RST, PIN_RADIO_BUSY, radioSPI);
LoRaWANNode node(&radio, &EU868);
TinyGPSPlus gps;

// ---------- Etat conserve en deep sleep ----------
enum State : uint8_t { MOVING = 0, PARKED = 1 };
RTC_DATA_ATTR uint8_t rtcState = MOVING;
RTC_DATA_ATTR double  rtcLat = 0, rtcLon = 0;

// ---------- MPU6050 (registres bruts, sans librairie) ----------
#define MPU_ADDR 0x68
static void mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}
static uint8_t mpuRead8(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR); Wire.write(reg); Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}
static bool mpuReadAccelG(float& g) {
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6) return false;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  float x = ax / 16384.0f, y = ay / 16384.0f, z = az / 16384.0f;
  g = sqrtf(x * x + y * y + z * z);
  return true;
}
static void mpuStartContinuous() {
  mpuWrite(0x6B, 0x00);  // reveil, horloge interne
  mpuWrite(0x6C, 0x07);  // gyros en veille, accelerometre actif
  mpuWrite(0x1A, 0x03);  // filtre passe-bas ~44 Hz
  mpuWrite(0x1C, 0x00);  // +-2 g
  mpuWrite(0x38, 0x00);  // pas d'interruption
  delay(50);
}
static void mpuArmMotionWake() {
  mpuWrite(0x6B, 0x00);
  mpuWrite(0x6C, 0x07);
  mpuWrite(0x1A, 0x01);
  mpuWrite(0x1C, 0x01);          // HPF 5 Hz, +-2 g
  mpuWrite(0x1F, MOTION_THR_2MG);
  mpuWrite(0x20, 1);             // duree
  mpuWrite(0x69, 0x15);
  mpuWrite(0x37, 0x20);          // INT actif haut, latch jusqu'a lecture de INT_STATUS
  mpuWrite(0x38, 0x40);          // interruption mouvement
  delay(100);
  mpuRead8(0x3A);                // efface un eventuel flag
  mpuWrite(0x6B, 0x28);          // mode cycle, temperature coupee
  mpuWrite(0x6C, 0x47);          // reveil 5 Hz, gyros en veille
}

// ---------- GPS ON/OFF via MOSFET P ----------
static void gpsPower(bool on) {
  digitalWrite(PIN_GPS_GATE, on ? LOW : HIGH);
  if (on) Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  else    Serial1.end();
}

struct Fix { bool ok; double lat, lon; int wday, hour, minute; };

static Fix acquireFix() {
  Fix f = {false, 0, 0, 0, 0, 0};
  gpsPower(true);
  unsigned long t0 = millis();
  while (millis() - t0 < GPS_FIX_TIMEOUT_MS) {
    while (Serial1.available()) gps.encode(Serial1.read());
    if (gps.location.isValid() && gps.location.age() < 2000 &&
        gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024) {
      f.lat = gps.location.lat(); f.lon = gps.location.lng();
      int64_t epoch = epochFromUtc(gps.date.year(), gps.date.month(), gps.date.day(),
                                   gps.time.hour(), gps.time.minute(), gps.time.second());
      time_t tt = (time_t)epoch;
      struct tm lt;
      localtime_r(&tt, &lt);           // TZ Europe/Zurich (setenv dans setup)
      f.wday = lt.tm_wday; f.hour = lt.tm_hour; f.minute = lt.tm_min;
      f.ok = true;
      break;
    }
    delay(20);
  }
  gpsPower(false);
  return f;
}

static bool inZone(double lat, double lon) {
#if DEBUG_ANY_POSITION_IS_ZONE
  (void)lat; (void)lon;
  return true;
#else
  for (int i = 0; i < N_ZONES; i++)
    if (inRect(lat, lon, ZONES[i].latMin, ZONES[i].latMax, ZONES[i].lonMin, ZONES[i].lonMax)) return true;
  return false;
#endif
}

// ---------- Ecran ----------
static void fmtHM(char* out, int minutes) {
  minutes %= 1440;
  sprintf(out, "%02d:%02d", minutes / 60, minutes % 60);
}
static void drawCentered(const char* s, int y, const GFXfont* font) {
  int16_t x1, y1; uint16_t w, h;
  display.setFont(font);
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((display.width() - w) / 2 - x1, y);
  display.print(s);
}
static void showDisc(const Disc& d) {
  char a[16], b[40];
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);
    if (d.needed) {
      fmtHM(a, d.discMin);
      drawCentered("DISQUE", 22, &FreeMonoBold12pt7b);
      drawCentered(a, 72, &FreeMonoBold24pt7b);
      char dep[8]; fmtHM(dep, d.departMin);
      snprintf(b, sizeof(b), "Depart max %s%s", dep, d.nextDay ? " (+1j)" : "");
      drawCentered(b, 108, &FreeMono9pt7b);
    } else {
      drawCentered("PAS DE DISQUE", 50, &FreeMonoBold12pt7b);
      drawCentered("(hors horaire / dimanche)", 90, &FreeMono9pt7b);
    }
  } while (display.nextPage());
  display.hibernate();
}
static void showMessage(const char* l1, const char* l2) {
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    drawCentered(l1, 55, &FreeMonoBold12pt7b);
    drawCentered(l2, 95, &FreeMono9pt7b);
  } while (display.nextPage());
  display.hibernate();
}

// ---------- LoRaWAN ----------
static bool loraSend(const uint8_t* payload, size_t len) {
  radioSPI.begin(PIN_RADIO_SCK, PIN_RADIO_MISO, PIN_RADIO_MOSI, PIN_RADIO_CS);
  pinMode(PIN_RADIO_POW, OUTPUT);
  digitalWrite(PIN_RADIO_POW, HIGH);
  int16_t st = radio.begin();
  if (st != RADIOLIB_ERR_NONE) { Serial.printf("radio.begin: %d\n", st); return false; }
  radio.setDio2AsRfSwitch(true);
  for (int attempt = 0; attempt < LORA_TX_RETRIES; attempt++) {
    node.beginOTAA(JOIN_EUI, DEV_EUI, NWK_KEY, APP_KEY);
    st = node.activateOTAA();
    if (st == RADIOLIB_LORAWAN_NEW_SESSION || st == RADIOLIB_LORAWAN_SESSION_RESTORED) {
      st = node.sendReceive(payload, len, 1);
      Serial.printf("sendReceive: %d\n", st);
      if (st >= 0) { radio.sleep(); return true; }
    } else {
      Serial.printf("join: %d\n", st);
    }
    delay(10000);
  }
  radio.sleep();
  return false;
}

// ---------- Mouvement ----------
static bool confirmRealMovement() {   // ~5 s d'echantillonnage apres un reveil
  mpuStartContinuous();
  float prev = 0, g; int moving = 0;
  mpuReadAccelG(prev);
  for (int i = 0; i < 100; i++) {
    delay(50);
    if (mpuReadAccelG(g)) { if (fabsf(g - prev) > STILL_DELTA_G) moving++; prev = g; }
  }
  return moving >= 20;
}

static void sleepArmed() {
  mpuArmMotionWake();
  digitalWrite(PIN_GPS_GATE, HIGH);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_MPU_INT, 1);
  Serial.flush();
  esp_deep_sleep_start();
}

static void doPark(const Fix& f) {
  Disc d = computeDisc(f.wday, f.hour, f.minute);
  uint8_t pl[11];
  int n = encodePayload(pl, 1, f.lat, f.lon, d);
  bool sent = loraSend(pl, n);
  showDisc(d);
  Serial.printf("PARK lat=%.5f lon=%.5f disc=%d lora=%d\n", f.lat, f.lon, d.discMin, sent);
  rtcState = PARKED; rtcLat = f.lat; rtcLon = f.lon;
  delay(ARM_DELAY_MS);
  sleepArmed();
}

static void doDepart() {
  Disc none = {false, 0, 0, false};
  uint8_t pl[11];
  int n = encodePayload(pl, 0, rtcLat, rtcLon, none);
  bool sent = loraSend(pl, n);
  Serial.printf("DEPART lora=%d\n", sent);
  showMessage("LIBRE", "depart envoye");
  rtcState = MOVING;
}

// ---------- Programme ----------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_GPS_GATE, OUTPUT);
  digitalWrite(PIN_GPS_GATE, HIGH);              // GPS OFF
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Europe/Zurich
  tzset();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  epdSPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
  display.epd2.selectSPI(epdSPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0 && rtcState == PARKED) {
    if (confirmRealMovement()) doDepart();
    else sleepArmed();                            // porte, vent... : on se rendort
  }
  mpuStartContinuous();
}

void loop() {
  static unsigned long stillSince = 0, checkedAt = 0;
  static bool checkedThisStop = false;
  static float prev = 0;
  float g;
  if (!mpuReadAccelG(g)) { delay(100); return; }
  bool moving = fabsf(g - prev) > STILL_DELTA_G;
  prev = g;
  unsigned long now = millis();
  if (moving) { stillSince = 0; checkedThisStop = false; }
  else if (stillSince == 0) stillSince = now;

  if (!checkedThisStop && stillSince && now - stillSince >= STILL_SECONDS * 1000UL) {
    Fix f = acquireFix();
    checkedThisStop = true; checkedAt = millis();
    if (f.ok && inZone(f.lat, f.lon)) doPark(f);
    Serial.println(f.ok ? "hors zone" : "pas de fix");
  }
  if (checkedThisStop && !moving && millis() - checkedAt > IDLE_SLEEP_MS) sleepArmed();
  delay(50);   // ~20 Hz
}
