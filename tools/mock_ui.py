#!/usr/bin/env python3
"""Sert l'app RumScale extraite de web_ui.h avec des données simulées, pour
retoucher l'interface dans un navigateur sans avoir à flasher l'ESP32.

    python3 tools/mock_ui.py     puis  http://127.0.0.1:8777

Le WebSocket n'existe pas côté simulateur : l'app bascule d'elle-même sur son
mode de secours (interrogation toutes les 3 s), ce qui teste aussi ce chemin.
"""
import http.server, json, os, re, socketserver, time

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "web_ui.h")
raw = open(SRC, encoding="utf-8").read()
HTML = re.search(r'R"RUMHTML\((.*)\)RUMHTML";', raw, re.S).group(1)
print(f"HTML extrait : {len(HTML)} octets")

NOW = int(time.time())
DENS = 0.94
CAP = 5000
EMPTY_G = 3120.0
FULL_G = EMPTY_G + CAP * DENS      # 4700 g de liquide

# 95 jours d'historique : évaporation lente + une dizaine de services
pts, events = [], []
liquid = CAP * DENS
serve_days = [12, 19, 26, 33, 41, 55, 62, 70, 78, 86, 91]
for h in range(95 * 24):
    t = NOW - (95 * 24 - h) * 3600
    liquid -= 0.043                                # part des anges, ~0,65 %/mois
    d = h // 24
    if d in serve_days and h % 24 == 19:
        amount = 40 + (d % 4) * 15
        liquid -= amount * DENS
        events.append({"t": t, "type": "serve", "dg": -amount * DENS,
                       "dml": -amount, "after_g": liquid})
    pts.append([t, round(liquid, 1)])

STATE = {
    "name": "Tonneau de Papa", "pct": round(100 * liquid / (FULL_G - EMPTY_G), 1),
    "ml": round(liquid / DENS), "liquid_g": round(liquid),
    "total_g": round(EMPTY_G + liquid), "capacity_ml": CAP, "density": DENS,
    "stable": True, "calibrated": True, "initialized": True,
    "empty_g": EMPTY_G, "full_g": round(FULL_G), "counts_per_g": 104.7,
    "aging_days": 95, "aging_start": NOW - 95 * 86400, "epoch": NOW,
    "time_valid": True, "last_serve_g": 66, "last_serve_at": NOW - 4 * 86400,
    "evaporated_g": round(95 * 24 * 0.043), "alert_on_serve": True,
    "telegram": True, "tg_error": 0, "tg_chat": "123456789",
    "hx": [True, True], "ip": "192.168.1.42", "rssi": -57,
    "heap": 191000, "uptime_s": 95 * 86400,
}


class H(http.server.BaseHTTPRequestHandler):
    def _send(self, body, ctype="application/json"):
        b = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        if self.path == "/":
            return self._send(HTML, "text/html; charset=utf-8")
        if self.path.startswith("/api/state"):
            return self._send(json.dumps(STATE))
        if self.path.startswith("/api/history"):
            m = re.search(r"days=(\d+)", self.path)
            days = int(m.group(1)) if m else 30
            sel = pts if days == 0 else [p for p in pts if p[0] >= NOW - days * 86400]
            step = max(1, len(sel) // 300)
            return self._send(json.dumps(sel[::step]))
        if self.path.startswith("/api/events"):
            return self._send(json.dumps(list(reversed(events))))
        self.send_error(404)

    def do_POST(self):
        self._send(json.dumps({"ok": True, "message": "Simulation : rien enregistré."}))

    def log_message(self, *a):
        pass


socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("127.0.0.1", 8777), H) as s:
    print("http://127.0.0.1:8777")
    s.serve_forever()
