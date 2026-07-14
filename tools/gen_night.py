#!/usr/bin/env python3
"""gen_night.py — night-vision (red/black) asset set.
Philosophy per Adam: OUTLINE red = negative/inactive, FILLED red = positive/active.
Pure red channel only (protects dark adaptation); no white, no blue, no green.
Names: n_<name> = outline/base, n_<name>_f = filled/active variant.
"""
from PIL import Image, ImageDraw, ImageFilter, ImageFont
import math, os

S = 4
OUT = "assets/screen_png"
os.makedirs(OUT, exist_ok=True)
RED    = (255, 40, 20)
RED_HI = (255, 70, 40)
RED_LO = (120, 18, 8)
DARKP  = (16, 3, 2)
FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

def canvas(w, h): return Image.new("RGBA", (w*S, h*S), (0,0,0,0))
def L(d): return int(round(d*S))

def glowize(img, blur=6, strength=1.2):
    a = img.split()[3]
    glow = Image.new("RGBA", img.size, RED + (0,))
    glow.putalpha(a)
    glow = glow.filter(ImageFilter.GaussianBlur(blur))
    r,g,b,ga = glow.split()
    ga = ga.point(lambda v: min(255,int(v*strength)))
    glow = Image.merge("RGBA",(r,g,b,ga))
    out = Image.new("RGBA", img.size,(0,0,0,0))
    out.alpha_composite(glow); out.alpha_composite(img)
    return out

def finish(img, w, h, name):
    img = img.resize((w,h), Image.LANCZOS)
    img.save(f"{OUT}/{name}.png"); print("wrote",name)

def wifi(filled):
    img=canvas(22,22); d=ImageDraw.Draw(img)
    cx,cy=L(11),L(17)
    for r in [L(4),L(8),L(12)]:
        d.arc([cx-r,cy-r,cx+r,cy+r],225,315,fill=RED_HI,width=int(L(2.4 if filled else 1.4)))
    d.ellipse([cx-L(1.8),cy-L(1.8),cx+L(1.8),cy+L(1.8)],fill=RED_HI)
    return glowize(img, strength=1.5 if filled else 0.7)

def link(filled):
    img=canvas(22,22); d=ImageDraw.Draw(img)
    w=int(L(2.6 if filled else 1.4))
    d.rounded_rectangle([L(2),L(7),L(12),L(15)],radius=L(4),outline=RED_HI,width=w)
    d.rounded_rectangle([L(10),L(7),L(20),L(15)],radius=L(4),outline=RED_HI,width=w)
    img=img.rotate(-30,resample=Image.BICUBIC,center=(L(11),L(11)))
    return glowize(img, strength=1.5 if filled else 0.7)

def padlock(open_, filled):
    img=canvas(22,22); d=ImageDraw.Draw(img)
    if filled:
        d.rounded_rectangle([L(5),L(10),L(17),L(19)],radius=L(2),fill=RED,outline=RED_HI,width=int(L(1)))
        d.ellipse([L(10),L(13),L(12.6),L(15.6)],fill=(20,0,0))
    else:
        d.rounded_rectangle([L(5),L(10),L(17),L(19)],radius=L(2),outline=RED_LO,width=int(L(1.4)))
        d.ellipse([L(10),L(13),L(12.6),L(15.6)],outline=RED_LO,width=int(L(1)))
    col = RED_HI if filled else RED_LO
    if open_:
        d.arc([L(9),L(1),L(19),L(11)],270,90,fill=col,width=int(L(2)))
        d.line([L(9),L(6),L(9),L(10)],fill=col,width=int(L(2)))
    else:
        d.arc([L(6.5),L(2),L(15.5),L(11)],180,360,fill=col,width=int(L(2)))
        d.line([L(6.5),L(6.5),L(6.5),L(10)],fill=col,width=int(L(2)))
        d.line([L(15.5),L(6.5),L(15.5),L(10)],fill=col,width=int(L(2)))
    return glowize(img, strength=1.4 if filled else 0.5)

