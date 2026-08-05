#!/usr/bin/env python3
"""
UI glyph set: 4-bit alpha, 16 px (status bar, footer prompts) and 24 px (icon
rail, action tiles). Drawn at 8x with PIL and boxed down.

Stroke weight is held constant in design units so the whole set has one optical
colour - the commonest failure in hand-assembled icon sets is glyphs that look
bolder or lighter than their neighbours at the same size.
"""
import os, numpy as np
from PIL import Image, ImageDraw, ImageFont
OUT='/home/claude/gen/out/glyphs'; os.makedirs(OUT, exist_ok=True)
SS=8; G=24.0                      # design grid
FONT='/home/claude/gen/out/fonts/OnStep-Strong.ttf'

def new(n):
    im=Image.new('L',(n*SS,n*SS),0); return im, ImageDraw.Draw(im), n*SS/G
def W(m,w=1.7): return max(1,int(round(w*m)))

def glyph(kind,n):
    im,d,m=new(n)
    S=lambda v: v*m
    sw=W(m)
    if kind=='info':
        d.ellipse([S(2.5),S(2.5),S(21.5),S(21.5)],outline=255,width=sw)
        d.ellipse([S(10.9),S(6.6),S(13.1),S(8.8)],fill=255)
        d.line([S(12),S(10.8),S(12),S(17.4)],fill=255,width=W(m,2.0))
    elif kind=='center':
        d.ellipse([S(7),S(7),S(17),S(17)],outline=255,width=sw)
        for a,b,c_,e in ((12,1.6,12,5.6),(12,18.4,12,22.4),(1.6,12,5.6,12),(18.4,12,22.4,12)):
            d.line([S(a),S(b),S(c_),S(e)],fill=255,width=sw)
        d.ellipse([S(11),S(11),S(13),S(13)],fill=255)
    elif kind=='goto':
        d.ellipse([S(8.5),S(8.5),S(19.5),S(19.5)],outline=255,width=sw)
        d.line([S(3),S(21),S(12.5),S(11.5)],fill=255,width=sw)
        d.polygon([(S(2),S(22)),(S(2),S(15.5)),(S(8.5),S(22))],fill=255)
    elif kind=='image':
        d.rounded_rectangle([S(2.5),S(4.5),S(21.5),S(19.5)],radius=S(2),outline=255,width=sw)
        d.polygon([(S(5),S(17)),(S(10),S(10)),(S(13.5),S(14)),(S(16),S(11.5)),(S(19),S(17))],fill=255)
        d.ellipse([S(15.4),S(7),S(18.2),S(9.8)],fill=255)
    elif kind=='notes':
        d.rounded_rectangle([S(4),S(2.5),S(20),S(21.5)],radius=S(2),outline=255,width=sw)
        for y in (7.5,11.5,15.5):
            d.line([S(7.5),S(y),S(16.5),S(y)],fill=255,width=sw)
    elif kind=='back':
        d.line([S(15.5),S(4.5),S(8),S(12)],fill=255,width=W(m,2.1))
        d.line([S(8),S(12),S(15.5),S(19.5)],fill=255,width=W(m,2.1))
    elif kind=='align':
        for a in range(5):
            pass
        pts=[]
        for i in range(10):
            r=S(8.5) if i%2==0 else S(3.6); t=np.deg2rad(-90+i*36)
            pts.append((S(12)+r*np.cos(t)/m*m, S(12)+r*np.sin(t)))
        pts=[(S(12)+ (8.5 if i%2==0 else 3.6)*m*np.cos(np.deg2rad(-90+i*36)),
              S(12)+ (8.5 if i%2==0 else 3.6)*m*np.sin(np.deg2rad(-90+i*36))) for i in range(10)]
        d.polygon(pts,outline=255,fill=255)
    elif kind=='guide':
        pts=[]
        for i in range(97):
            x=2+i*20/96.0
            y=12-5.2*np.sin(i/96.0*6.28*2)*np.exp(-((i-48)/60.0)**2)
            pts.append((S(x),S(y)))
        d.line(pts,fill=255,width=W(m,1.9),joint='curve')
    elif kind=='tools':
        for i,(y,kx) in enumerate(((7,15.5),(12,9.0),(17,17.5))):
            d.line([S(3.5),S(y),S(20.5),S(y)],fill=255,width=sw)
            d.ellipse([S(kx-2.1),S(y-2.1),S(kx+2.1),S(y+2.1)],fill=0)
            d.ellipse([S(kx-2.1),S(y-2.1),S(kx+2.1),S(y+2.1)],outline=255,width=W(m,1.9))
    elif kind.startswith('wifi'):
        lvl=int(kind[-1])
        d.ellipse([S(10.7),S(17.3),S(13.3),S(19.9)],fill=255 if lvl>=0 else 60)
        for i,r in enumerate((4.6,8.0,11.4)):
            col=255 if i<lvl else 45
            d.arc([S(12-r),S(18.6-r),S(12+r),S(18.6+r)],215,325,fill=col,width=W(m,1.9))
    elif kind=='link':
        d.arc([S(2.5),S(8),S(13.5),S(19)],40,260,fill=255,width=W(m,2.0))
        d.arc([S(10.5),S(5),S(21.5),S(16)],220,80,fill=255,width=W(m,2.0))
    elif kind=='tracking':
        d.arc([S(4),S(4),S(20),S(20)],35,320,fill=255,width=W(m,2.0))
        d.polygon([(S(19.6),S(2.6)),(S(21.8),S(9.0)),(S(15.2),S(7.4))],fill=255)
    elif kind=='battery':
        d.rounded_rectangle([S(2.5),S(7.5),S(19),S(16.5)],radius=S(1.4),outline=255,width=sw)
        d.rounded_rectangle([S(19.8),S(10.2),S(22),S(13.8)],radius=S(0.8),fill=255)
        d.rectangle([S(4.4),S(9.4),S(13.5),S(14.6)],fill=255)
    elif kind=='sdcard':
        d.polygon([(S(5),S(3.5)),(S(15),S(3.5)),(S(19),S(7.5)),(S(19),S(20.5)),(S(5),S(20.5))],
                  outline=255,width=sw)
        for x in (8,10.6,13.2):
            d.line([S(x),S(6),S(x),S(9.5)],fill=255,width=W(m,1.4))
    elif kind=='warning':
        d.polygon([(S(12),S(3)),(S(22),S(20.5)),(S(2),S(20.5))],outline=255,width=sw)
        d.line([S(12),S(9),S(12),S(15)],fill=255,width=W(m,2.0))
        d.ellipse([S(10.9),S(16.8),S(13.1),S(19.0)],fill=255)
    elif kind=='park':
        d.ellipse([S(2.5),S(2.5),S(21.5),S(21.5)],outline=255,width=sw)
        d.line([S(9.5),S(6.5),S(9.5),S(17.5)],fill=255,width=W(m,2.0))
        d.arc([S(9.5),S(6.5),S(16.5),S(13.0)],270,90,fill=255,width=W(m,2.0))
    elif kind=='filter':
        d.polygon([(S(2.5),S(4)),(S(21.5),S(4)),(S(14),S(12.5)),(S(14),S(20)),(S(10),S(17.5)),
                   (S(10),S(12.5))],outline=255,width=sw)
    elif kind=='sort':
        d.line([S(4),S(6),S(4),S(19)],fill=255,width=sw)
        d.polygon([(S(4),S(21)),(S(0.9),S(16.8)),(S(7.1),S(16.8))],fill=255)
        for i,w_ in enumerate((11,8,5)):
            d.line([S(11),S(7.5+i*4.6),S(11+w_),S(7.5+i*4.6)],fill=255,width=sw)
    elif kind=='moon':
        d.ellipse([S(3),S(3),S(21),S(21)],fill=255)
        d.ellipse([S(7.5),S(0.5),S(25.5),S(18.5)],fill=0)
    elif kind=='clock':
        d.ellipse([S(2.5),S(2.5),S(21.5),S(21.5)],outline=255,width=sw)
        d.line([S(12),S(12),S(12),S(6.5)],fill=255,width=W(m,1.9))
        d.line([S(12),S(12),S(16.4),S(14.4)],fill=255,width=W(m,1.9))
    elif kind=='check':
        d.line([S(3.5),S(12.5),S(9.5),S(18.5)],fill=255,width=W(m,2.3))
        d.line([S(9.5),S(18.5),S(20.5),S(5.5)],fill=255,width=W(m,2.3))
    elif kind=='close':
        d.line([S(5),S(5),S(19),S(19)],fill=255,width=W(m,2.1))
        d.line([S(19),S(5),S(5),S(19)],fill=255,width=W(m,2.1))
    elif kind in ('chev_left','chev_right','chev_up','chev_down'):
        pts={'chev_left':((15.5,4.5),(8,12),(15.5,19.5)),
             'chev_right':((8.5,4.5),(16,12),(8.5,19.5)),
             'chev_up':((4.5,15.5),(12,8),(19.5,15.5)),
             'chev_down':((4.5,8.5),(12,16),(19.5,8.5))}[kind]
        d.line([(S(a),S(b)) for a,b in pts],fill=255,width=W(m,2.1),joint='curve')
    elif kind=='stick':
        d.ellipse([S(2.5),S(2.5),S(21.5),S(21.5)],outline=255,width=sw)
        d.ellipse([S(8.6),S(8.6),S(15.4),S(15.4)],fill=255)
        for a in (0,90,180,270):
            t=np.deg2rad(a)
            d.line([S(12+6.6*np.cos(t)),S(12+6.6*np.sin(t)),
                    S(12+9.4*np.cos(t)),S(12+9.4*np.sin(t))],fill=255,width=W(m,1.5))
    elif kind=='dpad':
        d.polygon([(S(9),S(2.5)),(S(15),S(2.5)),(S(15),S(9)),(S(21.5),S(9)),(S(21.5),S(15)),
                   (S(15),S(15)),(S(15),S(21.5)),(S(9),S(21.5)),(S(9),S(15)),(S(2.5),S(15)),
                   (S(2.5),S(9)),(S(9),S(9))],outline=255,width=sw)
    elif kind.startswith('btn_'):
        ch=kind[-1].upper()
        d.ellipse([S(1.6),S(1.6),S(22.4),S(22.4)],outline=255,width=W(m,1.9))
        try:
            f=ImageFont.truetype(FONT,int(S(13.5)))
            bb=d.textbbox((0,0),ch,font=f)
            d.text((S(12)-(bb[2]-bb[0])/2-bb[0], S(12)-(bb[3]-bb[1])/2-bb[1]),ch,fill=255,font=f)
        except Exception:
            d.text((S(9),S(7)),ch,fill=255)
    else: raise KeyError(kind)
    a=np.asarray(im,np.float32)/255.0
    return np.clip(a.reshape(n,SS,n,SS).mean(axis=(1,3)),0,1)

