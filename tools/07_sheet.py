#!/usr/bin/env python3
import base64, os, glob
G='/home/claude/gen/out'
def b64(p): return base64.b64encode(open(p,'rb').read()).decode()
def img(p,w=None,cls=''):
    return f'<img class="{cls}" {"style=width:%dpx"%w if w else ""} src="data:image/png;base64,{b64(p)}">'

# font specimen rendered at true px
from PIL import Image, ImageDraw, ImageFont
sp=Image.new('RGB',(760,300),(8,8,16)); d=ImageDraw.Draw(sp)
LAD=[('Hero',34,'05:34:31.9  +22:00:52','hero readout'),
     ('Head',20,'NGC 7000 · North America Nebula','object names, titles'),
     ('Value',15,'ALT 62.1°   AZ 129.7°   MAG 8.8','inline values'),
     ('Body',12,'Surface brightness 13.4 mag/arcmin²  ·  transit 23:41','rows and tables'),
     ('Strong',12,'M 13 — Hercules Cluster (selected)','emphasis'),
     ('Label',9,'MERIDIAN FLIP · TRANSIT · ALTITUDE FLOOR','tracked labels')]
y=14
for role,px,txt,why in LAD:
    f=ImageFont.truetype(f'{G}/fonts/OnStep-{role}.ttf',px)
    d.text((14,y),txt,font=f,fill=(247,247,247))
    d.text((600,y+px-11),f'{role} {px}px',
           font=ImageFont.truetype(f'{G}/fonts/OnStep-Label.ttf',9),fill=(90,97,107))
    d.text((600,y+px-1),why,
           font=ImageFont.truetype(f'{G}/fonts/OnStep-Label.ttf',9),fill=(90,97,107))
    y+=px+18
# tabular proof
f=ImageFont.truetype(f'{G}/fonts/OnStep-Hero.ttf',34)
d.text((14,y+4),'11:11:11.1',font=f,fill=(90,201,239))
d.text((190,y+4),'00:00:00.0',font=f,fill=(90,201,239))
d.text((370,y+16),'identical widths — digits cannot shuffle',
       font=ImageFont.truetype(f'{G}/fonts/OnStep-Body.ttf',12),fill=(148,153,165))
sp.save(f'{G}/fonts/specimen.png')

