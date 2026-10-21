"""Phase 2 GPU fixture: production gather/filter and moving-camera RGBA8 readbacks.
Executed in relight_shader_check.py's WGL context. No viewer is launched.
"""
if __name__ == '__main__' and 'link_production' not in globals():
    import subprocess, sys
    from pathlib import Path
    raise SystemExit(subprocess.call([sys.executable,str(Path(__file__).with_name('relight_shader_check.py')),'--area-only']))
area_size=32
area_gather=link_production('class3/deferred/areaShadowF.glsl','','area gather production')
area_filter=link_production('class3/deferred/areaShadowFilterF.glsl','','area filter production')
getattrloc=gl('glGetAttribLocation',I,U,C.c_char_p)
area_vbo=U();genbuf(1,C.byref(area_vbo));bindbuf(0x8892,area_vbo)
area_vertices=np.array([-1,-1,0,3,-1,0,-1,3,0],np.float32)
bufferdata(0x8892,area_vertices.nbytes,area_vertices.ctypes.data,0x88E4)
def area_use(pr):
    use(pr)
    index=getattrloc(pr,b'position');assert index>=0
    bindbuf(0x8892,area_vbo);enableattr(index);attrib(index,3,0x1406,0,0,None)
def area_texture(unit,data,internal=0x8814):
    texture=U();genTex(1,C.byref(texture));active(0x84C0+unit);bindTex(0x0DE1,texture)
    param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2600)
    param(0x0DE1,0x2802,0x812F);param(0x0DE1,0x2803,0x812F)
    width=data.shape[1] if data is not None else area_size
    height=data.shape[0] if data is not None else area_size
    teximage(0x0DE1,0,internal,width,height,0,0x1908,0x1406,data.ctypes.data if data is not None else None)
    return texture
area_depth=np.full((area_size,area_size,4),.7,np.float32) # view Z -4
area_normal=np.zeros_like(area_depth);area_normal[:,:,0:2]=.5 # encoded +Z
area_texture(1,area_depth);area_texture(2,area_normal)
area_output=area_texture(0,None,0x8058)
area_fbo=U();genfbo(1,C.byref(area_fbo));bindfbo(0x8D40,area_fbo);attachment(0x8D40,0x8CE0,0x0DE1,area_output,0)
assert status(0x8D40)==0x8CD5
viewport(0,0,area_size,area_size)
area_use(area_gather)
identity=np.eye(4,dtype=np.float32);projection=identity.copy();projection[2,2]=-.1
inverse=identity.copy();inverse[2,2]=-10
for name,value in [(b'projection_matrix',projection),(b'inv_proj',inverse)]:
    matrix(loc(area_gather,name),1,0,value.ctypes.data_as(C.POINTER(F)))
ui(loc(area_gather,b'depthMap'),1);ui(loc(area_gather,b'normalMap'),2)
ui(loc(area_gather,b'area_count'),4);ui(loc(area_gather,b'area_samples'),8);ui(loc(area_gather,b'area_steps'),24)
uf[3](loc(area_gather,b'area_params'),8,.1,1)
uf[1](loc(area_gather,b'area_light_bias'),.25)
uf[2](loc(area_gather,b'area_trace_res'),area_size,area_size)
uniform4fv=gl('glUniform4fv',None,I,I,C.POINTER(F))
uniform4uiv=gl('glUniform4uiv',None,I,I,C.POINTER(U))
area_lights=np.array([[.8,0,-1,.3],[.8,0,-1,.3],[-.8,0,-1,0],[.8,0,-1,.3]],np.float32)
area_ids=np.array([12345,9999,7,54321],np.uint32)
def area_inputs(lights=area_lights,ids=area_ids):
    uniform4fv(loc(area_gather,b'area_lights'),4,lights.ctypes.data_as(C.POINTER(F)))
    uniform4uiv(loc(area_gather,b'area_seeds'),1,ids.ctypes.data_as(C.POINTER(U)))
def area_read():
    draw(4,0,3);pixels=np.empty((area_size,area_size,4),np.uint8)
    read(0,0,area_size,area_size,0x1908,0x1401,pixels.ctypes.data)
    return pixels