KINDS=['info','center','goto','image','notes','back','align','guide','tools',
       'wifi0','wifi1','wifi2','wifi3','link','tracking','battery','sdcard',
       'warning','park','filter','sort','moon','clock','check','close',
       'chev_left','chev_right','chev_up','chev_down','stick','dpad',
       'btn_a','btn_b','btn_x','btn_y']

def pack4(a):
    q=np.clip(np.round(a*15),0,15).astype(np.uint8)
    if q.shape[1]%2: q=np.pad(q,((0,0),(0,1)))
    return (q[:,0::2]<<4|q[:,1::2]).tobytes()

made={}
hdr=['#pragma once','#include <cstdint>','',
     '// Generated by tools/04_ui_glyphs.py - 4-bit alpha, tint at draw time.','',
     'namespace uiglyph {','']
for n in (16,24):
    blob=[]
    for k in KINDS:
        a=glyph(k,n); made[(k,n)]=a; blob.append(pack4(a))
    open(f'{OUT}/glyphs_{n}.bin','wb').write(b''.join(blob))
    hdr.append(f'constexpr int kSize{n} = {n};')
    hdr.append(f'constexpr int kBytes{n} = {len(blob[0])};')
    print(f'{n:3d} px : {len(KINDS)} glyphs, {len(blob[0])} B each, {len(blob[0])*len(KINDS)} B')
