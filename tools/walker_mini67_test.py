import sys, os
sys.path.insert(0,'tools')
import walker_sprites as ws
from PIL import Image, ImageDraw
P='romfs/sprites/pack'; G=[(255,255,255),(168,168,168),(80,80,80),(0,0,0)]
OUT='art_src/mini67test'; os.makedirs(OUT,exist_ok=True)
def squat(a,frac=0.8,dy=1):
    rows=[y for y in range(24) if any(a[y])]; top,bot=rows[0],rows[-1]
    cut=top+round(frac*(bot-top+1))
    out=[list(r) for r in a]
    for y in range(top, cut+dy):
        sy=y-dy
        out[y]=list(a[sy]) if top<=sy<cut else [0]*32
    return out,cut
def bob(a,dy=1):
    return [list(a[y-dy]) if 0<=y-dy<24 else [0]*32 for y in range(24)]
nums=[int(x) for x in sys.argv[1].split(',')]
sc=4; cells=[]
for n in nums:
    b=open(f'{P}/{n:03d}.bin','rb').read()
    a=ws.decode(b[:0xC0],32,24)
    s,cut=squat(a); o=bob(a)
    for tag,f2 in (('squat',s),('bob',o)):
        open(f'{OUT}/{n:03d}_{tag}.bin','wb').write(b[:0xC0]+ws.encode(f2,32,24)+b[0x180:])
    cells.append((n,a,s,o,cut))
cw=3*(32*sc+6)+8; ch=24*sc+18; cols=2
im=Image.new('RGB',(cw*cols,ch*((len(cells)+1)//2)+16),(230,230,230)); d=ImageDraw.Draw(im)
d.text((4,2),'frame 1 | frame 2 "squat" (upper body 1 px down, feet stay) | frame 2 "bob" (whole sprite 1 px down)',fill=(0,0,0))
def draw(s,x0,y0):
    for y in range(24):
        for x in range(32):
            d.rectangle([x0+x*sc,y0+y*sc,x0+x*sc+sc-1,y0+y*sc+sc-1],fill=G[s[y][x]])
for i,(n,a,s,o,cut) in enumerate(cells):
    ox=(i%cols)*cw; oy=16+(i//cols)*ch
    d.text((ox+2,oy),f'{n:03d}',fill=(200,0,0))
    for k,f in enumerate((a,s,o)): draw(f,ox+k*(32*sc+6),oy+12)
im.save(f'{OUT}/sheet.png')
# animated gif: each mon alternating f1/f2 for both variants, 2x scale grid
sc2=4; gw=cols*0+len(cells)
frames=[]
for t in (0,1):
    g=Image.new('RGB',(6*(2*32*sc2//2+8)+8, ((len(cells)+2)//3)*(24*sc2//2*1+40)),(255,255,255))
frames=[]
W=32*3; H=24*3
for t in (0,1):
    g=Image.new('RGB',(3*(2*W+30)+10,((len(cells)+2)//3)*(H+22)+20),(240,240,240)); dd=ImageDraw.Draw(g)
    dd.text((6,4),'left = squat, right = bob',fill=(0,0,0))
    for i,(n,a,s,o,cut) in enumerate(cells):
        ox=10+(i%3)*(2*W+30); oy=20+(i//3)*(H+22)
        dd.text((ox,oy),f'{n:03d}',fill=(200,0,0))
        for k,f in enumerate((s,o)):
            fr=a if t==0 else f
            for y in range(24):
                for x in range(32):
                    dd.rectangle([ox+k*(W+8)+x*3,oy+12+y*3,ox+k*(W+8)+x*3+2,oy+12+y*3+2],fill=G[fr[y][x]])
    frames.append(g)
frames[0].save(f'{OUT}/anim.gif',save_all=True,append_images=frames[1:],duration=500,loop=0)
print('done',len(cells))
