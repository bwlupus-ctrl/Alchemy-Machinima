"""WP1 hidden-WGL numeric checks using production coverage/blur/matte/Prism shaders.

No viewer build or launch. This is a shader/GL fixture, not a real avatar-pool or
K1-to-FinalizeGuides runtime test; those acceptance gates remain separately reported.
"""
import ctypes as C
import json
from pathlib import Path
import subprocess
import numpy as np
import relight_3d_gl as G

ROOT = Path(__file__).resolve().parents[2]
SH = ROOT / G.SHADERS / 'class1'
OUT = ROOT / '.tmp/ckey_20261102/gl'


class Fixture:
    def __init__(self):
        self.g = g = G.GL()
        print('GL:', g.renderer, flush=True)
        self.enable = g.fn('glEnable', None, g.U)
        self.clear = g.fn('glClearBufferfv', None, g.U, g.I, C.POINTER(g.F))
        self.blend = g.fn('glBlendFunc', None, g.U, g.U)
        self.eq = g.fn('glBlendEquation', None, g.U)
        self.scissor = g.fn('glScissor', None, g.I, g.I, g.I, g.I)
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x0B44, 0x8DB9, 0x0BD0):
            g.disable(cap)
        self.vbo = g.U(); g.gen_buf(1, C.byref(self.vbo))
        self.vertices([[-1,-1,0], [3,-1,0], [-1,3,0]])
        self.post = (SH / 'deferred/postDeferredNoTCV.glsl').read_text()

    def vertices(self, data):
        g = self.g
        values = np.array(data, np.float32)
        self.n = len(values)
        g.bind_buf(0x8892, self.vbo)
        g.buffer_data(0x8892, values.nbytes, values.ctypes.data, 0x88E4)

    def program(self, fragment, vertex=None):
        g = self.g; pr = g.program()
        for text, kind in ((vertex or self.post, 0x8B31), (fragment, 0x8B30)):
            shader = g.create_shader(kind)
            raw = ('#version 330\n' + text.replace('/*[EXTRA_CODE_HERE]*/', '')).encode()
            ptr = C.c_char_p(raw); length = g.I(len(raw))
            g.source(shader, 1, C.byref(ptr), C.byref(length)); g.compile(shader)
            ok = g.I(); g.getshader(shader, 0x8B81, C.byref(ok))
            if not ok.value:
                log = C.create_string_buffer(32000); g.slog(shader, len(log), None, log)
                raise AssertionError(log.value.decode())
            g.attach(pr, shader)
        g.link(pr); ok = g.I(); g.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(32000); g.plog(pr, len(log), None, log)
            raise AssertionError(log.value.decode())
        return pr

    def texture(self, data, internal=0x8229, size=(64,64), linear=False):
        g = self.g; tex = g.U(); g.gen_tex(1,C.byref(tex)); g.bind_tex(0x0DE1,tex)
        for p,v in ((0x2800,0x2601 if linear else 0x2600), (0x2801,0x2601 if linear else 0x2600),
                    (0x2802,0x812F),(0x2803,0x812F)):
            g.param(0x0DE1,p,v)
        w,h=size
        rgba = None if data is None else np.ascontiguousarray(data,np.float32)
        g.teximage(0x0DE1,0,internal,w,h,0,0x1908,0x1406,None if rgba is None else rgba.ctypes.data)
        return tex

    def target(self, internal=0x8229, size=(64,64)):
        g=self.g; tex=self.texture(None,internal,size,True)
        fbo=g.U(); g.gen_fbo(1,C.byref(fbo)); g.bind_fbo(0x8D40,fbo)
        g.fbo_tex(0x8D40,0x8CE0,0x0DE1,tex,0)
        assert g.fbo_status(0x8D40)==0x8CD5
        return (tex,fbo,size)

    def draw(self, program, target, uniforms=(), textures=(), clear=False, origin=(0,0)):
        g=self.g; tex,fbo,(w,h)=target
        g.bind_fbo(0x8D40,fbo); g.viewport(*origin,w,h); g.use(program)
        if clear: self.clear(0x1800,0,(g.F*4)(0,0,0,0))
        for name,value in uniforms:
            loc=g.loc(program,name.encode())
            if isinstance(value,int): g.ui(loc,value)
            elif isinstance(value,tuple): g.uf[len(value)](loc,*value)
            else: g.uf[1](loc,value)
        for unit,texture in enumerate(textures):
            g.active(0x84C0+unit); g.bind_tex(0x0DE1,texture)
        ix=g.attr_loc(program,b'position')
        g.bind_buf(0x8892,self.vbo); g.enable_attr(ix)
        g.attr_ptr(ix,3,0x1406,0,12,None); g.draw(4,0,self.n)
        g.fn('glDisableVertexAttribArray',None,g.U)(ix)

    def read(self,target):
        g=self.g; _,fbo,(w,h)=target; data=np.zeros((h,w,4),np.float32)
        g.bind_fbo(0x8D40,fbo); g.read(0,0,w,h,0x1908,0x1406,data.ctypes.data)
        return np.round(data*255).astype(np.uint8)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    f=Fixture(); g=f.g
    blur=f.program((SH/'alchemy/actorKeyBlurF.glsl').read_text())
    matte=f.program((SH/'alchemy/actorKeyMatteF.glsl').read_text())
    source=(SH/'deferred/silCoverF.glsl').read_text()+'\nvoid mirrorClip(vec3 p){}\n'
    cover_vertex='''in vec3 position; out vec3 vary_position; out vec2 vary_texcoord0;
out vec4 vertex_color; flat out int mask_slot;
void main(){gl_Position=vec4(position,1);vary_position=position;vary_texcoord0=position.xy*.5+.5;vertex_color=vec4(1);mask_slot=0;}
'''
    cover=f.program('#define MASK_CHANNELS 1\n'+source,cover_vertex)
    system=f.program('#define SIL_COVER_SYSTEM\n'+source,cover_vertex)
    coverage=f.target(); rgba=np.ones((64,64,4),np.float32); rgba[:,:,3]=.25
    authored=f.texture(rgba,0x8058)
    uniforms=[('mask_textures[0]',0),('mask_cutoff[0]',-.1),('mask_material_slots',0),
              ('mask_vertex_alpha',0),('sil_cover',(1.,0.)),('sil_blended',1),('sil_clone_progress',0.)]
    f.draw(cover,coverage,uniforms,[authored],True)
    f.enable(0x0BE2); f.blend(1,0x0301); f.eq(0x8006) # ONE, ONE_MINUS_SRC_COLOR
    f.draw(cover,coverage,uniforms,[authored])
    union=f.read(coverage)[:,:,0]; expected=round((64/255+(64/255)*(1-64/255))*255)
    assert np.max(np.abs(union.astype(int)-expected))<=1
    g.disable(0x0BE2)
    for program in (cover,system):
        f.draw(program,coverage,uniforms+[('diffuseMap',0),('minimum_alpha',0.),('sil_blended',0)], [authored],True)
        assert np.all(f.read(coverage)[:,:,0]==255), 'silhouette-OFF opaque coverage must be 1'
    print('PASS: production native/system coverage=1, continuous alpha union in R8',flush=True)

    # I-SILPARITY: key writes a different R8 target; same unchanged production
    # RG shader and gains retain exact bytes before and after those writes.
    sil=f.target(0x822B)
    silhouette_uniforms=uniforms+[('sil_cover',(.3,.7)),('sil_blended',0)]
    f.draw(cover,sil,silhouette_uniforms,[authored],True); before=f.read(sil)
    f.draw(cover,coverage,uniforms,[authored],True)
    assert np.array_equal(before,f.read(sil))
    f.draw(cover,sil,uniforms,[authored],True)
    assert not np.array_equal(before,f.read(sil)), "wrong-target negative control escaped"
    baseline=subprocess.check_output(['git','show','4afbf00ba09:indra/newview/app_settings/shaders/class1/deferred/silCoverF.glsl'])
    assert baseline.replace(b'\r\n',b'\n')==(SH/'deferred/silCoverF.glsl').read_bytes().replace(b'\r\n',b'\n')
    print('PASS: I-SILPARITY shader fixture RG(.3,.7) byte-identical after private key replay',flush=True)

    # Production matte: fractional raw coverage, endpoints, choke/grow direction,
    # both full/half blur paths, radius boundary and clamped hard remap.
    edge=np.zeros((64,64,4),np.float32); edge[:,:,3]=1; edge[:,20:44,0]=1
    edge[:,19,0]=.25; edge[:,44,0]=.75
    rawtex=f.texture(edge,0x8229,linear=True); result=f.target(0x822B)
    cases=[]
    def remap(feather,choke):
        radius=max(feather,abs(choke)); raw=radius<.5; half=radius>=1.5
        blurred=rawtex
        if not raw:
            size=(32,32) if half else (64,64); a=f.target(size=size); b=f.target(size=size)
            def pass_(src,dst,dx,dy,down=0):
                f.draw(blur,dst,[('keySource',0),('keyDirection',(dx,dy)),
                    ('keyRadius',radius*(.5 if half else 1.)),('keyDownsample',down)],[src])
            if half: pass_(rawtex,a,0.,0.,1)
            pass_(a[0] if half else rawtex,b,1./size[0],0.)
            pass_(b[0],a,0.,1./size[1]); blurred=a[0]
        r=max(radius,.5); c=choke/(2*r); ramp=max(feather/(2*r),1/255)
        f.draw(matte,result,[('keyCoverage',0),('keyBlur',1),('keyRaw',int(raw)),
            ('keyKeepInterior',int(choke<=0)),('keyDebug',0),('keyRemap',(max(0.,.5+c-ramp),min(1.,.5+c+ramp)))],[rawtex,blurred])
        pixels=f.read(result)
        assert np.all(pixels[:,:,1]==0), 'WP4 shadow stub must be zero'
        assert np.all(pixels[:,:2,0]==0), 'exterior exact zero'
        if choke<=0: assert np.all(pixels[:,28:36,0]==255), 'interior exact one'
        cases.append(dict(feather=feather,choke=choke,sum=int(pixels[:,:,0].sum())))
        return pixels[:,:,0]
    for feather,choke in ((0.,0.),(.49,0.),(.5,0.),(1.49,0.),(1.5,0.),(4.,0.),(8.,0.),(0.,2.),(0.,-2.),(2.,2.),(2.,-2.)):
        pixels=remap(feather,choke)
        if max(feather,abs(choke))<.5:
            assert np.array_equal(pixels,np.round(edge[:,:,0]*255).astype(np.uint8))
    assert remap(0.,2.).sum() < remap(0.,0.).sum() < remap(0.,-2.).sum()
    assert np.any((remap(4.,0.)>0)&(remap(4.,0.)<255))
    # A broken binary raw branch is detected by the authored fractional edge.
    assert not np.array_equal(remap(0.,0.), (edge[:,:,0]>.5).astype(np.uint8)*255)
    print('PASS: production blur/remap raw/full/half branches, exact endpoints, alpha, positive choke/negative grow; binary-raw negative control detected',flush=True)

    # Impulse reference for each separable pass at full and half target sizes.
    # Radius includes the target-height scaling used by the caller.
    blur_text=(SH/'alchemy/actorKeyBlurF.glsl').read_text()
    identity=f.program(blur_text.replace('total / weight', 'texture(keySource, vary_fragcoord).r'))
    wrong=f.program(blur_text.replace('keyDirection * float(i)', 'keyDirection.yx * float(i)'))
    def gaussian_check(program):
        for size,scale in (((64,64),1.),((32,32),.5)):
            for height_scale in (.5,1.,2.):
                for radius in (2.,4.,8.):
                    r=radius*scale*height_scale
                    impulse=np.zeros((size[1],size[0],4),np.float32)
                    impulse[size[1]//2,size[0]//2,0]=1; impulse[:,:,3]=1
                    tex=f.texture(impulse,0x8229,size=size,linear=True)
                    for axis in (0,1):
                        dst=f.target(size=size); direction=(1./size[0],0.) if axis==0 else (0.,1./size[1])
                        f.draw(program,dst,[('keySource',0),('keyDirection',direction),('keyRadius',r),('keyDownsample',0)],[tex])
                        actual=f.read(dst)[:,:,0].astype(int)
                        taps=min(16,max(1,int(np.ceil(r)))); sigma=max(r*.5,.25)
                        weights=np.exp(-.5*np.arange(-taps,taps+1)**2/(sigma*sigma)); weights/=weights.sum()
                        expected=np.zeros_like(actual); cy,cx=size[1]//2,size[0]//2
                        for offset,w in zip(range(-taps,taps+1),weights):
                            y,x=(cy,cx+offset) if axis==0 else (cy+offset,cx)
                            if 0<=x<size[0] and 0<=y<size[1]: expected[y,x]=round(w*255)
                        if np.max(np.abs(actual-expected))>1: return False
        return True
    assert gaussian_check(blur), 'Gaussian impulse reference failed'
    assert not gaussian_check(identity), 'identity Gaussian negative control escaped'
    assert not gaussian_check(wrong), 'wrong-axis Gaussian negative control escaped'
    print('PASS: Gaussian impulse oracle full/half, both axes, radii and height scales; identity and wrong-axis rejected',flush=True)

    # Independent numerical remap reference over all 256 coverage codes. Feed a
    # known blur ramp directly, so the reference cannot inherit a blur defect.
    ramp=np.zeros((4,256,4),np.float32); ramp[:,:,0]=np.arange(256)/255; ramp[:,:,3]=1
    ramptex=f.texture(ramp,0x8229,size=(256,4)); remapped=f.target(0x822B,size=(256,4))
    for lo,hi,keep in ((.2,.8,0),(.4,.6,0),(0.,1/255,0),(254/255,1.,0),(.2,.8,1)):
        f.draw(matte,remapped,[('keyCoverage',0),('keyBlur',1),('keyRaw',0),
            ('keyKeepInterior',keep),('keyDebug',0),('keyRemap',(lo,hi))],[ramptex,ramptex])
        x=np.clip((ramp[:,:,0]-lo)/(hi-lo),0,1); expected_remap=x*x*(3-2*x)
        if keep: expected_remap=np.maximum(expected_remap,(ramp[:,:,0]>=.999).astype(float))
        delta=np.abs(f.read(remapped)[:,:,0].astype(int)-np.round(expected_remap*255).astype(int))
        assert delta.max()<=1, ('remap numeric reference',lo,hi,int(delta.max()))
    # Odd dimensions are true 2x2 boxes, with the last odd pixel clamped.
    odd=np.zeros((5,7,4),np.float32); odd[:,:,0]=np.arange(35).reshape(5,7)/34; odd[:,:,3]=1
    oddtex=f.texture(odd,0x8229,size=(7,5),linear=True); small=f.target(size=(4,3))
    f.draw(blur,small,[('keySource',0),('keyDirection',(0.,0.)),('keyRadius',2.),('keyDownsample',1)],[oddtex])
    encoded=np.round(odd[:,:,0]*255)
    padded=np.pad(encoded,((0,1),(0,1)),mode='edge')
    box=padded.reshape(3,2,4,2).mean(axis=(1,3))
    assert np.max(np.abs(f.read(small)[:,:,0].astype(float)-box))<=1
    print('PASS: all-code remap vs independent smoothstep reference (<=1 code); odd 7x5 -> 4x3 exact 2x2/clamped box',flush=True)

    # W-K14: production virtual quad vertex + zero fragment, shared-depth LEQUAL,
    # actor foreground/behind, two-sided screen, scissor. Modes do not enter K1.
    occluder=f.program((SH/'alchemy/actorKeyOccluderF.glsl').read_text(), (SH/'alchemy/actorKeyOccluderV.glsl').read_text())
    depth=g.U(); g.gen_tex(1,C.byref(depth)); g.bind_tex(0x0DE1,depth)
    g.teximage(0x0DE1,0,0x8CAC,64,64,0,0x1902,0x1406,None)
    g.bind_fbo(0x8D40,coverage[1]); g.fbo_tex(0x8D40,0x8D00,0x0DE1,depth,0)
    f.enable(0x0B71); g.fn('glDepthFunc',None,g.U)(0x0203)
    g.fn('glDepthMask',None,C.c_ubyte)(0)
    identity=np.eye(4,dtype=np.float32)
    g.use(occluder); g.matrix(g.loc(occluder,b'modelview_projection_matrix'),1,0,identity.ctypes.data_as(C.POINTER(g.F)))
    for mode in range(3):
        for z,feather,expected in ((-.5,0.,0),(.5,0.,255),(-.5,-1.,255)):
            g.bind_fbo(0x8D40,coverage[1]); g.viewport(0,0,64,64)
            g.fn('glDepthMask',None,C.c_ubyte)(1); f.clear(0x1801,0,(g.F*1)(.5)); g.fn('glDepthMask',None,C.c_ubyte)(0)
            f.clear(0x1800,0,(g.F*4)(1,0,0,1))
            f.vertices([[-.5,-.5,z],[.5,-.5,z],[.5,.5,z],[-.5,-.5,z],[.5,.5,z],[-.5,.5,z]])
            f.enable(0x0C11); f.scissor(24,24,16,16)
            f.draw(occluder,coverage,[('edgeFeather',feather)])
            pixels=f.read(coverage)[:,:,0]
            assert np.all(pixels[24:40,24:40]==expected)
            assert np.all(pixels[:24]==255) and np.all(pixels[40:]==255)
            g.disable(0x0C11)
    assert g.fn('glGetError',g.U)()==0
    print('PASS: W-K14 production virtual quad across actor, foreground/behind depth, scissor, beauty discard, modes 0/1/2; GL error=0',flush=True)
    composite_checks(f)
    (OUT/'metrics.json').write_text(json.dumps(dict(renderer=g.renderer,remap=cases,coverage_union_byte=int(union[0,0])),indent=2))



def composite_checks(f):
    """Production late composite, real UNORM attachments. Dither starts ON per frame.

    GL fixtures exercise the pixel law and explicit DRAW copy with independent
    READ FBO/buffer, viewport origin and PBO. Viewer scheduling is a separate gate.
    """
    g=f.g
    composite=f.program((SH/'alchemy/actorKeyCompositeF.glsl').read_text())
    get=g.fn('glGetIntegerv',None,g.U,C.POINTER(g.I))
    read_buffer=g.fn('glReadBuffer',None,g.U)
    draw_buffer=g.fn('glDrawBuffer',None,g.U)
    copy=g.fn('glCopyTexSubImage2D',None,g.U,g.I,g.I,g.I,g.I,g.I,g.I,g.I)
    is_enabled=g.fn('glIsEnabled',C.c_ubyte,g.U)
    def integer(p):
        v=g.I();get(p,C.byref(v));return v.value
    def read_codes(target,bits):
        data=np.zeros((64,64,4),np.float32)
        g.bind_fbo(0x8CA8,target[1]);read_buffer(0x8CE0)
        g.read(7,9,64,64,0x1908,0x1406,data.ctypes.data)
        return np.rint(data[...,:3]*((1<<bits)-1)).astype(np.int32)
    rng=np.random.default_rng(20261102)
    key=np.array([0,177,64],np.float32)/255
    coverage=f.texture(np.zeros((64,64,4),np.float32),0x8814)
    cases=0
    f.vertices([[-1,-1,0],[3,-1,0],[-1,3,0]])
    for fmt,bits in ((0x8058,8),(0x8059,10)):
        codes=(1<<bits)-1
        # Two attachments let the READ fixture retain COLOR_ATTACHMENT1.
        target=f.target(fmt,(96,96)); other=f.target(fmt,(96,96))
        extra=f.texture(None,fmt,(96,96));g.bind_fbo(0x8D40,other[1])
        g.fbo_tex(0x8D40,0x8CE1,0x0DE1,extra,0);read_buffer(0x8CE1)
        scene=f.texture(None,fmt,(64,64))
        for frame in range(8):
            source=rng.integers(0,codes+1,(96,96,4)).astype(np.float32)/codes
            source[...,3]=1
            g.active(0x84C0);g.bind_tex(0x0DE1,target[0])
            g.teximage(0x0DE1,0,fmt,96,96,0,0x1908,0x1406,source.ctypes.data)
            # Bind an unrelated read FBO/buffer and a pack buffer before the copy.
            g.bind_fbo(0x8CA9,target[1]);draw_buffer(0x8CE0)
            g.bind_fbo(0x8CA8,other[1]);read_buffer(0x8CE1)
            pbo=g.U();g.gen_buf(1,C.byref(pbo));g.bind_buf(0x88EB,pbo)
            g.buffer_data(0x88EB,64*64*16,None,0x88E1)
            saved=(integer(0x8CAA),integer(0x0C02),integer(0x88ED))
            # Same explicit source/read restoration as production actor_key_copy.
            src_read=0x8CE0
            g.bind_fbo(0x8CA8,target[1]);src_read=integer(0x0C02);read_buffer(0x8CE0)
            g.bind_tex(0x0DE1,scene);copy(0x0DE1,0,0,0,7,9,64,64)
            read_buffer(src_read);g.bind_fbo(0x8CA8,saved[0]);read_buffer(saved[1])
            assert (integer(0x8CAA),integer(0x0C02),integer(0x88ED))==saved
            g.bind_buf(0x88EB,0)
            original=np.rint(source[9:73,7:71,:3]*codes).astype(np.int32)
            for mode in range(3):
                for m in ((0,0,0),(1,1,1),(1,0,0),(0,1,0),(0,0,1),(.25,.5,.75)):
                    cover=np.zeros((64,64,4),np.float32);cover[...,:3]=m
                    g.active(0x84C0);g.bind_tex(0x0DE1,coverage)
                    g.teximage(0x0DE1,0,0x8814,64,64,0,0x1908,0x1406,cover.ctypes.data)
                    f.enable(0x0BD0) # fixture starts with GL_DITHER enabled
                    f.enable(0x8DB9)
                    assert is_enabled(0x0BD0) and is_enabled(0x8DB9)
                    g.disable(0x0BD0);g.disable(0x8DB9);g.disable(0x0BE2)
                    assert not is_enabled(0x0BD0) and not is_enabled(0x8DB9)
                    uniforms=[('keyScene',0),('keyCoverage',1),('keyOrigin',(7.,9.)),
                              ('keyColour',tuple(float(v) for v in key)),('keyMode',mode),
                              ('keyInvert',0),('keyHard',0),('keyMatteShadow',0),
                              ('keyShadowStrength',1.),('keyPerChannel',1)]
                    f.draw(composite,(target[0],target[1],(64,64)),uniforms,[scene,coverage],origin=(7,9))
                    result=read_codes(target,bits)
                    cv=np.array(m,np.float32)
                    expected=(key*(1-cv)+source[9:73,7:71,:3]*cv if mode==0 else
                              source[9:73,7:71,:3]*(1-cv)+key*cv if mode==1 else np.full(3,np.dot(cv,[.2126,.7152,.0722])))
                    expected=np.broadcast_to(np.rint(expected*codes).astype(np.int32),result.shape)
                    assert np.max(np.abs(result-expected)) <= (0 if all(v in (0,1) for v in m) else 1), (bits,frame,mode,m,int(np.max(np.abs(result-expected))))
                    if mode==0 and m==(1,1,1):assert np.array_equal(result,original),'actor round trip'
                    if mode==0 and m==(0,0,0):assert np.all(result==np.rint(key*codes).astype(int)), 'non-flat key'
                    if mode==0 and m==(0,0,0):
                        byte_pixels=np.zeros((64,64,4),np.uint8)
                        g.read(7,9,64,64,0x1908,0x1401,byte_pixels.ctypes.data)
                        assert np.all(byte_pixels[...,:3]==(0,177,64)), '8-bit capture codes'

                    f.enable(0x0BD0);f.enable(0x8DB9) # scoped state restored
                    cases+=1
        print(f'PASS WP2: {bits}-bit exact/flat 64x64 blocks x8 frames; three modes; actor round trip; RGB one-hot varied S; DRAW/READ/PBO/origin',flush=True)
    # SRGB-LEAK: deliberately omit the sRGB disable on an SRGB8_ALPHA8 target.
    srgb=f.target(0x8C43,(96,96));zero=np.zeros((64,64,4),np.float32)
    g.active(0x84C0);g.bind_tex(0x0DE1,coverage);g.teximage(0x0DE1,0,0x8814,64,64,0,0x1908,0x1406,zero.ctypes.data)
    uniforms=[('keyScene',0),('keyCoverage',1),('keyOrigin',(7.,9.)),('keyColour',tuple(float(v) for v in key)),('keyMode',0),('keyPerChannel',1)]
    g.disable(0x0BD0);f.enable(0x8DB9)
    f.draw(composite,(srgb[0],srgb[1],(64,64)),uniforms,[scene,coverage],origin=(7,9))
    leaked=read_codes(srgb,8)
    assert not np.all(leaked==np.rint(key*255).astype(int)),'SRGB-LEAK negative control escaped'
    g.disable(0x8DB9)
    f.draw(composite,(srgb[0],srgb[1],(64,64)),uniforms,[scene,coverage],origin=(7,9))
    assert np.all(read_codes(srgb,8)==np.rint(key*255).astype(int))
    print(f'PASS WP2: SRGB-LEAK instrument detects double encoding; scoped disable fixes it; {cases} numeric cases',flush=True)
    assert g.fn('glGetError',g.U)()==0


if __name__=='__main__':
    main()
