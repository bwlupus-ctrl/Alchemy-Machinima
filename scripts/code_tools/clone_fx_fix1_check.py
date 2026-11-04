"""Additional permanent regression assertions for clone FX fix round 1."""
import ctypes as C
import hashlib
import json
import numpy as np
from clone_looks_gl_check import Runner, F, V, SH, ROOT
import relight_3d_gl as G

EYE = SH+'avatar/eyeballActorGhostV.glsl'
SYSTEM = SH+'avatar/avatarActorGhostV.glsl'


def source_assertions(tree):
    blocks=[]
    for path in (V,SYSTEM,EYE):
        s=tree.read(path)
        block=s.split('// BEGIN STACKABLE CLONE FX VERTEX')[1].split('// END STACKABLE CLONE FX VERTEX')[0]
        assert 'sin(' not in block and 'floatBitsToUint(p)' in block
        assert s.count('flat out int vary_fx_body;')==1
        for name in ('ghostAux','ghostFx2','ghostFx3','ghostTime','projection_matrix'):
            import re
            assert len(re.findall(r'uniform\s+\w+\s+'+name+r'\s*;',s))==1,(path,name)
        blocks.append(block)
    assert blocks[0]==blocks[1]==blocks[2], 'three integer hash/decision implementations must stay identical'
    fs=tree.read(F);main=fs.split('void main()')[1]
    capture = 'fxFootprintZ = fwidth(vary_object_position.z);'
    guard = 'if (ghostFx2.x > 0.0 || ghostFx2.w > 0.0)'
    assert main.index(guard) < main.index(capture) < main.index('discard;')
    assert main.count('fwidth(vary_object_position.z)') == 1
    assert main.split(guard)[1].split('}')[0].strip() == '{\n        '+capture
    assert '#if' not in main[:main.index('float fxFootprintZ')], 'FX footprint missing in a permutation'
    fx=main.split('float cloneFxDim')[1].split('vec2 uv =')[0]
    assert 'fwidth(' not in fx and 'dFdx(' not in fx and 'dFdy(' not in fx
    assert 'if (vary_fx_body == 2) P.z = sl_eye;' in fx
    assert 'if (vary_fx_body == 1 && ghostFx2.y > 0.0)' in fx
    assert 'cloneFxVertex(pos, 2);' in tree.read(EYE)
    assert 'vary_object_position = position;' in tree.read(EYE)
    assert 'float fxHeight = body == 2 ? 1.762 : position.z;' in blocks[0]
    return hashlib.sha256(blocks[0].encode()).hexdigest()


class EyeTree:
    def __init__(self,tree):self.tree=tree
    def read(self,path):
        s=self.tree.read(path)
        if path==EYE:s+='\nvoid calcAtmospherics(vec3 p) {}\n'
        return s


