"""Permanent real-head foreign bind-frame regression; production WGL, no viewer build.

The imported head is the real avatar_head.llm. Only its storage frame and
inverse binds change. Normal and foreign vertices therefore have identical
posed geometry. This tests the complete production shader, including the
stored-position varying and vertex Slice/Interlace reads. It is not an
in-world collector or driver-state test.
"""
import argparse
import ctypes as C
import json
import re
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, ImageDraw

import relight_3d_gl as G
from clone_looks_gl_check import ROOT, V, F
from clone_looks_safety_check import RealAvatar, Skeleton, TIMES, VIEWS, SEEDS, discover_looks
from clone_fx_safety_check import EFFECTS, knobs

OUT = ROOT/'.tmp/rest_frame/renders'
# Fixed before rendering. Float32 foreign positions/palettes can produce
# tiny quantization/interpolation differences; these limits never move.
LIMITS = dict(rgb_mae=.003, rgb_p99=.04, alpha_mae=.002, position_max=.00005)
SELECTED = (79, 93, 87, 100, 94, 69, 88, 89)


def default_world(nodes, name):
    """Viewer rule: sum DEFAULT positions of real normal joints, no pose/shape."""
    p = np.zeros(3, dtype=np.float32)
    while name:
        p += np.asarray(nodes[name]['pos'], np.float32)
        name = nodes[name]['parent']
    return p


def correction(inv_bind, default_position):
    """ALGhostRestFrame::compute, using numpy's conventional column vectors.

    LLMatrix4a's flat storage is the transpose of this representation:
    x*m[0] + y*m[1] + z*m[2] + m[3]. Translation composition only changes
    the last column because the normal-bone default rotations are identity.
    """
    inv_bind = np.asarray(inv_bind, np.float32)
    zero = np.zeros((3,4), np.float32)
    if not np.isfinite(inv_bind).all() or not np.isfinite(default_position).all():
        return zero
    m = inv_bind.copy()
    with np.errstate(over='ignore', invalid='ignore'):
        m[:3,3] += np.asarray(default_position, np.float32)
    if not np.isfinite(m[:3,3]).all():
        return zero
    columns = m[:3,:3].astype(np.float64).T
    lengths = np.linalg.norm(columns, axis=1)
    if lengths.min() <= 0 or lengths.max() > 1.05*lengths.min():
        return zero
    for i,j in ((0,1),(0,2),(1,2)):
        if abs(np.dot(columns[i],columns[j])) > .05*lengths[i]*lengths[j]:
            return zero
    linear_delta=m[:3,:3]-np.eye(3,dtype=np.float32)
    if np.all(np.abs(linear_delta) <= np.float32(.2)):
        return zero
    return np.asarray(m[:3]-np.eye(4,dtype=np.float32)[:3], np.float32)


def mesh_delta(names, inverse_binds, nodes):
    eligible = [n for n in names if n.startswith('m') and n in nodes and n in inverse_binds]
    if not eligible:
        return np.zeros((3,4),np.float32), None
    name = 'mPelvis' if 'mPelvis' in eligible else eligible[0]
    return correction(inverse_binds[name], default_world(nodes,name)), name


def frames():
    def make(a):
        m = np.eye(4,dtype=np.float32); m[:3,:3] = a
        return m
    # Matrices map STORED to canonical: Y is stored up, but lateral differs.
    return dict(head_cm=make(.01*np.array([[0,0,1],[1,0,0],[0,1,0]])),
                cuffs_cm=make(.01*np.array([[1,0,0],[0,0,-1],[0,1,0]])),
                y_up_metres=make(np.array([[0,0,1],[1,0,0],[0,1,0]])),
                mirrored=make(np.diag([1,-1,1])))


def custom_pose_cases(nodes):
    # Real inv0_move values from the 243-line worn-mesh diagnostic. The lead
    # requested identity rotation with these logged unit lengths; the log
    # itself omits the full 3x3. Keep these fixture values independent
    # of the generated audit JSON so a fresh checkout can run the gate.
    for name,joint,move in (
        ('hair_hand', 'mHandThumb1Left', (.513169,.090744,-1.1683)),
        ('dreamer_right', 'mElbowRight', (.035859,-.711574,-1.192)),
        ('dreamer_left', 'mShoulderLeft', (.036295,.959503,-1.19203)),
        ('armband_right', 'mElbowRight', (.035859,-.711574,-1.192)),
        ('armband_left', 'mElbowLeft', (.035859,.711574,-1.192))):
        ib=np.eye(4,dtype=np.float32);ib[:3,3]=move
        yield name,ib,default_world(nodes,joint)


def old_translation_rule(inv_bind,world):
    """Planted previous rule for the identity-linear custom-pose fixtures."""
    m=inv_bind.copy();m[:3,3]+=world
    assert np.array_equal(m[:3,:3],np.eye(3,dtype=np.float32))
    return np.zeros((3,4),np.float32) if np.linalg.norm(m[:3,3].astype(np.float64))<.5 else m[:3]-np.eye(4,dtype=np.float32)[:3]


