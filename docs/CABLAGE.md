# Cablage propose — revision physique a verifier

Ne pas alimenter avant de verifier la carte exacte, les tensions, la polarite et l'absence de court-circuit. Les numeros des connecteurs physiques ne sont pas connus : seuls les GPIO sont proposes.

| Module | Signal | ESP32 |
|---|---|---|
| Modem integre | ESP RX / TX | 27 / 26 |
| Modem integre | POWER / PWRKEY / RESET | 12 / 4 / 5 |
| E-paper | VCC / GND | 3V3 / GND |
| E-paper | DIN / CLK | 23 / 18 |
| E-paper | CS / DC | 19 / 21 |
| E-paper | RST / BUSY | 22 / 34 |
| MPU6050 | VCC / GND / AD0 | 3V3 / GND / GND |
| MPU6050 | SDA / SCL / INT | 32 / 33 / 36 |
| LED RGB cathode commune | R via 1 kohm | 13 |
| LED RGB cathode commune | G via 1 kohm | 14 |
| LED RGB cathode commune | B via 1 kohm | 15 |
| LED RGB | Cathode commune | GND |

Chaque canal de LED exige sa propre resistance. Le bleu n'est pas utilise par cette version. Une RGB a anode commune n'est PAS interchangeable sans changer le cablage et la logique.
GPIO 13/14/15 sont utilises par la carte SD : ne pas inserer/utiliser de microSD. GPIO15 est une broche de demarrage : verifier que le montage LED ne gene pas le boot. GPIO33 peut etre relie au signal RING du modem : ce partage doit etre verifie avant utilisation en SCL. Si RING est effectivement connecte et actif, changer SCL ou isoler RING, ne pas relier deux sorties entre elles.
Ne jamais raccorder directement une batterie au 3V3. Ne pas alimenter un modem nu via le 3V3 ESP32.
Le maintien de la LED pendant deep sleep utilise gpio_hold. A tester sur la carte.

```text
 Batterie 1S --> alimentation/charge de la carte LilyGo (a valider)
 USB-C ------> port ESP-USB de la carte (charge a valider)
                     |
     +---------------+--------------------+
     | ESP32         | modem LTE + GNSS   |
     | SPI ----------> e-paper 2,9 pouces  |
     | I2C ----------> MPU6050             |
     | RTC GPIO <---- INT MPU6050         |
     | GPIO 13 --1k--> R LED RGB           |
     | GPIO 14 --1k--> G LED RGB           |
     | GPIO 15 --1k--> B LED RGB           |
     +------------------------------------+
            LTE -> reseau -> VPS HTTPS
```