area_inputs()
empty=area_read();assert np.all(empty==255),'unblocked plane must be lit'
# An on-screen foreground slab blocks rays crossing it from the left receiver.
area_depth[:,16:,:]=.6 # view Z -2
# Re-upload depth into the existing sampler unit using a fresh fixture texture.
area_texture(1,area_depth)
blocked=area_read()
assert np.any(blocked[:,:16,0]<255),'positive occlusion control'
assert np.any((blocked[:,:16,0]>0)&(blocked[:,:16,0]<255)),'area penumbra control'
# Execute production planning/policy, then use the 1080p Cinematic plan in
# this small GPU fixture. Compare all seven persistent channels across K=7/8.
import runpy
policy=runpy.run_path(str(Path(__file__).with_name('relight_area_policy_check.py')))['check_policy']()
ui(loc(area_gather,b'area_samples'),policy['samples']);ui(loc(area_gather,b'area_steps'),policy['steps'])
persistent={}
for visible in (7,8):
    planes=[]
    for plane in (0,1):
        area_inputs(area_lights,area_ids+plane*100)
        ui(loc(area_gather,b'area_count'),min(4,visible-plane*4))
        planes.append(area_read())
    persistent[visible]=planes
for channel in range(7):
    plane,component=divmod(channel,4)
    assert np.array_equal(persistent[7][plane][:,:,component],persistent[8][plane][:,:,component]),'K7/K8 per-light visibility identity'
print('[RelightQ] K-change PASS: requested K8, 1080p Cinematic S2/steps32; seven persistent channels byte-identical',flush=True)
area_inputs();ui(loc(area_gather,b'area_count'),4)
ui(loc(area_gather,b'area_samples'),8);ui(loc(area_gather,b'area_steps'),24)
# Budget split draws must preserve each RGBA channel, its UUID seed and stale output.
color_mask=gl('glColorMask',None,C.c_ubyte,C.c_ubyte,C.c_ubyte,C.c_ubyte)
for channels in (1,2,3,4):
    for first in range(0,4,channels):
        last=min(4,first+channels)
        color_mask(*[int(first<=i<last) for i in range(4)])
        ui(loc(area_gather,b'area_first'),first);ui(loc(area_gather,b'area_count'),last)
        split=area_read()
    color_mask(1,1,1,1)
    assert np.array_equal(split,blocked),'split draws preserve all channel visibility'
ui(loc(area_gather,b'area_first'),0);ui(loc(area_gather,b'area_count'),4)
print('[RelightQ] budget split PASS: 1/2/3/4 channels per draw byte-identical',flush=True)
for repeat in range(8):
    assert area_read().tobytes()==blocked.tobytes(),'fixed-frame gather determinism'
# Moving the same ID/light to a different channel must move identical pixels.
area_inputs(area_lights[[3,2,0,1]],area_ids[[3,2,0,1]])
permuted=area_read();assert np.array_equal(permuted[:,:,2],blocked[:,:,0]),'ID seeded, not channel seeded'
area_inputs()
# Off-screen rays must stay lit: a light entirely to the left of the receiver.
ui(loc(area_gather,b'area_count'),1)
area_inputs(np.array([[-100,0,-1,0]]*4,np.float32))
offscreen=area_read();assert np.all(offscreen[:,:16,0]==255),'off-screen counts as lit'
# In-world regression: wall at Z=-4, lamp 5 cm in front, enclosed by a
# 0.30 x 0.30 x 0.45 m box housing. Orthographic camera sees the housing's
# front and the surrounding wall. Medium: K=4, S=8, steps=12, 8m/.35m.
area_depth[:]=.7
coords=(np.arange(area_size)+.5)/area_size*2-1
xx,yy=np.meshgrid(coords,coords)
housing=(xx>=.65)&(xx<=.95)&(abs(yy)<=.15)
area_depth[housing]=(.355+1)/2 # housing front Z=-3.55
area_texture(1,area_depth)
wall_lights=np.array([[.8,0,-3.95,0]]*4,np.float32)
area_inputs(wall_lights)
ui(loc(area_gather,b'area_count'),4);ui(loc(area_gather,b'area_samples'),8);ui(loc(area_gather,b'area_steps'),12)
uf[3](loc(area_gather,b'area_params'),8,.35,.5)
uf[2](loc(area_gather,b'area_trace_res'),area_size,area_size)
wall_region=(xx>=-.5)&(xx<=.5)&(abs(yy)<.3)
wall_lit=area_read()
for thick in (.001,.1,.35,1.,4.):
    uf[3](loc(area_gather,b'area_params'),8,thick,.5)
    assert np.array_equal(area_read()[wall_region],wall_lit[wall_region]),'thickness must not change housed-lamp visibility'