def rule_checks(skeleton):
    nodes=skeleton.nodes
    assert all(np.array_equal(n['rot'],[0,0,0]) for n in nodes.values()), 'normal bone default rotation changed'
    p=default_world(nodes,'mPelvis')
    def inverse(m):
        ib=m.copy();ib[:3,3]-=p;return ib
    cases=[]
    for name,m in frames().items():
        ib=inverse(m);delta=correction(ib,p)
        assert np.any(delta), (name,'foreign frame ignored')
        np.testing.assert_allclose(delta,m[:3]-np.eye(4)[:3],atol=2e-7)
        cases.append(dict(name=name,inv_bind_flat=ib.T.reshape(-1).tolist(),default_world=p.tolist(),delta=delta.reshape(-1).tolist()))
    for scale in (.93,1.,1.08):
        m=np.eye(4,dtype=np.float32);m[:3,:3]*=scale;m[:3,3]=(.1,-.1,.1)
        ib=inverse(m);delta=correction(ib,p)
        assert not np.any(delta), ('normal frame rewritten',scale)
        cases.append(dict(name=f'normal_{scale}',inv_bind_flat=ib.T.reshape(-1).tolist(),default_world=p.tolist(),delta=delta.reshape(-1).tolist()))
    custom=[]
    for name,ib,world in custom_pose_cases(nodes):
        delta=correction(ib,world)
        assert not delta.view(np.uint32).any(),(name,'custom pose must return positive zero bits')
        planted=old_translation_rule(ib,world)
        assert np.max(np.abs(planted[:,3]))>.7,(name,'old translation rule control escaped')
        custom.append(dict(name=name,translation_length=float(np.linalg.norm(planted[:,3]))))
        cases.append(dict(name=name,inv_bind_flat=ib.T.reshape(-1).tolist(),default_world=world.tolist(),delta=delta.reshape(-1).tolist()))
    for scale in (.93,1.08):
        ib=np.diag([scale,scale,scale,1.]).astype(np.float32);ib[:3,3]=(.035859,-.711574,-1.192)
        world=default_world(nodes,'mElbowRight');delta=correction(ib,world)
        assert not delta.view(np.uint32).any(),('near-identity custom pose rewritten',scale)
        cases.append(dict(name=f'custom_pose_scale_{scale}',inv_bind_flat=ib.T.reshape(-1).tolist(),default_world=world.tolist(),delta=delta.reshape(-1).tolist()))
    # A changed axis frame must retain translation, including metre/Y-up and
    # mirrored frames. The identity exemption must not discard their origins.
    for name,m in frames().items():
        m[:3,3]=(.25,-1.123574,.329)
        ib=inverse(m);delta=correction(ib,p)
        np.testing.assert_allclose(delta,m[:3]-np.eye(4)[:3],atol=2e-7)
        cases.append(dict(name=name+'_translated',inv_bind_flat=ib.T.reshape(-1).tolist(),default_world=p.tolist(),delta=delta.reshape(-1).tolist()))
    for why,m in [('anisotropic',np.diag([.01,.02,.01,1.])),
                  ('shear',np.array([[1,.1,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1.]])),
                  ('nonfinite',np.full((4,4),np.nan)),('singular',np.zeros((4,4)))]:
        ib=inverse(m);delta=correction(ib,p)
        assert not np.any(delta),why
        if np.isfinite(ib).all():
            cases.append(dict(name=why,inv_bind_flat=ib.T.reshape(-1).tolist(),default_world=p.tolist(),delta=delta.reshape(-1).tolist()))
    for name,m,world in [('large_finite',np.diag([1e30,1e30,1e30,1.]).astype(np.float32),np.zeros(3,np.float32)),
                         ('large_anisotropic',np.diag([1e30,2e30,1e30,1.]).astype(np.float32),np.zeros(3,np.float32)),
                         ('translation_overflow',np.eye(4,dtype=np.float32),np.full(3,np.finfo(np.float32).max,np.float32))]:
        if name=='translation_overflow':m[:3,3]=np.finfo(np.float32).max
        delta=correction(m,world)
        assert np.any(delta) if name=='large_finite' else not np.any(delta)
        cases.append(dict(name=name,inv_bind_flat=m.T.reshape(-1).tolist(),default_world=world.tolist(),delta=delta.reshape(-1).tolist()))
    boundary=np.eye(4,dtype=np.float32)
    c=np.sqrt(np.float32(.96));s=np.float32(.2)
    boundary[:2,:2]=[[c,-s],[s,c]]
    delta=correction(boundary,np.zeros(3,np.float32))
    assert not np.any(delta),'float32 .2 rotation boundary must remain exactly inert'
    cases.append(dict(name='near_rotation_boundary',inv_bind_flat=boundary.T.reshape(-1).tolist(),default_world=[0.,0.,0.],delta=delta.reshape(-1).tolist()))
    m=frames()['head_cm'];ib=inverse(m)
    collision=np.diag([8.3,20,19.7,1.]).astype(np.float32)
    delta,j=mesh_delta(['R_FOOT','mHead','mPelvis'],{'R_FOOT':collision,'mHead':np.eye(4),'mPelvis':ib},nodes)
    assert j=='mPelvis' and np.array_equal(delta,correction(ib,p)), 'pelvis preference / collision exclusion'
    delta,j=mesh_delta(['CHEST','mMissing','mHead'],{'CHEST':collision,'mMissing':ib,'mHead':ib},nodes)
    assert j=='mHead', 'fallback joint selection'
    delta,j=mesh_delta(['CHEST'],{'CHEST':collision},nodes)
    assert j is None and not np.any(delta), 'collision-only mesh corrected'
    # Planted loose identity threshold must fail the independent exact-zero oracle.
    normal=np.diag([.93,.93,.93,1.]).astype(np.float32)
    planted=normal[:3]-np.eye(4)[:3]
    assert np.any(planted) and not np.any(correction(inverse(normal),p))
    return dict(numeric_cases=cases,normal_scales_exact_zero=[.93,1.,1.08],
                custom_pose_exact_zero=custom,old_translation_rule_control='all five rejected by exact-zero oracle',
                invalid_rejected=['anisotropic','shear','nonfinite','singular'],
                joint_selection='pelvis, fallback mHead; collision-only rejected',
                loose_threshold_control='nonzero .93 delta rejected by exact-zero oracle')


