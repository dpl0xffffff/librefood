#!/usr/bin/env python3
"""Stand-in for opencv_scanner.py. FAKE_SCANNER_MODE: barcode | error | hang | exit"""
import os, sys, time
mode = os.environ.get("FAKE_SCANNER_MODE", "barcode")
with open(os.environ.get("FAKE_SCANNER_PIDFILE", "/dev/null"), "w") as f:
    f.write(str(os.getpid()))
if mode == "barcode":
    print("warning on stderr 123456789012", file=sys.stderr, flush=True)
    print("30176204", flush=True)
elif mode == "error":
    print("ERROR: Could not open camera", flush=True); sys.exit(1)
elif mode == "hang":
    time.sleep(600)
sys.exit(0 if mode == "barcode" else 1)
