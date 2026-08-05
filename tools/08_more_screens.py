#!/usr/bin/env python3
"""Extra surfaces for the motion system: a tall scrollable catalog, an object
detail screen, and settings. Rendered the same way as 06_screens.py."""
import os, numpy as np, json
from PIL import Image, ImageDraw, ImageFont
G='/home/claude/gen/out'; OUT=f'{G}/screens'
exec(open('06_screens.py').read().split('LST=4.025')[0])   # reuse helpers

# ---------- tall catalog list surface (scrollable) ----------
ROWS=[('M 31','Andromeda Galaxy','gal_spiral',3.4,64),('M 32','—','gal_elliptical',8.1,63),
('M 110','—','gal_elliptical',8.5,63),('M 33','Triangulum','gal_spiral',5.7,52),
('M 57','Ring Nebula','neb_planetary',8.8,61),('M 56','—','cl_globular',8.3,50),
('M 13','Hercules Cluster','cl_globular',5.8,58),('M 92','—','cl_globular',6.4,49),
('NGC 7000','North America','neb_emission',4.0,47),('IC 5070','Pelican','neb_emission',8.0,46),
('M 27','Dumbbell','neb_planetary',7.4,44),('M 71','—','cl_globular',8.2,43),
('NGC 891','—','gal_lenticular',10.0,41),('NGC 869','Double Cluster','cl_open',4.3,38),
('NGC 884','Double Cluster','cl_open',4.4,38),('IC 1396','Elephant Trunk','neb_dark',3.5,36),
('M 81',"Bode's Galaxy",'gal_spiral',6.9,33),('M 82','Cigar Galaxy','gal_irregular',8.4,33),
('M 76','Little Dumbbell','neb_planetary',10.1,31),('M 1','Crab Nebula','neb_supernova',8.4,70),
('M 45','Pleiades','cl_open',1.6,66),('NGC 1499','California','neb_emission',6.0,58),
('M 42','Orion Nebula','neb_emission',4.0,42),('M 43','De Mairan','neb_emission',9.0,42),
('NGC 2024','Flame','neb_emission',10.0,40),('IC 434','Horsehead','neb_dark',6.8,39),
('M 78','—','neb_reflection',8.3,41),('NGC 2264','Christmas Tree','cl_open_neb',3.9,37),
('M 35','—','cl_open',5.3,55),('NGC 2158','—','cl_open',8.6,55),
('M 44','Beehive','cl_open',3.7,44),('M 67','—','cl_open',6.1,40),
('NGC 2392','Eskimo','neb_planetary',9.1,45),('M 97','Owl Nebula','neb_planetary',9.9,55),
('M 108','—','gal_spiral',10.0,54),('M 51','Whirlpool','gal_interacting',8.4,48),
('M 101','Pinwheel','gal_spiral',7.9,50),('M 63','Sunflower','gal_spiral',8.6,46)]
TH_=len(ROWS)*32
tall=Image.new('RGB',(320,TH_),T['Ground']); d=ImageDraw.Draw(tall)
for i,(idn,nm,ic,mag,al) in enumerate(ROWS):
    y=i*32
    icon(tall,ic,8,y+4,T['TextMid'])
    d.text((40,y+8),idn,font=FONTS['Body'],fill=T['TextMid'])
    d.text((104,y+8),nm,font=FONTS['Body'],fill=T['TextLow'])
    d.text((250,y+8),f'{mag:.1f}',font=FONTS['Body'],fill=T['TextMid'],anchor='ra')
    d.text((308,y+8),f'+{al}°',font=FONTS['Body'],fill=T['TextMid'],anchor='ra')
    d.line([0,y+31,320,y+31],fill=T['RuleFaint'])
rgb565_dither(tall).save(f'{OUT}/catalog_list.png')
print(f'catalog_list.png  320x{TH_}  {len(ROWS)} rows')

# ---------- object detail ----------
mw4=np.load('/home/claude/sky/mw4k.npy')
crop=mw4[900:1052,2650:2920]                       # a rich real Milky Way field
crop=np.clip(crop/np.percentile(crop.sum(2),99.9)*3.0,0,1)**0.5
field=(crop*255).astype(np.uint8)

