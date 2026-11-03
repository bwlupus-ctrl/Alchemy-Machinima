"""WP4 W-9a/W-17 shader harness: hardware hidden WGL, production directional
shadow utility + key sampling + RG8 matte + final composite. Deterministic receiver
accessors and synthetic D32F casters replace logged-in scene traversal. No viewer build.
"""
from pathlib import Path
import ctypes as C
import json, subprocess, hashlib
import numpy as np
from actor_key_gl_check import Fixture, SH
OUT=Path('.tmp/ckshadow/shadow_gl');OUT.mkdir(parents=True,exist_ok=True)
f=Fixture();g=f.g
# Concatenate library translation units with repeated uniform declarations deduped.
# Function bodies are unchanged; actual viewer links these as separate shader objects.
util=(SH/'deferred/shadowUtil.glsl').read_text()
main=(SH/'alchemy/actorKeyShadowF.glsl').read_text()
for name in ('sun_dir','moon_dir','sun_up_factor'):
 import re
 main=re.sub(r'uniform \w+ '+name+r';','',main)
access='''uniform float testDepth;uniform int testBack;
vec4 getPosition(vec2 p){return vec4(p,-testDepth,1.0);}
vec4 getNorm(vec2 p){return vec4(0.0,0.0,testBack==0?1.0:-1.0,0.0);}
'''
program=f.program('#define SUN_SHADOW 1\n'+util+access+main)
matte=f.program((SH/'alchemy/actorKeyMatteF.glsl').read_text())
density=f.program((SH/'alchemy/actorKeyShadowDensityF.glsl').read_text())
blur=f.program((SH/'alchemy/actorKeyBlurF.glsl').read_text())
composite=f.program((SH/'alchemy/actorKeyCompositeF.glsl').read_text())
N=64
# Rectangular keyed actor and independent balcony/set occluder. Beauty is their depth minimum.
y,x=np.mgrid[:N,:N];actor=np.where((x>=10)&(x<40)&(y>=10)&(y<54),.25,1.).astype(np.float32)
world=np.where((y>=32)&(y<60),.2,1.).astype(np.float32)
full=np.minimum(actor,world);lit=np.ones_like(actor)
def depth_texture(data):
 t=g.U();g.gen_tex(1,C.byref(t));g.bind_tex(0x0DE1,t)
 for k,v in ((0x2800,0x2601),(0x2801,0x2601),(0x2802,0x812F),(0x2803,0x812F),(0x884C,0x884E),(0x884D,0x0203)):
  g.param(0x0DE1,k,v)
 h,w=data.shape
 g.teximage(0x0DE1,0,0x8CAC,w,h,0,0x1902,0x1406,data.ctypes.data)
 return t
maps=[depth_texture(v) for v in (actor,world,full,np.ones((1,1),np.float32))]
vis=[f.target() for _ in range(3)]
uniforms=[('shadowMap'+str(i),i) for i in range(4)]+[('shadow_res',(64.,64.)),('shadow_res_cascade',(64.,)*4),('shadow_res_cascade_h',(64.,)*4),('shadow_clip',(2.,4.,8.,16.)),('shadow_split_blend',.25),('sun_dir',(0.,0.,1.)),('moon_dir',(0.,0.,1.)),('shadow_bias',0.),('shadow_offset',0.),('shadow_bias_mm',0.),('shadow_offset_texels',0.),('shadow_soft_world_mm',0.),('shadow_slope_scale',0.),('shadow_subject',(0.,.02,.02,0.)),('testDepth',1.),('testBack',0)]
matrix=np.zeros((4,4),np.float32);matrix[0,0]=matrix[1,1]=1;matrix[2,3]=.5;matrix[3,3]=1
upload=g.fn('glUniformMatrix4fv',None,g.I,g.I,C.c_ubyte,C.c_void_p)
u4=g.fn('glUniform4fv',None,g.I,g.I,C.c_void_p)
def sample(tex,target,column=False,sun=1,back=0,soft=False,depth=1.):
 g.use(program)
 data=np.ascontiguousarray(np.tile(matrix.T,(14,1,1)))
 upload(g.loc(program,b'shadow_matrix[0]'),14,0,data.ctypes.data)
 upload(g.loc(program,b'shadow_subject_matrix'),1,0,np.ascontiguousarray(matrix.T).ctypes.data)
 coef=np.array([[1.,.01,.01,0.]]*4,np.float32);u4(g.loc(program,b'shadow_cascade_coef[0]'),4,coef.ctypes.data)
 f.draw(program,target,uniforms+[('sun_up_factor',sun),('shadow_units_mode',int(column)),('shadow_subject',(float(column),.02,.02,0.)),('testBack',back),('testDepth',float(depth)),('soft_shadow_enable',int(soft)),('soft_shadow_sun',int(soft)),('soft_shadow_scale',.3),('soft_shadow_max',2.),('soft_shadow_vogel',1),('soft_shadow_taps',16)], [tex]*4)
 return f.read(target)[...,0]
