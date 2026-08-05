#!/usr/bin/env python3
"""
22 DSO type icons as 4-bit alpha masks, rendered procedurally at 8x and boxed
down. Alpha rather than colour so one asset serves day and night - the draw call
tints it. 4bpp matches the existing antialiased font pipeline.

Sizes: 24 px for catalog rows, 48 px for detail headers. At 0.153 mm pitch a
24 px icon is 3.7 mm, so silhouettes have to carry the meaning; interior detail
below about 2 px of stroke disappears.
"""
import os, numpy as np
from PIL import Image
OUT='/home/claude/gen/out/icons'; os.makedirs(OUT, exist_ok=True)
SS=8

class C:
    def __init__(s,n): s.n=n*SS; s.a=np.zeros((s.n,s.n),np.float32)
    def _g(s):
        y,x=np.mgrid[0:s.n,0:s.n]; return x+0.5,y+0.5
    def disc(s,cx,cy,r,v=1.0):
        x,y=s._g(); d=np.hypot(x-cx*SS,y-cy*SS)
        s.a=np.maximum(s.a,np.clip((r*SS-d)/ (0.8*SS) ,0,1)*v)
    def ell(s,cx,cy,rx,ry,ang=0,v=1.0,soft=0.8):
        x,y=s._g(); t=np.deg2rad(ang)
        dx=(x-cx*SS)*np.cos(t)+(y-cy*SS)*np.sin(t)
        dy=-(x-cx*SS)*np.sin(t)+(y-cy*SS)*np.cos(t)
        d=np.hypot(dx/(rx*SS),dy/(ry*SS))
        s.a=np.maximum(s.a,np.clip((1-d)*rx*SS/(soft*SS),0,1)*v)
    def ering(s,cx,cy,rx,ry,w,ang=0,v=1.0):
        x,y=s._g(); t=np.deg2rad(ang)
        dx=(x-cx*SS)*np.cos(t)+(y-cy*SS)*np.sin(t)
        dy=-(x-cx*SS)*np.sin(t)+(y-cy*SS)*np.cos(t)
        d=np.hypot(dx/(rx*SS),dy/(ry*SS))
        s.a=np.maximum(s.a,np.clip((1-abs(d-1)*rx*SS/(w*SS))*1.4,0,1)*v)
    def line(s,x0,y0,x1,y1,w,v=1.0):
        x,y=s._g(); px,py=x0*SS,y0*SS; qx,qy=x1*SS,y1*SS
        vx,vy=qx-px,qy-py; L2=vx*vx+vy*vy+1e-9
        t=np.clip(((x-px)*vx+(y-py)*vy)/L2,0,1)
        d=np.hypot(x-(px+t*vx),y-(py+t*vy))
        s.a=np.maximum(s.a,np.clip((w*SS/2-d)/(0.7*SS),0,1)*v)
    def arc(s,cx,cy,r,a0,a1,w,v=1.0,squash=1.0):
        for k in np.linspace(a0,a1,90):
            t=np.deg2rad(k)
            s.disc(cx+r*np.cos(t), cy+r*np.sin(t)*squash, w/2, v)
    def blob(s,cx,cy,r,v=1.0):
        x,y=s._g(); d=np.hypot(x-cx*SS,y-cy*SS)/(r*SS)
        s.a=np.maximum(s.a,np.clip(np.exp(-d*d*2.2),0,1)*v)
    def spike(s,cx,cy,r,w,v=1.0):
        s.line(cx-r,cy,cx+r,cy,w,v); s.line(cx,cy-r,cx,cy+r,w,v)
    def out(s,n):
        a=s.a.reshape(n,SS,n,SS).mean(axis=(1,3))
        return np.clip(a,0,1)