def guide(filled):
    img=canvas(22,22); d=ImageDraw.Draw(img)
    cx=cy=L(11); r=L(7)
    w=int(L(2.2 if filled else 1.2))
    d.ellipse([cx-r,cy-r,cx+r,cy+r],outline=RED_HI,width=w)
    for a in range(4):
        dx,dy=[(0,-1),(1,0),(0,1),(-1,0)][a]
        d.line([cx+dx*L(4.5),cy+dy*L(4.5),cx+dx*L(9.5),cy+dy*L(9.5)],fill=RED_HI,width=w)
    if filled: d.ellipse([cx-L(2.4),cy-L(2.4),cx+L(2.4),cy+L(2.4)],fill=RED_HI)
    return glowize(img, strength=1.5 if filled else 0.6)

def slew(filled):
    img=canvas(22,22); d=ImageDraw.Draw(img)
    cx,cy,r=L(11),L(13),L(8.5)
    w=int(L(2.4 if filled else 1.4))
    d.arc([cx-r,cy-r,cx+r,cy+r],150,390,fill=RED_HI,width=w)
    na=math.radians(310)
    d.line([cx,cy,cx+math.cos(na)*r*0.7,cy+math.sin(na)*r*0.7],fill=RED_HI,width=int(L(1.8)))
    if filled: d.ellipse([cx-L(2),cy-L(2),cx+L(2),cy+L(2)],fill=RED_HI)
    return glowize(img, strength=1.4 if filled else 0.6)

def battery():
    img=canvas(22,22); d=ImageDraw.Draw(img)
    d.rounded_rectangle([L(2),L(7),L(18),L(15)],radius=L(1.5),outline=RED_HI,width=int(L(1.4)))
    d.rounded_rectangle([L(18.5),L(9.5),L(20.5),L(12.5)],radius=L(0.8),fill=RED_HI)
    for i in range(3):
        x=L(4.2)+i*L(4.4)
        d.rounded_rectangle([x,L(8.8),x+L(3.2),L(13.2)],radius=L(0.6),fill=RED_LO)
    return glowize(img, strength=0.7)

def temperature():
    img=canvas(22,22); d=ImageDraw.Draw(img)
    d.rounded_rectangle([L(9),L(2),L(13),L(13)],radius=L(2),outline=RED_HI,width=int(L(1.4)))
    d.ellipse([L(7),L(12),L(15),L(20)],outline=RED_HI,width=int(L(1.4)))
    d.ellipse([L(9),L(14),L(13),L(18)],fill=RED)
    d.line([L(11),L(8),L(11),L(15)],fill=RED,width=int(L(2)))
    return glowize(img, strength=0.7)

def nav(kind, filled):
    img=canvas(15,15); d=ImageDraw.Draw(img)
    col=RED_HI if filled else RED_LO
    w=int(L(1.6 if filled else 1.2))
    if kind=="home":
        d.polygon([L(7.5),L(1.5),L(13.5),L(7),L(12),L(7),L(12),L(13),L(3),L(13),L(3),L(7),L(1.5),L(7)],
                  outline=col,width=w, fill=RED_LO if filled else None)
    elif kind=="goto":
        d.ellipse([L(3),L(3),L(12),L(12)],outline=col,width=w)
        d.ellipse([L(6.4),L(6.4),L(8.6),L(8.6)],fill=col)
    elif kind=="align":
        cx=cy=L(7.5)
        for a in range(4):
            dx,dy=[(0,-1),(1,0),(0,1),(-1,0)][a]
            d.line([cx+dx*L(3),cy+dy*L(3),cx+dx*L(6.5),cy+dy*L(6.5)],fill=col,width=w)
        d.ellipse([cx-L(1.6),cy-L(1.6),cx+L(1.6),cy+L(1.6)],fill=col)
    else:
        for y in (4,7.5,11):
            d.rounded_rectangle([L(2.5),L(y-0.9),L(12.5),L(y+0.9)],radius=L(0.9),
                                fill=col if filled else None, outline=col, width=int(L(1)))
    return glowize(img, blur=4, strength=1.2 if filled else 0.4)

