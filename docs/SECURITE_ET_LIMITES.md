# Securite et limites bloquantes

Aucun prototype n'est garanti sans risque. Cette liste contient des points a lever AVANT usage reel.

1. Batterie : presence, seuils et courant du BMS de la revision exacte non verifies ici. Une cellule nue n'est a commander/utiliser qu'apres verification. Aucun seuil thermique batterie n'est surveille dans ce code. Ne pas charger ou laisser le module au soleil dans une voiture ; respecter les plages fabricant et retirer le prototype apres les essais surveilles.
2. La batterie fine demandee auparavant n'est pas resolue par cette carte a support 18650. Le choix d'un pack plat exige verification du chargeur, du courant de pointe et d'un raccordement adapte. Aucune promesse de charge USB-C compatible n'est faite sans schema exact.
3. TLS : les commandes HTTPS seules ne garantissent PAS l'authentification du serveur. Installer la CA et activer/tester la validation du certificat ET du nom de domaine sur le firmware SIMCom exact. Ne jamais desactiver la validation pour de vrais mots de passe. Le code bloque par defaut leur envoi.
4. Portail local : WPA2 et code aleatoire, mais HTTP local ; une personne connaissant le code WiFi peut interagir. Jeton CSRF ajoute. Ne pas exposer les identifiants dans les logs ou dans un URL. Pas d'effacement memoire garanti par la remise a vide d'une String.
5. VPS : secrets par appareil, cle Fernet separee, acces admin distinct. Le chiffrement au repos n'empeche pas un administrateur/root compromis de dechiffrer. Prevoir sauvegardes et revocation.
6. ParkingPay : aucune API publique integree ni paiement. La collecte de mots de passe est un choix sensible ; privilegier un mecanisme delegue fourni officiellement par l'operateur. Obtenir son accord avant connexion automatisee, compte fictif pour les essais.
7. LED verte : accuse de reception de notre VPS uniquement, PAS un ticket ParkingPay ni une preuve de paiement.
8. Detecteur : un accelerometre ne distingue pas tous les trajets lisses d'un arret ; un mouvement de portiere n'est pas forcement un depart. Les seuils sont experimentaux, pas une solution de paiement autonome validee.
9. Zones : aucune donnee officielle de Sion integree. Une geofence rectangulaire et un GNSS peuvent confondre route et parking ; ne pas lancer de paiement automatique sur ce critere seul.
10. Disque : l'heure est arrondie au trait de demi-heure suivant et reste figee. Cette version n'annonce pas une heure limite de depart : jours feries, panneaux, exceptions et reprise le lendemain ne sont pas calcules. Le rendu texte n'est pas un disque officiellement homologue ; respecter les conditions de l'autorisation specifique obtenue pour le projet.
11. File d'attente : un seul evenement persistant, reessaye avec le meme event_id ; serveur idempotent. En cas de deux evenements rapproches non transmis, le second n'est pas conserve : LED rouge, limite de ce prototype. Une vraie file multi-evenements est necessaire avant usage autonome.
12. Fixer solidement loin des airbags et du champ de vision ; pas de fils Dupont libres pendant la conduite. Connecter les antennes correctes avant emission.