def draw(kind,n):
    c=C(n); m=n/24.0                       # design grid is 24
    def S(v): return v*m
    if kind=='gal_spiral':
        c.arc(S(12),S(12),S(7.5),200,375,S(1.9),1,0.62)
        c.arc(S(12),S(12),S(7.5),20,195,S(1.9),1,0.62)
        c.ell(S(12),S(12),S(3.4),S(2.2),-20)
    elif kind=='gal_barred':
        c.line(S(7.6),S(13.4),S(16.4),S(10.6),S(2.6))
        c.arc(S(12),S(12),S(8),25,120,S(1.9),1,0.60)
        c.arc(S(12),S(12),S(8),205,300,S(1.9),1,0.60)
        c.ell(S(12),S(12),S(2.6),S(2.0),-18)
    elif kind=='gal_elliptical':
        c.ell(S(12),S(12),S(9),S(6.2),-20,1.0,1.6)
        c.ell(S(12),S(12),S(4.6),S(3.2),-20)
    elif kind=='gal_lenticular':
        c.ell(S(12),S(12),S(10),S(2.1),-18)
        c.ell(S(12),S(12),S(4.2),S(4.0),0)
    elif kind=='gal_irregular':
        # solid asymmetric mass with a hard edge - a SHAPE, not haze
        c.ell(S(10.4),S(10.6),S(5.0),S(4.0),-20,1.0,1.1)
        c.ell(S(14.6),S(13.8),S(3.8),S(3.2),20,1.0,1.1)
        c.ell(S(12.6),S(16.0),S(2.6),S(2.0),0,1.0,1.1)
    elif kind=='gal_interacting':
        c.ell(S(8),S(9.5),S(5.2),S(3.4),-25,1.0,1.4)
        c.ell(S(16.6),S(15),S(4.0),S(2.6),-25,1.0,1.4)
        c.line(S(10.8),S(11.4),S(14.2),S(13.4),S(1.5),0.85)
    elif kind=='gal_cluster':
        for x,y,rx,ry,a in ((7,8,3.0,1.9,-25),(15.5,7.4,2.4,1.6,15),
                            (11.6,13.2,2.7,1.8,-40),(17,15.4,2.2,1.4,-10),
                            (6.6,16.2,2.0,1.3,30)):
            c.ell(S(x),S(y),S(rx),S(ry),a,1.0,1.2)
    elif kind=='neb_emission':
        # bipolar lobes with a pinched waist - reads as structured, not just bright
        c.ell(S(12),S(7.6),S(5.6),S(4.2),0,1.0,1.3)
        c.ell(S(12),S(16.4),S(5.6),S(4.2),0,1.0,1.3)
        c.ell(S(12),S(12),S(2.2),S(2.6),0,1.0,1.1)
    elif kind=='neb_reflection':
        # ID mark is the dominant star with long diffraction spikes
        for dx,dy,r in ((-2.6,-1,4.2),(2.6,-2,3.8),(2.0,2.6,4.0),(-2.4,2.4,3.6)):
            c.blob(S(12+dx),S(12+dy),S(r),0.42)
        c.disc(S(12),S(12),S(2.4))
        c.spike(S(12),S(12),S(10.4),S(1.5),1.0)
    elif kind=='neb_dark':
        c.ering(S(12),S(12),S(8.4),S(7.0),S(1.7),-10)
        for dx,dy in ((-3,-2),(2.6,-2.6),(3,2.4),(-2.6,2.8)):
            c.disc(S(12+dx),S(12+dy),S(0.9),0.55)
    elif kind=='neb_planetary':
        c.ering(S(12),S(12),S(7.6),S(6.4),S(2.2),-15)
        c.disc(S(12),S(12),S(1.7))
    elif kind=='neb_supernova':
        for a0,a1 in ((8,74),(96,166),(188,256),(276,352)):
            c.arc(S(12),S(12),S(7.8),a0,a1,S(1.8),1,0.88)
        for a in (40,130,222,314):
            t=np.deg2rad(a)
            c.line(S(12+5.0*np.cos(t)),S(12+5.0*np.sin(t)*0.88),
                   S(12+9.6*np.cos(t)),S(12+9.6*np.sin(t)*0.88),S(1.1),0.8)
    elif kind=='neb_hii':
        # ID mark is the closed bubble outline around embedded stars
        c.ering(S(12),S(12),S(8.6),S(7.4),S(1.5),-8,0.95)
        for dx,dy,r in ((-1.8,-1.2,3.2),(2.2,1.4,3.0)):
            c.blob(S(12+dx),S(12+dy),S(r),0.42)
        for dx,dy in ((-2.6,-1.8),(2.4,-0.4),(0.4,2.6)):
            c.disc(S(12+dx),S(12+dy),S(1.5))
    elif kind=='neb_bright':
        # one smooth featureless cloud: the deliberate 'diffuse, unclassified' mark
        # angled oval, no rim: silhouette separates it from the globular ball
        x,y=c._g(); t=np.deg2rad(-28)
        dx=(x-12*c.n/24)*np.cos(t)+(y-12*c.n/24)*np.sin(t)
        dy=-(x-12*c.n/24)*np.sin(t)+(y-12*c.n/24)*np.cos(t)
        r=np.hypot(dx/(9.6*c.n/24),dy/(5.6*c.n/24))
        c.a=np.maximum(c.a,np.clip(np.exp(-r*r*1.6),0,1)*1.15)
    elif kind=='cl_open':
        for x,y,r in ((7.0,8.0,2.0),(12.4,6.0,1.7),(17.0,9.2,1.9),(9.0,13.0,1.8),
                      (14.6,13.8,2.0),(6.4,16.8,1.7),(11.6,17.6,1.8),(17.6,16.6,1.6)):
            c.disc(S(x),S(y),S(r))
    elif kind=='cl_globular':
        # dense graded ball with a crisp rim - no discrete dots, they vanish at 24 px
        c.blob(S(12),S(12),S(7.6),1.0)
        c.disc(S(12),S(12),S(4.4),0.85)
        c.ering(S(12),S(12),S(8.2),S(8.2),S(1.0),0,0.55)
    elif kind=='cl_open_neb':
        # dots dominant, haze is a faint halo behind them
        c.blob(S(12),S(12),S(8.0),0.30)
        for x,y,r in ((8.2,9.0,2.0),(13.6,7.6,1.8),(16.4,12.6,1.9),
                      (9.6,15.0,1.8),(14.2,16.2,1.7)):
            c.disc(S(x),S(y),S(r))
    elif kind=='asterism':
        pts=[(7,9),(11.4,6.0),(17.0,9.2),(14.4,15.2),(8.4,15.8)]
        for i in range(len(pts)-1):
            c.line(S(pts[i][0]),S(pts[i][1]),S(pts[i+1][0]),S(pts[i+1][1]),S(1.3),0.75)
        for x,y in pts: c.disc(S(x),S(y),S(2.0))
    elif kind=='star_double':
        c.disc(S(8.2),S(12),S(3.8)); c.disc(S(16.2),S(12),S(2.9))
    elif kind=='star_multiple':
        c.disc(S(12),S(7.4),S(3.2)); c.disc(S(7.6),S(15.4),S(2.8)); c.disc(S(16.4),S(15.4),S(2.5))
    elif kind=='star_variable':
        c.disc(S(12),S(12),S(3.0))
        c.ering(S(12),S(12),S(7.4),S(7.4),S(1.3),0,0.85)
        for a in (45,135,225,315):
            t=np.deg2rad(a)
            c.line(S(12+4.6*np.cos(t)),S(12+4.6*np.sin(t)),
                   S(12+6.2*np.cos(t)),S(12+6.2*np.sin(t)),S(1.0),0.75)
    elif kind=='quasar':
        # no haze at all: a hard point with spikes running to the edge
        # diagonal spikes, no halo: cannot be confused with the reflection mark
        c.disc(S(12),S(12),S(2.8))
        for a in (45,135,225,315):
            t=np.deg2rad(a)
            c.line(S(12),S(12),S(12+11.0*np.cos(t)),S(12+11.0*np.sin(t)),S(1.7),1.0)
    elif kind=='unknown':
        c.ering(S(12),S(12),S(7.6),S(7.6),S(1.6))
        c.disc(S(12),S(12),S(1.5))
    else: raise KeyError(kind)
    return c.out(n)

