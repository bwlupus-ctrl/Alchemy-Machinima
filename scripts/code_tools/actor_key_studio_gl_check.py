"""WP2 production Studio shader/fill checks; separate from real drawGeometryGhost acceptance.

Keeps the distinction explicit: these are full GLSL/driver fixtures. They do not
claim to execute the viewer's C++ sweep scheduler or an in-world snapshot.
"""
from pathlib import Path
import ctypes as C
import numpy as np
from PIL import Image
import relight_3d_gl as G
from clone_looks_gl_check import Runner,V,LEGACY_ANIMATED
from actor_key_gl_check import Fixture,SH

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.tmp/ckey_20261102/wp2'

def main():
    runner=Runner(256,256);g=runner.gl
    tree=G.SourceTree('WORKTREE');base=G.SourceTree('ab2329ba39f')
    key=np.array([0.,177/255,64/255],np.float32)
    for skin in ((),('HAS_SKIN',)):
        normal=runner.program(tree,skin)
        reference=runner.program(base,skin)
        fill=runner.program(tree,skin+('AL_KEY_FILL',))
        for look in (1,10,64,68,69,72,75,78,79,80,81,82,83,84,*range(85,101)):
            for opacity in (.4,1.):
                for distort in (0,3):
                    args=dict(opaque=look==1,opacity=opacity,distort=distort,scene=look==64)
                    g.disable(0x0BE2)
                    def render(pr):
                        if look==64:
                            # Full-scene Glass alpha with an otherwise transparent
                            # destination makes the geometry footprint measurable.
                            g.bind_fbo(0x8D40,runner.fbo)
                            runner.clear(0x1800,0,(g.F*4)(0,0,0,0))
                            args['keep']=True
                        return runner.render(pr,look,**args)
                    a=render(normal)
                    # Reviewed animations use real-avatar safety/t=0 similarity
                    # in clone_looks_safety_check; fill/alpha still run here.
                    if look < 85 and look not in LEGACY_ANIMATED:
                        b=render(reference)
                        assert np.array_equal(a,b),(skin,look,opacity,distort,'normal-program regression')
                    for colour in (key,np.ones(3,np.float32),np.zeros(3,np.float32)):
                        g.use(fill);g.uf[3](g.loc(fill,b'ghostKeyFill'),*map(float,colour))
                        g.disable(0x0BE2)
                        c=render(fill)
                        assert np.array_equal(a[...,3],c[...,3]),(skin,look,'alpha/footprint')
                        visible=c[...,3]>1e-5
                        assert visible.any(),(skin,look,'empty fixture')
                        assert np.all(c[visible,:3]==colour),(skin,look,'fill RGB')
        print('PASS WP2 Studio full GLSL:',skin,'existing normal vs incoming main exact; .4/1 opacity; existing plus original looks 85-100; Distort; key/white/invert fill RGB and footprint alpha',flush=True)
    assert g.fn('glGetError',g.U)()==0
    # Scratch-FBO snapshot fixture: window and image sizes deliberately differ.
    # This is a production shader/PNG round-trip, not rawSnapshot integration.
    f=Fixture();g=f.g;w,h=3840,2160
    scene=np.zeros((h,w,4),np.float32);scene[...,:3]=(.17,.36,.73);scene[...,3]=1
    cover=np.zeros_like(scene);cover[:,w//2:,0]=1
    s=f.texture(scene,0x8058,(w,h));m=f.texture(cover,0x822B,(w,h),True)
    target=f.target(0x8058,(w,h));program=f.program((SH/'alchemy/actorKeyCompositeF.glsl').read_text())
    f.enable(0x0BD0);f.enable(0x8DB9);g.disable(0x0BD0);g.disable(0x8DB9)
    f.draw(program,target,[('keyScene',0),('keyCoverage',1),('keyOrigin',(0.,0.)),('keyColour',tuple(map(float,key))),('keyMode',0),('keyPerChannel',0)],[s,m])
    pixels=f.read(target)[...,:3];assert np.all(pixels[:,:w//2]==(0,177,64))
    assert np.all(pixels[:,w//2:]==np.rint(np.array([.17,.36,.73])*255).astype(int))
    output=OUT/'scratch_snapshot_fixture_4k.png';Image.fromarray(pixels).save(output)
    assert np.array_equal(np.array(Image.open(output)),pixels)
    assert g.fn('glGetError',g.U)()==0
    print('PASS WP2 scratch-FBO 3840x2160 PNG shader fixture (simulated 1920x1080 window): key exact/flat, actor round trip, lossless decode; actual rawSnapshot/UI-hidden acceptance remains separate',flush=True)

if __name__=='__main__':main()
