#!/usr/bin/env python3
import sys
import cv2
from pyzbar.pyzbar import decode


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
            if text:
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

    cv2.namedWindow("librefood camera scan", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("librefood camera scan", 960, 540)

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue

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

            cv2.imshow("librefood camera scan", frame)
            if cv2.waitKey(1) & 0xFF == 27:
                return 1
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    raise SystemExit(main())