im=Image.new('RGB',(320,480),T['Ground']); d=ImageDraw.Draw(im)
statusbar(im,'M 13','')
d.text((60,8),'HERCULES CLUSTER',font=FONTS['Label'],fill=T['TextMid'])
RAIL=['info','center','goto','image','notes','back']
for i,g_ in enumerate(RAIL):
    y=32+i*68; on=(i==0)
    if on:
        d.rectangle([0,y,48,y+68],fill=T['Surface'])
        d.rectangle([0,y,2,y+68],fill=T['Accent'])
    gly(im,g_,12,y+18,T['Accent'] if on else T['TextLow'],24)
    d.text((24,y+48),g_.upper(),font=FONTS['Label'],fill=T['Accent'] if on else T['TextLow'],anchor='ma')
im.paste(Image.fromarray(field),(50,32))
d=ImageDraw.Draw(im)
d.rectangle([50,32,319,183],outline=T['Rule'])
d.text((58,166),'0.82° FIELD · 25 mm',font=FONTS['Label'],fill=T['TextMid'])
icon(im,'cl_globular',262,40,T['Accent'],48)
d.line([50,192,320,192],fill=T['Rule'])
for i,(k,v) in enumerate((('Type','Globular cluster'),('Constellation','Hercules'),
    ('RA (J2000)','16h 41m 41s'),('Dec (J2000)','+36° 27′ 35″'),('Magnitude','5.8'),
    ('Surface bright.','11.9'),('Size','20.0′'),('Altitude','58.2°'),('Transit','23:14'))):
    y=200+i*24
    d.text((58,y),k,font=FONTS['Body'],fill=T['TextMid'])
    d.text((308,y),v,font=FONTS['Value'] if i>3 else FONTS['Body'],
           fill=T['TextHi'],anchor='ra')
    d.line([50,y+21,320,y+21],fill=T['RuleFaint'])
d.line([0,426,320,426],fill=T['Warn'])
gly(im,'warning',12,436,T['Warn'])
d.text((36,438),'Press A again to slew',font=FONTS['Value'],fill=T['Warn'])
rgb565_dither(im).save(f'{OUT}/detail.png')

# ---------- settings ----------
im=Image.new('RGB',(320,480),T['Ground']); d=ImageDraw.Draw(im)
statusbar(im,'Settings','')
SET=[('Radar widget','SKY TRACK','Upper widget on Home'),
     ('Night mode','OFF','Red observing palette · hold START'),
     ('Brightness','25%','Auto-dims to 8% after 30 s idle'),
     ('Motion','FULL','Transitions, scrolling, live elements'),
     ('Temperature','HIDDEN','ESP32 chip temp, not outdoor air'),
     ('Wi-Fi / OnStep','PROFILE 1','OnStepWiFi · 192.168.0.1:9999'),
     ('Deep sleep','HOLD BTN 2','Not an electrical disconnect')]
for i,(k,v,why) in enumerate(SET):
    y=34+i*58; sel=(i==2)
    if sel:
        d.rectangle([0,y,320,y+56],fill=T['Surface2']); d.rectangle([0,y,2,y+56],fill=T['Accent'])
    d.text((14,y+9),k,font=FONTS['Value'],fill=T['TextHi'])
    d.text((306,y+9),v,font=FONTS['Value'],fill=T['Accent'] if sel else T['TextMid'],anchor='ra')
    d.text((14,y+32),why,font=FONTS['Body'],fill=T['TextLow'])
    d.line([0,y+56,320,y+56],fill=T['RuleFaint'])
d.line([0,454,320,454],fill=T['Rule'])
for i,(g_,lab) in enumerate((('chev_up','MOVE'),('btn_a','CHANGE'),('chev_left','BACK'))):
    gly(im,g_,10+i*104,460,T['TextLow'])
    d.text((28+i*104,463),lab,font=FONTS['Label'],fill=T['TextLow'])
rgb565_dither(im).save(f'{OUT}/settings.png')
print('wrote detail.png settings.png')
