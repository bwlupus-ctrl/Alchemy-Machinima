"""Real-GL exact hierarchy values and bounded dispatches per completed tile."""
import os
os.environ['PYTHONUTF8']='1'
import argparse
import ctypes as C
import numpy as np
from weather_particles_gl_check import run


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',default='WORKTREE')
    a=p.parse_args()
    c=run(a.reference,cost=False,profile_only=True,benchmark_context=True,context_key='hierarchy_batch')
    n,gl=c['native'],c['gl']
    n.metrics.argtypes=[C.c_void_p]
    n.cover_minimum.argtypes=[C.c_void_p]
    n.cover_texture.restype=C.c_uint
    n.cover_dirty.argtypes=[C.c_double]*3
    n.cover_animate.argtypes=[C.c_float]
    n.cover_prepare.argtypes=[C.c_double]*3+[C.c_int]
    read=gl.fn('glGetTexImage',None,C.c_uint,C.c_int,C.c_uint,C.c_uint,C.c_void_p)
    size=512
    getsize=gl.fn('glGetTexLevelParameteriv',None,C.c_uint,C.c_int,C.c_uint,C.c_void_p)
    maximum=0
    for frame in range(512):
        # Rebuild both replacement leaves and dirty patches on a moving roof.
        if frame in (0,256):
            n.cover_animate(float(frame)*.002)
            n.cover_dirty(4,0,4)
        before=np.zeros(11,np.float64);after=before.copy()
        n.metrics(before.ctypes.data)
        n.cover_prepare(0,0,0,0)
        n.metrics(after.ctypes.data)
        maximum=max(maximum,int(after[2]-before[2]))
    info=np.zeros(4,np.float64)
    n.cover_minimum(info.ctypes.data)
    assert info[1]==1,'completed hierarchy not published'
    gl.active(0x84C0);gl.bind_tex(0x0DE1,n.cover_texture())
    width=C.c_int();getsize(0x0DE1,0,0x1000,C.byref(width));size=width.value
    depth=np.empty((size,size),np.float32)
    read(0x0DE1,0,0x1902,0x1406,depth.ctypes.data)
    gl.bind_tex(0x0DE1,int(info[0]))
    expected=np.stack([depth,depth],axis=-1)
    mip=0
    while size>1:
        # Exact conservative min/max of each 2x2 block at every level.
        blocks=expected.reshape(size//2,2,size//2,2,2)
        expected=np.stack([blocks[...,0].min(axis=(1,3)),blocks[...,1].max(axis=(1,3))],axis=-1)
        size//=2
        actual=np.empty((size,size,2),np.float32)
        read(0x0DE1,mip,0x8227,0x1406,actual.ctypes.data)
        assert np.array_equal(actual,expected),'inexact hierarchy at mip '+str(mip)
        mip+=1
    # These image units belong to the caller. Even the new extra outputs must
    # restore non-default names, levels, access and formats after each tile.
    bind_image=gl.fn('glBindImageTexture',None,C.c_uint,C.c_uint,C.c_int,C.c_ubyte,C.c_int,C.c_uint,C.c_uint)
    get_image=gl.fn('glGetIntegeri_v',None,C.c_uint,C.c_uint,C.c_void_p)
    names=(0x8F3A,0x8F3B,0x8F3C,0x8F3D,0x8F3E,0x906E)
    def image_state():
        values=[]
        for unit in range(4):
            for name in names:
                value=C.c_int();get_image(name,unit,C.byref(value));values.append(value.value)
        return values
    for unit in range(4):bind_image(unit,int(info[0]),unit,0,0,0x88BA,0x8230)
    sentinel=image_state()
    n.fixture_sync()
    n.cover_dirty(4,0,4)
    for _ in range(16):
        n.cover_prepare(0,0,0,0)
        assert image_state()==sentinel,'hierarchy output image binding leaked'
    assert maximum<=3,'hierarchy tile dispatch budget exceeded: '+str(maximum)
    print('PASS exact hierarchy at every mip; maximum',maximum,'dispatches/tile',flush=True)


if __name__=='__main__':main()
