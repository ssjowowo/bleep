#!/usr/bin/env bash
# Regenerate the LVGL bitmap fonts in app/ui/fonts/: the UI font (Roboto Flex)
# and the icons (Material Symbols Outlined, mapped in icons.json).
# Needs Node (npx fetches lv_font_conv), curl, and Python with fontTools
# (pip install fonttools). Without a local fontTools, PYTHON can name another
# one, e.g. PYTHON="docker run --rm -u $(id -u) -v $PWD/..:/fw -w /fw/tools <image with fonttools> python".
set -euo pipefail
cd "$(dirname "$0")"
OUT=../app/ui/fonts
PYTHON=${PYTHON:-python3}
mkdir -p "$OUT" fonts

# Both fonts are variable; the Python scripts cut the static instances. Pinned
# to commits, so the same glyphs come out every time.
get() { # file url
  [ -s "fonts/$1" ] || curl -fsSL -o "fonts/$1" "$2"
}
get RobotoFlex-VF.ttf "https://raw.githubusercontent.com/google/fonts/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/robotoflex/RobotoFlex%5BGRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght%5D.ttf"
MS="https://raw.githubusercontent.com/google/material-design-icons/49d4db35df873165d6bd6ba09b063c7dafbac2f4/variablefont/MaterialSymbolsOutlined%5BFILL,GRAD,opsz,wght%5D"
get MaterialSymbolsOutlined-VF.ttf "$MS.ttf"
get MaterialSymbolsOutlined.codepoints "$MS.codepoints"

CONV="npx --yes lv_font_conv@1.5.3"
# Basic Latin, Latin-1, Greek (song and film titles), dashes, quotes, bullet, ellipsis, subscript 2, euro, minus
TEXT=0x20-0x7E,0xA0-0xFF,0x370-0x3FF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x20AC,0x2212

# name size weight
FONTS="
font_12  12 500
font_12b 12 600
font_14  14 500
font_14b 14 600
font_21  21 600
font_10  10 500
font_11  11 500
font_11b 11 600
font_13  13 500
font_13b 13 600
font_18  18 600
"
# The compact profile (240 x 320, the Bleep minis; app/ui/widgets.h) uses
# 10, 11, 13 and 18: one step down. Digits-only display fonts: the charging
# percentage and the lock-screen clock (44 and 84 px; 36 and 64 compact).
DIGITS="
font_44d 44 600 0123456789%
font_84d 84 300 0123456789:
font_36d 36 600 0123456789%
font_64d 64 300 0123456789:
"
$PYTHON make_text_fonts.py $(echo "$FONTS" "$DIGITS" | awk 'NF { print $2 ":" $3 }')

echo "$FONTS" | while read -r name size weight; do
  [ -n "$name" ] || continue
  $CONV --no-compress --no-prefilter --bpp 4 --size "$size" --format lvgl --lv-include lvgl.h \
    --font "fonts/RobotoFlex-$size-$weight.ttf" -r $TEXT -o "$OUT/$name.c" --lv-font-name "$name"
done
echo "$DIGITS" | while read -r name size weight symbols; do
  [ -n "$name" ] || continue
  $CONV --no-compress --bpp 2 --size "$size" --format lvgl --lv-include lvgl.h \
    --font "fonts/RobotoFlex-$size-$weight.ttf" --symbols "$symbols" -o "$OUT/$name.c" --lv-font-name "$name"
done

# Icons: make_icon_font.py writes fonts/icons-<px>.ttf and ../app/ui/icons.h.
# 14 px only for the compact profile; 32 px holds only the media keys.
$PYTHON make_icon_font.py
for s in 14 16 20 24 32; do
  $CONV --no-compress --bpp 4 --size $s --format lvgl --lv-include lvgl.h \
    --font fonts/icons-$s.ttf -r 0xE000-0xE1FF -o "$OUT/icons_$s.c" --lv-font-name icons_$s
done
ls -la "$OUT"

# Splash logo (make_logo_font.py): body + dots, drawn 80 px tall
$CONV --no-compress --bpp 4 --size 80 --format lvgl --lv-include lvgl.h \
  --font logo.otf -r 0xE100-0xE101 -o "$OUT/logo_80.c" --lv-font-name logo_80
