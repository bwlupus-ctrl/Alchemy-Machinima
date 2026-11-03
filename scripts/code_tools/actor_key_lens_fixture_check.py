"""WP-LENS production GLSL gates, hidden WGL, no viewer build.

Real GL MRTs use the production 3/2/1 attachment layouts. External view-depth
accessor is deterministic; this does not claim logged-in viewer acceptance.
"""
import ctypes as C
from pathlib import Path
import subprocess
import sys
import numpy as np
from actor_key_gl_check import Fixture, SH

ROOT=Path(__file__).resolve().parents[2]
N=64
POSITION='\nvec4 getPosition(vec2 p){return vec4(0.0,0.0,-(2.0+12.0*p.x),1.0);}\n'
FILES=['ultimateDiopterGatherF.glsl','ultimateDiopterF.glsl','ultimateKaleidoF.glsl','cineFisheyeF.glsl']

class Lens(Fixture):
    def __init__(self):
        super().__init__()
        self.native=None;self.owned={}
        if '--real-targets' in sys.argv:
            from actor_key_lens_native_check import load_targets
            self.native=load_targets()
            assert self.native.bytes()==0
        self.buffers=self.g.fn('glDrawBuffers',None,self.g.I,C.POINTER(self.g.U))
        self.read_buffer=self.g.fn('glReadBuffer',None,self.g.U)
        self.delete_tex=self.g.fn('glDeleteTextures',None,self.g.I,C.POINTER(self.g.U))
        self.delete_fbo=self.g.fn('glDeleteFramebuffers',None,self.g.I,C.POINTER(self.g.U))
        self.get=self.g.fn('glGetIntegerv',None,self.g.U,C.POINTER(self.g.I))
    def mrt(self,formats):
        if self.native:
            handle=self.native.make_target(N,N,len(formats));assert handle
            first=(self.g.U(self.native.texture(handle,0)),self.g.U(self.native.fbo(handle)),(N,N))
            textures=[self.g.U(self.native.texture(handle,i)) for i in range(len(formats))]
            self.native.lens_bind(handle);self.native.lens_flush(handle)
            self.owned[first[1].value]=handle
            self.g.bind_fbo(0x8D40,first[1]);self.buffers(len(formats),(self.g.U*len(formats))(*[0x8CE0+i for i in range(len(formats))]))
            assert self.g.fbo_status(0x8D40)==0x8CD5
            return first,textures
        first=self.target(formats[0]); tex=[first[0]]
        for i,fmt in enumerate(formats[1:],1):
            tex.append(self.texture(None,fmt,linear=True))
            self.g.bind_fbo(0x8D40,first[1]); self.g.fbo_tex(0x8D40,0x8CE0+i,0x0DE1,tex[-1],0)
        self.g.bind_fbo(0x8D40,first[1]); self.buffers(len(tex),(self.g.U*len(tex))(*[0x8CE0+i for i in range(len(tex))]))
        assert self.g.fbo_status(0x8D40)==0x8CD5
        return first,tex
    def pixels(self,target,attachment=0):
        self.g.bind_fbo(0x8CA8,target[1]);self.read_buffer(0x8CE0+attachment)
        w,h=target[2];data=np.empty((h,w,4),np.float32)
        self.g.read(0,0,w,h,0x1908,0x1406,data.ctypes.data)
        assert np.isfinite(data).all()
        return data
    def shader(self,index,key=False,hdr=False,taps=24,head=False):
        path='indra/newview/app_settings/shaders/class1/deferred/'+FILES[index]
        src=subprocess.check_output(['git','show','HEAD:'+path]).decode('utf-8') if head else (ROOT/path).read_text(encoding='utf-8')
        return self.program(('#define AL_KEY_COVERAGE_ONLY 1\n' if key else '')+
            ('#define AL_SENSOR_HDR 1\n' if hdr else '')+f'#define DIOPTER_TAPS {taps}\n'+src+(POSITION if index!=3 else ''))
    def destroy(self,target,textures):
        if self.native:
            self.g.bind_fbo(0x8D40,0);self.native.drop(self.owned.pop(target[1].value));return
        self.delete_fbo(1,C.byref(target[1]))
        for t in textures:self.delete_tex(1,C.byref(t))
        self.g.bind_fbo(0x8D40,0)


