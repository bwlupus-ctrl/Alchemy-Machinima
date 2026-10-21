"""Phase 2 GPU fixture: production gather/filter, fixed 32x32 RGBA8 readbacks.
Executed in relight_shader_check.py's WGL context. No viewer is launched.
"""
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
# Bilateral spatial filtering and upsample execute the production program.
area_source=area_texture(3,blocked[::2,::2].astype(np.float32)/255,0x8058)
area_use(area_filter)
matrix(loc(area_filter,b'inv_proj'),1,0,inverse.ctypes.data_as(C.POINTER(F)))
ui(loc(area_filter,b'depthMap'),1);ui(loc(area_filter,b'normalMap'),2);ui(loc(area_filter,b'area_source'),3)
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
print('[RelightQ] area runtime PASS production gather/filter RGBA8, blocker, penumbra, off-screen lit, ID/channel permutation',flush=True)
print('[RelightQ] determinism PASS fixed 32x32 production visibility and bilateral programs, repeated byte-identical RGBA8 readbacks',flush=True)
