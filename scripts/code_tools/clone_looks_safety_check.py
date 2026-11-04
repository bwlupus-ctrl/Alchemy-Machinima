"""Real SL mesh safety gate. No viewer build; requires the production WGL driver.

LLM layout: LLPolyMeshSharedData::loadMesh (llpolymesh.cpp). Legacy weight
palette order: LLAvatarJointMesh::setupJoint; adjacent-pair interpolation:
LLViewerJointMesh::updateGeometry. We convert those pairs to objectSkinV's
weight4 representation, preserving rest coordinates for actorghostF motifs.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import relight_3d_gl as G
from clone_looks_gl_check import Runner, AvatarRunner, F, V, ROOT, NAMES, MATRIX_NAMES, CINEMATIC_NAMES, LEGACY_ANIMATED
from clone_looks_anatomy import anatomy as skeleton_anatomy, verify as verify_anatomy

CHAR = ROOT / 'indra/newview/character'
TIMES = (.17, .73, 1.61, 3.17, 5.43, 7.85, 9.97)
VIEWS = dict(front=0., profile=np.pi/2, back=np.pi)
SEEDS = tuple(n*(2*np.pi/65536.) for n in (7614,25764))
# Display-space neutral reference is 0.42/0.72/0.94 (Y=.672). Limit is
# neutral + 0.078 highlight headroom. Fixed before measuring any look.
LUMA_CEILING = .75
AA_RMS, AA_P99 = .04, .15  # unchanged cinematic AA envelope
REPORT = ROOT / '.tmp/looks16/REPORT_SAFETY.md'


# Reviewed pre-existing failures only. Every pair is visible, dated and pinned.
# Removing a resolved pair requires an explicit review and pin update too.
LEGACY_KNOWN = {
    (64, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (67, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (73, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (76, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (77, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (78, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (80, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
    (83, 'authored hair alpha'): dict(date="2026-11-03", reason='Pre-existing authored hair alpha override; older-look alpha decision pending.'),
}
LEGACY_KNOWN_SHA256 = "1c2f46bcf992ae191b4ecba9738a444e99b8fd040bd0246b2139788130cd556e"


def validate_legacy_known(table=None):
    table = LEGACY_KNOWN if table is None else table
    payload = json.dumps([[look, check, meta['date'], meta['reason']]
                          for (look, check), meta in sorted(table.items())],
                         separators=(',', ':')).encode('utf-8')
    digest = hashlib.sha256(payload).hexdigest()
    assert digest == LEGACY_KNOWN_SHA256, 'LEGACY_KNOWN SHA-256 pin mismatch; review required'
    assert all(look < 85 for look, _ in table), 'LEGACY_KNOWN cannot exempt looks >= 85'
    return digest


def classify_legacy(look, observed):
    failures, warnings = [], []
    seen = {failure['reason'] for failure in observed}
    for failure in observed:
        pair = (look, failure['reason'])
        if look < 85 and pair in LEGACY_KNOWN:
            warnings.append(dict(look=look, check=failure['reason'],
                                 **LEGACY_KNOWN[pair], context=failure['context']))
        else:
            failures.append(failure)
    stale = [dict(look=look, check=check, **meta)
             for (entry_look, check), meta in sorted(LEGACY_KNOWN.items())
             if entry_look == look and check not in seen]
    for entry in stale:
        failures.append(dict(reason='stale LEGACY_KNOWN: remove '+entry['check'], context=entry))
    return failures, warnings, stale


def legacy_negative_controls():
    validate_legacy_known()
    enlarged = dict(LEGACY_KNOWN)
    enlarged[85, 'frozen'] = dict(date='2026-11-03', reason='Negative control')
    try:
        validate_legacy_known(enlarged)
    except AssertionError as error:
        assert 'pin mismatch' in str(error)
    else:
        raise AssertionError('new-look legacy entry escaped SHA-256 pin')
    for look in range(85, 101):
        for check in ('frozen', 'identical crowd seeds', 'authored hair alpha'):
            failed, warned, stale = classify_legacy(look, [dict(reason=check, context=[])])
            assert failed and not warned and not stale, ('new-look failure escaped', look, check)
    for look in sorted({look for look, _ in LEGACY_KNOWN}):
        observed = [dict(reason=check, context=[]) for entry_look, check in LEGACY_KNOWN if entry_look == look]
        failed, warned, stale = classify_legacy(look, observed)
        assert not failed and len(warned) == len(observed) and not stale
        failed, _, stale = classify_legacy(look, observed[1:])
        assert failed and stale, ('stale legacy entry escaped', look)
        failed, _, _ = classify_legacy(look, observed+[dict(reason='new safety failure', context=[])])
        assert any(f['reason'] == 'new safety failure' for f in failed), 'new legacy-look failure escaped'
    return dict(pin_growth_rejected=True, new_range_failures_rejected=list(range(85, 101)),
                stale_entries_rejected=True, new_legacy_checks_rejected=True)


def note(message):
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    with REPORT.open('a', encoding='utf-8') as f:
        f.write(message + '\n')
    print(message, flush=True)


def rotation(degrees):
    x, y, z = np.radians(degrees)
    rx = np.array([[1,0,0],[0,np.cos(x),-np.sin(x)],[0,np.sin(x),np.cos(x)]])
    ry = np.array([[np.cos(y),0,np.sin(y)],[0,1,0],[-np.sin(y),0,np.cos(y)]])
    rz = np.array([[np.cos(z),-np.sin(z),0],[np.sin(z),np.cos(z),0],[0,0,1]])
    return rz @ ry @ rx


def transform(pos=(0,0,0), rot=(0,0,0), scale=(1,1,1)):
    m = np.eye(4, dtype=np.float32)
    m[:3,:3] = rotation(rot) @ np.diag(scale)
    m[:3,3] = pos
    return m


def read_llm(path):
    data = path.read_bytes()
    assert data.startswith(b'Linden Binary Mesh 1.0'), path
    offset = 24
    def take(fmt, count=1):
        nonlocal offset
        dtype = np.dtype(fmt).newbyteorder('<')
        result = np.frombuffer(data, dtype, count, offset).copy()
        offset += dtype.itemsize * count
        return result
    weighted, detail = take('u1', 2)
    pos, rot = take('f4',3), take('f4',3)
    take('u1')  # viewer ignores stored rotation order and uses XYZ
    scale = take('f4',3)
    count = int(take('u2')[0])
    coords, normals, binormals = [take('f4',count*3).reshape(-1,3) for _ in range(3)]
    uv = take('f4',count*2).reshape(-1,2)
    if detail:
        take('f4',count*2)
    weights = take('f4',count) if weighted else None
    faces = take('u2',int(take('u2')[0])*3).reshape(-1,3)
    assert faces.max() < count and np.isfinite(coords).all(), path
    joints = []
    if weighted:
        for _ in range(int(take('u2')[0])):
            joints.append(data[offset:offset+64].split(b'\0')[0].decode('ascii'))
            offset += 64
    return dict(name=path.stem, coords=coords, normals=normals, uv=uv,
                weights=weights, faces=faces, joints=joints,
                transform=transform(pos,rot,scale), sha256=hashlib.sha256(data).hexdigest())


class Skeleton:
    def __init__(self):
        self.nodes = {}
        def visit(element, parent=None):
            name = element.get('name')
            vec = lambda key, default: np.fromstring(element.get(key,default),sep=' ')
            self.nodes[name] = dict(parent=parent, pos=vec('pos','0 0 0'),
                                    pivot=vec('pivot','0 0 0'), rot=vec('rot','0 0 0'),
                                    scale=vec('scale','1 1 1'), base=element.get('support')=='base')
            for child in element.findall('bone'):
                visit(child,name)
        for bone in ET.parse(CHAR/'avatar_skeleton.xml').getroot().findall('bone'):
            visit(bone)
        self.bind = self.world('bind')
        for name in ('mEyeLeft','mEyeRight'):
            assert self.nodes[name]['pos'][0] > .09

    def world(self, pose):
        result = {}
        for name, node in self.nodes.items():
            rot = node['rot'].copy()
            if pose == 'raised' and name in ('mShoulderLeft','mShoulderRight'):
                rot[0] += 55 if name.endswith('Left') else -55
            if pose == 'turned' and name == 'mTorso':
                rot[2] += 28
            if pose == 'turned' and name == 'mHead':
                rot[2] -= 18
            result[name] = result.get(node['parent'],np.eye(4)) @ transform(node['pos'],rot,node['scale'])
        return result

    def ancestor(self, name):
        parent = self.nodes[name]['parent']
        while parent and not self.nodes[parent]['base']:
            parent = self.nodes[parent]['parent']
        return parent

    def palette_names(self, joints):
        # Depth-first XML traversal is the viewer's setupJoint traversal.
        names = []
        for name in self.nodes:
            if name in joints:
                parent = self.ancestor(name)
                if not names or names[-1] != parent:
                    names.append(parent)
                names.append(name)
        return names


class RealAvatar(Runner):
    def __init__(self, width, height, gl=None):
        super().__init__(width,height,gl)
        self.skeleton = Skeleton()
        self.pose = 'bind'
        self.camera = 0.
        self.only = None
        self.include_blend = False
        self.show_hidden = False
        self.avatar_fixture = True
        self.real_draw_defaults = True
        self.production_avatar_basis = True
        self.rest_basis = np.array([[0,-1,0,0],[1,0,0,0],[0,0,1,0],[0,0,0,1]],np.float32)
        self.batches = []
        self.assets = []
        # Preserve the real ground-origin rest coordinates. Camera centering
        # must NEVER alter vary_object_position or the joint palette basis.
        self.origin = np.zeros(3)
        self.anatomy, _ = skeleton_anatomy()
        self.camera_center_z = (self.anatomy['foot']+self.anatomy['top'])*.5
        self.rigid_programs = {}
        y,x = np.mgrid[:128,:128]/127.
        pixels = np.stack((.25+.5*x,.18+.35*y,.12+.12*np.sin(x*17)**2,np.ones_like(x)),-1).astype(np.float32)
        bodytex = self.texture(pixels,128,128,0x8814,'mip')
        for part in ('head','upper_body','lower_body','eye','eyelashes','hair','skirt'):
            mesh = read_llm(CHAR/f'avatar_{part}.llm')
            self.assets.append(dict(file=f'avatar_{part}.llm',sha256=mesh['sha256'],
                                    vertices=len(mesh['coords']),triangles=len(mesh['faces'])))
            coords = mesh['coords'].copy()
            names = self.skeleton.palette_names(mesh['joints'])
            if part == 'eye':
                for eye in ('mEyeLeft','mEyeRight'):
                    p = (np.c_[coords,np.ones(len(coords))] @ self.skeleton.bind[eye].T)[:,:3]
                    self.add(eye,p,mesh['normals'],mesh['uv'],mesh['faces'],bodytex,[eye],None)
                continue
            texture, cutoff, alpha_mode = bodytex,0.,False
            if part in ('hair','eyelashes'):
                card = pixels.copy(); card[...,3] = ((np.floor(x*17)%3)!=0).astype(float)
                texture = self.texture(card,128,128,0x8814)
                cutoff,alpha_mode = .5,True
            self.add(part,coords,mesh['normals'],mesh['uv'],mesh['faces'],texture,names,mesh['weights'],cutoff,alpha_mode)
        # Retain every existing synthetic extra, including its actual local
        # non-unit attachment transform. They are independent draw batches.
        extras = AvatarRunner(32,32,self.gl)
        for b in extras.batches:
            if b['name'] in ('hair','blend','attachment','invisible','degenerate'):
                b = b.copy(); b['name'] = 'extra_'+b['name']; b['stride']=32
                data=np.empty((b['count'],8),np.float32)
                self.gl.bind_buf(0x8892,b['vbo'])
                self.gl.fn('glGetBufferSubData',None,self.gl.U,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x8892,0,data.nbytes,data.ctypes.data)
                b['points']=data[:,:3]
                self.batches.append(b)
        # Hidden native system body is submitted with authored alpha zero;
        # compare its presence to collector omission, with poisoned RGB.
        zero = pixels.copy(); zero[...,:3]=(12,4,9); zero[...,3]=0
        tex = self.texture(zero,128,128,0x8814)
        for b in list(self.batches):
            if b['name'] in ('head','upper_body','lower_body'):
                hidden = b.copy();hidden.update(name='hidden_'+b['name'],texture=tex,alpha_mode=True,cutoff=0.)
                self.batches.append(hidden)
        self.height_m = float(max(read_llm(CHAR/'avatar_hair.llm')['coords'][:,2])-min(read_llm(CHAR/'avatar_lower_body.llm')['coords'][:,2]))
        self.pose_bounds={pose:self.bounds(pose) for pose in ('bind','raised','turned')}

    def bounds(self,pose):
        world=self.skeleton.world(pose);points=[]
        for b in self.batches:
            if b['name'].startswith('hidden_') or b['name'] in ('extra_invisible','extra_degenerate','extra_blend'):continue
            p=np.c_[b['points'],np.ones(len(b['points']))]
            if 'names' not in b:
                points.append((p@b['transform'].T)[:,:3]);continue
            palette=np.array([world[n]@np.linalg.inv(self.skeleton.bind[n]) if n else np.eye(4) for n in b['names']])
            w=b['legacy_weights'];idx=np.floor(w).astype(int);f=(w-idx)[:,None]
            points.append(((1-f)*np.einsum('nij,nj->ni',palette[idx],p)+f*np.einsum('nij,nj->ni',palette[np.minimum(idx+1,len(palette)-1)],p))[:,:3])
        points=np.concatenate(points)
        return (float(points[:,2].min()),float(points[:,2].max()))

    def program(self, tree, defs=(), vertex=V, probe=False):
        pr = super().program(tree,defs,vertex,probe)
        if 'HAS_SKIN' in defs:
            self.rigid_programs[pr] = super().program(tree,tuple(d for d in defs if d!='HAS_SKIN'),vertex,probe)
        return pr

    def add(self,name,p,n,uv,faces,texture,names,weights,cutoff=0.,alpha_mode=False):
        if not names:
            names=[None]
        packed = np.zeros((len(p),4),np.float32)
        if weights is None:
            packed[:,0]=.5
        else:
            index = np.floor(weights).astype(int)
            frac = weights-index
            assert index.max()<len(names), (name,index.max(),names)
            assert np.all((frac==0)|(index+1<len(names)))
            # Half-size fractions avoid representing weight=1 as index+1.
            packed[:,0]=index+(1-frac)*.5
            packed[:,1]=np.minimum(index+1,len(names)-1)+frac*.5
        verts = np.c_[p,n,uv,packed].astype(np.float32)[faces.reshape(-1)]
        g=self.gl; vbo=g.U();g.gen_buf(1,C.byref(vbo));g.bind_buf(0x8892,vbo)
        g.buffer_data(0x8892,verts.nbytes,verts.ctypes.data,0x88E4)
        self.batches.append(dict(name=name,vbo=vbo,count=len(verts),texture=texture,
                                 names=names,cutoff=cutoff,alpha_mode=alpha_mode,stride=48,
                                 points=p,legacy_weights=np.zeros(len(p)) if weights is None else weights))

    def draw_avatar(self,pr,look,view,proj,phase):
        g=self.gl; loc=lambda n:g.loc(pr,n.encode())
        f=lambda n,*v:g.uf[len(v)](loc(n),*v)
        world=self.skeleton.world(self.pose)
        turn=transform(rot=(0,0,np.degrees(self.camera)))
        view=view@turn
        depthmask=g.fn('glDepthMask',None,g.U)
        colormask=g.fn('glColorMask',None,g.U,g.U,g.U,g.U)
        g.enable(0x0B71);depthmask(1);self.clear(0x1801,0,(g.F*1)(1.))
        batches=[b for b in self.batches if (self.only is None or b['name'] in self.only)
                 and (self.include_blend or b['name']!='extra_blend')
                 and (self.show_hidden or not b['name'].startswith('hidden_'))]
        main_pr = pr
        def draw(b):
            nonlocal pr,loc,f
            rigid = b['name']=='extra_attachment'
            pr = self.rigid_programs.get(main_pr,main_pr) if rigid else main_pr
            g.use(pr)
            if rigid and pr != main_pr:
                for uniform,(integer,values) in self.render_uniforms.items():
                    (g.ui if integer else g.uf[len(values)])(loc(uniform),*values)
                for uniform,mat in (('projection_matrix',proj),('texture_matrix0',np.eye(4,dtype=np.float32))):
                    a=np.ascontiguousarray(mat.T,dtype=np.float32)
                    g.matrix(loc(uniform),1,0,a.ctypes.data_as(C.POINTER(g.F)))
            matrices=[]
            for name in b.get('names',[None]):
                m=np.eye(4)
                if name:
                    m=world[name]@np.linalg.inv(self.skeleton.bind[name])
                elif 'transform' in b:
                    m=b['transform']
                # objectSkinV consumes mat3(matrixPalette) as COLUMN vectors,
                # but translation from each column's .w. Match the viewer's
                # packed 3x4 palette rather than a numpy affine row layout.
                packed=m[:3].copy();packed[:,:3]=m[:3,:3].T
                matrices.append(packed)
            palette=np.ascontiguousarray(matrices,dtype=np.float32)
            self.mat34(loc('matrixPalette[0]'),len(matrices),0,palette.ctypes.data_as(C.POINTER(g.F)))
            modelview=view@b['transform'] if rigid else view
            a=np.ascontiguousarray(modelview.T);g.matrix(loc('modelview_matrix'),1,0,a.ctypes.data_as(C.POINTER(g.F)))
            a=np.ascontiguousarray(np.linalg.inv(modelview[:3,:3]),dtype=np.float32)
            self.mat3(loc('normal_matrix'),1,0,a.ctypes.data_as(C.POINTER(g.F)))
            g.ui(loc('ghostLook'),draw_look)
            f('ghostAux',b['cutoff'],1.,self.pixel_size,phase)
            f('ghostAlpha',int(b['alpha_mode']),1.)
            g.bind_tex(0x0DE1,b['texture']);g.bind_buf(0x8892,b['vbo'])
            for name,count,offset in (('position',3,0),('normal',3,12),('texcoord0',2,24),('weight4',4,32)):
                ix=g.attr_loc(pr,name.encode())
                if ix>=0:
                    if name=='weight4' and b['stride']==32:
                        self.disable_attr(ix);self.attr4(ix,.5,0,0,0)
                    else:
                        g.enable_attr(ix);g.attr_ptr(ix,count,0x1406,0,b['stride'],C.c_void_p(offset))
            g.draw(0x0004,0,b['count'])
        colormask(0,0,0,0);g.disable(0x0BE2);g.fn('glDepthFunc',None,g.U)(0x0201)
        draw_look=1
        for b in batches:
            if b['name']!='extra_blend':draw(b)
        colormask(1,1,1,1);depthmask(0);g.fn('glDepthFunc',None,g.U)(0x0203)
        destination = 1 if getattr(self,'additive_control',False) else 0x0303
        g.enable(0x0BE2);g.fn('glBlendFuncSeparate',None,g.U,g.U,g.U,g.U)(0x0302,destination,1,destination)
        draw_look=look
        for b in batches:draw(b)
        depthmask(1);g.disable(0x0B71);g.use(main_pr)

    def frame(self,pr,look,**kw):
        kw.setdefault('opacity',1.)
        kw.setdefault('raw_time',True)
        kw.setdefault('phase',SEEDS[0])
        return self.render(pr,look,**kw)


def mask_dilate(mask):
    return np.asarray(Image.fromarray(mask).filter(ImageFilter.MaxFilter(5)))>0


def hazards(frame, mask, dilated=None):
    failures=[]
    if not np.isfinite(frame).all():failures.append('NaN/Inf')
    if np.any(np.abs(frame[~(mask_dilate(mask) if dilated is None else dilated)])>1e-6):failures.append('outside silhouette')
    rgb=frame[mask,:3]
    luma=float(np.mean(rgb[:,0]*.2126+rgb[:,1]*.7152+rgb[:,2]*.0722))
    white=float(np.mean(np.all(rgb>=.98,axis=-1)))
    if luma>LUMA_CEILING:failures.append('luminance')
    if white>.25:failures.append('saturated white')
    return failures,dict(mean_luminance=luma,saturated_white=white)


def save(frame,path):
    rgb=frame[...,:3]+.035*(1-frame[...,3:4])
    Image.fromarray((np.clip(rgb[::-1],0,1)*255+.5).astype(np.uint8)).save(path)


class Variant:
    """In-memory mutations; production files are never modified by controls."""
    def __init__(self, tree, fragment=(), vertex=()):
        self.tree,self.fragment,self.vertex=tree,fragment,vertex
    def read(self,path):
        source=self.tree.read(path)
        for before,after in self.fragment if path==F else self.vertex if path==V else ():
            assert source.count(before)==1,(path,before,source.count(before))
            source=source.replace(before,after)
        return source


def animation_failure(frames, mask):
    deltas=[float(np.mean(np.abs(a[mask,:3]-b[mask,:3]))) for a,b in zip(frames,frames[1:])]
    return max(deltas,default=0.) <= 1e-6, deltas


def aa_check(r,pr,look,scale,time,seed):
    dx=2/(r.projection_x*r.width*scale)
    dy=2/(.85*r.height*scale)
    opts=dict(time=time,phase=seed,scale=scale,screen_size=(3840,2160),frag_offset=(768.,0.))
    actual=np.clip(r.frame(pr,look,shift=.5*dx,**opts),0,1)
    ref=np.zeros_like(actual)
    for jy in (-.375,-.125,.125,.375):
        for jx in (-.375,-.125,.125,.375):
            ref+=np.clip(r.frame(pr,look,shift=(.5+jx)*dx,shift_y=jy*dy,**opts),0,1)/16
    mask=r.frame(pr,1,shift=.5*dx,**opts)[...,3]>.99
    original=mask.copy()
    for axis in (0,1):mask &= np.roll(original,2,axis)&np.roll(original,-2,axis)
    assert mask.sum()>10,'empty real mesh AA oracle'
    error=actual[mask]-ref[mask]
    return dict(rms=float(np.sqrt(np.mean(error**2))),p99=float(np.quantile(np.abs(error),.99)))


class BranchProbe:
    def __init__(self, tree, look, expression):
        self.tree,self.look,self.expression=tree,look,expression
    def read(self,path):
        source=self.tree.read(path)
        if path==F:
            start=source.index(f'if (ghostLook == {self.look}) //')
            following='else // 100:' if self.look==99 else f'else if (ghostLook == {self.look+1}) //'
            end=source.index(following,start);close=source.rfind('        }',start,end)
            source=source[:close]+'            rgb = '+self.expression+';\n'+source[close:]
        return source


def anatomy(r,tree,pr):
    verify_anatomy(tree.read(F))
    a,joints=skeleton_anatomy()
    result=dict(joints={k:v.tolist() for k,v in joints.items() if k in (
        'mFootLeft','mAnkleLeft','mKneeLeft','mPelvis','mChest','mCollarLeft',
        'mNeck','mHead','mEyeLeft','mEyeRight')},derived=a,negative_controls={})
    programs=[]
    def program(tree,look,expression,defs=('HAS_SKIN',)):
        p=r.program(BranchProbe(tree,look,expression),defs);programs.append(p);return p
    body={'head','upper_body','lower_body','mEyeLeft','mEyeRight'}
    r.only=body;r.pose='bind';r.camera=0.
    pos_pr=r.program(tree,('HAS_SKIN',),probe=True);programs.append(pos_pr)
    pos=r.frame(pos_pr,1,phase=0);mask=pos[...,3]>.99;z=pos[...,2]
    assert z[mask].min()<a['ankle'] and z[mask].max()>a['eye'], 'mesh lost ground-origin rest frame'
    eye_region=mask&(abs(z-a['eye'])<(a['eye']-a['head'])*.5)
    foot_region=mask&(z<a['ankle'])
    pelvis_region=mask&(abs(z-a['pelvis'])<.08)
    head_region=mask&(z>a['head'])
    assert min(eye_region.sum(),foot_region.sum(),pelvis_region.sum(),head_region.sum())>10
    def render(p,look,time=2.37):return r.frame(p,look,time=time,phase=0)
    def rejects(name,check):
        try:check()
        except AssertionError as e:result['negative_controls'][name]=str(e)
        else:raise AssertionError(name+' negative control escaped')
    def eye_check(p,look):
        frame=render(p,look);signal=frame[...,0]
        # Cold Gaze is an eye slit; Torn Husk deliberately has a broad sensor
        # patch spanning the head, centred on the same XML eye line.
        region=eye_region if look==94 else mask&(abs(z-a['eye'])<(a['eye']-a['head'])*1.5)
        peak=float(np.sum(z[mask]*signal[mask])/max(signal[mask].sum(),1e-8))
        fraction=float(signal[region].sum()/max(signal[mask].sum(),1e-8))
        assert signal[region].max()>.2 and abs(peak-a['eye'])<.025 and fraction>.7, ('eye band misses XML eyes',peak,fraction)
        return dict(peak_z=peak,eye_fraction=fraction)
    for look,expression,old in ((94,'vec3(eyes*front)',('P.z - sl_eye','P.z - 0.76')),
                                (98,'vec3(sensor)',('float sensor=exp(-sq((P.z-sl_eye)', 'float sensor=exp(-sq((P.z-0.75)'))):
        p=program(tree,look,expression)
        result[str(look)]=eye_check(p,look)
        bad=program(Variant(tree,[old]),look,expression)
        rejects(str(look)+'_fixture_eye_height',lambda:eye_check(bad,look))
    # Front/profile/back uses actual head/eye triangles and the exact shared
    # production front factor. A -Y forward implementation must be rejected.
    r.only={'head','mEyeLeft','mEyeRight'}
    def facing(p):
        values={}
        for name,angle in VIEWS.items():
            r.camera=angle;frame=render(p,94);covered=frame[...,3]>.99
            values[name]=float(frame[covered,0].mean())
        r.camera=0.
        assert values['front']>.1 and values['front']>values['profile']*1.5 and values['front']>values['back']*3, ('front/profile/back',values)
        return values
    p=program(tree,94,'vec3(front)');result['facing']=facing(p)
    before='float front = smoothstep(0.15, 0.75, restN.x)\n                    * smoothstep(-0.04, 0.02, P.x);'
    wrong=program(Variant(tree,[(before,'float front = smoothstep(0.15,0.75,-restN.y)*smoothstep(-0.04,0.02,-P.y);')]),94,'vec3(front)')
    rejects('minus_y_forward',lambda:facing(wrong))
    r.only=body
    def pool_check(p):
        f=render(p,89);signal=f[...,0]
        peak=float(np.sum(z[mask]*signal[mask])/max(signal[mask].sum(),1e-8))
        assert f[foot_region,0].mean()>.5 and peak<a['ankle']*2, ('pool misses XML feet',peak)
        # The height ramp must remain unsaturated through chest and neck.
        assert .45<f[pelvis_region,1].mean()<.7 and f[eye_region,1].mean()>.9, 'height ramp is not feet->head'
        return dict(pool_z=peak,pelvis_height=float(f[pelvis_region,1].mean()))
    result['89']=pool_check(program(tree,89,'vec3(pool,height,0)'))
    bad=program(Variant(tree,[('float pool = exp(-sq((P.z - sl_foot) / (2.0 * (sl_ankle - sl_foot)))) * anatomical;',
                             'float pool = exp(-sq((P.z + 0.94)/0.16));')]),89,'vec3(pool,height,0)')
    rejects('emitter_fixture_pool',lambda:pool_check(bad))
    def sweep_check(p):
        early=render(p,88,0);late=render(p,88,4.95)
        assert early[mask,0].mean()<.1 and late[mask,0].mean()>.98 and late[head_region,0].mean()>.98, 'sweep does not cover XML feet through head'
        return dict(early=float(early[mask,0].mean()),late=float(late[mask,0].mean()),head=float(late[head_region,0].mean()))
    result['88']=sweep_check(program(tree,88,'vec3(changed)'))
    old_plane=('float plane = mix(sl_foot - sl_ankle, sl_top + sl_ankle, min(cycle / 0.82, 1.0));','float plane = mix(-1.35,1.35,cycle);')
    rejects('rapture_fixture_range',lambda:sweep_check(program(Variant(tree,[old_plane]),88,'vec3(changed)')))
    def crown_check(p):
        f=render(p,85)
        assert f[pelvis_region,0].mean()<.01 and f[head_region,0].mean()>.65, 'crown not confined to XML head/shoulders'
        return dict(pelvis=float(f[pelvis_region,0].mean()),head=float(f[head_region,0].mean()))
    result['85']=crown_check(program(tree,85,'vec3(crown)'))
    rejects('aureole_fixture_crown',lambda:crown_check(program(Variant(tree,[
        ('float crown = smoothstep(sl_shoulder, sl_eye, P.z) * anatomical;', 'float crown = smoothstep(0.20,0.72,P.z);')]),85,'vec3(crown)')))
    def hunter_check(p):
        f=render(p,99);target=(a['neck']+a['knee'])*.5
        error=float(np.max(abs(f[mask,:2]-np.abs(pos[mask][:,[1,2]]-[0,target]))))
        signal=f[...,2];active=mask&(signal>.1)
        span=float(np.ptp(z[active])) if active.any() else 0.
        assert error<1e-6 and span>(a['neck']-a['knee'])*.7, ('brackets fail XML body front',error,span)
        return dict(coordinate_error=error,visible_height_span=span)
    result['99']=hunter_check(program(tree,99,'vec3(b,vertical+horizontal)'))
    rejects('hunter_fixture_height',lambda:hunter_check(program(Variant(tree,[
        ('target.y += (sl_neck+sl_knee)*0.5;','target.y += 0.12;')]),99,'vec3(b,vertical+horizontal)')))
    rejects('hunter_depth_axis',lambda:hunter_check(program(Variant(tree,[
        ('vec2 b=abs(P.yz-target);','vec2 b=abs(P.xz-target);')]),99,'vec3(b,vertical+horizontal)')))
    # Read the actual impact centres from inside Reform's shader loop. Across
    # real clone seeds they must reach upper body as well as the legs.
    impact_probe=Variant(tree,[
        ('float hit = 0.0, molten = 0.0;', 'float hit = 0.0, molten = 0.0; vec2 oracleCentre=vec2(0);'),
        ('float age=fract(t/(3.1+fi*0.7)+fi*0.31);',
         'if (i==0) oracleCentre=centre; float age=fract(t/(3.1+fi*0.7)+fi*0.31);')])
    def impact_check(p):
        centres=[]
        for seed in np.linspace(0,6,17):
            f=r.frame(p,97,time=2.37,phase=float(seed))
            centres.append(f[mask,:2].mean(axis=0))
        centres=np.array(centres)
        low,high=float(centres[:,1].min()),float(centres[:,1].max())
        assert low>=a['ankle']-1e-4 and high<=a['eye']+1e-4 and low<a['knee'] and high>a['chest'], ('Reform impacts miss XML body range',low,high)
        assert np.max(abs(centres[:,0]))<=a['half_width']+1e-4, 'Reform impacts outside body width'
        return dict(low=low,high=high)
    result['97']=impact_check(program(impact_probe,97,'vec3(oracleCentre,0)'))
    old_impacts=Variant(impact_probe,[
        ('mix(sl_ankle,sl_eye,ghost_hash(vec2(fi+7.0,seed)))', 'ghost_hash(vec2(fi+7.0,seed))*1.4-0.65')])
    rejects('reform_fixture_impacts',lambda:impact_check(program(old_impacts,97,'vec3(oracleCentre,0)')))
    # The rigid program really has NO HAS_SKIN permutation. Probe the same
    # prim-local, non-unit attachment through the mixed production replay.
    r.only={'extra_attachment'}
    for look,expression in ((88,'vec3(changed,sweep,0)'),(94,'vec3(eyes)'),
                            (98,'vec3(spine,sensor,0)'),(99,'vec3(vertical,horizontal,ticks)')):
        f=render(program(tree,look,expression),look)
        assert np.count_nonzero(f[...,3])>10,'empty rigid attachment control'
        assert np.max(abs(f[...,:3]))==0, ('rigid attachment has local anatomy',look)
    result['rigid_anatomy_suppressed']=True
    r.only=body
    # New looks no longer reset time at 4096s. Compare immediately adjacent
    # frames on each side of that old discontinuity, including hash looks.
    result['wrap_deltas']={}
    for look in range(85,101):
        left=r.frame(pr,look,time=4096.-.001,phase=0)
        right=r.frame(pr,look,time=4096.+.001,phase=0)
        delta=float(np.mean(abs(left[mask,:3]-right[mask,:3])))
        result['wrap_deltas'][str(look)]=delta
    assert result['wrap_deltas']['88']<.01, 'Rapture discontinuity at 4096s'
    wrapped=r.program(Variant(tree,[('float t = ghostTime + seed * 1.73;',
                                    'float t = mod(ghostTime,4096.0) + seed * 1.73;')]),('HAS_SKIN',))
    programs.append(wrapped)
    left=r.frame(wrapped,88,time=4096.-.001,phase=0)
    right=r.frame(wrapped,88,time=4096.+.001,phase=0)
    assert np.mean(abs(left[mask,:3]-right[mask,:3]))>.05, '4096 wrap negative control insensitive'
    result['negative_controls']['mod4096_wrap']='Rapture discontinuity rejected'
    r.only=None;r.camera=0.
    for p in programs:
        r.gl.fn('glDeleteProgram',None,r.gl.U)(p)
    return result


def negative_controls(r,tree,pr):
    result={}
    mask=r.frame(pr,1)[...,3]>0
    # Recreate a displaced additive white shell. Both source transformations
    # are temporary; use its own neutral prime just like production replay.
    shell=r.program(Variant(tree,
        [('frag_color = max(vec4(rgb, alpha), vec4(0));','frag_color = vec4(vec3(6.0), alpha);')],
        [('gl_Position = projection_matrix * pos;','gl_Position = projection_matrix * (pos + vec4(normalize(vary_normal)*0.18,0.0));')]),('HAS_SKIN',))
    r.additive_control=True
    try:a=r.frame(shell,85)
    finally:r.additive_control=False
    failed,metrics=hazards(a,mask)
    assert 'outside silhouette' in failed and 'saturated white' in failed,('shell control escaped',failed,metrics)
    result['ash_arc_shell']=dict(rejected=failed,metrics=metrics,blend='SRC_ALPHA, ONE')
    clock=re.search(r'uniform float ghostTime\s*;',tree.read(F)).group(0)
    static=r.program(Variant(tree,[(clock,clock+'\n#define ghostTime 2.0\n')]),('HAS_SKIN',))
    frames=[r.frame(static,69,time=t) for t in TIMES]
    frozen,deltas=animation_failure(frames,mask)
    assert frozen,('static Magma control escaped',deltas)
    result['magma_static']=dict(rejected='frozen',deltas=deltas)
    # Historical Decode treated the real default .5 as a manual scrub.
    source=tree.read(F)
    start=source.index('else if (ghostLook == 78) //')
    end=source.index('else if (ghostLook == 79) //',start)
    branch=source[start:end]
    parked=branch.replace('float t = mod(ghostTime, 4096.0);',
                          'float t = ghostDissolveProgress > 0.0 ? ghostDissolveProgress*6.0 : mod(ghostTime,4096.0);')
    parked=parked.replace('if (ghostDecodeScrub != 0)','if (ghostDissolveProgress > 0.0)')
    decode=r.program(Variant(tree,[(branch,parked),
        (clock,clock+'\n#define ghostTime (ghostDissolveProgress > 0.0 ? ghostDissolveProgress*6.0 : ghostTime)\n')]),('HAS_SKIN',))
    frames=[r.frame(decode,78,time=t) for t in TIMES]
    frozen,deltas=animation_failure(frames,mask)
    assert frozen,('parked Decode control escaped',deltas)
    result['decode_default_progress']=dict(rejected='frozen',defaults=r.draw_defaults,deltas=deltas)
    alpha=r.program(Variant(tree,[('tex.a = authoredAlpha;','tex.a = 1.0;')]),('HAS_SKIN',))
    r.only={'extra_invisible'}
    a=r.frame(alpha,85);r.only=None
    assert np.any(a), 'ignore-alpha control escaped invisible-face assertion'
    result['ignore_texture_alpha']=dict(rejected='invisible faces',visible_pixels=int(np.count_nonzero(a[...,3])))
    # Guard seeds and nonfinite detection with the same classifier as normal.
    unseeded=r.program(Variant(tree,[('float seed = ghostAux.w;','float seed = 0.0;')]),('HAS_SKIN',))
    a=r.frame(unseeded,85,phase=SEEDS[0]);b=r.frame(unseeded,85,phase=SEEDS[1])
    assert np.array_equal(a,b),'unseeded control not identical'
    result['ignored_seed']=dict(rejected='identical crowd seeds')
    nonfinite=np.zeros((3,3,4),np.float32);nonfinite[1,1,0]=np.nan
    assert 'NaN/Inf' in hazards(nonfinite,np.ones((3,3),bool))[0]
    for p in (shell,static,decode,alpha,unseeded):r.gl.fn('glDeleteProgram',None,r.gl.U)(p)
    return result


def discover_looks():
    # UI/persistence additions automatically enter the gate; no 100 ceiling.
    looks=set()
    for item in ET.parse(ROOT/'indra/newview/skins/default/xui/en/panel_ghost_studio.xml').iter('combo_box'):
        if item.get('name')=='style_combo':
            looks.update(int(n.get('value')) for n in item.findall('combo_box.item') if n.get('value','').isdigit() and int(n.get('value'))>=64)
    assert set(range(64,101))-{68,72} <= looks
    return sorted(looks-{68,72})


# Perceptibility is evaluated in display code values AFTER compositing and
# clipping to [0,1], never on invisible HDR energy or alpha differences.
PERCEPT_TIMES = (0., 2., 4., 6., 8., 10.)
PERCEPT_LIMITS = dict(motion_mae=.012, motion_p95=.04, seed_mae=.01,
                      adjacent_ratio=.25)


def display_rgb(frame):
    return np.clip(frame[..., :3] + .035*(1-frame[..., 3:4]), 0., 1.)


def rgb_change(a, b, mask):
    error = np.abs(display_rgb(a)[mask] - display_rgb(b)[mask])
    return dict(mae=float(error.mean()), p95=float(np.quantile(error, .95)))


def perceptibility_failures(row):
    failures = []
    if row['motion_mae'] < PERCEPT_LIMITS['motion_mae']: failures.append('imperceptible motion mean')
    if row['motion_p95'] < PERCEPT_LIMITS['motion_p95']: failures.append('imperceptible motion p95')
    if row['seed_mae'] < PERCEPT_LIMITS['seed_mae']: failures.append('imperceptible crowd seeds')
    if row['motion_seed_mae'] < PERCEPT_LIMITS['seed_mae']: failures.append('unseeded motion')
    if row['adjacent_mae_ratio'] >= PERCEPT_LIMITS['adjacent_ratio'] or row['adjacent_p95_ratio'] >= PERCEPT_LIMITS['adjacent_ratio']:
        failures.append('animation strobing')
    return failures


def perceptibility_checks(out, r, tree, current, looks=None):
    """600 px real SL avatar; report all 64-100, gate only reviewed five.

    MAE and p95 pool absolute R/G/B channel errors over the fixed neutral
    silhouette. Motion uses EVERY consecutive 2 s pair from 0 through 10 s,
    both seeds, fps 0/12, all poses/views. Crowd tests use t=2/4/6/8/10:
    the authored t=0 still is deliberately identical for previously static
    looks. Motion-only seed errors subtract each seed's own t=0 RGB first.
    Smoothness compares 1/60 s vs 2 s at identical starts, fps 0 only.
    """
    out.mkdir(parents=True, exist_ok=True)
    result = dict(metric='Absolute display RGB channel error over neutral silhouette, clipped [0,1], composited on RGB .035; mean and pooled-channel p95',
                  size=600, times=PERCEPT_TIMES, seed_times=PERCEPT_TIMES[1:],
                  limits=PERCEPT_LIMITS, gated_ids=LEGACY_ANIMATED, looks={})
    names = dict(NAMES); names.update(dict(enumerate(MATRIX_NAMES,78))); names.update(dict(enumerate(CINEMATIC_NAMES,85)))
    for look in range(64,101) if looks is None else looks:
        rows=[]
        for pose in ('bind','raised','turned'):
            r.pose=pose; r.camera_center_z=sum(r.pose_bounds[pose])*.5
            scale=600/(.85*r.height*.5*(r.pose_bounds[pose][1]-r.pose_bounds[pose][0]))
            for camera,angle in VIEWS.items():
                r.camera=angle
                mask=r.frame(current,1,scale=scale)[...,3]>0
                for scene in ((False,True) if look==64 else (False,)):
                    for fps in (0,12):
                        opts=dict(scale=scale,scene=scene,effect_fps=fps)
                        frames=[[r.frame(current,look,time=t,phase=seed,**opts) for t in PERCEPT_TIMES] for seed in SEEDS]
                        motion=[];smooth=[];seed_errors=[];motion_seed=[]
                        for seed,sequence in zip(SEEDS,frames):
                            for t,a,b in zip(PERCEPT_TIMES,sequence,sequence[1:]):
                                delta=rgb_change(a,b,mask);motion.append(dict(time=t,seed=seed,**delta))
                                if fps==0:
                                    near=r.frame(current,look,time=t+1/60,phase=seed,**opts)
                                    short=rgb_change(a,near,mask)
                                    smooth.append(dict(time=t,seed=seed,mae=short['mae'],p95=short['p95'],
                                                       mae_ratio=short['mae']/max(delta['mae'],1e-12),
                                                       p95_ratio=short['p95']/max(delta['p95'],1e-12)))
                        for i,t in enumerate(PERCEPT_TIMES[1:],1):
                            seed_errors.append(dict(time=t,**rgb_change(frames[0][i],frames[1][i],mask)))
                            residual=[display_rgb(sequence[i])[mask]-display_rgb(sequence[0])[mask] for sequence in frames]
                            motion_seed.append(float(np.abs(residual[0]-residual[1]).mean()))
                        row=dict(pose=pose,camera=camera,fps=fps,scene=scene,
                                 motion_mae=min(d['mae'] for d in motion),motion_p95=min(d['p95'] for d in motion),
                                 seed_mae=min(d['mae'] for d in seed_errors),seed_p95=min(d['p95'] for d in seed_errors),
                                 motion_seed_mae=min(motion_seed),
                                 adjacent_mae_ratio=max((d['mae_ratio'] for d in smooth),default=0.),
                                 adjacent_p95_ratio=max((d['p95_ratio'] for d in smooth),default=0.),
                                 motion=motion,seeds=seed_errors,adjacent=smooth)
                        rows.append(row)
        summary={key:min(row[key] for row in rows) for key in ('motion_mae','motion_p95','seed_mae','seed_p95','motion_seed_mae')}
        summary.update({key:max(row[key] for row in rows) for key in ('adjacent_mae_ratio','adjacent_p95_ratio')})
        failures=[dict(reason=reason,context={k:row[k] for k in ('pose','camera','fps','scene')})
                  for row in rows for reason in perceptibility_failures(row)] if look in LEGACY_ANIMATED else []
        result['looks'][str(look)]=dict(name=names.get(look,'Retired (shader fallback)' if look in (68,72) else 'Legacy'),gated=look in LEGACY_ANIMATED,
                                      summary=summary,failures=failures,matrix=rows)
        print('PERCEPT',look,summary,'FAIL' if failures else 'PASS' if look in LEGACY_ANIMATED else 'REPORT ONLY',flush=True)
        (out/'perceptibility_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    table=['| Look | 2 s MAE min | 2 s p95 min | Seed MAE min | Seed p95 min | Motion-only seed MAE min | 1/60 s / 2 s MAE max | p95 ratio max | Mode |',
           '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
    for look,data in result['looks'].items():
        v=data['summary']
        table.append(f"| {look} {data['name']} | {v['motion_mae']:.6f} | {v['motion_p95']:.6f} | {v['seed_mae']:.6f} | {v['seed_p95']:.6f} | {v['motion_seed_mae']:.6f} | {v['adjacent_mae_ratio']:.6f} | {v['adjacent_p95_ratio']:.6f} | {'GATE' if data['gated'] else 'report only'} |")
    (out/'perceptibility_table.md').write_text(result['metric']+'\n\n'+'\n'.join(table)+'\n',encoding='utf-8')
    # The same classifier must reject static, synchronized, low-amplitude and
    # high-frequency motion; the pre-animation real rendering fails all five.
    base=r.program(G.SourceTree('69cfd4637fd'),('HAS_SKIN',))
    r.pose='bind';r.camera=0.;r.camera_center_z=sum(r.pose_bounds['bind'])*.5
    scale=600/(.85*r.height*.5*(r.pose_bounds['bind'][1]-r.pose_bounds['bind'][0]))
    mask=r.frame(current,1,scale=scale)[...,3]>0
    controls={}
    for look in LEGACY_ANIMATED:
        delta=rgb_change(r.frame(base,look,time=2,scale=scale),r.frame(base,look,time=4,scale=scale),mask)
        assert delta['mae'] < .012 or delta['p95'] < .04, ('pre-animation perceptibility control escaped',look,delta)
        controls[str(look)]=delta
    good=dict(motion_mae=.02,motion_p95=.06,seed_mae=.02,motion_seed_mae=.02,adjacent_mae_ratio=.01,adjacent_p95_ratio=.01)
    for field,value in (('motion_mae',.0015),('motion_p95',.039),('seed_mae',.0005),('motion_seed_mae',0.),('adjacent_mae_ratio',.25),('adjacent_p95_ratio',.25)):
        assert perceptibility_failures(dict(good,**{field:value})), ('perceptibility classifier insensitive',field)
    result['negative_controls']=controls
    (out/'perceptibility_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    r.pose='bind';r.camera=0.
    return result


def legacy_animation_checks(out, r, tree, current):
    """Real SL still similarity replaces exact parity for LEGACY_ANIMATED.

    At t=0, silhouette display RGB MAE <= .01 and p99 <= .03. Authored alpha
    stays exact at three times. Every other look retains exact image parity
    against the pre-animation commit, including looks 85-100.
    """
    out.mkdir(parents=True, exist_ok=True)
    base_ref = '69cfd4637fd'
    base = G.SourceTree(base_ref)
    previous = r.program(base, ('HAS_SKIN',))
    result = dict(base=base_ref, metric='silhouette display RGB absolute error',
                  limits=dict(mae=.01, p99=.03, alpha_max_error=0.), looks={})
    # No shader edits outside the five reviewed branches, including uniforms.
    def untouched(source):
        for look in LEGACY_ANIMATED:
            pattern = r'    else if \(ghostLook == '+str(look)+r'\) // Studio:[\s\S]*?(?=    else if \(ghostLook ==)'
            source, count = re.subn(pattern, f'    // reviewed legacy animation {look}\n', source)
            assert count == 1, ('legacy branch boundary', look)
        return source
    assert untouched(base.read(F)) == untouched(tree.read(F)), 'unreviewed shader edit'
    for look in LEGACY_ANIMATED:
        rows = []
        for pose in ('bind', 'raised', 'turned'):
            r.pose = pose
            bottom, top = r.pose_bounds[pose]
            r.camera_center_z = (bottom+top)*.5
            scale = 600/(.85*r.height*.5*(top-bottom))
            for camera, angle in VIEWS.items():
                r.camera = angle
                mask = r.frame(current, 1, scale=scale)[..., 3] > 0
                for seed in SEEDS:
                    for scene in ((False, True) if look == 64 else (False,)):
                        opts = dict(scale=scale, phase=seed, scene=scene)
                        before = r.frame(previous, look, time=0., **opts)
                        after = r.frame(current, look, time=0., **opts)
                        error = np.abs(display_rgb(before)[mask]-display_rgb(after)[mask])
                        mae, p99 = float(error.mean()), float(np.quantile(error, .99))
                        assert mae <= .01 and p99 <= .03, ('legacy still similarity', look, pose, camera, seed, scene, mae, p99)
                        for time in (0., 2.37, 9.97):
                            a = r.frame(previous, look, time=time, **opts)
                            b = r.frame(current, look, time=time, **opts)
                            assert np.array_equal(a[..., 3], b[..., 3]), ('legacy authored alpha changed', look, pose, camera, seed, scene, time)
                        rows.append(dict(pose=pose, camera=camera, seed=seed, scene=scene, mae=mae, p99=p99, alpha_max_error=0.))
        result['looks'][str(look)] = rows
        # Each strip has front/profile/back rows and four time columns.
        r.pose = 'bind'; r.camera_center_z = sum(r.pose_bounds['bind'])*.5
        scale = 600/(.85*r.height*.5*(r.pose_bounds['bind'][1]-r.pose_bounds['bind'][0]))
        images = {}
        cell_w, cell_h = 720, 656
        font = ImageFont.truetype('arial.ttf', 16)
        for label, program in (('before', previous), ('after', current)):
            strip = Image.new('RGB', (4*cell_w, 3*cell_h), '#111111')
            draw = ImageDraw.Draw(strip)
            for row, (camera, angle) in enumerate(VIEWS.items()):
                r.camera = angle
                neutral = r.frame(current,1,scale=scale)[...,3]>0
                yy,xx=np.where(neutral[::-1]); box=(max(0,int(xx.min())-12),max(0,int(yy.min())-12),min(r.width,int(xx.max())+13),min(r.height,int(yy.max())+13))
                for col, time in enumerate((0., 2., 4., 6.)):
                    path = out/f'{look}_{label}_{camera}_{col}.png'
                    save(r.frame(program, look, time=time, scale=scale, phase=SEEDS[0]), path)
                    with Image.open(path) as thumb:
                        thumb=thumb.crop(box)
                        thumb.save(path)
                        strip.paste(thumb, (col*cell_w+(cell_w-thumb.width)//2, row*cell_h+28))
                    draw.text((col*cell_w+8, row*cell_h+8), f'{look} {NAMES[look]} {label} {camera} t={time}', fill='white', font=font)
            strip.save(out/f'{look}_{label}_strip.png'); images[label] = strip
        comparison = Image.new('RGB', (4*cell_w, 6*cell_h), '#111111')
        comparison.paste(images['before'], (0, 0)); comparison.paste(images['after'], (0, 3*cell_h))
        comparison.save(out/f'{look}_comparison.png')
        r.camera=0.
        neutral=r.frame(current,1,scale=scale)[...,3]>0
        yy,xx=np.where(neutral[::-1]);box=(max(0,int(xx.min())-12),max(0,int(yy.min())-12),min(r.width,int(xx.max())+13),min(r.height,int(yy.max())+13))
        crowd=Image.new('RGB',(2*cell_w,2*cell_h),'#111111');draw=ImageDraw.Draw(crowd)
        crowd_seeds=(*SEEDS,40123*(2*np.pi/65536.),59001*(2*np.pi/65536.))
        for index,seed in enumerate(crowd_seeds):
            row,col=divmod(index,2)
            path=out/f'{look}_crowd_seed{index}.png'
            save(r.frame(current,look,time=4.,phase=seed,scale=scale),path)
            with Image.open(path) as im:
                im=im.crop(box);im.save(path)
                crowd.paste(im,(col*cell_w+(cell_w-im.width)//2,row*cell_h+28))
            draw.text((col*cell_w+8,row*cell_h+8),f'{look} {NAMES[look]} seed={seed:.5f} t=4.0',fill='white',font=font)
        crowd.save(out/f'{look}_crowd_seed_tile.png')
    p = RealAvatar(320, 256, r.gl)
    old, new = p.program(base, ('HAS_SKIN',)), p.program(tree, ('HAS_SKIN',))
    count = 0
    for look in discover_looks():
        if look in LEGACY_ANIMATED:
            continue
        for pose in ('bind', 'raised', 'turned'):
            p.pose = pose; p.camera_center_z = sum(p.pose_bounds[pose])*.5
            for angle in VIEWS.values():
                p.camera = angle
                for time in (0., 2.37, 9.97):
                    for seed in SEEDS:
                        assert np.array_equal(p.frame(old, look, time=time, phase=seed), p.frame(new, look, time=time, phase=seed)), ('pre-animation exact parity', look, pose, angle, time, seed)
                        count += 1
    result['unchanged_parity'] = dict(cases=count, max_error=0.0)
    result['crowd_tile'] = dict(time=4.,seeds=crowd_seeds,body_height_px=600)
    (out/'legacy_animation_metrics.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    note(f'- PASS legacy t=0 similarity (silhouette display RGB MAE <= .01, p99 <= .03), exact authored alpha; all other looks parity 0.0 ({count} real-avatar cases).')
    return result


def _check(out=None,looks=None):
    out=out or ROOT/'.tmp/looks16/renders/safety'
    out.mkdir(parents=True,exist_ok=True)
    note('\n## Real SL avatar safety run\n\nStatus: RUNNING')
    legacy_controls=legacy_negative_controls()
    note('- PASS legacy negative controls: SHA-256 growth pin, looks 85-100, new checks and stale entries.')
    tree=G.SourceTree('WORKTREE')
    # One context, three framebuffers. The 4K viewport is a central 2304 px
    # crop of 3840x2160 with whole-image screen uniforms. It retains the full
    # 1800 px avatar and >300 px margin; never downsamples the tested pixels.
    runners=[RealAvatar(2304,2160),None,None]
    runners[1]=RealAvatar(1024,768,runners[0].gl)
    runners[2]=RealAvatar(320,256,runners[0].gl)
    programs=[r.program(tree,('HAS_SKIN',)) for r in runners]
    names=dict(NAMES);names.update(dict(enumerate(MATRIX_NAMES,78)));names.update(dict(enumerate(CINEMATIC_NAMES,85)))
    result=dict(renderer=runners[0].gl.renderer,assets=runners[0].assets,
                skeleton_sha256=hashlib.sha256((CHAR/'avatar_skeleton.xml').read_bytes()).hexdigest(),
                shader_sha256={p:hashlib.sha256(tree.read(p).encode()).hexdigest() for p in (V,F)},
                thresholds=dict(luminance=LUMA_CEILING,white_fraction=.25,dilation_px=2,aa_rms=AA_RMS,aa_p99=AA_P99),
                times=TIMES,perceptibility_limits=PERCEPT_LIMITS,looks={})
    result['legacy_summary']=dict(sha256=validate_legacy_known(),
        known=[dict(look=look, check=check, **meta) for (look,check),meta in sorted(LEGACY_KNOWN.items())],
        warnings=[], stale_entries=[], failures={}, negative_controls=legacy_controls)
    result['clone_seeds']=dict(uuid_low16=(7614,25764),phases=SEEDS)
    result['legacy_animation'] = legacy_animation_checks(out.parent/'legacy_anim', runners[1], tree, programs[1])
    from clone_looks_legacy_animation_check import check as legacy_stability_check
    result['legacy_animation_stability'] = legacy_stability_check(out.parent/'legacy_anim', runners[1], tree, programs[1])
    result['perceptibility'] = perceptibility_checks(out.parent/'legacy_anim', runners[1], tree, programs[1])
    result['anatomy']=anatomy(runners[1],tree,programs[1])
    result['negative_controls']=negative_controls(runners[1],tree,programs[1])
    note('- Negative controls rejected: displaced white shell, static Magma, default-parked Decode, ignored texture alpha, wrong -Y anatomy, ignored clone seeds, NaN.')
    # Cache the exact neutral silhouette for every camera/pose/size.
    masks={}
    for index,(r,pr) in enumerate(zip(runners,programs)):
        for pose in ('bind','raised','turned'):
            r.pose=pose
            bottom,top=r.pose_bounds[pose]
            r.camera_center_z=(bottom+top)*.5
            scale=(1800,600,130)[index]/(.85*r.height*.5*(top-bottom))
            for camera,angle in VIEWS.items():
                r.camera=angle
                a=r.frame(pr,1,scale=scale)
                mask=a[...,3]>0
                assert mask.sum()>50,('empty silhouette',index,pose,camera)
                assert not np.any(mask[[0,-1]]) and not np.any(mask[:,[0,-1]]),('clipped silhouette',index,pose,camera)
                masks[index,pose,camera]=(mask,mask_dilate(mask),scale)
    strips=[]
    for look in looks or discover_looks():
        failures=list(result['perceptibility']['looks'][str(look)]['failures']);rows=[];strip=Image.new('RGB',(3*288,3*356),'#111111');draw=ImageDraw.Draw(strip)
        def fail(reason,context):
            if not any(f['reason']==reason for f in failures):failures.append(dict(reason=reason,context=context))
        for index,(r,pr) in enumerate(zip(runners,programs)):
            for pose in ('bind','raised','turned'):
                r.pose=pose
                r.camera_center_z=sum(r.pose_bounds[pose])*.5
                for camera,angle in VIEWS.items():
                    r.camera=angle
                    mask,dilated,scale=masks[index,pose,camera]
                    opts=dict(scale=scale,screen_size=(3840,2160) if index==0 else (r.width,r.height),
                              frag_offset=(768.,0.) if index==0 else (0.,0.))
                    for fps in (0,12):
                        first=None;previous=None;deltas=[]
                        worst_luma=worst_white=0.
                        for ti,t in enumerate(TIMES):
                            try:a=r.frame(pr,look,time=t,effect_fps=fps,phase=SEEDS[0],**opts)
                            except AssertionError as error:
                                fail(str(error),[index,pose,camera,fps,t]);continue
                            bad,m=hazards(a,mask,dilated)
                            for reason in bad:fail(reason,[index,pose,camera,fps,t,m])
                            worst_luma=max(worst_luma,m['mean_luminance']);worst_white=max(worst_white,m['saturated_white'])
                            if previous is not None:deltas.append(float(np.mean(np.abs(a[mask,:3]-previous))))
                            previous=a[mask,:3].copy()
                            if first is None:first=a.copy()
                            if index==1 and pose=='bind' and fps==0 and ti in (0,3,6):
                                save(a,out/f'{look}_{camera}_{ti}.png')
                                with Image.open(out/f'{look}_{camera}_{ti}.png') as thumb:
                                    thumb.thumbnail((288,324),Image.Resampling.LANCZOS)
                                    col=(0,3,6).index(ti);row=list(VIEWS).index(camera)
                                    strip.paste(thumb,(col*288+(288-thumb.width)//2,row*356+28))
                                    draw.text((col*288+8,row*356+8),f'{look} {names.get(look,"New look")} {camera} t={t}',fill='white')
                        if max(deltas,default=0.)<=1e-6:fail('frozen',[index,pose,camera,fps,deltas])
                        if look in LEGACY_ANIMATED and min(deltas,default=0.)<=1e-6:
                            fail('static animation dwell',[index,pose,camera,fps,deltas])
                        b=r.frame(pr,look,time=TIMES[0],effect_fps=fps,phase=SEEDS[1],**opts)
                        seed_error=float(np.mean(np.abs(first[mask]-b[mask]))) if first is not None else 0.
                        if seed_error<=1e-6:fail('identical crowd seeds',[index,pose,camera,fps,seed_error])
                        rows.append(dict(size=(1800,600,130)[index],pose=pose,camera=camera,fps=fps,
                                         max_mean_luminance=worst_luma,max_saturated_white=worst_white,
                                         adjacent_rgb_deltas=deltas,seed_delta=seed_error))
        r=runners[1];pr=programs[1];r.pose='bind';r.camera=0.;r.camera_center_z=sum(r.pose_bounds['bind'])*.5
        for only in ({'extra_invisible'},{'hidden_head','hidden_upper_body','hidden_lower_body'}):
            r.only=only;r.show_hidden=True
            if np.any(r.frame(pr,look)):fail('invisible faces' if 'extra_invisible' in only else 'hidden system body',sorted(only))
        r.only=None;r.show_hidden=False
        a=r.frame(pr,look);r.show_hidden=True;b=r.frame(pr,look);r.show_hidden=False
        if not np.array_equal(a,b):fail('hidden system body changes replay',float(np.max(np.abs(a-b))))
        for part in ('extra_hair','extra_blend'):
            r.only={part};r.include_blend=True
            a=r.frame(pr,look);b=r.frame(pr,1)
            if not np.array_equal(a[...,3],b[...,3]):fail('authored hair alpha',[part,float(np.max(np.abs(a[...,3]-b[...,3])))])
        r.only=None;r.include_blend=False
        held_a=r.frame(pr,look,time=.501,effect_fps=12)
        held_b=r.frame(pr,look,time=.54,effect_fps=12)
        if not np.array_equal(held_a,held_b):fail('Effect fps hold',12)
        aa=[];r=runners[0];pr=programs[0];r.pose='bind';r.camera_center_z=sum(r.pose_bounds['bind'])*.5
        for camera,angle in VIEWS.items():
            r.camera=angle
            metrics=aa_check(r,pr,look,masks[0,'bind',camera][2],2.37,SEEDS[0])
            aa.append(dict(camera=camera,**metrics))
            if metrics['rms']>=AA_RMS or metrics['p99']>=AA_P99:fail('4K AA instability',aa[-1])
        strip.save(out/f'{look}_strip.png');strips.append((look,strip.copy()))
        if look in LEGACY_ANIMATED and not any(f['reason'] != 'authored hair alpha' for f in failures):
            note(f'- {look} {names[look]}: PASS motion, crowd seeds, Effect fps hold, no static dwell (authored hair alpha policy unchanged).')
        failures,warnings,stale=classify_legacy(look,failures)
        summary=result['legacy_summary']
        summary['warnings'].extend(warnings)
        summary['stale_entries'].extend(stale)
        if failures:summary['failures'][str(look)]=failures
        for warning in warnings:
            note(f"- WARN LEGACY_KNOWN {look} / {warning['check']} [{warning['date']}]: {warning['reason']}")
        result['looks'][str(look)]=dict(name=names.get(look,'New look'),failures=failures,
                                      legacy_warnings=warnings,stale_entries=stale,matrix=rows,aa=aa)
        (out/'safety_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        note(f'- {look} {names.get(look,"New look")}: '+('FAIL: '+', '.join(f['reason'] for f in failures) if failures else ('WARN: LEGACY_KNOWN' if warnings else 'PASS'))+' (378 time/size/view/pose/FPS combinations; seeds, alpha and 4K AA).')
    sheet=Image.new('RGB',(4*432,((len(strips)+3)//4)*534),'#111111')
    for i,(look,strip) in enumerate(strips):
        strip.thumbnail((432,534),Image.Resampling.LANCZOS);sheet.paste(strip,((i%4)*432,(i//4)*534))
    sheet.save(out/'safety_contact_sheet.png')
    failures={look:row['failures'] for look,row in result['looks'].items() if row['failures']}
    note('- Safety failures: '+json.dumps(failures))
    assert not failures, 'Real SL safety gate failed; see safety_metrics.json and REPORT_SAFETY.md'
    return result


def check(out=None,looks=None):
    try:
        result=_check(out,looks)
    except Exception as error:
        note('\nStatus: BLOCKED - '+str(error))
        raise
    note('\nStatus: COMPLETE'+(' (development subset only)' if looks is not None else ''))
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--looks',type=int,nargs='+',help='development subset; normal gate always discovers all looks')
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    from clone_looks_runtime_check import check as runtime_check
    runtime_check()
    check(args.out,args.looks)