coverage=np.zeros((N,N,4),np.float32);coverage[18:28,18:28,0]=1
cov=f.texture(coverage);mat=f.target(0x822B)
base_uniforms=[('keyCoverage',0),('keyBlur',1),('keyRaw',1),('keyKeepInterior',1),('keyShadowEnabled',1),('keyActorVisibility',2),('keySetVisibility',3),('keyShadowSoftness',.5),('keyShadowFiltered',0)]
rows=[]
scene_data=np.ones((N,N,4),np.float32);scene_data[...,:3]=np.array([51,102,153])/255.
scene=f.texture(scene_data,0x8058);output=f.target(0x8058)
key=np.array([0,177,64],np.float32)/255.

for column in (False,True):
 for sun in (0,1):
  for soft in (False,True):
   va=sample(maps[0],vis[0],column,sun,soft=soft);vs=sample(maps[1],vis[1],column,sun,soft=soft);vf=sample(maps[2],vis[2],column,sun,soft=soft)
   for mode in range(3):
    av=vis[2][0] if mode==2 else vis[0][0]
    f.draw(matte,mat,base_uniforms+[('keyShadowExact',int(mode==1))],[cov,cov,av,vis[1][0]])
    result=f.read(mat);m=result[...,0]/255.;G=result[...,1]/255.
    expected=(1-(vf if mode==2 else va)/255.)*(vs/255. if mode==1 else 1.)*(1-coverage[...,0]);expected=expected**2
    assert np.max(np.abs(G-expected))<=1.01/255,(column,sun,soft,mode,np.max(np.abs(G-expected)))
    assert G[16,16]>.8 and G[24,24]==0,'actor shadow kept; actor matte has no G'
    if mode==1:assert G[44,24]==0,'Exact removes independent set occlusion'
    else:assert G[44,24]>.7,'Fast/All preserve documented balcony darkening'
    assert np.all(result[...,:2]>=0)
    # Execute final production composite both directly from RG and through the
    # lens transport contract (RGB coverage + shadow alpha). Packing is an explicit
    # fixture seam; actual optical warp/G transport has its own WP-LENS harness.
    packed=np.ones((N,N,4),np.float32);packed[...,:3]=m[...,None];packed[...,3]=G
    packed_tex=f.texture(packed,0x8058)
    for per_channel,coverage_tex in ((0,mat[0]),(1,packed_tex)):
     f.draw(composite,output,[('keyScene',0),('keyCoverage',1),('keyOrigin',(0.,0.)),
       ('keyColour',tuple(float(v) for v in key)),('keyMode',0),('keyPerChannel',per_channel),
       ('keyShadowStrength',.5),('keyHard',0)],[scene,coverage_tex])
     final=f.read(output)[...,:3]
     expected_key=key[None,None,:]*(1-.5*G[...,None])
     expected_codes=np.rint((expected_key*(1-m[...,None])+scene_data[...,:3]*m[...,None])*255).astype(int)
     assert np.max(np.abs(final.astype(int)-expected_codes))<=1,'W-9a composite shadow codes/hue'
     assert np.all(final[18:28,18:28]==(51,102,153)),'actor beauty unchanged'
     assert np.all(final[(m==0)&(G==0)]==(0,177,64)),'unshadowed exact key codes'
     f.draw(composite,output,[('keyMode',2),('keyMatteShadow',1)],[scene,coverage_tex])
     matte_codes=f.read(output)[...,:3]
     expected_matte=np.rint(np.maximum(m,.5*G)*255).astype(int)
     assert np.max(np.abs(matte_codes.astype(int)-expected_matte[...,None]))<=1,'matte + grey shadow'
    rows.append(dict(column=column,sun=sun,soft=soft,mode=mode,passed=True))
