#pragma once
// Logique pure (sans Arduino) : testable sur PC.
#include <stdint.h>

struct Disc {
  bool needed;      // un disque est-il necessaire ?
  int  discMin;     // heure a regler (minutes depuis 00:00), multiple de 30 (peut valoir 1440 = 24:00)
  int  departMin;   // heure limite de depart (minutes depuis 00:00)
  bool nextDay;     // depart le lendemain
};

// wday : 0 = dimanche ... 6 = samedi. Regle suisse : fleche sur le trait qui SUIT l'arrivee.
inline Disc computeDisc(int wday, int hour, int minute) {
  int m = hour * 60 + minute;
  if (wday == 0)                   return {false, 0, 0, false};        // dimanche
  if (m >= 19 * 60 || m < 8 * 60)  return {false, 0, 8 * 60, false};   // 19h00-07h59
  int disc = (m / 30 + 1) * 30;
  if (m >= 11 * 60 + 30 && m < 13 * 60 + 30) return {true, disc, 14 * 60 + 30, false};
  if (m >= 18 * 60)                          return {true, disc, 9 * 60, true};
  return {true, disc, disc + 60, false};
}

// Heure UTC -> epoch (algorithme days_from_civil, sans timegm)
inline int64_t epochFromUtc(int y, int mo, int d, int hh, int mi, int ss) {
  y -= mo <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = era * 146097 + doe - 719468;
  return days * 86400 + hh * 3600 + mi * 60 + ss;
}

inline bool inRect(double lat, double lon, double latMin, double latMax, double lonMin, double lonMax) {
  return lat >= latMin && lat <= latMax && lon >= lonMin && lon <= lonMax;
}

// Payload LoRaWAN, 11 octets :
// [0] etat (1 = gare, 0 = depart)   [1..4] lat*1e5 int32 BE   [5..8] lon*1e5 int32 BE
// [9] slot disque (minutes/30, 0..48) ou 0xFF   [10] slot depart (minutes/30) | 0x80 si lendemain, ou 0xFF
inline void putI32(uint8_t* p, int32_t v) {
  uint32_t u = (uint32_t)v;
  p[0] = u >> 24; p[1] = u >> 16; p[2] = u >> 8; p[3] = u;
}
inline int encodePayload(uint8_t* out, uint8_t state, double lat, double lon, const Disc& d) {
  out[0] = state;
  putI32(out + 1, (int32_t)(lat * 100000.0 + (lat >= 0 ? 0.5 : -0.5)));
  putI32(out + 5, (int32_t)(lon * 100000.0 + (lon >= 0 ? 0.5 : -0.5)));
  out[9]  = d.needed ? (uint8_t)(d.discMin / 30) : 0xFF;
  out[10] = (state == 1) ? (uint8_t)((d.departMin / 30) | (d.nextDay ? 0x80 : 0)) : 0xFF;
  return 11;
}
