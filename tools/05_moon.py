#!/usr/bin/env python3
"""
Moon: real albedo texture + a physically-shaped renderer.

Texture orientation was verified against five known features - Mare Crisium
reads 104, Mare Imbrium 124, Tranquillitatis 126, farside highland 134, and the
Tycho ray system 186 (the brightest thing on the Moon). Longitude 0 sits at the
image centre, latitude +90 at the top.

Shading is Lommel-Seeliger, not Lambert: I = A * mu0/(mu0+mu). The Moon's
regolith backscatters, which is why a gibbous Moon looks flat and bright to the
limb instead of falling off like a billiard ball. Lambert gets the terminator
right and everything else wrong.

Libration is a rotation applied to the surface normal before the texture lookup,
so the same code that draws the phase also swings Mare Crisium around the limb
over a month - which is the bit that makes it look alive rather than drawn.
"""
import os, numpy as np
from PIL import Image
OUT='/home/claude/gen/out/moon'; os.makedirs(OUT, exist_ok=True)
D=np.pi/180

src=np.asarray(Image.open('/home/claude/moon/moon_1024.jpg').convert('L')).astype(np.float32)/255.
print(f'source albedo {src.shape[1]}x{src.shape[0]}')

def box(a,W,H):
    h,w=a.shape; return a.reshape(H,h//H,W,w//W).mean(axis=(1,3))

for W in (256,512):
    t=box(src,W,W//2)
    # mild stretch: the raw map is low contrast, and the maria are the whole point
    t=np.clip((t-t.min())/(np.percentile(t,99.5)-t.min()),0,1)**0.92
    (t*255).astype(np.uint8).tofile(f'{OUT}/moon_{W}.bin')
    Image.fromarray((t*255).astype(np.uint8)).save(f'{OUT}/moon_{W}.png')
    print(f'  {W}x{W//2}  {W*W//2//1024} KB')

TEX=box(src,512,256)
TEX=np.clip((TEX-TEX.min())/(np.percentile(TEX,99.5)-TEX.min()),0,1)**0.92

def render(n, phase_deg, lib_lon=0.0, lib_lat=0.0, limb_pa=0.0, ss=3):
    """phase 0 = new, 180 = full. limb_pa rotates the bright limb in the sky."""
    N=n*ss
    y,x=np.mgrid[0:N,0:N]
    X=(x+0.5)/N*2-1; Y=(y+0.5)/N*2-1
    r2=X*X+Y*Y
    disc=r2<=1.0
    Z=np.sqrt(np.clip(1-r2,0,1))
    # surface normal in view frame
    nx,ny,nz=X,-Y,Z
    # libration: rotate the sphere under the viewer
    a=lib_lon*D; b=lib_lat*D
    ca,sa=np.cos(a),np.sin(a); cb,sb=np.cos(b),np.sin(b)
    px = ca*nx + sa*nz
    pz = -sa*nx + ca*nz
    py = ny
    py2 = cb*py - sb*pz
    pz2 = sb*py + cb*pz
    lat=np.arcsin(np.clip(py2,-1,1))/D
    lon=np.arctan2(px,pz2)/D
    u=np.clip(((lon+180)/360*TEX.shape[1]).astype(int),0,TEX.shape[1]-1)
    v=np.clip(((90-lat)/180*TEX.shape[0]).astype(int),0,TEX.shape[0]-1)
    alb=TEX[v,u]
    # sun direction: phase 180 = full = sun behind viewer
    th=(180-phase_deg)*D; pa=limb_pa*D
    s=np.array([np.sin(th)*np.cos(pa), np.sin(th)*np.sin(pa), np.cos(th)])
    mu0 = nx*s[0]+ny*s[1]+nz*s[2]          # cos(incidence)
    mu  = nz                                # cos(emission), viewer on +z
    lit = np.clip(mu0,0,1)
    shade = np.where(lit>0, lit/np.clip(lit+mu,1e-3,None), 0.0)*2.0
    img = np.clip(alb*shade,0,1)*disc
    # tiny earthshine so the dark limb is not a hole
    img = np.clip(img + disc*(1-np.clip(mu0,0,1))*0.035*alb, 0, 1)
    return np.clip(img.reshape(n,ss,n,ss).mean(axis=(1,3)),0,1)

# phase strip
phases=[(20,'waxing crescent'),(60,'waxing crescent'),(90,'first quarter'),
        (125,'waxing gibbous'),(175,'full'),(235,'waning gibbous'),
        (270,'last quarter'),(320,'waning crescent')]
def norm(p): return p if p<=180 else 360-p
def pa_for(p): return 0 if p<=180 else 180

strip=Image.new('RGB',(8*84,84+22),(8,8,16))
for i,(p,lbl) in enumerate(phases):
    a=render(72,norm(p),limb_pa=pa_for(p))
    rgb=np.stack([(a*255).astype(np.uint8)]*3,-1)
    rgb[...,2]=np.clip(a*243,0,255).astype(np.uint8)     # faintly warm
    strip.paste(Image.fromarray(rgb),(i*84+6,6))
strip.save(f'{OUT}/phases.png')

# libration demo at full, one month of swing
lib=Image.new('RGB',(5*116,116),(8,8,16))
for i,(lo,la) in enumerate([(-7.5,-6.5),(-4,0),(0,0),(4,3),(7.5,6.5)]):
    a=render(104,175,lib_lon=lo,lib_lat=la)
    lib.paste(Image.fromarray(np.stack([(a*255).astype(np.uint8)]*3,-1)),(i*116+6,6))
lib.save(f'{OUT}/libration.png')

big=render(240,118,lib_lon=-5,lib_lat=3)
Image.fromarray(np.stack([(big*255).astype(np.uint8)]*3,-1)).save(f'{OUT}/big.png')

# device cost
for n in (48,72,104):
    px=int(np.pi*(n/2)**2)
    print(f'  {n:3d} px disc: {px:6d} lit pixels, ~{px*22/240e6*1000:.2f} ms '
          f'(22 cyc/px estimate, geometry LUT precomputed)')
print(f'\nrenderer needs: sqrt + 2 rotations + texture fetch per pixel.')
print(f'the disc geometry (X, Y, Z) never changes, so it is a boot-time LUT of '
      f'{104*104*3*2//1024} KB at 104 px; only the sun vector moves.')