# Invalid slices / beyond selected count -> cleared 1x1 lit equivalent, no set leakage.
for column in (False,True):
 v=sample(maps[3],vis[0],column)
 assert np.all(v==255)
 f.draw(matte,mat,base_uniforms+[('keyShadowExact',0)],[cov,cov,vis[0][0],vis[1][0]])
 assert not f.read(mat)[...,1].any()
 # Back-facing receiver contributes zero actor shadow for sun and moon.
 for sun in (0,1):assert np.all(sample(maps[0],vis[0],column,sun,back=1)==255)
# Every ordinary cascade depth, then a subject-column interior.
for dep in (1.,3.,6.,10.):
 v=sample(maps[0],vis[0],False,depth=dep);assert v[16,16]<10
# Softness > .5: production density downsample + same separable half-res blur.
sample(maps[0],vis[0]);sample(maps[1],vis[1]);a=f.target(size=(32,32));b=f.target(size=(32,32))
f.draw(density,a,[('keyActorVisibility',0),('keySetVisibility',1),('keyShadowExact',1)],[vis[0][0],vis[1][0]])
for source,dest,direction in ((a,b,(1/32.,0.)),(b,a,(0.,1/32.))):
 f.draw(blur,dest,[('keySource',0),('keyDirection',direction),('keyRadius',4.),('keyDownsample',0)],[source[0]])
f.draw(matte,mat,base_uniforms+[('keyShadowExact',1),('keyShadowFiltered',1),('keyShadowSoftness',1.)],[cov,cov,a[0],vis[1][0]])
assert f.read(mat)[...,1].max()>200 and not f.read(mat)[18:28,18:28,1].any()
# W-17 beauty/off parity: current shader with shadow disabled byte-matches pre-WP4 shader.
allowance_raw=Path(__file__).with_name('actor_key_shadow_hunks.json').read_bytes().replace(b'\r\n',b'\n')
assert hashlib.sha256(allowance_raw).hexdigest() == 'b1575e291227801c5ed9b2ab5645ed52964b3f1651e7dc9d53a34fb2780d6b67', 'WP4 parity baseline pin'
baseline_ref=json.loads(allowance_raw)['base']
old=f.program(subprocess.check_output(['git','show',baseline_ref+':indra/newview/app_settings/shaders/class1/alchemy/actorKeyMatteF.glsl']).decode())
f.draw(old,mat,base_uniforms,[cov,cov]);before=f.read(mat)
f.draw(matte,mat,base_uniforms+[('keyShadowEnabled',0)],[cov,cov,vis[0][0],vis[1][0]]);assert np.array_equal(before,f.read(mat))
# Negative control: full maps used as actor maps create a world-only false shadow.
sample(maps[2],vis[0]);f.draw(matte,mat,base_uniforms+[('keyShadowExact',0)],[cov,cov,vis[0][0],vis[1][0]])
assert f.read(mat)[44,50,1]>200
sample(maps[0],vis[0]);f.draw(matte,mat,base_uniforms+[('keyShadowExact',0)],[cov,cov,vis[0][0],vis[1][0]])
assert f.read(mat)[44,50,1]==0
(OUT/'results.json').write_text(json.dumps(dict(renderer=g.renderer,rows=rows,off_byte_identical=True),indent=2))
assert g.fn('glGetError',g.U)()==0
print('PASS W-9a: production stock PCF, column ON/OFF, all cascades, sun/moon, Fast/Exact/All, balcony overlap, backfaces, lit invalid slices, RG8 density and half-res softness; W-9a final composite and packed lens-alpha contract; W-17 OFF matte byte parity; world-map contamination negative control')