H=f'''<!DOCTYPE html><html><head><meta charset="utf-8"><title>OnStep 320x480 asset set</title>
<style>
*{{box-sizing:border-box;margin:0;padding:0}}
body{{background:#0e0f12;color:#a8adb5;font:13px/1.6 "Helvetica Neue",Inter,Helvetica,Arial,sans-serif;
padding:24px 20px 70px}}
h1{{font-size:19px;font-weight:500;color:#f0f1f3;margin-bottom:4px}}
h2{{font-size:9px;letter-spacing:.2em;text-transform:uppercase;color:#5c626c;font-weight:500;
margin:34px 0 12px;border-top:1px solid #242830;padding-top:14px}}
.sub{{font-size:12.5px;color:#7a8089;max-width:820px;margin-bottom:8px}}
.row{{display:flex;flex-wrap:wrap;gap:18px;align-items:flex-start}}
.card{{background:#15171c;border:1px solid #242830;border-radius:5px;padding:12px}}
.cap{{font-size:11.5px;color:#7a8089;margin-top:9px;max-width:330px;line-height:1.5}}
.cap b{{color:#a8adb5}}
img{{display:block;image-rendering:pixelated}}
table{{border-collapse:collapse;font-size:12px}}
td{{padding:3px 16px 3px 0;border-bottom:1px solid #1c1f25;color:#8d939c}}
td:last-child{{color:#e6e9ee;text-align:right;font-variant-numeric:tabular-nums}}
code{{background:#1c1f25;padding:1px 5px;border-radius:3px;color:#5ac9ef;font-size:11.5px}}
</style></head><body>
<h1>OnStep 320×480 — generated asset set</h1>
<p class="sub">Everything below was generated for the Elecrow 3.5″ panel and rendered through the
same RGB565 + 4×4 ordered dither the ST77922 will apply. The screens are integration renders: real
fonts, real icons, real palette, real sky texture, real Moon.</p>

<h2>Screens — 320 × 480, 1:1, RGB565 dithered</h2>
<div class="row">
<div><div class="card">{img(f'{G}/screens/home.png')}</div>
<p class="cap"><b>Home.</b> 208 px dome over the real Milky Way at the matched 512×256 texture,
constellation figures traced from the NASA raster, trajectory arc and reticle, 34 px hero readout
with tabular figures.</p></div>
<div><div class="card">{img(f'{G}/screens/catalog.png')}</div>
<p class="cap"><b>Catalog.</b> 13 rows at 32 px pitch, each with its 24 px type icon. Selection is
an accent edge and a weight change, not a filled bar.</p></div>
<div><div class="card">{img(f'{G}/screens/sky.png')}</div>
<p class="cap"><b>Sky.</b> The Moon is rendered from NASA albedo with a Lommel–Seeliger terminator
and libration applied — not a two-tone disc.</p></div>
</div>

<h2>Type ladder — Inter, tabular figures baked in</h2>
<div class="card" style="display:inline-block">{img(f'{G}/fonts/specimen.png')}</div>
<p class="cap" style="max-width:820px">Inter's default figures are proportional — digit <code>1</code>
is 833 units against <code>0</code> at 1292, which makes a live RA/Dec readout shuffle sideways every
second. The <code>tnum</code> and <code>zero</code> substitutions are baked into the cmap of each
instance, so your rasteriser never needs to know about OpenType features. Optical size is pinned per
role: 14 for small text, 32 for the hero.</p>

<h2>DSO type icons — 23 types, 4-bit alpha</h2>
<div class="row">
<div><div class="card">{img(f'{G}/icons/sheet_24.png')}</div><p class="cap"><b>24 px</b> for catalog rows.</p></div>
<div><div class="card">{img(f'{G}/icons/sheet_48.png')}</div><p class="cap"><b>48 px</b> for detail headers.</p></div>
</div>
<p class="cap" style="max-width:820px">Every one of the 253 pairs is separated by at least 0.13 RMS
over the alpha map. The first pass failed that badly — six of the nebula and cluster marks were all
soft haze and collapsed into each other — so they were redesigned around distinct silhouettes:
reflection is identified by spikes, HII by a closed bubble, globular by a graded ball, quasar by
diagonal rays with no haze at all.</p>

<h2>UI glyphs — 35, two sizes</h2>
<div class="row">
<div><div class="card">{img(f'{G}/glyphs/sheet_16.png')}</div><p class="cap"><b>16 px</b> status bar and footer prompts.</p></div>
<div><div class="card">{img(f'{G}/glyphs/sheet_24.png')}</div><p class="cap"><b>24 px</b> icon rail and action tiles.</p></div>
</div>

<h2>Moon</h2>
<div class="card" style="display:inline-block">{img(f'{G}/moon/phases.png')}</div>
<div class="card" style="display:inline-block;margin-left:14px">{img(f'{G}/moon/libration.png')}</div>
<div class="card" style="display:inline-block;margin-left:14px">{img(f'{G}/moon/big.png')}</div>
<p class="cap" style="max-width:820px">Illuminated fraction tracks the geometric value
(1−cos φ)/2 to within 0.02 across all phases. Libration is a rotation of the surface normal before
the texture fetch, so Mare Crisium swings around the limb over a month using the same code path that
draws the phase.</p>

<h2>Storage</h2>
<div class="row"><div class="card"><table>
<tr><td>DSO icons 24 px × 23</td><td>6.5 KB</td></tr>
<tr><td>DSO icons 48 px × 23</td><td>25.9 KB</td></tr>
<tr><td>UI glyphs 16 px × 35</td><td>4.4 KB</td></tr>
<tr><td>UI glyphs 24 px × 35</td><td>9.8 KB</td></tr>
<tr><td>Moon albedo 256×128</td><td>32 KB</td></tr>
<tr><td>Moon albedo 512×256</td><td>128 KB</td></tr>
<tr><td>Milky Way 512×256 + palettes</td><td>129 KB</td></tr>
<tr><td>Star map 512×256</td><td>128 KB</td></tr>
<tr><td>Constellation vectors</td><td>14 KB</td></tr>
<tr><td>Theme565.h</td><td>—</td></tr>
<tr><td><b>Total art</b></td><td><b>478 KB</b></td></tr>
</table></div></div>
<p class="cap" style="max-width:820px">Against 8 MB of PSRAM and a card measured in gigabytes. The
fonts are TTFs for your own <code>generate_fonts.py</code> to rasterise, so they are not counted —
the rasterised atlases will depend on the glyph set you keep.</p>
</body></html>'''
open(f'{G}/asset_sheet.html','w').write(H)
print('contact sheet: %.0f KB' % (os.path.getsize(f'{G}/asset_sheet.html')/1024))
