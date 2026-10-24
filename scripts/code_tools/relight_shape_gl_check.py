"""Production relight GLSL readbacks; run inside relight_shader_check's WGL host.

--shape-only skips the longer shadow fixtures. --shape-measure-only records a
pre-fix comparison without hiding failed assertions; --shader-ref HEAD selects
the original production sources. No shader lighting formula is copied here.
"""
import json

if 'link_production' not in globals():
    import subprocess
    import sys
    from pathlib import Path
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name(
        'relight_shader_check.py')), '--shape-only', *sys.argv[1:]]))

shape_failures = []
shape_measurements = {}


def shape_require(ok, message):
    if not ok:
        shape_failures.append(message)


shape_wrapper = '''
uniform vec3 shapeNormal;
uniform vec3 shapeLight;
uniform vec4 shapeData;
uniform float shapeRange;
uniform int shapeMode;
uniform sampler2D shapeGloss;
out vec4 result;
void main() {
    vec3 n=shapeNormal, v=vec3(0,0,1);
    if (shapeMode==0) result=vec4(relightAtten(length(shapeLight),shapeRange,.5,shapeData));
    else if (shapeMode==1) result=vec4(relightRepresentative(n,v,shapeLight,shapeData.x),1);
    else if (shapeMode==2) result=vec4(relightPbr(vec3(.6),vec3(0),.3,0,n,v,shapeLight,shapeRange,.5,shapeData,0),1);
    else if (shapeMode==3) result=vec4(relightPbr(vec3(0),vec3(.04),.3,0,n,v,shapeLight,shapeRange,.5,shapeData,1),1);
    else {
        vec3 highlight;
        vec3 total=relightLegacy(shapeGloss,vec3(0),vec4(vec3(.5),.8),n,v,shapeLight,shapeRange,.5,shapeData,highlight);
        result=vec4(highlight,1);
    }
}
'''
shape_program=program()
attach(shape_program,compile_part(math_source+shape_wrapper,'','light-shape regression'))
attach(shape_program,vs);link(shape_program);getprog(shape_program,0x8B82,C.byref(ok))
if not ok.value:
    out=C.create_string_buffer(32000);plog(shape_program,len(out),None,out)
    raise AssertionError(out.value.decode())
use(shape_program)
uniform2(loc(shape_program,b'relight_limits'),.1,100)
ui(loc(shape_program,b'shapeGloss'),1) # the existing nonzero constant LUT


def shape_sample(light, radius, mode, physical=0, gain=1, scale=1, normal=(0,0,1), range_=15):
    uf[3](loc(shape_program,b'shapeLight'),*light)
    uf[3](loc(shape_program,b'shapeNormal'),*normal)
    uf[4](loc(shape_program,b'shapeData'),radius,physical,gain,-1)
    uf[1](loc(shape_program,b'shapeRange'),range_)
    uf[1](loc(shape_program,b'relight_highlight_scale'),scale)
    ui(loc(shape_program,b'shapeMode'),mode)
    draw(4,0,3);pixels=np.empty(4,np.float32)
    read(0,0,1,1,0x1908,0x1406,pixels.ctypes.data)
    assert np.all(np.isfinite(pixels)),(light,radius,mode,pixels)
    return pixels[:3].copy()


# Short references discriminate radius-dependent calibration/regularization.
reference_values=[]
for radius in (0,.01,.03,.12,.45,.9,1):
    value=float(shape_sample((0,0,.75),radius,0,physical=1,gain=.32,range_=1.5)[0])
    reference_values.append(value)
    shape_require(abs(value-.5)<=.01,('reference exposure',radius,value))
shape_measurements['reference_attenuation']=reference_values

a=shape_sample((.0001,0,-1),1-1.e-6,1)
b=shape_sample((.0001,0,-1),1+1.e-6,1)
shape_measurements['representative_boundary_dot']=float(a@b)
shape_require(float(a@b)>.9999,('representative boundary',a.tolist(),b.tolist()))
for light in ((.5,0,-1.e-5),(.5,0,0),(.5,0,1.e-5),(0,0,0)):
    shape_sample(light,1,1)

for mode in (2,3,4):
    point=shape_sample((0,0,2),0,mode)
    hard=shape_sample((0,0,2),.45,mode,scale=0)
    soft=shape_sample((0,0,2),.45,mode)
    if mode!=2:
        shape_require(np.allclose(point,hard,rtol=3.e-6,atol=1.e-7),('specular-only scale zero',mode))
        shape_require(np.any(np.abs(soft-hard)>1.e-5),('highlight scale positive control',mode))
    else:
        shape_require(np.array_equal(soft,hard),'highlight scale leaves diffuse bytes unchanged')
    shape_measurements['mode_'+str(mode)]=dict(point=point.tolist(),hard=hard.tolist(),soft=soft.tolist())

alpha=np.sqrt(2/(.8*.8*368+2))
expected=(alpha/(alpha+.45/6))**2
actual=float(shape_measurements['mode_4']['soft'][0]/shape_measurements['mode_4']['point'][0])
shape_measurements['legacy_normalization']=actual
shape_require(abs(actual-expected)<2.e-5,('legacy energy normalization',actual,float(expected)))

# The untrusted finite horizon step is refuted: geometry drives the old and
# new expressions to zero. Preserve this behavior instead of replacing it.
horizon=[float(shape_sample((1,0,z),.45,4)[0]) for z in (.001,.0001,.00001,0,-.00001)]
shape_measurements['legacy_horizon']=horizon
shape_require(horizon[0]>horizon[1]>horizon[2]>=horizon[3]==horizon[4]==0,'continuous legacy horizon')
for mode in (0,1,2,3,4):
    first=shape_sample((.5,0,.2),.45,mode)
    shape_require(np.array_equal(first,shape_sample((.5,0,.2),.45,mode)),('deterministic shape',mode))

if '--shape-json' in sys.argv:
    Path(sys.argv[sys.argv.index('--shape-json')+1]).write_text(json.dumps(
        dict(shader_ref=shader_ref or 'worktree',measurements=shape_measurements,failures=shape_failures),indent=2)+'\n')
print('[shape-gl]', 'FAIL' if shape_failures else 'PASS',len(shape_failures),'failures;',shape_measurements,flush=True)
assert '--shape-measure-only' in sys.argv or not shape_failures,shape_failures