def params():
    return dict(screen_res=(64.,64.),diffuseRect=0,diffuseMap=0,emissiveRect=1,
        exposureMap=6,exposure=1.,tonemap_type=8,sensor_diagnostics=0,
        diopter_shape=(.43,.54,.29,1.),diopter_shape2=(.43,.07,0.,0.),
        diopter_shape3=(4.,0.,.2,0.),diopter_shape4=(.05,4.,.2,.02),
        diopter_shape5=(6.,5.,.5,4.),diopter_shape6=(5.,.4,3.,.4),diopter_shape7=(.6,.5,6.2831853,0.),
        diopter_focus=(5.,9.,.5,2.),diopter_focus2=(1.1,1.,1.,5.),
        diopter_focus3=(8.,.3,1.2,1.08),diopter_focus4=(.7,.5,2.,0.),
        diopter_aperture=(1.,6.,.2,.3),diopter_aperture2=(.4,.7,.1,.4),
        diopter_glass=(0.,1.4,.3,.2),diopter_glass2=(.4,0.,0.,0.),
        diopter_halo=(3.,.2,.3,.2),diopter_halo2=(.1,3.,.2,1.),diopter_pattern=(1.,6.,.2,1.1),
        diopter_comp=(3.,.8,.025,0.),diopter_comp2=(0.,0.,1.,0.),
        diopter_ghost=(0.,.5,.4,.4),diopter_ghost2=(.4,.1,.3,.5),
        kal_pattern=(0.,7.,3.,0.),kal_shape=(.4,.1,.6,.3),kal_center=(.45,.52,.4,.5),
        kal_source=(.3,1.1,.12,.03),kal_fx=(.3,.4,.2,5.),kal_protect=(3.,.5,.15,0.),
        kal_depth=(7.,2.,0.,0.),kal_wave=(.2,.3,.1,5.),kal_look=(.9,.7,0.,0.),
        kal_cell=(.2,.2,.1,.1),kal_cell2=(0.,0.,0.,0.),
        fisheye_params=(1.2,.3,.92,.18),fisheye_params2=(.2,-.1,1.,0.),
        keyCoverage=3,keyClear=4,keyDio=5,keyInputPacked=0)


