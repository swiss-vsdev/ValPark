#!/usr/bin/env python3
import os,json,sqlite3,hmac,re,math,threading
from datetime import datetime,timezone
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from cryptography.fernet import Fernet
DB=os.environ.get("DB_PATH","parking.db")
TOKENS=json.loads(os.environ.get("DEVICE_TOKENS_JSON",'{}'))
KEY=os.environ.get("PARKING_SECRET_KEY","")
ADMIN=os.environ.get("ADMIN_TOKEN","")
LOCK=threading.Lock()
def connect():
    c=sqlite3.connect(DB,timeout=10);c.row_factory=sqlite3.Row;return c
def init_db():
    Fernet(KEY.encode())
    if not TOKENS or not ADMIN:raise RuntimeError("Configurer DEVICE_TOKENS_JSON, ADMIN_TOKEN et PARKING_SECRET_KEY")
    with connect() as c:
        c.executescript("""
        CREATE TABLE IF NOT EXISTS devices(device TEXT PRIMARY KEY,user_cipher BLOB,pass_cipher BLOB,updated TEXT);
        CREATE TABLE IF NOT EXISTS events(device TEXT,event_id TEXT,payload TEXT,received TEXT,PRIMARY KEY(device,event_id));
        CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY,device TEXT,started TEXT,ended TEXT,lat REAL,lon REAL,disc TEXT);
        """)
def authenticate(msg):
    if not isinstance(msg,dict):raise ValueError("Objet attendu")
    device=msg.get("device","")
    if not isinstance(device,str) or not re.fullmatch(r"PK-[0-9A-F]{12}",device):raise ValueError("ID invalide")
    supplied=msg.get("token","");expected=TOKENS.get(device)
    if not expected or not isinstance(supplied,str) or not hmac.compare_digest(supplied,expected):raise PermissionError("Refuse")
    return device
def register(msg):
    device=authenticate(msg)
    for name in ("pp_user","pp_pass"):
        v=msg.get(name)
        if not isinstance(v,str) or not 1<=len(v)<=64 or any(ord(x)<32 for x in v):raise ValueError("Identifiants invalides")
    f=Fernet(KEY.encode());now=datetime.now(timezone.utc).isoformat()
    with LOCK,connect() as c:
        c.execute("INSERT INTO devices VALUES(?,?,?,?) ON CONFLICT(device) DO UPDATE SET user_cipher=excluded.user_cipher,pass_cipher=excluded.pass_cipher,updated=excluded.updated",
                  (device,f.encrypt(msg['pp_user'].encode()),f.encrypt(msg['pp_pass'].encode()),now))
    return {"ok":True,"parkingpay_verified":False}
def event(msg):
    device=authenticate(msg);eid=msg.get("event_id","")
    if not isinstance(eid,str) or not re.fullmatch(r"[A-Za-z0-9_-]{8,80}",eid):raise ValueError("event_id invalide")
    state=msg.get("state")
    if state not in ("PARKED","DEPART"):raise ValueError("Etat invalide")
    lat,lon=msg.get('lat'),msg.get('lon')
    if any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in (lat,lon)):raise ValueError("Coordonnees invalides")
    if not -90<=lat<=90 or not -180<=lon<=180:raise ValueError("Coordonnees hors limites")
    disc=msg.get('disc')
    if disc is not None and (not isinstance(disc,str) or not re.fullmatch(r"(?:[01]\d|2[0-3]):(?:00|30)",disc)):raise ValueError("Heure du disque invalide")
    now=datetime.now(timezone.utc).isoformat()
    safe={k:msg.get(k) for k in ('state','lat','lon','disc','arrival_epoch','zone')}
    with LOCK,connect() as c:
        if not c.execute("SELECT 1 FROM devices WHERE device=?",(device,)).fetchone():return 403,{"error":"unregistered"}
        if c.execute("SELECT 1 FROM events WHERE device=? AND event_id=?",(device,eid)).fetchone():return 200,{"ok":True,"duplicate":True,"parkingpay_verified":False}
        c.execute("INSERT INTO events VALUES(?,?,?,?)",(device,eid,json.dumps(safe),now))
        if state=="PARKED":
            active=c.execute("SELECT id FROM sessions WHERE device=? AND ended IS NULL",(device,)).fetchone()
            if not active:c.execute("INSERT INTO sessions(device,started,lat,lon,disc) VALUES(?,?,?,?,?)",(device,now,lat,lon,disc))
        else:c.execute("UPDATE sessions SET ended=? WHERE device=? AND ended IS NULL",(now,device))
    # AUCUN paiement ou appel ParkingPay : integration non implementee.
    return 200,{"ok":True,"parkingpay_verified":False}
class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup();self.connection.settimeout(20)
    def send_json(self,status,data):
        raw=json.dumps(data).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
    def do_POST(self):
        if self.path not in ('/api/register','/api/event'):return self.send_json(404,{"error":"not found"})
        try:
            n=int(self.headers.get('Content-Length','0'))
            if not 1<=n<=4096:raise ValueError('taille invalide')
            msg=json.loads(self.rfile.read(n))
            if self.path=='/api/register':status,data=200,register(msg)
            else:status,data=event(msg)
        except PermissionError:return self.send_json(401,{"error":"unauthorized"})
        except (ValueError,TypeError,KeyError):return self.send_json(400,{"error":"invalid request"})
        self.send_json(status,data)
    def do_GET(self):
        if self.path=='/health':return self.send_json(200,{"ok":True})
        if self.path!='/sessions':return self.send_json(404,{"error":"not found"})
        supplied=self.headers.get('X-Admin-Token','')
        if not ADMIN or not hmac.compare_digest(supplied,ADMIN):return self.send_json(401,{"error":"unauthorized"})
        with connect() as c:rows=[dict(r) for r in c.execute('SELECT * FROM sessions ORDER BY id DESC LIMIT 100')]
        self.send_json(200,rows)
    def log_message(self,fmt,*args):pass
if __name__=='__main__':
    init_db();ThreadingHTTPServer(('127.0.0.1',int(os.environ.get('PORT','8080'))),Handler).serve_forever()