KINDS=['gal_spiral','gal_barred','gal_elliptical','gal_lenticular','gal_irregular',
       'gal_interacting','gal_cluster','neb_emission','neb_reflection','neb_dark',
       'neb_planetary','neb_supernova','neb_hii','neb_bright','cl_open','cl_globular',
       'cl_open_neb','asterism','star_double','star_multiple','star_variable',
       'quasar','unknown']

def pack4(a):
    q=np.clip(np.round(a*15),0,15).astype(np.uint8)
    h,w=q.shape
    if w%2: q=np.pad(q,((0,0),(0,1)))
    return (q[:,0::2]<<4|q[:,1::2]).tobytes()

hdr=['#pragma once','#include <cstdint>','',
     '// Generated by tools/03_dso_icons.py - 4-bit alpha, row-major, two',
     '// nibbles per byte, high nibble = left pixel. Tint at draw time.','',
     'namespace dsoicon {','']
sheet={}
for n in (24,48):
    blob=[]
    for k in KINDS:
        a=draw(k,n); sheet[(k,n)]=a
        blob.append(pack4(a))
    data=b''.join(blob)
    open(f'{OUT}/dso_{n}.bin','wb').write(data)
    per=len(blob[0])
    hdr.append(f'constexpr int kSize{n} = {n};')
    hdr.append(f'constexpr int kBytes{n} = {per};   // per icon')
    print(f'{n:3d} px : {len(KINDS)} icons, {per} B each, {len(data)} B total')
hdr.append('')
hdr.append('enum class Type : uint8_t {')
for i,k in enumerate(KINDS): hdr.append(f'  {"".join(p.capitalize() for p in k.split("_"))} = {i},')
hdr.append(f'  Count = {len(KINDS)}')
hdr.append('};')
hdr.append('')
hdr.append('}  // namespace dsoicon')
open(f'{OUT}/DsoIcons.h','w').write('\n'.join(hdr)+'\n')

# contact sheet, day tint on ground, at 1:1 and 3x
for n,scale in ((24,3),(48,2)):
    cols=8; rows=(len(KINDS)+cols-1)//cols
    cw,ch=n*scale+16, n*scale+26
    img=Image.new('RGB',(cols*cw, rows*ch),(8,8,16))
    for i,k in enumerate(KINDS):
        a=sheet[(k,n)]
        rgb=np.zeros((n,n,3),np.uint8)
        for c,(lo,hi) in enumerate(((8,247),(8,247),(16,247))):
            rgb[...,c]=(lo+(hi-lo)*a).astype(np.uint8)
        tile=Image.fromarray(rgb).resize((n*scale,n*scale),Image.NEAREST)
        img.paste(tile,((i%cols)*cw+8,(i//cols)*ch+8))
    img.save(f'{OUT}/sheet_{n}.png')
print('wrote', OUT)