def numeric_cases():
    """Shared numbers for the native pure C++ helper parity executable."""
    return rule_checks(Skeleton())['numeric_cases']


class ForeignAvatar(RealAvatar):
    def __init__(self,width,height,gl,matrix=None):
        super().__init__(width,height,gl)
        self.foreign_matrix=np.eye(4,dtype=np.float32) if matrix is None else matrix
        self.head=next(b for b in self.batches if b['name']=='head')
        b=self.head;g=self.gl
        data=np.empty((b['count'],12),np.float32)
        g.bind_buf(0x8892,b['vbo'])
        g.fn('glGetBufferSubData',None,g.U,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x8892,0,data.nbytes,data.ctypes.data)
        self.canonical_data=data.copy()
        inv=np.linalg.inv(self.foreign_matrix)
        data[:,:3]=(np.c_[data[:,:3],np.ones(len(data))]@inv.T)[:,:3]
        # Production skins the raw normal with the palette. Invert that same
        # linear map here; no object-space normal enters actorghostF.
        data[:,3:6]=data[:,3:6]@inv[:3,:3].T
        g.buffer_data(0x8892,data.nbytes,data.ctypes.data,0x88E4)
        b['points']=(np.c_[b['points'],np.ones(len(b['points']))]@inv.T)[:,:3]
        b['inverse_binds']={n:np.linalg.inv(self.skeleton.bind[n])@self.foreign_matrix for n in b['names'] if n}
        # Real mesh skin info may include pelvis even when its vertex weights
        # happen to be zero: the joint-choice rule must remain independent.
        b['inverse_binds']['mPelvis']=np.linalg.inv(self.skeleton.bind['mPelvis'])@self.foreign_matrix
        b['rest_delta'],self.chosen_joint=mesh_delta(['R_FOOT','mPelvis',*b['names']],b['inverse_binds'],self.skeleton.nodes)
        self.correct_delta=b['rest_delta'].copy()
        self.stored_data=data


class VertexCapture:
    """Production vertex outputs at corresponding IDs, before rasterization.

    Separate storage frames produce sub-micro clip-coordinate rounding changes.
    At grazing triangles a shared screen pixel can sample DIFFERENT material
    points. Transform feedback measures the rest field at the same vertices;
    production framebuffer image parity remains an independent full-shader gate.
    """
    def __init__(self,runner,program):
        self.runner,self.program=runner,program
        g=runner.gl
        varyings=(C.c_char_p*2)(b'vary_object_position',b'gl_Position')
        g.fn('glTransformFeedbackVaryings',None,g.U,g.I,C.POINTER(C.c_char_p),g.U)(program,2,varyings,0x8C8C)
        g.link(program);ok=g.I();g.getprog(program,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);g.plog(program,len(log),None,log)
            raise AssertionError('vertex feedback link: '+log.value.decode())

    def capture(self,time,opts):
        r,pr=self.runner,self.program;g=r.gl
        assert r.only=={'head'},'feedback requires exactly the corresponding head batch'
        # RealAvatar submits one depth prime and one colour pass. Capture both
        # and initialize with NaNs so a missing/short draw cannot pass silently.
        output=np.full((2*r.head['count'],7),np.nan,np.float32)
        buffer=g.U();g.gen_buf(1,C.byref(buffer));g.bind_buf(0x8C8E,buffer)
        g.buffer_data(0x8C8E,output.nbytes,output.ctypes.data,0x88E1)
        bind=g.fn('glBindBufferBase',None,g.U,g.U,g.U)
        bind(0x8C8E,0,buffer);g.use(pr);g.enable(0x8C89)
        use=g.use
        def same_program(candidate):
            # glUseProgram itself is forbidden during active feedback, even
            # when reselecting this program. Assert then omit redundant binds.
            assert candidate==pr,('feedback program changed',candidate,pr)
        g.use=same_program
        try:
            g.fn('glBeginTransformFeedback',None,g.U)(0x0004)
            try:r.frame(pr,1,time=time,**opts)
            finally:g.fn('glEndTransformFeedback',None)()
            g.bind_buf(0x8C8E,buffer)
            g.fn('glGetBufferSubData',None,g.U,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x8C8E,0,output.nbytes,output.ctypes.data)
            assert g.fn('glGetError',g.U)()==0,'vertex feedback GL error'
            assert np.isfinite(output).all(),'missing/nonfinite feedback vertex output'
            return output
        finally:
            g.use=use;g.disable(0x8C89);bind(0x8C8E,0,0)
            g.fn('glDeleteBuffers',None,g.I,C.POINTER(g.U))(1,C.byref(buffer))