uf[3](loc(area_gather,b'area_params'),8,.35,.5)
assert np.min(wall_lit[wall_region])/255>=1-1/8-1/255,'wall pool stays lit within one Medium ray (RGBA8 tolerance)'
# Bias edits are live uniforms on this SAME linked program. The housing's
# disk directions can cross the housing: zero bias must expose that occlusion.
uf[1](loc(area_gather,b'area_light_bias'),0)
unbiased=area_read()
assert np.mean(unbiased[wall_region])<np.mean(wall_lit[wall_region]),'live housing bias preserves area-source pool'
uf[1](loc(area_gather,b'area_light_bias'),.25)
assert area_read().tobytes()==wall_lit.tobytes(),'live bias on/off/on without relink'
# Actual half-size viewport/readback, then return to full trace resolution.
# The depth/normal inputs remain full sized, just as in the viewer.
area_size=16;viewport(0,0,area_size,area_size)
uf[2](loc(area_gather,b'area_trace_res'),area_size,area_size)
half_lit=area_read()
for thick in (.001,.1,.35,1.,4.):
    uf[3](loc(area_gather,b'area_params'),8,thick,.5)
    assert np.array_equal(area_read(),half_lit),'half-res thickness must not change housed-lamp visibility'
uf[3](loc(area_gather,b'area_params'),8,.35,.5)
half_coords=(np.arange(area_size)+.5)/area_size*2-1
half_x,half_y=np.meshgrid(half_coords,half_coords)
half_region=(half_x>=-.5)&(half_x<=.5)&(abs(half_y)<.3)
assert np.min(half_lit[half_region])/255>=1-1/8-1/255,'half-resolution wall pool stays lit within one Medium ray'
area_size=32;viewport(0,0,area_size,area_size)
uf[2](loc(area_gather,b'area_trace_res'),area_size,area_size)
# Move the source forward so the visible slab front actually crosses the ray,
# outside the light-end exclusion (a front-depth column alone is insufficient).
area_inputs(np.array([[.3,0,-2.5,0]]*4,np.float32))
blocker=(xx>=-.15)&(xx<=.05)
area_depth[blocker]=.65 # Z=-3, a separate opaque object
area_texture(1,area_depth)
wall_shadow=area_read()
shadow_region=(xx<-.3)&(abs(yy)<.3)
assert np.min(wall_shadow[shadow_region])/255<.5,'separate blocker still shadows wall'
assert np.mean(wall_shadow[shadow_region])<np.mean(wall_lit[shadow_region]),'blocker discriminates housing'
area_size=16;viewport(0,0,area_size,area_size)
uf[2](loc(area_gather,b'area_trace_res'),area_size,area_size)
half_shadow=area_read()
assert np.min(half_shadow[(half_x<-.3)&(abs(half_y)<.3)])/255<.5,'half-resolution separate blocker'
area_size=32;viewport(0,0,area_size,area_size)
uf[2](loc(area_gather,b'area_trace_res'),area_size,area_size)
# Budget split draws must preserve each RGBA channel, its UUID seed and stale output.
color_mask=gl('glColorMask',None,C.c_ubyte,C.c_ubyte,C.c_ubyte,C.c_ubyte)
for channels in (1,2,3,4):
    for first in range(0,4,channels):
        last=min(4,first+channels)
        color_mask(*[int(first<=i<last) for i in range(4)])
        ui(loc(area_gather,b'area_first'),first);ui(loc(area_gather,b'area_count'),last)
        split=area_read()
    color_mask(1,1,1,1)
    assert np.array_equal(split,wall_shadow),'split draws preserve housed-lamp channels'
ui(loc(area_gather,b'area_first'),0);ui(loc(area_gather,b'area_count'),4)
print('[RelightQ] budget split PASS: 1/2/3/4 channels per draw byte-identical',flush=True)
for repeat in range(8):
    assert area_read().tobytes()==wall_shadow.tobytes(),'wall lamp determinism'
