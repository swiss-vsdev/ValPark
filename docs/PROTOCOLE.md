# API

POST /api/register : token (secret appareil), device (PK- + 12 hexadecimaux), pp_user, pp_pass.
Reponse : {"ok":true,"parkingpay_verified":false}. Les deux identifiants sont chiffres avec Fernet en SQLite.

POST /api/event : token, device, event_id (unique, reutilise lors des retries), state (PARKED/DEPART), lat, lon, disc (HH:00 ou HH:30, ou null), arrival_epoch, zone.
Code 403 si pas enregistre, 401 si secret incorrect, 400 si JSON invalide. Doublon : 200 sans nouvelle session. Aucun paiement.

GET /health public. GET /sessions prive, entete X-Admin-Token, non expose par Caddy.

Compatibilite : ce protocole remplace les anciens fichiers. Il n'est pas compatible tel quel avec les anciennes bases devices/sessions : utiliser une nouvelle base ou faire une migration explicite. Ne jamais supprimer une base contenant de vraies donnees sans sauvegarde.