def metrics(a,b,mask):
    assert np.isfinite(a).all() and np.isfinite(b).all(), 'nonfinite rest-frame render'
    if not np.any(mask):
        # Wipe at one and Power Flicker blackout legitimately submit no
        # visible pixels. Require EXACT whole-frame parity in that case.
        assert np.array_equal(a,b),'empty silhouette must be bit-identical'
        return dict(rgb_mae=0.,rgb_p99=0.,alpha_mae=0.)
    d=np.abs(a-b)
    return dict(rgb_mae=float(d[mask,:3].mean()),rgb_p99=float(np.quantile(d[mask,:3],.99)),
                alpha_mae=float(d[...,3].mean()))


def failures(m):
    return [key for key in ('rgb_mae','rgb_p99','alpha_mae') if m[key]>LIMITS[key]]


def upload_contract(source):
    apply=source.split('auto apply_program =',1)[1].split('apply_program(shader);',1)[0]
    assert 'const ALGhostRestFrame::Delta zero_rest{};' in apply
    assert 'uniform4fv(sGhostRestDelta, 3, zero_rest.data())' in apply
    switch=source.split('if (batch_shader != active_shader)',1)[1].split('cur_cutoff =',1)[0]
    assert 'apply_program(batch_shader)' in switch and 'rest_bound = false;' in switch
    upload=source.split('const std::pair<LLUUID, U64> rest_mesh',1)[1].split('const bool is_mask',1)[0]
    assert '!rest_bound || rest_mesh != last_rest_mesh' in upload
    assert 'ghost_rest_delta(di->mAvatar, di->mSkinInfo)' in upload
    assert 'uniform4fv(sGhostRestDelta, 3, delta.data())' in upload
    assert source.index('const std::pair<LLUUID, U64> rest_mesh') > source.index('if (!palette_ok')


def source_checks():
    """Additional static guard; GL fixtures separately exercise real uniforms."""
    source=(ROOT/'indra/newview/llactormover.cpp').read_text(encoding='utf-8')
    shader=(ROOT/V).read_text(encoding='utf-8')
    assert 'uniform vec4 ghostRestDelta[3]' in shader
    assert 'vec3 rest = position;' in shader
    for row,basis in enumerate(('1.0, 0.0, 0.0, 0.0','0.0, 1.0, 0.0, 0.0','0.0, 0.0, 1.0, 0.0')):
        assert f'any(notEqual(ghostRestDelta[{row}], vec4(0.0)))' in shader
        assert f'dot(ghostRestDelta[{row}] + vec4({basis}), p)' in shader
    assert re.search(r'vary_object_position\s*=\s*rest\s*;',shader)
    assert 'ghostRestDelta' in source
    upload_contract(source)
    for before,after in [('rest_bound = false;', 'rest_bound = true;'),
                         ('uniform4fv(sGhostRestDelta, 3, zero_rest.data())','uniform4fv(sGhostRestDelta, 3, delta.data())'),
                         ('!rest_bound || rest_mesh != last_rest_mesh','rest_mesh != last_rest_mesh')]:
        # First reset is declaration; replace all resets to also poison switch.
        planted=source.replace(before,after)
        try:upload_contract(planted)
        except AssertionError:pass
        else:raise AssertionError('program-switch/upload static negative escaped: '+before)
    helper=(ROOT/'indra/newview/alghostrestframe.h').read_text(encoding='utf-8')
    assert 'std::abs(delta[r*4+c]) <= 0.2f' in helper
    assert 'return near_identity ? Delta{} : delta;' in helper
    assert 'translation_squared' not in helper
    # Exact source thresholds prevent a rewritten normal frame from being
    # silently accepted by an independently ported Python rule.
    planted=helper.replace('<= 0.2f','<= 0.02f')
    assert 'std::abs(delta[r*4+c]) <= 0.2f' not in planted
    return dict(shader_delta_rows=True,stored_position_uses_rest=True,
                program_switch='static: zero on apply_program, reset on switch, upload after either palette path',
                static_negative_controls=['omitted switch reset','omitted zero clear','omitted switch upload','loose normal rewrite threshold'])


