# Installation Digital Republic / Infomaniak

## SIM
Carte SIM physique nano Digital Republic Flat 0.4, activee. APN utilise : internet. Si la SIM demande un PIN, definir SIM_PIN dans config.h. Aucun changement de forfait ni achat n'est effectue par ce projet.

## VPS
Ubuntu ou Debian, acces SSH par cle. Le script installe caddy depuis les paquets de la distribution. La disponibilite de ce paquet doit etre verifiee pour l'image choisie.
Creer un enregistrement DNS A vers l'IPv4 du VPS pour le domaine choisi. Ne pas creer d'AAAA sans IPv6 fonctionnelle.
Copier le dossier sur le VPS, puis :

```bash
cd deploy
sudo bash setup.sh parking.tondomaine.ch PK-0123456789AB
sudo systemctl status parking caddy
curl https://parking.tondomaine.ch/health
sudo cat /etc/parking/parking.env
```

Ne pas publier le dernier resultat : il contient les secrets.
Le script autorise les ports 22/80/443 mais laisse l'activation du pare-feu manuelle pour eviter une coupure SSH. Si SSH utilise un autre port, l'autoriser avant sudo ufw enable. Autoriser egalement les ports dans le pare-feu Infomaniak s'il est actif.
Sauvegarder /etc/parking/parking.env ET la base /var/lib/parking/parking.db. Ne pas transmettre ces sauvegardes dans un chat.

Le fichier d'environnement est cree seulement s'il n'existe pas. Pour ajouter un appareil, ajouter son ID et son propre secret dans DEVICE_TOKENS_JSON puis redemarrer le service. Ne pas partager le secret admin avec les appareils.

## Firmware

```bash
cp esp32/src/config.example.h esp32/src/config.h
# modifier SERVER_BASE et DEVICE_TOKEN
cd esp32
pio run
pio run -t upload
pio device monitor
```

Les commandes pio n'ont pas ete executees dans l'environnement de generation.
Ecran : driver GxEPD2_290c choisi pour un GDEW029Z10/IL0373 ; verifier le panneau exact avant achat. Une autre revision doit utiliser son pilote, pas un remplacement au hasard.

## Configuration utilisateur
Au premier boot : LED orange, ID et code WiFi affiches. Rejoindre le reseau Parking-... avec le code affiche ; ouvrir http://192.168.4.1. Apres 15 minutes le portail se coupe et le module dort. Un mouvement doit le reveiller.
Les identifiants sont transmis au VPS et chiffres, pas sauvegardes dans la flash ESP32. En cas d'echec, les ressaisir. Le marqueur registered signifie uniquement « VPS a stocke les identifiants », pas « ParkingPay a accepte le compte ».
L'option ALLOW_CREDENTIAL_UPLOAD bloque cet envoi par defaut jusqu'a configuration TLS. Pour un test avec des identifiants fictifs, mettre 1.

## Reconfiguration
BOOT au demarrage peut selectionner le bootloader : la procedure de maintien apres boot est indicative et a tester. Alternative fiable pour le developpement : effacer la flash puis reflasher (pio run -t erase, puis upload). Cela efface aussi les evenements non transmis.

## Consultation privee
/sessions n'est pas expose par Caddy. Utiliser un tunnel SSH :
```bash
ssh -L 8081:127.0.0.1:8080 utilisateur@IP_VPS
curl -H 'X-Admin-Token: SECRET_ADMIN' http://127.0.0.1:8081/sessions
```
