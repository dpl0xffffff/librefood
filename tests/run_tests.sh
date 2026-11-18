#!/bin/bash
# Builds and runs all tests. GLib critical warnings are fatal.
#   unit:        tests/test_product.c  (parser + input validation)
#   integration: tests/test_ui.c       (real UI code, headless Broadway,
#                fake OFF server, fake camera scanner)
#
#   tests/run_tests.sh --screenshots DIR   renders pages to PNGs instead
set -euo pipefail
cd "$(dirname "$0")/.."
SHOTS=
if [ "${1:-}" = "--screenshots" ]; then SHOTS=$(realpath -m "${2:?output dir}"); mkdir -p "$SHOTS"; fi
FLAGS="$(pkg-config --cflags --libs gtk4 libsoup-3.0 json-glib-1.0) -lm"
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"
B=tests/build
mkdir -p "$B"
export ASAN_OPTIONS=detect_leaks=0   # GTK/fontconfig keep global caches

if [ -z "$SHOTS" ]; then
echo "== unit tests"
gcc -Wall -Wextra -g $SAN tests/test_product.c product.c additives.c -o "$B/test_product" $FLAGS
G_DEBUG=fatal-criticals "$B/test_product"
fi

echo "== integration tests"
APP_SRC="ui.c fetch.c product.c style.c additives.c"
if [ -n "$SHOTS" ]; then
    gcc -Wall -Wextra -g tests/screenshots.c $APP_SRC -o "$B/screenshots" $FLAGS
else
    gcc -Wall -Wextra -g $SAN tests/test_ui.c $APP_SRC -o "$B/test_ui" $FLAGS
fi
cp tests/fake_scanner.py "$B/opencv_scanner.py"   # found next to the executable

PORT_FILE=$(mktemp); PIDFILE=$(mktemp)
python3 tests/fake_off_server.py > "$PORT_FILE" &
SERVER=$!
BROADWAY_PID=
cleanup() { kill $SERVER ${BROADWAY_PID:-} 2>/dev/null || true; rm -f "$PORT_FILE" "$PIDFILE"; }
trap cleanup EXIT
for _ in $(seq 50); do [ -s "$PORT_FILE" ] && break; sleep 0.1; done

DISP=:$(( (RANDOM % 50) + 50 ))
gtk4-broadwayd "$DISP" >/dev/null 2>&1 &
BROADWAY_PID=$!
sleep 0.5

export GDK_BACKEND=broadway BROADWAY_DISPLAY="$DISP" G_DEBUG=fatal-criticals
export LIBREFOOD_API_URL="http://127.0.0.1:$(cat "$PORT_FILE")"
export FAKE_SCANNER_PIDFILE="$PIDFILE"
if [ -n "$SHOTS" ]; then
    "$B/screenshots" "$SHOTS"
else
    "$B/test_ui" "$PIDFILE"
fi
