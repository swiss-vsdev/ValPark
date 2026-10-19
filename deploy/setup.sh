#!/usr/bin/env bash
set -euo pipefail
DOMAIN="${1:?Usage: sudo bash setup.sh parking.example.ch PK-0123456789AB}"
DEVICE="${2:?Fournir ID du module affiche sur ecran}"
[[ "$DOMAIN" =~ ^[A-Za-z0-9.-]+$ ]] || { echo Domaine invalide;exit 1; }
[[ "$DEVICE" =~ ^PK-[0-9A-F]{12}$ ]] || { echo ID invalide;exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
apt-get update
apt-get install -y python3 python3-cryptography caddy ufw
install -d /opt/parking /etc/parking
install -m 644 "$HERE/../server/server.py" /opt/parking/server.py
if [ ! -f /etc/parking/parking.env ]; then
 export DEVICE
 umask 077
 python3 - <<'PY' > /etc/parking/parking.env
import os,json,secrets
from cryptography.fernet import Fernet
print('DEVICE_TOKENS_JSON='+json.dumps({os.environ['DEVICE']:secrets.token_hex(24)},separators=(',',':')))
print('PARKING_SECRET_KEY='+Fernet.generate_key().decode())
print('ADMIN_TOKEN='+secrets.token_hex(24))
PY
fi
install -m 644 "$HERE/parking.service" /etc/systemd/system/parking.service
sed "s/__DOMAIN__/$DOMAIN/" "$HERE/Caddyfile" > /etc/caddy/Caddyfile
caddy validate --config /etc/caddy/Caddyfile
ufw allow 22/tcp
ufw allow 80/tcp
ufw allow 443/tcp
ufw allow 443/udp
# Activation manuelle pour ne pas couper un SSH sur un port personnalise.
systemctl daemon-reload
systemctl enable parking caddy
systemctl restart parking caddy
echo "Configurer puis activer ufw : sudo ufw enable"
echo "Sauvegarder /etc/parking/parking.env ; recopier le secret de ce module dans config.h."
echo "Test : curl https://$DOMAIN/health"
