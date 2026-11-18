#!/usr/bin/env python3
"""Camera barcode scanner for librefood.

Protocol (stdout, one line): the decoded numeric barcode on success, or a
line starting with "ERROR:" on failure. Everything else goes to stderr.
Exit status: 0 = barcode printed, 1 = error/cancelled.
"""
import sys
import time

try:
    import cv2
    from pyzbar.pyzbar import decode
except ImportError as exc:  # missing py3-opencv / py3-pyzbar
    print(f"ERROR: Scanner dependencies missing ({exc.name}). Run build.sh.", flush=True)
    raise SystemExit(1)

WINDOW = "librefood camera scan"
MAX_CONSECUTIVE_READ_FAILURES = 50   # ~2.5 s at 50 ms/retry


def detect_barcode(frame):
    # Try multiple representations; laptop webcams often need preprocessing.
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8)).apply(gray)
    blur = cv2.GaussianBlur(clahe, (3, 3), 0)
    _, thresh = cv2.threshold(blur, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    up = cv2.resize(gray, None, fx=2.0, fy=2.0, interpolation=cv2.INTER_CUBIC)

    candidates = [frame, gray, clahe, thresh, up]
    for img in candidates:
        symbols = decode(img)
        for sym in symbols:
            text = sym.data.decode("utf-8", errors="ignore").strip()
            # Product barcodes are numeric; ignore QR codes with URLs etc.
            if len(text) >= 8 and text.isdigit():
                return text, sym
    return None, None


def main() -> int:
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    if not cap.isOpened():
        print("ERROR: Could not open camera", flush=True)
        return 1

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    cap.set(cv2.CAP_PROP_FPS, 30)

    try:
        cv2.namedWindow(WINDOW, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(WINDOW, 960, 540)
    except cv2.error as exc:
        cap.release()
        print("ERROR: Could not open preview window (no display or GUI support).", flush=True)
        print(exc, file=sys.stderr)
        return 1

    failures = 0
    try:
        while True:
            ok, frame = cap.read()
            if not ok or frame is None:
                # Camera unplugged / in use: don't spin forever at 100% CPU
                # with an unresponsive window.
                failures += 1
                if failures >= MAX_CONSECUTIVE_READ_FAILURES:
                    print("ERROR: Camera stopped delivering frames.", flush=True)
                    return 1
                time.sleep(0.05)
                continue
            failures = 0

            text, sym = detect_barcode(frame)
            if text:
                print(text, flush=True)
                return 0

            # Lightweight guidance overlay for framing.
            h, w = frame.shape[:2]
            cv2.rectangle(frame, (w // 8, h // 3), (w * 7 // 8, h * 2 // 3), (0, 255, 0), 2)
            cv2.putText(
                frame,
                "Center barcode in box, hold ~1 sec",
                (20, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 255, 0),
                2,
                cv2.LINE_AA,
            )

            cv2.imshow(WINDOW, frame)
            if cv2.waitKey(1) & 0xFF == 27:
                return 1
            # Window closed via its title bar: treat as cancel (otherwise
            # imshow would silently re-create it on the next frame).
            if cv2.getWindowProperty(WINDOW, cv2.WND_PROP_VISIBLE) < 1:
                return 1
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    raise SystemExit(main())