print('[RelightQ] wall lamp Medium PASS: near wall minimum visibility=%.3f, separate blocker minimum=%.3f' %
      (np.min(wall_lit[wall_region])/255,np.min(wall_shadow[shadow_region])/255),flush=True)
# Isolate a grazing point ray from the area-source housing directions.
area_inputs(np.array([[.8,0,-3.95,1.e-6]]*4,np.float32))
area_depth[:]=.7
area_depth[(xx>.3)&(abs(yy)<.15)]=.6925 # Z=-3.85, within .25 of light depth
area_texture(1,area_depth)
uf[3](loc(area_gather,b'area_params'),8,.001,1)
shallow=area_read()
assert np.all(shallow[shadow_region]==255),'no false crossing behind foreground slab'
# Restore the original filter fixture and verify direct grayscale inspection
# of every channel, including the second RGBA plane's local channel indices.
area_depth[:]=.7;area_depth[:,16:]=.6;area_texture(1,area_depth)
# Bilateral spatial filtering and upsample execute the production program.
area_source=area_texture(3,blocked[::2,::2].astype(np.float32)/255,0x8058)
area_use(area_filter)
matrix(loc(area_filter,b'inv_proj'),1,0,inverse.ctypes.data_as(C.POINTER(F)))
ui(loc(area_filter,b'depthMap'),1);ui(loc(area_filter,b'normalMap'),2);ui(loc(area_filter,b'area_source'),3)
for channel in range(4):
    ui(loc(area_filter,b'area_debug_channel'),channel+1)
    debug=area_read()
    expected=np.repeat(np.repeat(blocked[::2,::2,channel],2,axis=0),2,axis=1)
    assert np.array_equal(debug[:,:,0],expected),'visibility debug channel'
    assert np.array_equal(debug[:,:,0],debug[:,:,1]) and np.array_equal(debug[:,:,1],debug[:,:,2]),'grayscale visibility'
ui(loc(area_filter,b'area_debug_channel'),0)
uf[1](loc(area_filter,b'area_radius'),2)
for axis in [(1,0),(0,1),(0,0)]:
    uf[2](loc(area_filter,b'area_axis'),*axis)
    filtered=area_read()
    assert np.all(filtered>=0) and np.all(filtered<=255)
    assert area_read().tobytes()==filtered.tobytes(),'spatial filter determinism'
    assert np.any(filtered[:,:16,0]<255),'filter preserves occlusion'
# Discriminating silhouette tests for half-res -> full-res upsample. A sharp
# visibility step remains sharp across a depth edge, then across a normal edge.
step_mask=np.zeros((16,16,4),np.float32);step_mask[:,8:]=1
area_texture(3,step_mask,0x8058)
uf[2](loc(area_filter,b'area_axis'),0,0)
depth_edge=area_read()
assert np.all(depth_edge[:,15]==0) and np.all(depth_edge[:,16]==255),'bilateral depth edge'
area_depth[:]=.7;area_texture(1,area_depth)
area_normal[:,16:,0:2]=1 # encoded -Z
area_texture(2,area_normal)
normal_edge=area_read()
assert np.all(normal_edge[:,15]==0) and np.all(normal_edge[:,16]==255),'bilateral normal edge'
# Measure the production receiver-offset helper under a perspective camera.
# Literal pixel footprints: 1.5 * 2 * distance / (focal_length * trace_res).
probe=link_production('class3/deferred/areaShadowF.glsl','','receiver footprint probe',test_fragment='''
out vec4 frag_color;
uniform sampler2D depthMap;
vec4 getPositionWithDepth(vec2 uv, float depth);
float areaReceiverBias(vec2 uv, vec3 pos);
void main() {
    vec2 uv=vec2(.5);
    vec3 pos=getPositionWithDepth(uv,textureLod(depthMap,uv,0).r).xyz;
    frag_color=vec4(vec3(4.0*areaReceiverBias(uv,pos)),1);
}
''')
use(probe);ui(loc(probe,b'depthMap'),1)
perspective=np.zeros((4,4),np.float32)
perspective[0,0]=perspective[1,1]=1.5
perspective[2,2]=-(100+.1)/(100-.1);perspective[2,3]=-2*100*.1/(100-.1);perspective[3,2]=-1
inv_p=np.ascontiguousarray(np.linalg.inv(perspective).T)
matrix(loc(probe,b'inv_proj'),1,0,inv_p.ctypes.data_as(C.POINTER(F)))
for distance,res,expected in [(2,256,.02),(8,256,.0625),(8,128,.125),(8,512,.03125)]:
    clip=perspective@np.array([0,0,-distance,1],np.float32)
    area_depth[:]=(clip[2]/clip[3]+1)/2;area_texture(1,area_depth)
    uf[2](loc(probe,b'area_trace_res'),res,res)
    measured=area_read()[0,0,0]/255/4
    assert abs(measured-expected)<.002,('receiver bias distance/resolution',distance,res,measured,expected)