def check(out=None, gl=None, tree=None):
    from clone_looks_safety_check import RealAvatar, read_llm, CHAR, save, transform, SEEDS
    out=out or ROOT/'.tmp/looks16/renders/clone_fx_fix1'
    out.mkdir(parents=True,exist_ok=True)
    tree=tree or G.SourceTree('WORKTREE')
    result=dict(vertex_block_sha256=source_assertions(tree))
    # Additional link assertions cover the remaining indexed/key permutations.
    link=Runner(64,64,gl)
    world=('GHOST_WORLD_PASS','GHOST_SHARED_DISSOLVE')
    for defs,vertex in [
        (world+('GHOST_INDEXED_WORLD',),V),
        (('GHOST_WORLD_PASS',),V),
        (('AL_KEY_COVERAGE_OUT',),V),
        (('AL_KEY_COVERAGE_OUT','HAS_SKIN'),V),
        (world+('AL_KEY_COVERAGE_OUT',),SYSTEM),
        (world+('AL_KEY_COVERAGE_OUT',),EYE)]:
        program=link.program(tree,defs,vertex)
        assert np.isfinite(link.render(program,1,fx2=(.2,.2,.2,.2),fx3=(.2,.2))).all(),('FX link permutation',defs,vertex)
        link.gl.fn('glPolygonMode',None,link.gl.U,link.gl.U)(0x0408,0x1B01)
        assert np.isfinite(link.render(program,1,fx2=(.2,.2,.2,.2),fx3=(.2,.2))).all(),('wire FX link',defs,vertex)
        link.gl.fn('glPolygonMode',None,link.gl.U,link.gl.U)(0x0408,0x1B02)
        link.gl.fn('glDeleteProgram',None,link.gl.U)(program)
    result['extra_key_indexed_wire_links']=6
    # Run the actual rigid-eye vertex program on the native LLM eye geometry.
    r=Runner(640,640,gl);r.production_avatar_basis=True
    r.rest_basis=np.array([[0,-1,0,0],[1,0,0,0],[0,0,1,0],[0,0,0,1]],np.float32)
    r.camera_center_z=1.762
    eye=read_llm(CHAR/'avatar_eye.llm')
    mesh=np.c_[eye['coords'],eye['normals'],eye['uv']].astype(np.float32)[eye['faces'].reshape(-1)]
    ep=r.program(EyeTree(tree),(),EYE)
    bp=r.program(tree,('HAS_SKIN',))
    body=mesh.copy();body[:,2]+=1.762
    opts=dict(mesh=mesh,object_matrix=transform((0,0,1.762)),scale=6.,mask=False,opacity=1.)
    def center(a):
        y,x=np.nonzero(a[...,3]>.1)
        assert len(x)>20,'eye frame unexpectedly empty'
        return np.array([x.mean(),y.mean()])
    baseline=r.render(ep,1,**opts)
    bbase=r.render(bp,1,mesh=body,scale=6.,mask=False,opacity=1.)
    assert np.linalg.norm(center(baseline)-center(bbase)) < .1,'eye/head test must share projected geometry'
    deltas=[]
    for t in (.17,.58,.99,1.40,1.81,2.22):
        a=r.render(ep,1,time=t,fx2=(1,0,0,0),**opts)
        # Head-slab reference: keep displacement but remove only the gap in memory.
        class NoGap:
            def read(self,path):
                s=tree.read(path)
                if path==F:s=s.replace('if (gap >= fxFootprintZ &&', 'if (false && gap >= fxFootprintZ &&')
                if path==V:s=s.replace('floor(fxHeight / 0.22)', 'floor(1.762 / 0.22)')
                return s
        if not deltas:head=r.program(NoGap(),('HAS_SKIN',))
        b=r.render(head,1,time=t,fx2=(1,0,0,0),mesh=body,scale=6.,mask=False,opacity=1.)
        assert np.linalg.norm((center(a)-center(baseline))-(center(b)-center(bbase))) < .1,('eyes must follow head slab exactly',t)
        deltas.append(float(np.linalg.norm(center(a)-center(baseline))))
    assert max(deltas)>5,'head-slab displacement fixture never moved'
    assert not np.any(r.render(ep,1,fx2=(0,0,0,.97),**opts)), 'Wipe .97 leaves floating eyes'
    assert np.array_equal(baseline,r.render(ep,1,fx2=(0,0,0,.5),**opts)), 'Wipe below eyes removed them'
    for t in np.arange(0,3,.125):
        assert np.array_equal(baseline,r.render(ep,1,time=float(t),fx2=(0,1,0,0),**opts)), 'Dropout affects eyes'
    result['eyes']=dict(slice_matches_head_pixels=True,slice_displacement_px=deltas,wipe_097_empty=True,dropout_disabled=True)
    # Actual production fragment output over 12 seconds, including stepped frames.
    small=Runner(64,64,r.gl)
    p=small.program(tree,('HAS_SKIN',))
    amounts=[]
    for amount in (.125,.25):
        energy=[]
        for t in np.arange(0,12,1/60):
            a=small.render(p,1,time=float(t),effect_fps=24,fx2=(0,0,amount,0),scale=.5,mask=False)
            assert np.any(a[...,3]),('low flicker fully empty',amount,float(t))
            energy.append(float(a[...,:3].sum()))
        assert min(energy)<max(energy), 'low flicker never dims'
        amounts.append(min(energy)/max(energy))
    assert amounts[1]<amounts[0], 'flicker dim depth must increase with amount'
    result['low_flicker_min_energy_ratios']=amounts
    # A coarse front-facing rest-space plane has a known Z footprint. Disable
    # vertex movement ONLY to isolate whether low Slice invents subpixel holes.
    class FixedVertex:
        def read(self,path):return tree.read(path).replace('pos.xyz += ghostFxLateral * offset;','pos.xyz += ghostFxLateral * 0.0;')
    gp=r.program(FixedVertex(),('HAS_SKIN',))
    plane=np.array([[0,-.4,.2,1,0,0,0,0],[0,.4,.2,1,0,0,1,0],[0,.4,1.7,1,0,0,1,1],
                    [0,-.4,.2,1,0,0,0,0],[0,.4,1.7,1,0,0,1,1],[0,-.4,1.7,1,0,0,0,1]],np.float32)
    r.camera_center_z=.95
    a=r.render(gp,1,mesh=plane,mask=False)
    b=r.render(gp,1,mesh=plane,mask=False,fx2=(.02,0,0,0))
    assert np.array_equal(a,b),'Slice .02 produced unresolved dotted gap'
    result['slice_002_no_subpixel_holes']=True
    # Placement render at both requested scales and 37-degree world yaw. The
    # reference translates the same real mesh by the measured shader offset.
    r.camera_center_z=0.
    lateral_rows=[]
    for scale in (.5,2.):
        yaw=np.radians(37);m=transform(rot=(0,0,37),scale=(scale,)*3)
        local=np.array([0,1,0.]);world=m[:3,:3]@local
        assert abs(np.linalg.norm(world)-scale)<1e-6
        assert abs(np.dot(world,m[:3,:3]@np.array([1,0,0.])))<1e-6
        # Diagnostic FS publishes displaced eye-space position on native geometry.
        class Position:
            def read(self,path):
                if path==F:return 'in vec3 vary_position; out vec4 frag_color; void main(){frag_color=vec4(vary_position,1);}'
                return tree.read(path)
        dp=r.program(Position(),('HAS_SKIN',))
        sample=body.copy();sample[:,2]-=1.762
        kw=dict(mesh=sample,object_matrix=m,scale=8.,mask=False)
        z=r.render(dp,1,**kw)
        moved=None
        for t in np.arange(0,4,1/24):
            q=r.render(dp,1,time=float(t),fx3=(0,1),**kw)
            if not np.array_equal(z,q):moved=q;break
        assert moved is not None
        # Subtract projected screen coordinates: motion must be a bounded
        # translation on the yawed lateral vector, including its depth component.
        za=z[z[...,3]>0,:3].mean(axis=0);qa=moved[moved[...,3]>0,:3].mean(axis=0)
        delta=qa-za
        # Raster centroid quantization affects x/y; z is constant on this plane
        # only approximately, so allow one pixel in eye space for projection.
        view=np.array([[0,-1,0],[0,0,1],[-1,0,0]],np.float32)
        axis=view@world
        scalar=float(np.dot(delta,axis)/np.dot(axis,axis))
        residual=np.linalg.norm(delta-scalar*axis)
        assert abs(scalar)<=.035+1/(8*r.projection_x*r.width*.5),('double scaled jitter',scale,scalar)
        assert residual<.002,('jitter not lateral at yaw',scale,delta,axis,residual)
        save(moved,out/f'lateral_scale_{scale:g}_yaw37_position.png')
        lateral_rows.append(dict(scale=scale,yaw=37,lateral_world=world.tolist(),rendered_offset_m=scalar,residual_m=float(residual)))
    result['placement']=lateral_rows
    # Large native body frame, with the ACTUAL eye program composited at its
    # native joint transforms. This fixture does not relabel skinned eyes.
    class RealEyes(RealAvatar):
        def draw_avatar(self,pr,look,view,proj,phase):
            only=self.only
            self.only={b['name'] for b in self.batches if not b['name'].startswith('mEye') and (only is None or b['name'] in only)}
            super().draw_avatar(pr,look,view,proj,phase)
            self.only=only
            g=self.gl;g.use(self.eye_program)
            loc=lambda n:g.loc(self.eye_program,n.encode())
            for name,(integer,values) in self.render_uniforms.items():
                (g.ui if integer else g.uf[len(values)])(loc(name),*values)
            for joint in ('mEyeLeft','mEyeRight'):
                if only is not None and joint not in only:continue
                mv=view@self.skeleton.world(self.pose)[joint]
                for name,mat in (('modelview_matrix',mv),('modelview_projection_matrix',proj@mv),('projection_matrix',proj),('texture_matrix0',np.eye(4))):
                    a=np.ascontiguousarray(mat.T,dtype=np.float32);g.matrix(loc(name),1,0,a.ctypes.data_as(C.POINTER(g.F)))
                n=np.ascontiguousarray(np.linalg.inv(mv[:3,:3]),dtype=np.float32);self.mat3(loc('normal_matrix'),1,0,n.ctypes.data_as(C.POINTER(g.F)))
                g.bind_tex(0x0DE1,self.batches[0]['texture']);g.bind_buf(0x8892,self.eye_vbo)
                for name,count,offset in (('position',3,0),('normal',3,12),('texcoord0',2,24)):
                    ix=g.attr_loc(self.eye_program,name.encode())
                    if ix>=0:g.enable_attr(ix);g.attr_ptr(ix,count,0x1406,0,32,C.c_void_p(offset))
                g.enable(0x0B71);g.fn('glDepthFunc',None,g.U)(0x0203)
                g.draw(0x0004,0,len(mesh))
            g.disable(0x0B71);g.use(pr)
    real=RealEyes(1200,1400,r.gl);real.eye_program=Runner.program(real,EyeTree(tree),(),EYE)
    vbo=real.gl.U();real.gl.gen_buf(1,C.byref(vbo));real.gl.bind_buf(0x8892,vbo)
    real.gl.buffer_data(0x8892,mesh.nbytes,mesh.ctypes.data,0x88E4);real.eye_vbo=vbo
    rp=real.program(tree,('HAS_SKIN',))
    real.only={'head','upper_body','lower_body','mEyeLeft','mEyeRight'}
    real.only={'mEyeLeft','mEyeRight'}
    neutral=real.frame(rp,1,scale=.92)
    def eye_x(a):
        y,x=np.nonzero(a[...,3]>.1)
        assert len(x)>0,'real eye shader output missing'
        return x.mean()
    base_x=eye_x(neutral)
    candidates=[.17+.41*i for i in range(25)]
    slice_time=max(candidates,key=lambda value:abs(eye_x(real.frame(rp,1,time=value,fx2=(1,0,0,0),scale=.92))-base_x))
    result['slice_render_time']=slice_time
    real.only={'head','upper_body','lower_body','mEyeLeft','mEyeRight'}
    for name,fx,time in [('wipe_05_edge',(0,0,0,.5),.58),('wipe_097_no_floating_eyes',(0,0,0,.97),.58),('slice_1_head_slab',(1,0,0,0),slice_time)]:
        frame=real.frame(rp,1,time=time,fx2=fx,scale=.92)
        assert np.isfinite(frame).all()
        save(frame,out/(name+'.png'))
    # Choose a real stepped blackout window, then render .25 at its midpoint.
    real.only={'upper_body'}
    hit=next(float(value) for value in np.arange(0,12,1/480)
             if .21 <= ((value+SEEDS[0]*.73)*2)%1 <= .23
             and not np.any(real.frame(rp,1,time=float(value),fx2=(0,0,1,0),scale=.92)))
    real.only={'head','upper_body','lower_body','mEyeLeft','mEyeRight'}
    frame=real.frame(rp,1,time=hit,fx2=(0,0,.25,0),scale=.92)
    assert np.any(frame[...,3]),'real .25 flicker mid-window empty'
    save(frame,out/'power_025_mid_window.png')
    result['power_render_time']=hit
    # Close views retain native geometry and shader programs.
    real.camera_center_z=.94
    save(real.frame(rp,1,fx2=(0,0,0,.5),scale=2.8),out/'wipe_05_edge_close.png')
    real.camera_center_z=1.76
    save(real.frame(rp,1,time=slice_time,fx2=(1,0,0,0),scale=2.8),out/'slice_1_head_close.png')
    save(real.frame(rp,1,fx2=(0,0,0,.97),scale=2.8),out/'wipe_097_head_close.png')
    for clone_scale in (.5,2.):
        real.camera_center_z=sum(real.pose_bounds['bind'])*.5*clone_scale
        options=dict(object_matrix=transform(rot=(0,0,37)),clone_scale=clone_scale,scale=.92/max(1.,clone_scale))
        save(real.frame(rp,1,**options),out/f'avatar_scale_{clone_scale:g}_yaw37_reference.png')
        save(real.frame(rp,1,time=float(t),fx3=(0,1),**options),out/f'avatar_scale_{clone_scale:g}_yaw37_jitter.png')
    (out/'fix1_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS fix1: identical integer decisions; eye Slice/Wipe/Dropout; low flicker; subpixel gap; scale/yaw rendered checks',flush=True)
    return result

if __name__=='__main__':check()
