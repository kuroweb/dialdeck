#!/bin/sh
# Generate firmware/main/fonts/font_ja_16.c: M PLUS 1p (OFL) at 16 px with ASCII, kana and every
# JIS X 0208 character, so agent titles in Japanese render on the device.
# Needs Node.js (runs lv_font_conv through npx) and python3. Run from anywhere.
set -eu

cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

curl -fsSL -o "$work/MPLUS1p-Regular.ttf" \
    https://github.com/google/fonts/raw/main/ofl/mplus1p/MPLUS1p-Regular.ttf

# Every character of JIS X 0208 (rows 1-84), taken from the EUC-JP code table.
python3 -I - "$work/chars.txt" <<'PY'
import sys
chars = set()
for hi in range(0xA1, 0xF5):
    for lo in range(0xA1, 0xFF):
        try:
            chars.add(bytes([hi, lo]).decode("euc_jp"))
        except UnicodeDecodeError:
            pass
open(sys.argv[1], "w", encoding="utf-8").write("".join(sorted(chars)))
PY

mkdir -p main/fonts
npx --yes lv_font_conv@1.5.2 \
    --font "$work/MPLUS1p-Regular.ttf" --range 0x20-0x7E --symbols "$(cat "$work/chars.txt")" \
    --size 16 --bpp 2 --format lvgl --no-compress --lv-include lvgl.h \
    -o main/fonts/font_ja_16.c
echo main/fonts/font_ja_16.c