def image(frame):
    rgb=frame[...,:3]+.035*(1-frame[...,3:4])
    return Image.fromarray((np.clip(rgb[::-1],0,1)*255+.5).astype(np.uint8))


class RawRestBaseline:
    """Pre-fix raw position vertex behavior, with current unrelated source."""
    def __init__(self,tree):self.tree=tree
    def read(self,path):
        source=self.tree.read(path)
        if path==V:
            prefix='    vec3 rest = position;\n#ifdef HAS_SKIN\n'
            assert source.count(prefix)==1
            start=source.index(prefix)+len(prefix);end=source.index('#endif',start)
            block=source[start:end]
            assert 'any(notEqual(ghostRestDelta[0]' in block
            assert 'dot(ghostRestDelta[2] + vec4(0.0, 0.0, 1.0, 0.0), p)' in block
            source=source[:start]+'    // Baseline: original raw stored position, no rest correction.\n'+source[end:]
        return source


def custom_pose_render_check(tree,gl):
    """The logged 1.17075 m bind offset must use raw rest coordinates exactly."""
    _,ib,world=next(c for c in custom_pose_cases(Skeleton().nodes) if c[0]=='dreamer_right')
    m=ib.copy();m[:3,3]+=world
    assert 1.17<float(np.linalg.norm(m[:3,3]))<1.18
    r=ForeignAvatar(384,320,gl,m)
    assert not r.correct_delta.view(np.uint32).any()
    r.only={'head'};r.pose='bind';r.camera_center_z=1.74
    raw=RawRestBaseline(tree)
    pr=r.program(tree,('HAS_SKIN',));baseline=r.program(raw,('HAS_SKIN',))
    probe=r.program(tree,('HAS_SKIN',),probe=True)
    raw_probe=r.program(raw,('HAS_SKIN',),probe=True)
    cases=[(str(look),look,{}) for look in discover_looks()]
    cases += [(effect,1,knobs(i,1.)) for i,effect in enumerate(EFFECTS)]
    count=0
    for angle in VIEWS.values():
        r.camera=angle
        for t in TIMES:
            for label,look,fx in cases:
                a=r.frame(baseline,look,time=t,scale=5.,**fx)
                b=r.frame(pr,look,time=t,scale=5.,**fx)
                assert np.array_equal(a,b),('custom pose differs from raw path',label,float(angle),t)
                count+=1
    r.camera=0.
    expected=r.frame(raw_probe,1,scale=5.)
    actual=r.frame(probe,1,scale=5.)
    assert np.array_equal(actual,expected),'custom pose varying differs from raw'
    mask=expected[...,3]>.99
    assert mask.sum()>1000,'custom pose fixture empty'
    r.head['rest_delta']=old_translation_rule(ib,world)
    planted=r.frame(probe,1,scale=5.)
    common=mask&(planted[...,3]>.99)
    assert common.any(),'planted custom pose fixture empty'
    err=float(np.max(np.abs(expected[common,:3]-planted[common,:3])))
    assert err>.1,('old translation rule escaped raw-path oracle',err)
    visible={}
    for label,look,fx in [('Glyph Skin',79,{}),('Cold Gaze',94,{}),('Slice',1,knobs(0,1.))]:
        a=r.frame(baseline,look,time=3.17,scale=5.,**fx)
        b=r.frame(pr,look,time=3.17,scale=5.,**fx)
        visible[label]=metrics(a,b,(a[...,3]>0)|(b[...,3]>0))
    assert any(failures(m) for m in visible.values()),'old translation rule escaped all visible-look parity limits'
    r.head['rest_delta']=r.correct_delta.copy()
    print('PASS custom-pose 1.17075 m translation: exact-zero delta; '+str(count)+
          ' raw-path images bit-identical; old translation rule rejected (position error '+str(err)+' m)',flush=True)
    return dict(translation=m[:3,3].tolist(),translation_length=float(np.linalg.norm(m[:3,3])),
                delta_exact_zero_bits=True,raw_path_bit_identical_cases=count,
                raw_varying_bit_identical=True,old_rule_rejected=True,
                old_rule_position_max=err,old_rule_visible_metrics=visible)


