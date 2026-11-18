#!/usr/bin/env python3
"""Fake OpenFoodFacts server for integration tests (fault injection).

Barcode → behaviour:
  30176204  valid product          11111111  status 0 (not found)
  22222222  HTTP 503 (HTML body)   33333333  garbage (not JSON)
  44444444  all fields null        55555555  sleeps 5 s (timeout)
  66666666  valid after 1.5 s      77777777  JSON array root
Prints the bound port on stdout, then serves until killed.
"""
import json, struct, sys, time, zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

GOOD = {"status": 1, "product": {
    "product_name": "Nutella", "brands": "Ferrero", "nutriscore_grade": "e",
    "nova_group": 4, "image_front_url": "IMG", "quantity": "400 g",
    "ecoscore_grade": "d",
    "ingredients_text": "Sugar, palm oil, hazelnuts 13%, skimmed milk powder 8.7%, "
                        "fat-reduced cocoa 7.4%, emulsifier: lecithins (soya), vanillin.",
    "nutriments": {"energy-kcal_100g": 539, "fat_100g": 30.9, "saturated-fat_100g": 10.6,
                   "carbohydrates_100g": 57.5, "sugars_100g": 56.3, "fiber_100g": 0,
                   "proteins_100g": 6.3, "salt_100g": 0.107},
    "additives_tags": ["en:e322", "en:e322i"],
    "allergens_tags": ["en:milk", "en:nuts", "en:soybeans"],
    "traces_tags": ["en:gluten"],
    "labels_tags": ["en:no-gluten", "en:green-dot", "en:sustainable-palm-oil"]}}


def make_png(w=240, h=240):
    """Solid-colour PNG with a darker square, no external deps."""
    rows = b""
    for y in range(h):
        row = b"\x00"
        for x in range(w):
            inner = w // 4 < x < 3 * w // 4 and h // 4 < y < 3 * h // 4
            row += bytes((120, 70, 40) if inner else (230, 180, 120))
        rows += row
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))

PNG = make_png()

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def send(self, code, body, ctype="application/json"):
        data = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try: self.wfile.write(data)
        except BrokenPipeError: pass

    def good(self):
        port = self.server.server_address[1]
        return json.dumps(GOOD).replace('"IMG"', f'"http://127.0.0.1:{port}/img.png"')

    def do_GET(self):
        if self.path == "/img.png":
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(len(PNG)))
            self.end_headers()
            self.wfile.write(PNG)
            return
        code = self.path.rsplit("/", 1)[-1].removesuffix(".json")
        if code == "30176204":   self.send(200, self.good())
        elif code == "11111111": self.send(200, '{"status":0,"status_verbose":"product not found"}')
        elif code == "22222222": self.send(503, "<html>busy</html>", "text/html")
        elif code == "33333333": self.send(200, "{not json")
        elif code == "44444444": self.send(200, json.dumps({"status": 1, "product": {
                k: None for k in ("product_name", "brands", "nutriscore_grade", "nova_group",
                                  "nutriments", "additives_tags", "image_front_url")}}))
        elif code == "55555555": time.sleep(5); self.send(200, self.good())
        elif code == "66666666":
            time.sleep(1.5)
            slow = json.loads(self.good()); slow["product"]["product_name"] = "Slow"
            self.send(200, json.dumps(slow))
        elif code == "77777777": self.send(200, "[1,2,3]")
        else: self.send(404, '{"status":0}')

srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
srv.daemon_threads = True
print(srv.server_address[1], flush=True)
srv.serve_forever()