print('[RelightQ] receiver offset PASS perspective distance x4, trace resolution half/full/double, 2cm floor',flush=True)

# Two moving cameras observing the SAME world-space wall, blocker and light.
# Build exact per-frame G-buffer samples by ray/plane intersection; the gather
# and upsample are the actual linked production programs above. No history or
# shader relink occurs between frames. Odd sizes and a divisor-sized target
# exercise ceil-half sizing independently of any window/world-view rectangle.
def camera_fixture(width,height,frame):
    angle=frame*.04
    c,s=np.cos(angle),np.sin(angle)
    view=np.eye(4,dtype=np.float64)
    view[:3,:3]=[[c,0,s],[0,1,0],[-s,0,c]]
    eye=np.array([frame*.35,0,frame*.15])
    view[:3,3]=-view[:3,:3]@eye
    proj=np.zeros((4,4),np.float64)
    proj[0,0]=2.5*height/width;proj[1,1]=2.5
    proj[2,2]=-(80+.1)/(80-.1);proj[2,3]=-2*80*.1/(80-.1);proj[3,2]=-1
    # Include a sub-pixel asymmetric projection, even though viewer T2x jitter
    # is currently disabled. A future jitter path must use this exact matrix.
    proj[0,2]=(frame*.6-.2)/width;proj[1,2]=(frame*.4-.1)/height
    x,y=np.meshgrid((np.arange(width)+.5)/width*2-1,(np.arange(height)+.5)/height*2-1)
    ndc=np.stack([x,y,np.zeros_like(x),np.ones_like(x)],axis=-1)
    world=ndc@np.linalg.inv(proj@view).T
    direction=world[:,:,:3]/world[:,:,3:]-eye
    wall=eye+direction*((-6-eye[2])/direction[:,:,2])[:,:,None]
    slab=eye+direction*((-4-eye[2])/direction[:,:,2])[:,:,None]
    blocker=(abs(slab[:,:,0])<=.35)&(abs(slab[:,:,1])<3)
    position=np.where(blocker[:,:,None],slab,wall)
    clip=np.concatenate([position,np.ones((height,width,1))],axis=2)@(proj@view).T
    depth=np.repeat(((clip[:,:,2]/clip[:,:,3]+1)/2)[:,:,None],4,axis=2).astype(np.float32)
    normal=view[:3,:3]@np.array([0,0,1.])
    encoded=normal/(abs(normal).sum())
    normals=np.zeros_like(depth);normals[:,:,:2]=encoded[:2]*.5+.5
    light=(view@np.array([1.6,0,-2,1]))[:3]
    return view,proj,depth,normals,wall,blocker,light

def camera_matrix(pr,name,value):
    value=np.ascontiguousarray(value.T,dtype=np.float32)
    matrix(loc(pr,name),1,0,value.ctypes.data_as(C.POINTER(F)))

def camera_draw(pr,width,height,probe=False):
    output=area_texture(0,np.zeros((height,width,4),np.float32),0x8058)
    attachment(0x8D40,0x8CE0,0x0DE1,output,0)
    assert status(0x8D40)==0x8CD5
    viewport(0,0,width,height)
    if probe: use(pr)
    else: area_use(pr)
    draw(4,0,3)
    pixels=np.empty((height,width,4),np.uint8)
    read(0,0,width,height,0x1908,0x1401,pixels.ctypes.data)
    return pixels

