# Parking PoC — version reconstruite

Archive complete, fichiers separes. Cette reconstruction remplace les versions precedentes : ce n'est pas une copie identique de fichiers perdus.

## Architecture

ESP32 LilyGo T-A7670E R2 avec GNSS -> 4G Digital Republic Flat 0.4 -> HTTPS -> API Python sur Infomaniak VPS Lite.
Ecran Waveshare 2,9 pouces tricolore et MPU6050 externes. LED RGB externe cathode commune.

## Contenu

- esp32/platformio.ini : projet PlatformIO.
- esp32/src/config.example.h : copier en config.h avant compilation.
- esp32/src/main.cpp : portail WiFi, configuration, LED, detection, GNSS, affichage, envois.
- esp32/src/modem.h : commandes AT modem.
- esp32/src/logic.h : arrondi du disque, dates et parseurs.
- esp32/src/zones.h : zones a remplir, aucune zone officielle importe actuellement.
- server/server.py : API, chiffrement des identifiants et deduplication des evenements.
- server/requirements.txt et test_server.py : dependances et tests.
- deploy/ : service systemd, configuration Caddy, script d'installation et modele de secrets.
- docs/ : cablage, securite, installation, protocole et resultats des tests.
- commande.xlsx : materiel et services, prix historiques et validations restantes.
- tests/test_logic.cpp : tests C++ independants du materiel.

## Demarrage

1. Copier esp32/src/config.example.h vers config.h.
2. Flasher une premiere fois pour afficher l'ID PK-... et le code du WiFi local.
3. Sur le VPS : deploy/setup.sh DOMAINE ID_MODULE, voir docs/INSTALLATION.md.
4. Reporter le domaine et le secret propre a ce module dans config.h puis reflasher.
5. Se connecter au WiFi Parking-... et ouvrir http://192.168.4.1.
6. Pour tester la saisie et l'enregistrement, utiliser UNIQUEMENT des identifiants fictifs.

## Important

L'envoi d'identifiants est bloque par defaut (ALLOW_CREDENTIAL_UPLOAD=0). Le support de validation du certificat TLS du modem n'est pas encore configure/teste. Activer l'envoi uniquement pour des donnees fictives jusqu'a correction.
Le serveur ne contacte PAS ParkingPay, ne valide PAS le compte et ne paie aucun stationnement. La LED verte signifie uniquement « evenement recu par le VPS ». Elle ne prouve jamais un paiement.
Le vert, le rouge et l'orange sont obtenus par une LED RGB cathode commune ; l'orange est un melange R+G qui peut apparaitre jaune selon la LED. Il faut regler le melange/les resistances au besoin.
Pas de precision GPS garantie pour distinguer une place de la route adjacente. Test a Sion sur une zone selectionnee manuellement.

## Tests reels de cette reconstruction

- Serveur Python : 3 tests d'integration reussis (HTTP, autorisation, chiffrement, erreurs, deduplication, depart).
- Logique C++ : compilation g++ et execution reussies.
- Script installation : bash -n reussi.
- Firmware ESP32 complet : PAS compile avec la toolchain ESP32 ; PAS teste sur carte.
- VPS, TLS modem, GPS, MPU et GPIO : PAS valides sur le materiel.
Aucun resultat de compilation avec des faux en-tetes n'est presente comme une validation du firmware.
