#!/usr/bin/env python3
"""
Integration render: every generated asset composited into real 320x480 screens,
quantised to RGB565 with 4x4 ordered dither exactly as the panel will show them.
If a font size, icon or colour fails, it fails here rather than on the bench.
"""
import os, numpy as np, pickle
from PIL import Image, ImageDraw, ImageFont
G='/home/claude/gen/out'; OUT=f'{G}/screens'; os.makedirs(OUT,exist_ok=True)
SKY='/home/claude/sky/assets2'
D=np.pi/180; R2D=180/np.pi

T=dict(Ground=(0x08,0x08,0x10),Surface=(0x10,0x10,0x18),Surface2=(0x18,0x1c,0x29),
       Rule=(0x31,0x35,0x4a),RuleFaint=(0x21,0x24,0x31),TextHi=(0xf7,0xf7,0xf7),
       TextMid=(0x94,0x99,0xa5),TextLow=(0x5a,0x61,0x6b),Accent=(0x5a,0xc9,0xef),
       AccentDim=(0x18,0x4d,0x5a),Warn=(0xef,0xa2,0x39),Alert=(0xe7,0x49,0x4a))
F=lambda r,px: ImageFont.truetype(f'{G}/fonts/OnStep-{r}.ttf',px)
FONTS=dict(Label=F('Label',9),Body=F('Body',12),Value=F('Value',15),
           Head=F('Head',20),Hero=F('Hero',34),Strong=F('Strong',12))

