#pragma once
// Copier en config.h et garder les secrets hors du depot Git.
#define SERVER_BASE "https://parking.example.ch"
#define DEVICE_TOKEN "REMPLACER_PAR_UN_SECRET_PROPRE_A_CE_MODULE"
#define APN "internet"
#define SIM_PIN ""
// Tant que la validation TLS du modem n'est pas configuree et testee : uniquement des identifiants fictifs.
#define ALLOW_CREDENTIAL_UPLOAD 0
#define TEST_ANY_ZONE 0
// T-A7670 R2 : revision physique a verifier avant cablage.
#define MODEM_RX 27
#define MODEM_TX 26
#define MODEM_POWER 12
#define MODEM_KEY 4
#define MODEM_RESET 5
// Proposition de cablage externe, non validee sur le schema exact de ta carte.
#define EPD_CS 19
#define EPD_DC 21
#define EPD_RST 22
#define EPD_BUSY 34
#define EPD_SCK 18
#define EPD_MOSI 23
#define MPU_SDA 32
#define MPU_SCL 33
#define MPU_INT 36
#define LED_R 13
#define LED_G 14
#define LED_B 15
// Une seule LED RGB cathode commune : R/G melanges pour l'orange. Resistance 1 kohm par canal.
// Pas de carte microSD : ses lignes sont reutilisees.
