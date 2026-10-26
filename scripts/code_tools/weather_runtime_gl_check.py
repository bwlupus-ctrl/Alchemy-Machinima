"""Real-GL runtime variant OFF identity and toggle residency; no viewer build.

Run only with AlchemyTest.exe closed. Geometry/depth services are fixtures;
production rain and both lightning fragment programs execute unchanged.
"""
import ctypes as C
import numpy as np
from relight_3d_gl import GL, SourceTree, SHADERS
from weather_rain_gl_check import RainRunner, native_adapter, BASE
from weather_cpu_check import ROOT


class LightningTree:
    def __init__(self, tree, quality=False): self.tree, self.quality = tree, quality
    def read(self, path):
        text = self.tree.read(path)
        if path.endswith('/weatherLightningF.glsl'):
            # Flat G-buffer normal is the scene service, not a shader algorithm.
            text = ('#define WEATHER_LIGHTNING_QUALITY 1\n' if self.quality else '') + text
            text += '\nvec4 getNorm(vec2 uv){return vec4(0.0,0.0,1.0,0.0);}\n'
        return text


def main():
    gl=GL();current=SourceTree('WORKTREE');baseline=SourceTree(BASE)
    created=[0];original=gl.create_shader
    def create(stage): created[0]+=1;return original(stage)
    gl.create_shader=create
    rain=RainRunner(current,gl);shelter=RainRunner(current,gl,True)
    old_light=RainRunner(LightningTree(baseline),gl,shader='weatherLightningF.glsl')
    light=RainRunner(LightningTree(current),gl,shader='weatherLightningF.glsl')
    quality=RainRunner(LightningTree(current,True),gl,shader='weatherLightningF.glsl')
    count=created[0]
    values=native_adapter(current,ROOT/'.tmp/weather_port/runtime_gl/rain')(0,5)
    for covered in (False,True):
        assert np.array_equal(rain.render(values),shelter.render(values,covered=covered,cover_enabled=False)), 'Rain shelter OFF changes pixels'
    def render(runner,on):
        program=runner.program;gl.use(program)
        def scalar(name,value): gl.uf[1](gl.loc(program,name.encode()),float(value))
        def vector(name,values):
            gl.fn('glUniform%df'%len(values),None,C.c_int,*([C.c_float]*len(values)))(gl.loc(program,name.encode()),*values)
        world=np.eye(4,dtype=np.float32)
        projection=np.diag([.015,.015,.015,1.]).astype(np.float32)
        for name,matrix in [('weather_world_to_view',world),('weather_view_to_world',world),('weather_view_projection',projection)]:
            packed=np.ascontiguousarray(matrix.T)
            gl.matrix(gl.loc(program,name.encode()),1,0,packed.ctypes.data_as(C.POINTER(C.c_float)))
        for name,vals in [('weather_strike_base',(0.,0.,-20.)),('weather_strike_top',(0.,60.,-20.)),
                          ('weather_lightning_color',(.6,.7,1.)),('weather_screen_res',(127.,73.)),
                          ('weather_distance_grade',(1.,1.,1.)),('weather_bolt_bounds',(-1.,-1.,1.,1.))]:vector(name,vals)
        for name,val in {'weather_lightning_flash':int(on),'weather_lightning_bolt':int(on),
                         'weather_quality_bolt':int(on),'weather_lightning_afterglow':0,
                         'weather_lightning_width':1,'weather_lightning_brightness':4,
                         'weather_lightning_ambient':.03,'weather_far_clip':128,
                         'weather_lightning_energy_ceiling':85,'weather_cloud_scale':.42}.items():scalar(name,val)
        for name,val in {'weather_quality_tier':1,'weather_stroke_index':0,'weather_sheet_enabled':0,
                         'weather_wet_glint_enabled':0,'weather_rain_occlusion_enabled':0}.items():gl.ui(gl.loc(program,name.encode()),val)
        return runner.render({})
    old_off=render(old_light,False)
    for _ in range(8):
        assert np.array_equal(old_off,render(light,False)), 'Quality OFF differs from immutable baseline'
        assert np.array_equal(old_off,render(quality,False)), 'Inactive resident quality program changes pixels'
        assert render(light,True)[...,:3].sum()>0, 'native lightning has no visible flash'
        assert render(quality,True)[...,:3].sum()>0, 'quality lightning has no visible flash'
    assert created[0]==count, 'runtime weather toggles compile shaders'
    assert gl.fn('glGetError',C.c_uint)()==0, 'GL error'
    print('PASS Rain shelter OFF and lightning OFF byte identity; visible baseline/quality flashes; 32 toggles with zero shader compilation')


if __name__=='__main__':main()
