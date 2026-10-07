import base64, json, os, tempfile, threading, urllib.request, urllib.error
os.environ["DB_PATH"] = os.path.join(tempfile.mkdtemp(), "t.db")
os.environ["TTN_WEBHOOK_TOKEN"] = "s3cret"
import server
from http.server import ThreadingHTTPServer

server.init_db()
srv = ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
port = srv.server_address[1]
threading.Thread(target=srv.serve_forever, daemon=True).start()

def post(hexpayload, token="s3cret"):
    body = json.dumps({"end_device_ids": {"device_id": "car-1"}, "received_at": "2026-10-07T13:20:00Z",
                       "uplink_message": {"f_port": 1, "frm_payload": base64.b64encode(bytes.fromhex(hexpayload)).decode()}}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}/ttn", body, {"X-Webhook-Token": token, "Content-Type": "application/json"})
    try:
        return urllib.request.urlopen(req).status
    except urllib.error.HTTPError as e:
        return e.code

assert server.decode_payload(bytes.fromhex("0100468bcf000b3b3b1f21")) == {
    "state": "PARKED", "lat": 46.23311, "lon": 7.36059, "disc": "15:30", "depart_limit": "16:30"}
d = server.decode_payload(bytes.fromhex("01" + "00468bcf" + "000b3b3b" + "25" + "92"))
assert d["disc"] == "18:30" and d["depart_limit"] == "09:00 (+1j)", d
assert post("0100468bcf000b3b3b1f21") == 200
assert post("0100468bcf000b3b3b1f21", token="x") == 401
assert post("0100") == 400
rows = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/sessions"))
assert len(rows) == 1 and rows[0]["departed_at"] is None
assert post("0000468bcf000b3b3bffff") == 200
rows = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/sessions"))
assert rows[0]["departed_at"] == "2026-10-07T13:20:00Z"
print("TOUS LES TESTS OK")
