# Tests executes pendant la generation

Serveur : commande python3 test_server.py, code retour 0.
...
----------------------------------------------------------------------
Ran 3 tests in 0.524s

OK

Logique portable : g++ -std=c++17 tests/test_logic.cpp puis execution, codes retour 0/0.
Script : bash -n deploy/setup.sh, code retour 0.

NON EXECUTE : compilation du firmware avec PlatformIO, flash ESP32, verification radio/GNSS, sequence AT, portail captif sur telephone, RGB et sommeil, deploiement Caddy/systemd sur VPS reel.
Aucun binaire ESP32 n'est fourni. Le code est un prototype a valider, pas un firmware certifie.
