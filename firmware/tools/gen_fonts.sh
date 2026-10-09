#!/usr/bin/env bash
# Regenerate the LVGL bitmap fonts in app/ui/fonts/ from the UI font family and the icon font.
# To change the UI font, change FAMILY (any Google Fonts family with weights 300, 500 and 600).
# Needs Node (npx fetches lv_font_conv) and curl. Run make_icon_font.py first
# if icons.json changed.
set -euo pipefail
cd "$(dirname "$0")"
OUT=../app/ui/fonts
mkdir -p "$OUT" fonts

FAMILY=Figtree

# Google Fonts serves static TrueType instances to clients that don't ask for woff2.
for w in 300 500 600; do
  f=fonts/$FAMILY-$w.ttf
  [ -s "$f" ] && continue
  url=$(curl -fsS -A "Mozilla/4.0" "https://fonts.googleapis.com/css2?family=$FAMILY:wght@$w" | grep -o 'https://[^)]*\.ttf')
  curl -fsS -o "$f" "$url"
done

CONV="npx --yes lv_font_conv@1.5.3"
# Basic Latin, Latin-1, dashes, quotes, bullet, ellipsis, subscript 2, euro, minus
TEXT=0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x20AC,0x2212

text() { # name size weight
  $CONV --no-compress --no-prefilter --bpp 4 --size "$2" --format lvgl --lv-include lvgl.h \
    --font fonts/$FAMILY-$3.ttf -r $TEXT -o "$OUT/$1.c" --lv-font-name "$1"
}
text font_11  11 500
text font_12  12 500
text font_12b 12 600
text font_14  14 500
text font_14b 14 600
text font_21  21 600

# Digits-only display fonts: charging percentage and the lock-screen clock
$CONV --no-compress --bpp 2 --size 44 --format lvgl --lv-include lvgl.h \
  --font fonts/$FAMILY-600.ttf --symbols "0123456789%" -o "$OUT/font_44d.c" --lv-font-name font_44d
$CONV --no-compress --bpp 2 --size 84 --format lvgl --lv-include lvgl.h \
  --font fonts/$FAMILY-300.ttf --symbols "0123456789:" -o "$OUT/font_84d.c" --lv-font-name font_84d

for s in 14 16 18 20 24; do
  $CONV --no-compress --bpp 4 --size $s --format lvgl --lv-include lvgl.h \
    --font icons.otf -r 0xE000-0xE0FF -o "$OUT/icons_$s.c" --lv-font-name icons_$s
done
ls -la "$OUT"

# Splash logo (make_logo_font.py): body + dots, drawn 80 px tall
$CONV --no-compress --bpp 4 --size 80 --format lvgl --lv-include lvgl.h \
  --font logo.otf -r 0xE100-0xE101 -o "$OUT/logo_80.c" --lv-font-name logo_80
