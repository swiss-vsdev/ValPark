#pragma once
#include <stdint.h>

// ---------- LoRaWAN / TTN (OTAA, EU868) : a remplir depuis la console TTN ----------
static const uint64_t JOIN_EUI = 0x0000000000000000ULL;
static const uint64_t DEV_EUI  = 0x0000000000000000ULL;
static uint8_t APP_KEY[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
// LoRaWAN 1.0.x : NwkKey = AppKey
static uint8_t NWK_KEY[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// ---------- Pins : LilyGo T3-S3 E-Paper (doc officielle) ----------
#define PIN_EPD_BUSY 48
#define PIN_EPD_RST  47
#define PIN_EPD_DC   16
#define PIN_EPD_CS   15
#define PIN_EPD_SCK  14
#define PIN_EPD_MOSI 11

#define PIN_RADIO_SCK  5
#define PIN_RADIO_MISO 3
#define PIN_RADIO_MOSI 6
#define PIN_RADIO_CS   7
#define PIN_RADIO_DIO1 33
#define PIN_RADIO_BUSY 34
#define PIN_RADIO_RST  8
#define PIN_RADIO_POW  35

// ---------- Pins : peripheriques externes (a verifier sur le header) ----------
#define PIN_GPS_RX   44   // ESP32 RX  <- TX du Gravity GNSS (QWIIC UART1)
#define PIN_GPS_TX   43   // ESP32 TX  -> RX du Gravity GNSS (QWIIC UART1)
#define PIN_I2C_SDA  10   // QWIIC IO10 -> SDA MPU6050
#define PIN_I2C_SCL  21   // QWIIC IO21 -> SCL MPU6050
#define PIN_MPU_INT  4    // GPIO RTC (0..21) obligatoire pour le reveil deep sleep
#define PIN_GPS_GATE 12   // grille du MOSFET P : LOW = GPS ON, HIGH = GPS OFF

// ---------- Reglages comportement ----------
#define STILL_SECONDS        15      // arret requis avant verification
#define STILL_DELTA_G        0.05f   // variation d'accel (g) consideree comme "mouvement"
#define GPS_FIX_TIMEOUT_MS   90000UL
#define ARM_DELAY_MS         60000UL // apres parcage : pas de detection de depart pendant 60 s
#define MOTION_THR_2MG       20      // seuil reveil MPU6050 (unites de 2 mg) = 40 mg
#define IDLE_SLEEP_MS        120000UL
#define LORA_TX_RETRIES      3

// 1 = toute position est consideree comme zone de parc (test a la maison). Mettre 0 ensuite.
#define DEBUG_ANY_POSITION_IS_ZONE 1

struct Zone { double latMin, latMax, lonMin, lonMax; const char* name; };
// PLACEHOLDER autour du centre de Sion : remplacer par les rectangles de tes zones bleues
static const Zone ZONES[] = {
  {46.2300, 46.2360, 7.3560, 7.3650, "TEST-SION"},
};
static const int N_ZONES = sizeof(ZONES) / sizeof(ZONES[0]);