def unpack4(buf,n,i):
    per=n*((n+1)//2); b=np.frombuffer(buf,np.uint8,per,i*per)
    q=np.zeros((n,n),np.uint8); f=np.stack([(b>>4)&15,b&15],1).reshape(-1)
    q.flat[:f.size]=f[:n*n]; return q.astype(np.float32)/15.0
ICO=open(f'{G}/icons/dso_24.bin','rb').read(); ICO48=open(f'{G}/icons/dso_48.bin','rb').read()
GLY=open(f'{G}/glyphs/glyphs_16.bin','rb').read(); GLY24=open(f'{G}/glyphs/glyphs_24.bin','rb').read()
KI=['gal_spiral','gal_barred','gal_elliptical','gal_lenticular','gal_irregular','gal_interacting',
    'gal_cluster','neb_emission','neb_reflection','neb_dark','neb_planetary','neb_supernova',
    'neb_hii','neb_bright','cl_open','cl_globular','cl_open_neb','asterism','star_double',
    'star_multiple','star_variable','quasar','unknown']
KG=['info','center','goto','image','notes','back','align','guide','tools','wifi0','wifi1','wifi2',
    'wifi3','link','tracking','battery','sdcard','warning','park','filter','sort','moon','clock',
    'check','close','chev_left','chev_right','chev_up','chev_down','stick','dpad',
    'btn_a','btn_b','btn_x','btn_y']

def blitA(img,a,x,y,col):
    h,w=a.shape; x0,y0=int(x),int(y)
    reg=np.asarray(img.crop((x0,y0,x0+w,y0+h))).astype(np.float32)
    if reg.shape[:2]!=(h,w): return
    c=np.array(col,np.float32)
    out=reg*(1-a[...,None])+c*a[...,None]
    img.paste(Image.fromarray(out.astype(np.uint8)),(x0,y0))
def icon(img,name,x,y,col,n=24):
    buf=ICO if n==24 else ICO48
    blitA(img,unpack4(buf,n,KI.index(name)),x,y,col)
def gly(img,name,x,y,col,n=16):
    buf=GLY if n==16 else GLY24
    blitA(img,unpack4(buf,n,KG.index(name)),x,y,col)

# ---------- sky dome ----------
mw=np.frombuffer(open(f'{SKY}/mw_1024.bin','rb').read(),np.uint8).reshape(512,1024)
st=np.frombuffer(open(f'{SKY}/star_1024.bin','rb').read(),np.uint8).reshape(512,1024)
pal=np.frombuffer(open(f'{SKY}/mw_1024.pal','rb').read(),np.uint8).reshape(256,3)
mw=mw.reshape(256,2,512,2).mean(axis=(1,3)).astype(np.uint8)      # 512x256, the matched size
st=st.reshape(256,2,512,2).mean(axis=(1,3)).astype(np.uint8)
TW,TH=512,256
figv=None
try:
    import json; fj=json.load(open(f'{SKY}/figures.json')); figv=fj['v']; figp=fj['p']
except Exception: pass

def hd2altaz(H,dec,lat):
    h,d,p=H*D,dec*D,lat*D
    sa=np.sin(d)*np.sin(p)+np.cos(d)*np.cos(p)*np.cos(h)
    alt=np.arcsin(np.clip(sa,-1,1))
    az=np.arctan2(-np.sin(h)*np.cos(d),np.sin(d)*np.cos(p)-np.cos(d)*np.sin(p)*np.cos(h))
    return alt*R2D,(az*R2D)%360
def altaz2hd(alt,az,lat):
    a,z,p=alt*D,az*D,lat*D
    sd=np.sin(a)*np.sin(p)+np.cos(a)*np.cos(p)*np.cos(z)
    dec=np.arcsin(np.clip(sd,-1,1))
    H=np.arctan2(-np.sin(z)*np.cos(a),(np.sin(a)-np.sin(dec)*np.sin(p))/np.cos(p))
    return (H*R2D)%360, dec*R2D

def dome(img,cx,cy,R,lst,lat,target=None):
    y,x=np.mgrid[int(cy-R):int(cy+R)+1,int(cx-R):int(cx+R)+1]
    dx,dy=x-cx,y-cy; rr=np.hypot(dx,dy); m=rr<=R
    alt=90*(1-np.clip(rr,0,R)/R); az=(np.arctan2(-dx,-dy)*R2D)%360
    H,dec=altaz2hd(alt,az,lat)
    u=((np.round((lst-H)/360*TW).astype(int))%TW)
    v=np.clip(np.round((90-dec)/180*TH).astype(int),0,TH-1)
    idx=mw[v,u]; sv=st[v,u].astype(np.float32)/255.0
    rgb=pal[idx].astype(np.float32)
    rgb=np.clip(rgb+np.stack([sv*236,sv*242,sv*255],-1),0,255)
    floor=(alt<30)&m
    rgb[floor]*=0.62
    base=np.asarray(img).astype(np.float32)
    reg=base[int(cy-R):int(cy-R)+m.shape[0], int(cx-R):int(cx-R)+m.shape[1]]
    reg[m]=rgb[m]
    img.paste(Image.fromarray(base.astype(np.uint8)),(0,0))
    d=ImageDraw.Draw(img)
    d.ellipse([cx-R,cy-R,cx+R,cy+R],outline=T['Rule'])
    d.ellipse([cx-R/2,cy-R/2,cx+R/2,cy+R/2],outline=T['RuleFaint'])
    d.line([cx,cy-R,cx,cy+R],fill=T['RuleFaint'])
    if figv:
        for poly in figp:
            pts=[]
            for i in poly:
                a_,z_=hd2altaz(lst-figv[i][0],figv[i][1],lat)
                if a_<0: 
                    if len(pts)>1: d.line(pts,fill=(70,92,112),width=1)
                    pts=[]; continue
                r=R*(1-a_/90); pts.append((cx-r*np.sin(z_*D),cy-r*np.cos(z_*D)))
            if len(pts)>1: d.line(pts,fill=(70,92,112),width=1)
    for lbl,off in (('N',(-3,-R+1)),('S',(-3,R-9)),('E',(-R+2,-4)),('W',(R-9,-4))):
        d.text((cx+off[0],cy+off[1]),lbl,font=FONTS['Label'],fill=T['TextLow'])
    if target:
        ra,dec_=target
        pts=[]
        for h in np.arange(0,7,0.05):
            a_,z_=hd2altaz(lst-ra+h*15.041,dec_,lat)
            if a_<0: break
            r=R*(1-a_/90); pts.append((cx-r*np.sin(z_*D),cy-r*np.cos(z_*D)))
        if len(pts)>1: d.line(pts,fill=T['Accent'],width=2)
        pts=[]
        for h in np.arange(-7,0,0.05):
            a_,z_=hd2altaz(lst-ra+h*15.041,dec_,lat)
            if a_<0: continue
            r=R*(1-a_/90); pts.append((cx-r*np.sin(z_*D),cy-r*np.cos(z_*D)))
        if len(pts)>1: d.line(pts,fill=(74,80,88),width=1)
        a_,z_=hd2altaz(lst-ra,dec_,lat)
        r=R*(1-a_/90); px,py=cx-r*np.sin(z_*D),cy-r*np.cos(z_*D)
        d.ellipse([px-9,py-9,px+9,py+9],outline=T['Accent'])
        for a,b in ((0,-14),(0,14),(-14,0),(14,0)):
            d.line([px+a*0.64,py+b*0.64,px+a,py+b],fill=T['Accent'])
        d.ellipse([px-2,py-2,px+2,py+2],fill=T['Accent'])
        return a_,z_
    return None,None

def statusbar(img,title,right='22:45'):
    d=ImageDraw.Draw(img)
    d.text((12,8),title.upper(),font=FONTS['Label'],fill=T['TextHi'])
    gly(img,'wifi3',232,6,T['TextMid']); gly(img,'battery',252,6,T['TextMid'])
    d.text((308,8),right,font=FONTS['Label'],fill=T['TextLow'],anchor='ra')
    d.line([0,27,320,27],fill=T['Rule'])

def rgb565_dither(img):
    a=np.asarray(img).astype(np.float32)
    B=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],np.float32)/16.0-0.46875
    h,w,_=a.shape
    bo=np.tile(B,(h//4+1,w//4+1))[:h,:w][...,None]
    q=a.copy()
    q[...,0]=np.clip(np.round(a[...,0]/8.2258+bo[...,0]),0,31)
    q[...,1]=np.clip(np.round(a[...,1]/4.0476+bo[...,0]),0,63)
    q[...,2]=np.clip(np.round(a[...,2]/8.2258+bo[...,0]),0,31)
    r=(q[...,0].astype(int)<<3)|(q[...,0].astype(int)>>2)
    g=(q[...,1].astype(int)<<2)|(q[...,1].astype(int)>>4)
    b=(q[...,2].astype(int)<<3)|(q[...,2].astype(int)>>2)
    return Image.fromarray(np.stack([r,g,b],-1).astype(np.uint8))

LST=4.025*15; LAT=42.0; RA_T=5.5755*15; DEC_T=22.0145

# ---------- HOME ----------
im=Image.new('RGB',(320,480),T['Ground']); d=ImageDraw.Draw(im)
statusbar(im,'Home')
d.ellipse([12,36,17,41],fill=T['Accent'])
d.text((23,34),'TRACKING · M 1',font=FONTS['Label'],fill=T['Accent'])
d.text((308,34),'TRANSIT 00:18',font=FONTS['Label'],fill=T['TextLow'],anchor='ra')
alt,az=dome(im,160,168,104,LST,LAT,(RA_T,DEC_T))
d=ImageDraw.Draw(im)
d.line([0,282,320,282],fill=T['Rule'])
d.text((12,292),'RA',font=FONTS['Label'],fill=T['TextLow'])
d.text((42,286),'05:34:31.9',font=FONTS['Hero'],fill=T['TextHi'])
d.text((12,330),'DEC',font=FONTS['Label'],fill=T['TextLow'])
d.text((42,324),'+22:00:52',font=FONTS['Hero'],fill=T['TextHi'])
d.line([0,368,320,368],fill=T['Rule'])
for i,(k,v,c) in enumerate((('ALT',f'{alt:.1f}°',T['TextHi']),('AZ',f'{az:.1f}°',T['TextHi']),
                            ('PIER','EAST',T['Accent']))):
    d.text((14+i*104,378),k,font=FONTS['Label'],fill=T['TextLow'])
    d.text((14+i*104,390),v,font=FONTS['Value'],fill=c)
d.line([0,414,320,414],fill=T['Rule'])
for i,(g_,lab,on) in enumerate((('goto','GOTO',1),('align','ALIGN',0),
                                ('guide','GUIDE',0),('tools','TOOLS',0))):
    x=8+i*77
    col=T['Accent'] if on else T['TextMid']
    if on: d.rounded_rectangle([x,422,x+70,466],3,outline=T['AccentDim'])
    gly(im,g_,x+23,426,col,24)
    d.text((x+35,454),lab,font=FONTS['Label'],fill=col,anchor='ma')
rgb565_dither(im).save(f'{OUT}/home.png')

# ---------- CATALOG ----------
im=Image.new('RGB',(320,480),T['Ground']); d=ImageDraw.Draw(im)
statusbar(im,'Catalog','312 SHOWN')
rows=[('M 31','Andromeda','gal_spiral',3.4,64),('M 57','Ring Nebula','neb_planetary',8.8,61),
      ('M 13','Hercules Cluster','cl_globular',5.8,58),('M 33','Triangulum','gal_spiral',5.7,52),
      ('M 92','—','cl_globular',6.4,49),('NGC 7000','North America','neb_emission',4.0,47),
      ('M 27','Dumbbell','neb_planetary',7.4,44),('NGC 891','—','gal_lenticular',10.0,41),
      ('NGC 869','Double Cluster','cl_open',4.3,38),('IC 1396','Elephant Trunk','neb_dark',3.5,36),
      ('M 81',"Bode's Galaxy",'gal_spiral',6.9,33),('M 76','Little Dumbbell','neb_planetary',10.1,31),
      ('M 1','Crab Nebula','neb_supernova',8.4,70)]
d.text((12,38),'EAST · NORTHEAST · ABOVE 30°',font=FONTS['Label'],fill=T['TextMid'])
d.line([0,54,320,54],fill=T['Rule'])
y=58
for i,(idn,nm,ic,mag,al) in enumerate(rows):
    sel=(i==2)
    if sel: d.rectangle([0,y,320,y+31],fill=T['Surface2']); d.rectangle([0,y,2,y+31],fill=T['Accent'])
    icon(im,ic,8,y+4,T['Accent'] if sel else T['TextMid'])
    d.text((40,y+8),idn,font=FONTS['Strong'] if sel else FONTS['Body'],
           fill=T['TextHi'] if sel else T['TextMid'])
    d.text((104,y+8),nm,font=FONTS['Body'],fill=T['TextHi'] if sel else T['TextLow'])
    d.text((250,y+8),f'{mag:.1f}',font=FONTS['Body'],fill=T['TextMid'],anchor='ra')
    d.text((308,y+8),f'+{al}°',font=FONTS['Body'],fill=T['TextMid'],anchor='ra')
    d.line([0,y+31,320,y+31],fill=T['RuleFaint'])
    y+=32
d.line([0,454,320,454],fill=T['Rule'])
for i,(g_,lab) in enumerate((('chev_up','SCROLL'),('btn_a','SELECT'),('btn_y','SORT'),('filter','FILTER'))):
    gly(im,g_,10+i*78,460,T['TextLow'])
    d.text((28+i*78,463),lab,font=FONTS['Label'],fill=T['TextLow'])
rgb565_dither(im).save(f'{OUT}/catalog.png')

# ---------- SKY / MOON ----------
import importlib.util
ns={}; exec(open('05_moon.py').read().split('# phase strip')[0].replace('print(','pass#('),ns)
moon=ns['render'](96,118,lib_lon=-5,lib_lat=3)
im=Image.new('RGB',(320,480),T['Ground']); d=ImageDraw.Draw(im)
statusbar(im,'Sky','LST 04:02')
d.text((12,40),'NOW',font=FONTS['Label'],fill=T['TextLow'])
d.text((12,54),'Astronomical night',font=FONTS['Head'],fill=T['TextHi'])
d.text((12,82),'1 h 11 m until nautical dawn',font=FONTS['Body'],fill=T['TextMid'])
d.line([0,106,320,106],fill=T['Rule'])
mrgb=np.stack([(moon*255).astype(np.uint8)]*3,-1); mrgb[...,2]=np.clip(moon*243,0,255).astype(np.uint8)
base=np.asarray(im).astype(np.float32)
sub=base[124:220,16:112]; msk=(moon>0.004)[...,None]
sub[:]=np.where(msk,mrgb,sub)
im=Image.fromarray(base.astype(np.uint8)); d=ImageDraw.Draw(im)
d.text((128,126),'MOON',font=FONTS['Label'],fill=T['TextLow'])
d.text((128,140),'62% waxing',font=FONTS['Head'],fill=T['TextHi'])
d.text((128,168),'alt 12° · sets 05:26',font=FONTS['Body'],fill=T['TextMid'])
d.text((128,186),'41° from target',font=FONTS['Body'],fill=T['TextMid'])
d.line([0,232,320,232],fill=T['Rule'])
d.text((12,242),'NEXT EVENTS',font=FONTS['Label'],fill=T['TextLow'])
for i,(k,v) in enumerate((('Meridian flip','00:18'),('Moonset','05:26'),
                          ('Nautical dawn','04:52'),('Sunrise','06:03'))):
    yy=262+i*30
    d.text((12,yy),k,font=FONTS['Body'],fill=T['TextMid'])
    d.text((308,yy),v,font=FONTS['Value'],fill=T['TextHi'],anchor='ra')
    d.line([0,yy+22,320,yy+22],fill=T['RuleFaint'])
d.line([0,454,320,454],fill=T['Rule'])
for i,(g_,lab) in enumerate((('chev_left','BACK'),('btn_a','SELECT'),('clock','EPHEM'))):
    gly(im,g_,10+i*104,460,T['TextLow'])
    d.text((28+i*104,463),lab,font=FONTS['Label'],fill=T['TextLow'])
rgb565_dither(im).save(f'{OUT}/sky.png')
print('wrote home.png catalog.png sky.png to',OUT)