def main():
    f=Lens(); g=f.g; u=params();
    if '--native-only' in sys.argv:
        native_chain_cases(f,u);return
    rng=np.random.default_rng(52)
    y,x=np.mgrid[:N,:N]; mask=(((x-37)**2+(y-26)**2<21**2)|(x>59)).astype(np.float32)
    shape=np.repeat(mask[:,:,None],4,axis=2);shape[:,:,3]=mask*.37
    image=rng.random((N,N,4),dtype=np.float32)*2.;image[:,:,3]=1.
    source=f.texture(image,0x881A,linear=True);incoming=f.texture(shape,0x881A,linear=True)
    exposure=f.texture(np.ones((N,N,4),np.float32),0x881A)
    textures=[source,source,source,incoming,incoming,incoming,exposure]
    # I-LENSINPUTS compares feature-OFF sources from HEAD against the unchanged
    # production permutation in the edited source, every beauty attachment.
    for hdr in (False,True):
        for i in range(4):
            for taps in ((12,24,40,64) if i==0 else (24,)):
                beauty=f.shader(i,hdr=hdr,taps=taps);old=f.shader(i,hdr=hdr,taps=taps,head=True)
                key=f.shader(i,True,hdr,taps)
                a,at=f.mrt([0x881A,0x881A,0x822F] if i in (0,2) else [0x881A])
                b,bt=f.mrt([0x881A]* (2 if i==0 else 1))
                f.draw(old,a,list(u.items()),textures);before=[f.pixels(a,j).copy() for j in range(len(at))]
                f.draw(beauty,a,list(u.items()),textures)
                assert all(np.array_equal(v,f.pixels(a,j)) for j,v in enumerate(before)),('beauty source changed',hdr,i,taps)
                f.draw(key,b,list(u.items()),textures)
                assert all(np.array_equal(v,f.pixels(a,j)) for j,v in enumerate(before)),('coverage touched beauty',hdr,i)
                for j in range(len(bt)):
                    out=f.pixels(b,j);assert out.min()>=0 and out.max()<=1,(hdr,i,out.min(),out.max())
                f.destroy(a,at);f.destroy(b,bt)
    print('PASS I-LENSINPUTS: all beauty attachments bit-identical to HEAD, separate coverage draws; both chains, all gather qualities',flush=True)

    # Image -> matte and gather -> composite -> fisheye use the actual formats,
    # including the gather metadata, with nontrivial beauty weights and selections.
    for hdr in (False,True):
        pg,pc,pk,pf=[f.shader(i,hdr=hdr) for i in range(4)]
        cg,cc,ck,cf=[f.shader(i,True,hdr) for i in range(4)]
        gather,gt=f.mrt([0x881A,0x881A,0x822F]);gc,gct=f.mrt([0x881A,0x881A])
        comp,ct=f.mrt([0x881A]);cov,cvt=f.mrt([0x881A]);fish,ft=f.mrt([0x881A]);fc,fct=f.mrt([0x881A])
        for val in (0.,1.):
            constant=f.texture(np.full((N,N,4),val,np.float32),0x881A,linear=True)
            t=[source,source,source,constant,constant,constant,exposure]
            f.draw(pg,gather,list(u.items()),t);saved=[f.pixels(gather,j).copy() for j in range(3)]
            f.draw(cg,gc,list(u.items()),t)
            assert all(np.array_equal(saved[j],f.pixels(gather,j)) for j in range(3))
            for j in (0,1):assert np.max(abs(f.pixels(gc,j)-val))<.002
            t=[gt[0],gt[1],source,constant,gct[0],gct[1],exposure]
            cu=u|{'diffuseMap':2}
            f.draw(pc,comp,list(cu.items()),t);saved_comp=f.pixels(comp).copy()
            for blend in (.3,.7,1.):
                for ghosts in (0.,4.):
                    cu2=cu|{'diopter_comp2':(0.,0.,blend,0.),'diopter_ghost':(ghosts,.7,.4,.4)}
                    f.draw(cc,cov,list(cu2.items()),t);v=f.pixels(cov)
                    assert np.max(abs(v-val))<.003,('constant composite',hdr,val,blend,ghosts,v.min(),v.max())
            assert np.array_equal(saved_comp,f.pixels(comp))
            f.draw(cf,fc,list(u.items()),[ct[0],gt[1],source,cvt[0],gct[0],gct[1],exposure]);out=f.pixels(fc)
            assert out.min()>=0 and out.max()<=1
            assert np.any(out==0) and (val==0 or np.max(out)>.99)
        # White/black image reference in working-linear HDR. In legacy, nearest
        # input taps keep the transfer function from changing fractional samples.
        white=shape.copy();white[:,:,3]=1.
        # Same incoming RGB and shadow=G: all channels can be checked against image.
        whitecov=white.copy();whitecov[:,:,3]=mask
        src=f.texture(white,0x881A,linear=hdr);inc=f.texture(whitecov,0x881A,linear=hdr)
        noab=None
        for blend in (1.,.3,.7):
            base=u|{'diopter_comp2':(0.,0.,blend,0.),'diopter_focus3':(8.,.3,0.,1.08),'diffuseMap':2}
            t=[src,src,src,inc,inc,inc,exposure]
            f.draw(pg,gather,list(base.items()),t);f.draw(cg,gc,list(base.items()),t)
            t=[gt[0],gt[1],src,inc,gct[0],gct[1],exposure]
            f.draw(pc,comp,list(base.items()),t);f.draw(cc,cov,list(base.items()),t)
            reference=f.pixels(comp)[:,:,:3];expected=reference if hdr else np.power(np.maximum(reference,0),2.2)
            # Legacy transfer is colour-only. Establish the working-linear
            # white/black reference without A/B, then apply A8's convex A/B law.
            if blend==1.:noab=expected.copy()
            expected_cov=noab*blend+whitecov[:,:,:3]*(1-blend)
            assert np.max(abs(expected_cov-f.pixels(cov)[:,:,:3]))<.004,('diopter image-to-matte',hdr,blend)
            if hdr:assert np.max(abs(expected-f.pixels(cov)[:,:,:3]))<.004
            f.draw(pf,fish,list(u.items()),[ct[0],gt[1],src,cvt[0],gct[0],gct[1],exposure])
            f.draw(cf,fc,list(u.items()),[ct[0],gt[1],src,cvt[0],gct[0],gct[1],exposure])
            assert np.max(abs(f.pixels(fc)-fisheye_oracle(f.pixels(cov),u)))<.003,('fisheye image-to-matte',hdr,blend)
            if hdr:
                uv=(np.indices((N,N))[::-1].transpose(1,2,0)+.5)/N
                radius=np.linalg.norm(uv*2-1-np.array(u['fisheye_params2'][:2])*2,axis=2)
                edge=np.clip((radius-(.92-.18))/(2*.18),0,1);v=1-edge*edge*(3-2*edge)
                assert np.max(abs(f.pixels(fish)[:,:,:3]-f.pixels(fc)[:,:,:3]*v[:,:,None]))<.004

        # Kaleidoscope is a convex image map in both chains, including hold, soften,
        # all 24 folds and mirror/tile/clamp modes; shadow follows identical taps.
        ka,kat=f.mrt([0x881A,0x881A,0x822F]);kc,kct=f.mrt([0x881A])
        for mode in range(24):
            for blend in (.3,.7):
                ku=u|{'kal_pattern':(float(mode),7.,3.,float(mode%3)),'kal_look':(.9,blend,0.,0.)}
                t=[src,src,src,inc,inc,inc,exposure]
                f.draw(pk,ka,list(ku.items()),t);f.draw(ck,kc,list(ku.items()),t)
                assert np.max(abs(f.pixels(ka)[:,:,:3]-f.pixels(kc)[:,:,:3]))<.003,('kaleido image matte',hdr,mode)
                assert np.max(abs(f.pixels(kc)[:,:,1]-f.pixels(kc)[:,:,3]))<.003
        for a,ts in ((gather,gt),(gc,gct),(comp,ct),(cov,cvt),(fish,ft),(fc,fct),(ka,kat),(kc,kct)):f.destroy(a,ts)
    print('PASS real-GL-target 3/2/1 MRT gather -> composite -> fisheye: completeness, every attachment, constants actor/shadow 0/1, beauty parity',flush=True)
    print('PASS image-to-matte: both-chain diopter seam/double image/CA + partial A/B; kaleidoscope all 24 modes/hold/soften/both chains',flush=True)
    seam_cases(f,u)
    if f.native:
        native_chain_cases(f,u)
        assert f.native.bytes()==0 and not f.owned
        for size in (64,128,96,64):
            handle=f.native.make_target(size,size,2);assert handle and f.native.bytes()==size*size*16
            f.native.drop(handle);assert f.native.bytes()==0
        print('PASS actual LLRenderTarget allocator/bind/flush/release: full shader corpus, 2/1 attachments, first ON, OFF memory baseline, snapshot resize/rearm',flush=True)
    print('PASS W-K11 harness: diopter, kaleidoscope, off-centre fisheye border, diopter -> fisheye; viewer visual acceptance remains external',flush=True)