def check(out=None,gl=None):
    out=out or OUT;out.mkdir(parents=True,exist_ok=True)
    from clone_rest_frame_native_check import check as native_check
    native=native_check()
    tree=G.SourceTree('WORKTREE')
    normal=ForeignAvatar(384,320,gl)
    foreign={name:ForeignAvatar(384,320,normal.gl,m) for name,m in frames().items()}
    result=dict(renderer=normal.gl.renderer,limits=LIMITS,times=TIMES,views=VIEWS,
                rule=rule_checks(normal.skeleton),source=source_checks(),frames={},failures=[])
    result['native']=native
    result['custom_pose']=custom_pose_render_check(tree,normal.gl)
    result['views']={k:float(v) for k,v in VIEWS.items()}
    result['assumptions']=['Requested Doze interpreted as existing Ooze 100; no Doze look exists.']
    runners=[normal,*foreign.values()]
    programs=[r.program(tree,('HAS_SKIN',)) for r in runners]
    probes=[r.program(tree,('HAS_SKIN',),probe=True) for r in runners]
    captures=[VertexCapture(r,pr) for r,pr in zip(runners,probes)]
    result['varying_measurement']='GPU transform feedback at corresponding vertex IDs; production vertex shader unchanged'
    for r in runners:r.only={'head'}
    # Close-up samples preserve the production +X forward ground-origin rest
    # coordinates. Only the view's centre/zoom changes.
    for r in runners:r.camera_center_z=1.74
    opts=dict(scale=5.,phase=SEEDS[0])
    normal.pose='bind';normal.camera=0.
    reference=normal.frame(probes[0],1,**opts);mask=reference[...,3]>.99
    assert mask.sum()>1000,'head fixture empty'
    for index,(name,r) in enumerate(foreign.items(),1):
        err=0.;clip_error=0.;raster_error=0.;position_cases=0
        for pose in ('bind','raised','turned'):
            normal.pose=r.pose=pose
            for view,angle in VIEWS.items():
                normal.camera=r.camera=angle
                for t in TIMES:
                    reference=normal.frame(probes[0],1,time=t,**opts)
                    actual=r.frame(probes[index],1,time=t,**opts)
                    common=(reference[...,3]>.99)&(actual[...,3]>.99)
                    assert common.any(),(name,'empty varying probe',pose,view,t)
                    raster_error=max(raster_error,float(np.max(np.abs(reference[common,:3]-actual[common,:3]))))
                    expected_vertices=captures[0].capture(t,opts)
                    actual_vertices=captures[index].capture(t,opts)
                    current_error=float(np.max(np.abs(expected_vertices[:,:3]-actual_vertices[:,:3])))
                    clip_error=max(clip_error,float(np.max(np.abs(expected_vertices[:,3:]-actual_vertices[:,3:]))))
                    err=max(err,current_error);position_cases+=1
                    assert current_error<=LIMITS['position_max'],(name,'vary_object_position at matching vertices',pose,view,t,current_error)
        # Verify the fixture skinning algebra against every real head vertex,
        # independently of the rendered image (and for all three real poses).
        skin_error=0.
        for pose in ('bind','raised','turned'):
            world=r.skeleton.world(pose)
            for joint in r.head['names']:
                if not joint:continue
                a=np.c_[normal.canonical_data[:,:3],np.ones(len(normal.canonical_data))]@(world[joint]@np.linalg.inv(r.skeleton.bind[joint])).T
                b=np.c_[r.stored_data[:,:3],np.ones(len(r.stored_data))]@(world[joint]@r.head['inverse_binds'][joint]).T
                skin_error=max(skin_error,float(np.max(np.abs(a-b))))
        assert skin_error<1e-6,(name,'fixture skinning changed',skin_error)
        result['frames'][name]=dict(position_max=err,position_cases=position_cases,clip_vertex_max=clip_error,raster_sample_difference_max=raster_error,skin_vertex_max=skin_error,cases=0,worst=dict(rgb_mae=0.,rgb_p99=0.,alpha_mae=0.),
                                   full_avatar_cases=0,full_avatar_worst=dict(rgb_mae=0.,rgb_p99=0.,alpha_mae=0.))
    combo=next(n for n in ET.parse(ROOT/'indra/newview/skins/default/xui/en/panel_ghost_studio.xml').iter('combo_box') if n.get('name')=='style_combo')
    names={int(n.get('value')):n.get('label') for n in combo.findall('combo_box.item') if n.get('value','').isdigit()}
    looks=discover_looks()
    cases=[(str(look),look,{}) for look in looks]
    cases += [(effect,1,knobs(i,1.)) for i,effect in enumerate(EFFECTS)]
    strips={};saved={}
    for label,look,fx in cases:
        thumbs=[]
        for pose in ('bind','raised','turned'):
            for r in runners:r.pose=pose
            for view,angle in VIEWS.items():
                for r in runners:r.camera=angle
                coverage=normal.frame(probes[0],1,**opts)[...,3]>.99
                for ti,t in enumerate(TIMES):
                    expected=normal.frame(programs[0],look,time=t,**opts,**fx)
                    # Use union alpha to catch lost fragments/displacement.
                    for index,(frame_name,r) in enumerate(foreign.items(),1):
                        actual=r.frame(programs[index],look,time=t,**opts,**fx)
                        active=coverage|(expected[...,3]>0)|(actual[...,3]>0)
                        m=metrics(expected,actual,active)
                        row=result['frames'][frame_name];row['cases']+=1
                        row['worst']={k:max(row['worst'][k],m[k]) for k in m}
                        if failures(m):result['failures'].append(dict(frame=frame_name,look=label,pose=pose,view=view,time=t,metrics=m))
                        if frame_name=='head_cm' and pose=='bind' and ti in (0,3,6):
                            thumbs.extend([image(expected),image(actual)])
                        if frame_name=='head_cm' and pose=='bind' and view=='front' and ti==3:
                            saved[label]=(expected.copy(),actual.copy())
        strip=Image.new('RGB',(6*192,3*180),'#111111');d=ImageDraw.Draw(strip)
        for i,thumb in enumerate(thumbs):
            thumb.thumbnail((192,160),Image.Resampling.LANCZOS);strip.paste(thumb,((i%6)*192,(i//6)*180+20))
            d.text(((i%6)*192+4,(i//6)*180+3),('normal' if i%2==0 else 'foreign')+' '+label,fill='white')
        safe=re.sub(r'[^A-Za-z0-9_-]','_',label)
        strip.save(out/f'{safe}_strip.png');strips[label]=strip
        print('REST FRAME',label,names.get(look,label),'PASS' if not any(f['look']==label for f in result['failures']) else 'FAIL',flush=True)
    # Full-avatar suite at all existing times/views: transformed head is mixed
    # with normal body batches, exercising explicit per-batch zero resets.
    for r in runners:r.only={'head','upper_body','lower_body','mEyeLeft','mEyeRight'};r.pose='bind';r.camera_center_z=1.
    for label,look,fx in cases:
        for angle in VIEWS.values():
            for r in runners:r.camera=angle
            for t in TIMES:
                expected=normal.frame(programs[0],look,time=t,phase=SEEDS[0],**fx)
                for index,(name,r) in enumerate(foreign.items(),1):
                    actual=r.frame(programs[index],look,time=t,phase=SEEDS[0],**fx)
                    active=(expected[...,3]>0)|(actual[...,3]>0)
                    m=metrics(expected,actual,active)
                    row=result['frames'][name];row['full_avatar_cases']+=1
                    row['full_avatar_worst']={k:max(row['full_avatar_worst'][k],m[k]) for k in m}
                    if failures(m):result['failures'].append(dict(frame=name,look=label,view=float(angle),time=t,full_avatar=True,metrics=m))
    result['negative_controls']=negative_controls(normal,foreign,programs,probes,opts)
    # Exact body parity: a near-identity candidate is intentionally NOT applied.
    normal.only={'upper_body','lower_body'};normal.pose='bind';normal.camera=0.;normal.camera_center_z=1.
    near=np.diag([.93,.93,.93,1.]).astype(np.float32)
    near[:3,3]=(.1,.1,.1);p=default_world(normal.skeleton.nodes,'mPelvis');near[:3,3]-=p
    delta=correction(near,p);assert not delta.any()
    for b in normal.batches:b['rest_delta']=delta.copy()
    baseline=normal.program(RawRestBaseline(tree),('HAS_SKIN',))
    body_cases=0
    for look in looks:
        for angle in VIEWS.values():
            normal.camera=angle
            for t in TIMES:
                body_before=normal.frame(baseline,look,time=t)
                body_after=normal.frame(programs[0],look,time=t)
                assert np.array_equal(body_before,body_after),('normal body is not bit-identical to raw pre-fix behavior',look,float(angle),t)
                body_cases+=1
    result['normal_body_bit_identical']=dict(cases=body_cases,baseline='pre-fix raw vertex position; same fragment/Slice source',delta_exact_zero=True)
    sheet=Image.new('RGB',(768,8*350),'#111111');d=ImageDraw.Draw(sheet)
    for row,look in enumerate(SELECTED):
        label=str(look);a,b=saved[label]
        for col,frame in enumerate((a,b)):
            im=image(frame);sheet.paste(im,(col*384,row*350+30))
            d.text((col*384+8,row*350+8),f'{names[look]} {look}: '+('normal' if col==0 else 'foreign cm/Y-up'),fill='white')
    sheet.save(out/'contact_sheet.png')
    slice_strip(normal,programs[0],out)
    (out/'rest_frame_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    assert not result['failures'], 'Foreign-frame render parity failed: see rest_frame_metrics.json (thresholds remain fixed)'
    print('PASS rest frame: four foreign frames; all 35 looks and six FX, seven times, three views/poses; normal body exact; planted controls rejected',flush=True)
    return result


def negative_controls(normal,foreign,programs,probes,opts):
    result={}
    for r in [normal,*foreign.values()]:r.only={'head'};r.pose='bind';r.camera=0.;r.camera_center_z=1.74
    expected=normal.frame(probes[0],1,**opts);mask=expected[...,3]>.99
    def rejects(name,r,pr,delta):
        saved=r.head['rest_delta'];r.head['rest_delta']=delta
        try:
            actual=r.frame(pr,1,**opts)
            common=mask&(actual[...,3]>.99)
            err=float(np.max(np.abs(expected[common,:3]-actual[common,:3])))
            assert err>.1,(name,'planted coordinate bug did not reproduce strongly',err)
            result[name]=dict(position_max=err,rejected=True)
        finally:r.head['rest_delta']=saved
    for i,(name,r) in enumerate(foreign.items(),1):rejects('zero_delta_'+name,r,probes[i],np.zeros((3,4),np.float32))
    rejects('hard_coded_axis_swap',foreign['cuffs_cm'],probes[2],foreign['head_cm'].correct_delta)
    collision=np.diag([8.3,20.,19.7,1.]).astype(np.float32)
    collision_delta=correction(collision,default_world(normal.skeleton.nodes,'mPelvis'))
    rejects('collision_volume_joint',foreign['head_cm'],probes[1],collision_delta)
    # Leave a real corrected draw's uniform resident, then submit an ordinary
    # head on that SAME program while planting an omitted next-batch upload.
    # No incorrect delta is explicitly uploaded to the normal mesh.
    shared=probes[1]
    normal.definitions[shared]=foreign['head_cm'].definitions[shared]
    foreign['head_cm'].frame(shared,1,**opts)
    try:
        normal.omit_rest_upload=True
        actual=normal.frame(shared,1,**opts)
        common=mask&(actual[...,3]>.99)
        err=float(np.max(np.abs(expected[common,:3]-actual[common,:3])))
        assert err>.1,('retained previous mesh delta escaped',err)
        result['previous_mesh_delta_leak']=dict(position_max=err,rejected=True,kind='GL: actual retained uniform, same program')
    finally:
        normal.omit_rest_upload=False
        normal.frame(shared,1,**opts)  # explicit next-batch zero reset
    # A freshly linked program starts at zero. Omitting its upload recreates
    # the failure even when the previous program already has the right delta.
    r=foreign['head_cm'];new_program=r.program(G.SourceTree('WORKTREE'),('HAS_SKIN',),probe=True)
    try:
        # Program one already holds the correct delta; program two has never
        # received any ghostRestDelta upload. Its driver default is zero.
        r.frame(probes[1],1,**opts)
        r.omit_rest_upload=True
        actual=r.frame(new_program,1,**opts)
        common=mask&(actual[...,3]>.99)
        err=float(np.max(np.abs(expected[common,:3]-actual[common,:3])))
        assert err>.1,('fresh program missing upload escaped',err)
        result['program_switch_missing_upload']=dict(position_max=err,rejected=True,kind='GL: never-uploaded freshly linked program')
    finally:
        r.omit_rest_upload=False
        r.gl.fn('glDeleteProgram',None,r.gl.U)(new_program)
    r=foreign['head_cm'];correct=r.head['rest_delta'];r.head['rest_delta']=np.zeros((3,4),np.float32)
    try:
        a=normal.frame(programs[0],79,time=3.17,**opts);b=r.frame(programs[1],79,time=3.17,**opts)
        m=metrics(a,b,(a[...,3]>0)|(b[...,3]>0))
        assert m['rgb_mae']>.02,('zero delta did not visibly reproduce Glyph Skin bug',m)
        result['visible_zero_delta_glyph_skin']=m
    finally:r.head['rest_delta']=correct
    return result


def slice_strip(r,pr,out):
    r.only={'head','upper_body','lower_body','hair','eyelashes','mEyeLeft','mEyeRight','skirt'}
    r.pose='bind';r.camera=0.;r.camera_center_z=1.
    strip=Image.new('RGB',(384*3,350),'#111111');d=ImageDraw.Draw(strip)
    for i,t in enumerate((.17,3.17,7.85)):
        im=image(r.frame(pr,1,time=t,fx2=(1.,0,0,0),phase=SEEDS[0]));strip.paste(im,(i*384,30))
        d.text((i*384+8,8),f'Slice t={t}; cuts 1.68 m / 1.90 m',fill='white')
        # Rendered image is vertically flipped for PNG. Projection .85,
        # camera centre 1 m, unit zoom: y=(1-(z-1)*.85)/2*height.
        for z in (1.68,1.90):
            y=30+int((1-(z-1)*.85)*.5*r.height)
            d.line((i*384,y,i*384+70,y),fill='#ff9d3d');d.text((i*384+74,y-7),f'{z:.2f} m',fill='#ff9d3d')
    strip.save(out/'Slice_real_avatar_strip.png')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=type(OUT),default=OUT)
    parser.add_argument('--source-only',action='store_true',help='numeric and source controls; full WGL suite is also called by clone_looks_safety_check')
    args=parser.parse_args()
    if args.source_only:
        from clone_rest_frame_native_check import check as native_check
        rule_checks(Skeleton());source_checks();native_check()
        print('PASS rest frame numeric rules and planted source controls')
    else:check(args.out)