def camera_visibility(fixture,half_res,stale=None,bias_res=None):
    view,proj,depth,normals,wall,blocker,light=fixture
    height,width=depth.shape[:2]
    trace_w,trace_h=((width+1)//2,(height+1)//2) if half_res else (width,height)
    area_texture(1,depth);area_texture(2,normals);area_use(area_gather)
    projection_for_draw=proj if stale is None else stale[1]
    camera_matrix(area_gather,b'projection_matrix',projection_for_draw)
    camera_matrix(area_gather,b'inv_proj',np.linalg.inv(projection_for_draw))
    ui(loc(area_gather,b'area_count'),1);ui(loc(area_gather,b'area_samples'),1)
    ui(loc(area_gather,b'area_steps'),64)
    uf[3](loc(area_gather,b'area_params'),8,.001,1)
    uf[1](loc(area_gather,b'area_light_bias'),.1)
    uf[2](loc(area_gather,b'area_trace_res'),*(bias_res or (trace_w,trace_h)))
    current_light=light if stale is None else stale[-1]
    area_inputs(np.array([[*current_light,1.e-6]]*4,np.float32))
    gathered=camera_draw(area_gather,trace_w,trace_h)
    area_texture(3,gathered.astype(np.float32)/255,0x8058)
    area_use(area_filter)
    camera_matrix(area_filter,b'inv_proj',np.linalg.inv(projection_for_draw))
    ui(loc(area_filter,b'area_debug_channel'),0)
    uf[2](loc(area_filter,b'area_axis'),0,0)
    return camera_draw(area_filter,width,height)

def shadow_edges(image):
    # Use the central wall scanline, away from the blocker and image borders.
    row=image[image.shape[0]//2,:,0]<128
    return np.flatnonzero(row[1:]!=row[:-1])+.5

texel_probe=link_production('class3/deferred/areaShadowF.glsl','','full-res texel centre probe',test_fragment='''
out vec4 frag_color;
uniform vec2 probe_res;
vec2 areaTexelUV(vec2 uv);
void main() { frag_color=vec4(areaTexelUV(gl_FragCoord.xy/probe_res),0,1); }
''')
for width,height in [(64,48),(63,47),(32,24)]:
    fixtures=[camera_fixture(width,height,frame) for frame in range(2)]
    area_texture(1,fixtures[0][2]);use(texel_probe);ui(loc(texel_probe,b'depthMap'),1)
    trace_w,trace_h=(width+1)//2,(height+1)//2
    uf[2](loc(texel_probe,b'probe_res'),trace_w,trace_h)
    centres=camera_draw(texel_probe,trace_w,trace_h,probe=True)[:,:,:2]/255
    expected_x=(((2*np.arange(trace_w)+1)*width//(2*trace_w))+.5)/width
    expected_y=(((2*np.arange(trace_h)+1)*height//(2*trace_h))+.5)/height
    # Allow one RGBA8 quantization/dither level, well below half a depth texel.
    assert np.max(abs(centres[:,:,0]-expected_x[None,:]))<=1/255,'full-res depth texel centre X'
    assert np.max(abs(centres[:,:,1]-expected_y[:,None]))<=1/255,'full-res depth texel centre Y'
    frames=[]
    for frame,fixture in enumerate(fixtures):
        # Isolate sampling/upsample error: use the SAME world-space receiver
        # bias for the full-density reference and half-density production draw.
        bias_res=((width+1)//2,(height+1)//2)
        full=camera_visibility(fixture,False,bias_res=bias_res)
        half=camera_visibility(fixture,True)
        edges,half_edges=shadow_edges(full),shadow_edges(half)
        assert len(edges)==2 and len(half_edges)==2,('moving camera shadow positive control',width,frame,edges,half_edges)
        error=np.max(abs(edges-half_edges))
        assert error<=1,('half-res alignment exceeds one full-res pixel',width,frame,error)
        view,proj,depth,normals,wall,blocker,light=fixture
        wall_view=np.concatenate([wall,np.ones((height,width,1))],axis=2)@view.T
        bias=np.maximum(.02,1.5*np.maximum(-2*wall_view[:,:,2]/(proj[0,0]*bias_res[0]),
                                          -2*wall_view[:,:,2]/(proj[1,1]*bias_res[1])))
        origin=wall.copy();origin[:,:,2]+=bias
        world_light=np.array([1.6,0,-2])
        t=(-4-origin[:,:,2])/(world_light[2]-origin[:,:,2])
        hit=origin+(world_light-origin)*t[:,:,None]
        # The geometry available to the production pass is a point-sampled
        # G-buffer. Evaluate the analytic ray hit against THAT frame's texels;
        # a continuous sub-texel blocker edge is unavailable to either pass.
        hit_clip=np.concatenate([hit,np.ones((height,width,1))],axis=2)@(proj@view).T
        hit_uv=hit_clip[:,:,:2]/hit_clip[:,:,3:]*.5+.5
        hit_pixel=np.floor(hit_uv*np.array([width,height])+1.e-3).astype(int)
        on_screen=np.all((hit_pixel>=0)&(hit_pixel<np.array([width,height])),axis=2)
        hit_pixel=np.clip(hit_pixel,0,np.array([width-1,height-1]))
        expected=on_screen&blocker[hit_pixel[:,:,1],hit_pixel[:,:,0]]&~blocker
        oracle=np.full_like(full,255);oracle[expected]=0
        oracle_edges=shadow_edges(oracle)
        assert len(oracle_edges)==2 and np.max(abs(edges-oracle_edges))<=1,('current-frame geometry oracle',width,frame,edges,oracle_edges)
        assert np.max(abs(half_edges-oracle_edges))<=1,('half-res current-frame geometry oracle',width,frame,half_edges,oracle_edges)
        assert np.array_equal(half,camera_visibility(fixture,True)),'moving camera frame determinism'
        frames.append(full)
    # A cached visibility image or stale light/projection must fail this same
    # alignment criterion. These negative controls prove the motion is visible.
    assert np.max(abs(shadow_edges(frames[0])-shadow_edges(frames[1])))>1,'one-frame lag negative control'
    stale=camera_visibility(fixtures[1],False,stale=fixtures[0],bias_res=bias_res)
    assert not np.array_equal(stale,frames[1]),'stale matrices negative control'
    print('[RelightQ] camera-motion alignment PASS %dx%d: two current-frame camera/G-buffer/light transforms, half-res edge error <=1px, lag/stale negative controls' % (width,height),flush=True)

# A blocker 20 cm behind the emitter in camera depth, but 36 cm from it in
# space, must not be erased by the 25 cm source exclusion. Both cameras see
# the same world-space crossing; the old camera-depth slab incorrectly lit it.
source_probe=link_production('class3/deferred/areaShadowF.glsl','','source-local exclusion probe',test_fragment='''
out vec4 frag_color;
uniform vec3 ray_origin;
uniform vec3 ray_light;
float areaTrace(vec3 origin, vec3 target, vec3 light, float end_gap,
                float light_bias, float max_distance, float thickness, int steps, float offset);
void main() {
    float v=areaTrace(ray_origin,ray_light,ray_light,.35,.25,8,.001,64,.5);
    frag_color=vec4(vec3(v),1);
}
''')
for frame in range(2):
    view,proj,depth,normals,wall,*_=camera_fixture(64,48,frame)
    eye=-view[:3,:3].T@view[:3,3]
    plane=eye+(wall-eye)*((-4-eye[2])/(-6-eye[2]))
    clip=np.concatenate([plane,np.ones((48,64,1))],axis=2)@(proj@view).T
    depth[:]=((clip[:,:,2]/clip[:,:,3]+1)/2)[:,:,None]
    area_texture(1,depth);area_texture(2,normals);use(source_probe)
    ui(loc(source_probe,b'depthMap'),1);ui(loc(source_probe,b'normalMap'),2)
    camera_matrix(source_probe,b'projection_matrix',proj)
    camera_matrix(source_probe,b'inv_proj',np.linalg.inv(proj))
    origin=(view@np.array([-2.5,0,-6,1]))[:3]
    light=(view@np.array([.85,0,-3.8,1]))[:3]
    uf[3](loc(source_probe,b'ray_origin'),*origin);uf[3](loc(source_probe,b'ray_light'),*light)
    source_pixels=camera_draw(source_probe,4,4,probe=True)
    assert np.all(source_pixels[:,:,:3]==0),('camera-invariant source exclusion',frame,source_pixels[0,0])
print('[RelightQ] source-local exclusion PASS: real blocker near emitter camera depth retained in both camera views',flush=True)
print('[RelightQ] area runtime PASS production gather/filter RGBA8, blocker, penumbra, off-screen lit, ID/channel permutation',flush=True)
print('[RelightQ] determinism PASS fixed 32x32 production visibility and bilateral programs, repeated byte-identical RGBA8 readbacks',flush=True)

# Raw gather only: a separated half-plane blocker must have a wider penumbra
# than a near-contact blocker. No filter or history can manufacture this result.
def contact_visibility(samples,steps,half,blocker_z,radius):
    size=128
    coords=(np.arange(size)+.5)/size*2-1
    depth=np.full((size,size,4),.7,np.float32)
    depth[:,size//2:]=(-blocker_z*.1+1)/2
    normals=np.zeros_like(depth);normals[:,:,:2]=.5
    area_texture(1,depth);area_texture(2,normals);area_use(area_gather)
    camera_matrix(area_gather,b'projection_matrix',projection)
    camera_matrix(area_gather,b'inv_proj',inverse)
    ui(loc(area_gather,b'depthMap'),1);ui(loc(area_gather,b'normalMap'),2)
    ui(loc(area_gather,b'area_count'),1);ui(loc(area_gather,b'area_samples'),samples)
    ui(loc(area_gather,b'area_steps'),steps)
    trace=size//2 if half else size
    uf[2](loc(area_gather,b'area_trace_res'),trace,trace)
    uf[3](loc(area_gather,b'area_params'),8,.35,1)
    uf[1](loc(area_gather,b'area_light_bias'),.25)
    area_inputs(np.array([[.8,0,-1,radius]]*4,np.float32))
    return camera_draw(area_gather,trace,trace)
# Execute the derived CPU radii through the production disk/contact path with
# shape disabled (the gather has no shape-master dependency).
prim_disks = {}
for name, radius in prim_source_radii.items():
    actual = contact_visibility(24,32,False,-2.8,radius)
    expected = {'tiny':.01,'housing':1,'panel':.01,'child':.2,
                'attachment':.06,'fixture':1.6,'mirror':1.6,'catchlight':.37}[name]
    assert np.array_equal(actual,contact_visibility(24,32,False,-2.8,expected)),(name,'prim disk upload')
    prim_disks[name] = actual
assert np.array_equal(prim_disks['tiny'],prim_disks['panel'])
assert np.array_equal(prim_disks['fixture'],prim_disks['mirror'])
assert not np.array_equal(prim_disks['tiny'],prim_disks['housing']),'derived disk radius positive control'
print('[RelightQ] prim disk runtime PASS tiny/housing/panel/child/attachment/fixture/mirror/catchlight; shape independent',flush=True)
for label,samples,steps,half in [('Low',4,8,True),('Medium',8,12,True),
                                  ('High',12,20,True),('Cinematic',24,32,False)]:
    contact=contact_visibility(samples,steps,half,-3.8,.3)
    separated=contact_visibility(samples,steps,half,-2.8,.3)
    def partial(image):
        wall=image[:,:image.shape[1]//2,0]
        return np.count_nonzero((wall>0)&(wall<255))
    assert partial(separated)>partial(contact),('contact hardening',label,partial(contact),partial(separated))
    assert np.array_equal(separated,contact_visibility(samples,steps,half,-2.8,.3)),'preset determinism'
    fallback=contact_visibility(samples,steps,half,-2.8,0)
    assert np.array_equal(fallback,contact_visibility(samples,steps,half,-2.8,.1)),'zero radius uses 10 cm disk'
    small=contact_visibility(samples,steps,half,-2.8,.15)
    large=contact_visibility(samples,steps,half,-2.8,.6)
    assert partial(large)>partial(small),('source radius widens penumbra',label)
    print('[RelightQ] contact hardening PASS %s S=%d steps=%d: contact/separated partial pixels %d/%d; live radius, zero fallback and deterministic raw gather' %
          (label,samples,steps,partial(contact),partial(separated)),flush=True)