def fisheye_oracle(image,u):
    uv=(np.indices((N,N))[::-1].transpose(1,2,0)+.5)/N
    center=np.array(u['fisheye_params2'][:2]);zoom=u['fisheye_params2'][2]
    c=uv*2-1-center*2;r=np.linalg.norm(c,axis=2);rn2=(r/u['fisheye_params'][2])**2
    gain=(1+1.2*rn2+.3*rn2**2)/(1+1.2+.3)
    source=c*gain[:,:,None]/zoom*.5+.5+center
    pixel=source*N-.5;base=np.floor(pixel).astype(int);fraction=pixel-base
    x0=np.clip(base[:,:,0],0,N-1);x1=np.clip(base[:,:,0]+1,0,N-1)
    y0=np.clip(base[:,:,1],0,N-1);y1=np.clip(base[:,:,1]+1,0,N-1)
    fx=fraction[:,:,0,None];fy=fraction[:,:,1,None]
    sampled=((image[y0,x0]*(1-fx)+image[y0,x1]*fx)*(1-fy)+
             (image[y1,x0]*(1-fx)+image[y1,x1]*fx)*fy)
    sampled[np.any((source<0)|(source>1),axis=2)]=0
    return sampled


def native_chain_cases(f,u):
    d=f.native;g=f.g
    get=g.fn('glGetIntegerv',None,g.U,C.POINTER(g.I));use=g.use
    def integer(token):v=g.I();get(token,C.byref(v));return v.value
    def state():
        values=[integer(t) for t in (0x8CA6,0x8CAA,0x84E0,0x0C02)]
        values.append(tuple(integer(0x8825+i) for i in range(4))) # DRAW_BUFFER0..3
        v=(g.I*4)();get(0x0BA2,v);values.append(tuple(v))
        saved=integer(0x84E0)
        for i in range(12):g.active(0x84C0+i);values.append(integer(0x8069))
        g.active(saved)
        return values
    y,x=np.mgrid[:N,:N];m=((x-39)**2+(y-30)**2<21**2).astype(np.float32)
    image=np.repeat(m[:,:,None],4,axis=2);image[:,:,3]=1
    packed=np.zeros((N,N,4),np.float32);packed[:,:,0]=m;packed[:,:,1]=m*.37
    src=f.texture(image,0x881A,linear=True);inc=f.texture(packed,0x881A,linear=True)
    exp=f.texture(np.ones_like(image),0x881A)
    gather,gt=f.mrt([0x881A,0x881A,0x822F]);comp,ct=f.mrt([0x881A]);fish,ft=f.mrt([0x881A])
    reference,rt=f.mrt([0x881A]);referenceGather,rgt=f.mrt([0x881A,0x881A])
    for hdr in (False,True):
        # First frame ON: the complete production renderActorKeyLens body runs
        # immediately after each beauty draw with actual LLRenderTarget inputs.
        for stages in ((0,1,3),(2,3)):
            d.begin_lens(N,N,packed.ctypes.data_as(C.POINTER(C.c_float)))
            assert d.lens_valid()==1 # no lens yet
            incoming=inc
            for stage in stages:
                target = gather if stage in (0,2) else comp if stage==1 else fish
                beauty=f.shader(stage,hdr=hdr);key=f.shader(stage,True,hdr)
                if stage==0:t=[src,src,src,incoming,incoming,incoming,exp];cu=u|{'keyInputPacked':1}
                elif stage==2:t=[src,src,src,incoming,incoming,incoming,exp];cu=u|{'keyInputPacked':1}
                elif stage==1:t=[gt[0],gt[1],src,incoming,g.U(d.lens_gather(0)),g.U(d.lens_gather(1)),exp];cu=u|{'diffuseMap':2,'keyInputPacked':1}
                else:t=[gt[0] if 2 in stages else ct[0],ct[0],src,g.U(d.lens_output_texture()),inc,inc,exp];cu=u|{'keyInputPacked':0}
                # Production bindTexture explicitly requests bilinear for BOTH gather
                # attachments; the allocator initially makes attachment 1 nearest.
                for unit,texture in enumerate(t):
                    g.active(0x84C0+unit);g.bind_tex(0x0DE1,texture)
                    g.param(0x0DE1,0x2800,0x2601);g.param(0x0DE1,0x2801,0x2601)
                expectedTarget=referenceGather if stage==0 else reference
                f.draw(key,expectedTarget,list(cu.items()),t)
                expected=[f.pixels(expectedTarget,j).copy() for j in range(2 if stage==0 else 1)]
                f.draw(beauty,target,list(cu.items()),t)
                savedBeauty=[f.pixels(target,j).copy() for j in range(3 if stage in (0,2) else 1)]
                g.bind_fbo(0x8CA9,target[1]);g.bind_fbo(0x8CA8,reference[1]);f.read_buffer(0x8CE0)
                g.viewport(3,5,51,47);d.lens_active(0);d.lens_active(7);use(0)
                before=state()
                valid=d.run_lens(stage,beauty,key,N,N)
                assert state()==before,('production lens helper GL state leak',hdr,stage,[(i,a,b) for i,(a,b) in enumerate(zip(before,state())) if a!=b])
                assert valid==(0 if stage==0 else 1),('gather is intermediate; completed first activation must be valid',hdr,stage)
                if stage==0:
                    # Read the two native key-owned gather textures through a probe FBO.
                    for j in range(2):
                        g.bind_fbo(0x8D40,referenceGather[1]);g.fbo_tex(0x8D40,0x8CE0,0x0DE1,d.lens_gather(j),0)
                        assert np.max(abs(f.pixels(referenceGather)-expected[j]))<.002
                    # Restore probe attachment, never alias a later native write.
                    g.bind_fbo(0x8D40,referenceGather[1]);g.fbo_tex(0x8D40,0x8CE0,0x0DE1,rgt[0],0)
                else:
                    output=(g.U(d.lens_output_texture()),g.U(d.lens_output()),(N,N))
                    assert np.max(abs(f.pixels(output)-expected[0]))<.002,(hdr,stage,float(np.max(abs(f.pixels(output)-expected[0]))),f.pixels(output)[32,40],expected[0][32,40])
                assert all(np.array_equal(savedBeauty[j],f.pixels(target,j)) for j in range(len(savedBeauty)))
            d.lens_missing();assert d.lens_valid()==0
            d.lens_stale();assert d.lens_valid()==0
            d.lens_off();assert d.lens_output()==0
            d.lens_finish()


    baseline_bytes=d.bytes()
    d.begin_lens(N,N,packed.ctypes.data_as(C.POINTER(C.c_float)))
    for size,value in ((64,0.),(128,1.),(96,0.),(64,1.)):
        data=np.full((size,size,4),value,np.float32)
        d.begin_lens(size,size,data.ctypes.data_as(C.POINTER(C.c_float)))
        beauty=f.shader(3);key=f.shader(3,True)
        handle=d.make_target(size,size,1);assert handle
        target=(g.U(d.texture(handle,0)),g.U(d.fbo(handle)),(size,size))
        cu=u|{'screen_res':(float(size),float(size))}
        f.draw(beauty,target,list(cu.items()),[src])
        d.lens_active(0);d.lens_active(7);use(0)
        assert d.run_lens(3,beauty,key,size,size)==1
        output=(g.U(d.lens_output_texture()),g.U(d.lens_output()),(size,size))
        pixels=f.pixels(output)
        assert pixels.min()>=0 and pixels.max()<=1
        assert np.all(pixels==0) if value==0 else pixels[size//2,size//2,0]>.99
        # One K1 RG16F plus one RGBA16F lens output; no old-size allocations.
        assert d.bytes()==baseline_bytes+size*size*(4+8+8),('snapshot allocation leak',size,d.bytes(),baseline_bytes)
        g.bind_fbo(0x8D40,0);d.drop(handle)
    d.lens_off();assert d.bytes()==baseline_bytes+64*64*4
    before=state();d.lens_off_probe();assert state()==before
    d.lens_finish();assert d.bytes()==baseline_bytes
    print('PASS production helper snapshot reallocation/rearm 64 -> 128 -> 96 -> 64 without OFF; exact live byte count, zero/one output, OFF probe inert',flush=True)
    for a,t in ((gather,gt),(comp,ct),(fish,ft),(reference,rt),(referenceGather,rgt)):f.destroy(a,t)
    assert d.bytes()==0
    print('PASS actual production C++ renderActorKeyLens + texture/framebuffer RAII + actual LLRenderTarget: both gather->composite->fisheye chains, first activation, every attachment, source parity, GL bindings/draw buffers/viewport restored, missing/stale refused, OFF resources zero',flush=True)


def seam_cases(f,u):
    # Hand-picked one-pixel seam candidates: bright/dark reversal and exact tie.
    # Compare per-channel coverage against a separately calculated D52 oracle.
    cc=f.shader(1,True,True);out,ot=f.mrt([0x881A])
    y,x=np.mgrid[:N,:N];left=x<32
    for reverse in (False,True):
        for tie in (False,True):
            a=np.zeros((N,N,4),np.float32);b=a.copy();a[:,:,3]=b[:,:,3]=.5
            a[:,:,:3]=np.where(left[:,:,None],np.array([.1,.8,.4]),np.array([.9,.2,.4]))
            b[:,:,:3]=a[:,:,:3]
            if reverse:a[:,:,:3]=1-a[:,:,:3];b[:,:,:3]=a[:,:,:3]
            if tie:a[:,:,:3]=b[:,:,:3]=.4
            cov=np.repeat(left[:,:,None].astype(np.float32),4,axis=2)
            ta=f.texture(a,0x881A);tb=f.texture(b,0x881A);tc=f.texture(cov,0x881A)
            zero=f.texture(np.zeros_like(a),0x881A)
            cu=u|{'diopter_shape2':(0.,.07,0.,0.),'diopter_comp':(4.,1.,0.,0.),'diopter_comp2':(0.,0.,1.,0.),'diffuseMap':2}
            t=[ta,tb,zero,zero,tc,tc]
            baseline=None
            for threshold in (0.,2.):
                for ghosts in (0.,6.):
                    f.draw(cc,out,list((cu|{'diopter_ghost':(ghosts,.7,.5,.5),'diopter_ghost2':(threshold,.1,.6,2.)}).items()),t)
                    got=f.pixels(out)
                    if baseline is None:baseline=got.copy()
                    else:assert np.array_equal(got,baseline),'halo added coverage'
            # At x=31, shifted taps at 35 and 27 average .5; local is 1.
            shifted=.5*(a[32,35,:3]+a[32,27,:3]);local=a[32,31,:3]
            expected=np.where(shifted>local,.5,1.)
            assert np.max(abs(baseline[32,31,:3]-expected))<.002,(reverse,tie,baseline[32,31],expected)
            assert abs(baseline[32,31,3]-expected[1])<.002,'shadow must follow G winner'
    f.destroy(out,ot)
    print('PASS D52 seam: reversed beauty candidates, ties -> local, fractional mask, channel winners; halo below/above threshold contributes zero coverage',flush=True)

if __name__=='__main__':main()

