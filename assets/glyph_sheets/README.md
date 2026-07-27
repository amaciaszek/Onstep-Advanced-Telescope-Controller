# DSO glyph design sheets

The controller has 22 deep-sky object types. Each has an outline (inactive)
and filled (active/selected) form in full-color day and red-only night modes:
88 runtime glyphs total.

- `glyphs_final_color_18px.png` and `glyphs_final_red_18px.png` show the exact
  18×18 firmware images both at literal 1:1 size and as a 5× nearest-neighbor
  pixel inspection. The zoom does not smooth or alter the pixels.
- `glyphs_master_color_72px.png` and `glyphs_master_red_72px.png` show the true
  72×72 procedural artwork before firmware downsampling.
- `glyphs_all_88_master_overview.png` puts all variants on one comparison sheet.
- Individual transparent 72×72 masters are in `../master_png/`.

When designing replacements, work at 72×72, keep important strokes at least
4 master pixels wide, and test the final LANCZOS reduction at 18×18. Night-mode
types must remain distinguishable by silhouette and texture, not by hue.
