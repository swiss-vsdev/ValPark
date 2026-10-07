#!/usr/bin/env python3
"""Serveur PoC parking : recoit les uplinks TTN (webhook), decode, stocke, declenche le process.
Aucune dependance externe (stdlib uniquement). Python >= 3.9.
Lancer : TTN_WEBHOOK_TOKEN=monsecret python3 server.py
"""
import base64
import json
import os
import sqlite3
import struct
import threading
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TOKEN = os.environ.get("TTN_WEBHOOK_TOKEN", "change-me")
DB_PATH = os.environ.get("DB_PATH", "parking.db")
PORT = int(os.environ.get("PORT", "8080"))
_lock = threading.Lock()

def db():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    return con

def init_db():
    with db() as con:
        con.execute(
            """CREATE TABLE IF NOT EXISTS sessions (
                 id INTEGER PRIMARY KEY AUTOINCREMENT,
                 device TEXT NOT NULL,
                 parked_at TEXT NOT NULL,
                 lat REAL, lon REAL,
                 disc TEXT, depart_limit TEXT,
                 departed_at TEXT)"""
        )

def slot_to_hm(slot):
    return f"{slot // 2:02d}:{(slot % 2) * 30:02d}"

def decode_payload(raw: bytes) -> dict:
    """11 octets : etat, lat*1e5, lon*1e5 (int32 BE), slot disque, slot depart (+0x80 = lendemain)."""
    if len(raw) != 11:
        raise ValueError(f"payload de {len(raw)} octets, 11 attendus")
    state, lat, lon, disc, dep = struct.unpack(">BiiBB", raw)
    return {
        "state": "PARKED" if state == 1 else "DEPART",
        "lat": lat / 1e5,
        "lon": lon / 1e5,
        "disc": None if disc == 0xFF else slot_to_hm(disc),
        "depart_limit": None if dep == 0xFF else slot_to_hm(dep & 0x7F) + (" (+1j)" if dep & 0x80 else ""),
    }

# ---------- Processus de parking (a brancher sur ton vrai systeme) ----------
def start_parking_process(session: dict):
    print(f"[PARKING] START  {session}", flush=True)

def end_parking_process(session: dict):
    print(f"[PARKING] END    {session}", flush=True)

def handle_uplink(device: str, received_at: str, data: dict):
    with _lock, db() as con:
        if data["state"] == "PARKED":
            con.execute(
                "UPDATE sessions SET departed_at=? WHERE device=? AND departed_at IS NULL",
                (received_at, device),
            )
            cur = con.execute(
                "INSERT INTO sessions(device,parked_at,lat,lon,disc,depart_limit) VALUES (?,?,?,?,?,?)",
                (device, received_at, data["lat"], data["lon"], data["disc"], data["depart_limit"]),
            )
            start_parking_process({"id": cur.lastrowid, "device": device, "at": received_at, **data})
        else:
            row = con.execute(
                "SELECT id FROM sessions WHERE device=? AND departed_at IS NULL ORDER BY id DESC LIMIT 1",
                (device,),
            ).fetchone()
            if row:
                con.execute("UPDATE sessions SET departed_at=? WHERE id=?", (received_at, row["id"]))
                end_parking_process({"id": row["id"], "device": device, "at": received_at, **data})

class Handler(BaseHTTPRequestHandler):
    def _send(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            return self._send(200, {"ok": True})
        if self.path == "/sessions":
            with db() as con:
                rows = [dict(r) for r in con.execute("SELECT * FROM sessions ORDER BY id DESC LIMIT 100")]
            return self._send(200, rows)
        self._send(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/ttn":
            return self._send(404, {"error": "not found"})
        if self.headers.get("X-Webhook-Token") != TOKEN:
            return self._send(401, {"error": "bad token"})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            msg = json.loads(self.rfile.read(length))
            device = msg["end_device_ids"]["device_id"]
            received_at = msg.get("received_at") or datetime.now(timezone.utc).isoformat()
            raw = base64.b64decode(msg["uplink_message"]["frm_payload"])
            data = decode_payload(raw)
        except (KeyError, ValueError, json.JSONDecodeError) as e:
            return self._send(400, {"error": str(e)})
        handle_uplink(device, received_at, data)
        self._send(200, {"ok": True, "decoded": data})

    def log_message(self, fmt, *args):
        print("[HTTP]", fmt % args, flush=True)

if __name__ == "__main__":
    init_db()
    print(f"Ecoute sur 0.0.0.0:{PORT}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
