#!/usr/bin/env python3
"""Fake OpenFoodFacts server for integration tests (fault injection).

Barcode → behaviour:
  30176204  valid product          11111111  status 0 (not found)
  22222222  HTTP 503 (HTML body)   33333333  garbage (not JSON)
  44444444  all fields null        55555555  sleeps 5 s (timeout)
  66666666  valid after 1.5 s      77777777  JSON array root
Prints the bound port on stdout, then serves until killed.
"""
import json, sys, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

GOOD = {"status": 1, "product": {
    "product_name": "Nutella", "brands": "Ferrero", "nutriscore_grade": "e",
    "nova_group": 4, "image_front_url": "", "ingredients_text": "sugar",
    "nutriments": {"energy-kcal_100g": 539, "fat_100g": 30.9},
    "additives_tags": ["en:e322"]}}

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

    def do_GET(self):
        code = self.path.rsplit("/", 1)[-1].removesuffix(".json")
        if code == "30176204":   self.send(200, json.dumps(GOOD))
        elif code == "11111111": self.send(200, '{"status":0,"status_verbose":"product not found"}')
        elif code == "22222222": self.send(503, "<html>busy</html>", "text/html")
        elif code == "33333333": self.send(200, "{not json")
        elif code == "44444444": self.send(200, json.dumps({"status": 1, "product": {
                k: None for k in ("product_name", "brands", "nutriscore_grade", "nova_group",
                                  "nutriments", "additives_tags", "image_front_url")}}))
        elif code == "55555555": time.sleep(5); self.send(200, json.dumps(GOOD))
        elif code == "66666666":
            time.sleep(1.5)
            slow = json.loads(json.dumps(GOOD)); slow["product"]["product_name"] = "Slow"
            self.send(200, json.dumps(slow))
        elif code == "77777777": self.send(200, "[1,2,3]")
        else: self.send(404, '{"status":0}')

srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
srv.daemon_threads = True
print(srv.server_address[1], flush=True)
srv.serve_forever()
