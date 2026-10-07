# ValPark — LilyGo T3-S3 E-Paper + NEO-6M + MPU6050 + TTN

Module qui détecte l'arrêt d'une voiture (15 s), vérifie par GPS si elle est en zone de parc,
envoie la position par LoRaWAN (TTN) à un serveur Python, et affiche l'heure du disque bleu
selon la règle suisse.

## Arborescence

```

valpark/
├── README.md
├── index.html              ← explication + pinouts (à ouvrir dans un navigateur)
├── esp32/
│   ├── platformio.ini
│   └── src/
│       ├── config.h
│       ├── parking_logic.h
│       └── main.cpp
└── server/
├── server.py
└── test_server.py

```

## Démarrage rapide

### 1. Firmware ESP32

```bash
cd esp32
# Éditer src/config.h : JOIN_EUI, DEV_EUI, APP_KEY, NWK_KEY
pio run -t upload
pio device monitor -b 115200
```

- Pour tester n'importe où : garder `DEBUG_ANY_POSITION_IS_ZONE 1`.
- En production : passer à `0` et remplir `ZONES[]` avec les rectangles lat/lon de Sion.

### 2. TTN

1. Créer l'application et le device (OTAA, EU868).
2. Activer « reset join nonces » (le firmware refait un join à chaque envoi).
3. Integrations → Webhooks → Custom, URL HTTPS publique, chemin uplink `/ttn`,
header `X-Webhook-Token` = valeur de `TTN_WEBHOOK_TOKEN`.

### 3. Serveur

```
cd server
TTN_WEBHOOK_TOKEN=monsecret python3 server.py
# Exposer en HTTPS (ngrok, Cloudflare Tunnel…)
```

Brancher le vrai processus dans `start_parking_process()` / `end_parking_process()`.

### 4. Tests

```
cd server
python3 test_server.py     # → TOUS LES TESTS OK
```

## Format du paquet LoRaWAN (11 octets)

| Octets | Contenu |
|---|---|
| 0 | État : 1 = garé, 0 = départ |
| 1–4 | Latitude × 1e5, int32 big-endian |
| 5–8 | Longitude × 1e5, int32 big-endian |
| 9 | Disque : minutes / 30 (0–48), ou 0xFF |
| 10 | Départ max : minutes / 30, + 0x80 si lendemain, ou 0xFF |

Exemple : `01 00468bcf 000b3b3b 1f 21` = garé à 46,23311 / 7,36059, disque 15:30, départ 16:30.

## Règle du disque (zone bleue, Suisse)

Flèche sur le trait qui suit l'heure d'arrivée (demi-heure strictement suivante).

| Arrivée (lun–sam) ↕▾ | Disque ↕▾ | Départ au plus tard ↕▾ |
|---|---|---|
| −08h00–11h29 et 13h30–17h59 | Demi-heure suivante | Disque + 1 h |
| 11h30–13h29 | Demi-heure suivante | 14h30 |
| 18h00–18h59 | Demi-heure suivante | 09h00 le lendemain |
| 19h00–07h59 | Pas de disque | Avant 08h00 |
| Dimanche | Pas de disque | – (jours fériés non gérés) |
⚙

## Limites connues

Le firmware n'a pas été compilé avec la vraie toolchain ESP32.
Si `pio run` échoue, l'erreur viendra probablement de :

- `sendReceive(...)` (RadioLib 7.x)
- `setDio2AsRfSwitch`
- `GxEPD2_213_BN`

</BDS:create_file>

<BDS:create_file fileName="parking-poc/esp32/platformio.ini">

```ini
[env:t3s3_epaper]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_upload.flash_size = 4MB
board_build.partitions = default.csv
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    jgromes/RadioLib@^7.1.0
    zinggjm/GxEPD2@^1.6.0
    adafruit/Adafruit GFX Library
    mikalhart/TinyGPSPlus@^1.1.0

