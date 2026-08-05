#!/usr/bin/env bash
# Inter TTFs -> LVGL C fonts. Run 01_fonts.py first.
# The range is deliberately tight: every glyph costs flash.
set -e
R="0x20-0x7E,0x00B0,0x2032,0x2033,0x00B7,0x2713"
for spec in "Label 9 onstep_label" "Body 12 onstep_body" "Value 15 onstep_value" \
            "Strong 12 onstep_strong" "Head 20 onstep_head" "Hero 34 onstep_hero"; do
  set -- $spec
  lv_font_conv --font out/fonts/OnStep-$1.ttf -r "$R" --size $2 --bpp 4 \
    --format lvgl --no-compress --lv-include lvgl.h \
    -o ../repo/OnStep_Elecrow_LVGL/assets/$3.c
  echo "$3 ($2px)"
done