def radar():
    img=canvas(73,73); d=ImageDraw.Draw(img)
    cx=cy=L(36.5)
    d.ellipse([cx-L(33),cy-L(33),cx+L(33),cy+L(33)],fill=(8,1,1,235))
    d.ellipse([cx-L(33),cy-L(33),cx+L(33),cy+L(33)],outline=RED,width=int(L(1.6)))
    d.ellipse([cx-L(16.5),cy-L(16.5),cx+L(16.5),cy+L(16.5)],outline=RED_LO,width=int(L(1)))
    d.line([cx-L(31),cy,cx+L(31),cy],fill=RED_LO,width=int(L(1)))
    d.line([cx,cy-L(31),cx,cy+L(31)],fill=RED_LO,width=int(L(1)))
    for a in range(0,360,30):
        if a%90==0: continue
        ar=math.radians(a)
        d.line([cx+math.cos(ar)*L(30.5),cy+math.sin(ar)*L(30.5),
                cx+math.cos(ar)*L(33),cy+math.sin(ar)*L(33)],fill=RED,width=int(L(1)))
    img=glowize(img,strength=0.8)
    d=ImageDraw.Draw(img)
    f=ImageFont.truetype(FONT_B,L(9))
    for txt,(x,y) in [("N",(36.5,5.5)),("S",(36.5,67.5)),("E",(5.5,36.5)),("W",(67.5,36.5))]:
        bb=d.textbbox((0,0),txt,font=f)
        d.text((L(x)-(bb[2]-bb[0])/2,L(y)-(bb[3]-bb[1])/2-bb[1]),txt,font=f,fill=RED_HI)
    return img

def tech_panel(w,h):
    img=canvas(w,h); d=ImageDraw.Draw(img)
    r=L(6)
    mask=Image.new("L",img.size,0)
    ImageDraw.Draw(mask).rounded_rectangle([0,0,L(w)-1,L(h)-1],radius=r,fill=255)
    fill=Image.new("RGBA",img.size,DARKP+(210,))
    img.paste(fill,(0,0),mask)
    d=ImageDraw.Draw(img)
    d.rounded_rectangle([L(0.5),L(0.5),L(w-0.5),L(h-0.5)],radius=r,outline=RED_LO,width=int(L(1)))
    cl=L(min(8,w//6))
    for (x0,y0,dx,dy) in [(1.2,1.2,1,1),(w-1.2,1.2,-1,1),(1.2,h-1.2,1,-1),(w-1.2,h-1.2,-1,-1)]:
        d.line([L(x0),L(y0),L(x0)+cl*dx,L(y0)],fill=RED,width=int(L(1.2)))
        d.line([L(x0),L(y0),L(x0),L(y0)+cl*dy],fill=RED,width=int(L(1.2)))
    return glowize(img,blur=4,strength=0.4)

if __name__=="__main__":
    finish(wifi(False),22,22,"n_icon_wifi");   finish(wifi(True),22,22,"n_icon_wifi_f")
    finish(link(False),22,22,"n_icon_link");   finish(link(True),22,22,"n_icon_link_f")
    finish(padlock(False,False),22,22,"n_icon_park_locked")
    finish(padlock(False,True),22,22,"n_icon_park_locked_f")
    finish(padlock(True,False),22,22,"n_icon_park_unlocked")
    finish(padlock(True,True),22,22,"n_icon_park_unlocked_f")
    finish(guide(False),22,22,"n_icon_guide"); finish(guide(True),22,22,"n_icon_guide_f")
    finish(slew(False),22,22,"n_icon_slew");   finish(slew(True),22,22,"n_icon_slew_f")
    finish(battery(),22,22,"n_icon_battery");  finish(temperature(),22,22,"n_icon_temperature")
    for k in ("home","goto","align","menu"):
        finish(nav(k,False),15,15,f"n_nav_{k}"); finish(nav(k,True),15,15,f"n_nav_{k}_f")
    finish(radar(),73,73,"n_sky_radar_frame")
    finish(tech_panel(75,25),75,25,"n_panel_metric")
    finish(tech_panel(76,17),76,17,"n_panel_button")
    print("night set complete")