hdr += ['','enum class Id : uint8_t {']
for i,k in enumerate(KINDS):
    hdr.append(f'  {"".join(p.capitalize() for p in k.split("_"))} = {i},')
hdr += [f'  Count = {len(KINDS)}','};','','}  // namespace uiglyph']
open(f'{OUT}/UiGlyphs.h','w').write('\n'.join(hdr)+'\n')

for n,scale in ((16,4),(24,3)):
    cols=9; rows=(len(KINDS)+cols-1)//cols
    cw=ch=n*scale+14
    img=Image.new('RGB',(cols*cw,rows*ch),(8,8,16))
    for i,k in enumerate(KINDS):
        a=made[(k,n)]
        rgb=np.zeros((n,n,3),np.uint8)
        for c,(lo,hi) in enumerate(((8,247),(8,247),(16,247))):
            rgb[...,c]=(lo+(hi-lo)*a).astype(np.uint8)
        img.paste(Image.fromarray(rgb).resize((n*scale,n*scale),Image.NEAREST),
                  ((i%cols)*cw+7,(i//cols)*ch+7))
    img.save(f'{OUT}/sheet_{n}.png')

# optical weight consistency: ink coverage should cluster, not scatter
cov=np.array([made[(k,24)].mean() for k in KINDS])
print(f'\noptical weight: ink {cov.min()*100:.1f}%..{cov.max()*100:.1f}%  '
      f'median {np.median(cov)*100:.1f}%  sd {cov.std()*100:.1f}pp')
out=[(k,made[(k,24)].mean()) for k in KINDS]
out.sort(key=lambda t:t[1])
print('  lightest:', ', '.join(f'{k}({v*100:.0f}%)' for k,v in out[:3]))
print('  heaviest:', ', '.join(f'{k}({v*100:.0f}%)' for k,v in out[-3:]))
