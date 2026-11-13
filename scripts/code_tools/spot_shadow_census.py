"""Registration-derived GLSL reachability inventory for native spot-map sampling."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]
SHADERS=ROOT/'indra/newview/app_settings/shaders'


def uncomment(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: '\n'*m[0].count('\n'), text, flags=re.S)


def functions(text):
    text=uncomment(text); result={}
    for m in re.finditer(r'\b(?:void|float|int|bool|[biu]?vec[234]|mat[234]|GBufferInfo)\s+(\w+)\s*\([^;{}]*\)\s*\{',text):
        a=m.end();b=a;depth=1
        while depth:
            depth+=(text[b]=='{')-(text[b]=='}');b+=1
        result.setdefault(m[1],[]).append(text[a:b-1])
    return result


def reaches_spots(local, shared):
    seen=set();todo=['main']
    while todo:
        name=todo.pop()
        if name in seen:continue
        seen.add(name)
        for code in local.get(name,shared.get(name,[])):
            if re.search(r'\bshadowMap(?:[4-9]|1[0-9])\b',code):return True
            todo.extend(re.findall(r'\b(\w+)\s*\(',code))
    return False


def inventory():
    sites={}
    # Include auxiliary managers (Actor Key, dynamic beauty replays), not just
    # llviewershadermgr. Clone registrations inherit their root's classification.
    for p in sorted((ROOT/'indra/newview').glob('*.cpp')):
        text=p.read_text(encoding='utf-8')
        if 'mShaderFiles' not in text:continue
        text=uncomment(text)
        for m in re.finditer(r'"([^"\n]+\.glsl)"',text):
            name=re.sub(r'^app_settings/shaders/class[0-9]+/', '', m[1])
            sites.setdefault(name,[]).append('%s:%d'%(p.name,text.count('\n',0,m.start())+1))
    assert sites
    shared={}; sources={}
    for p in sorted(SHADERS.rglob('*.glsl')):
        funcs=functions(p.read_text(encoding='utf-8'));sources[p]=funcs
        if 'main' not in funcs:
            for name,bodies in funcs.items():shared.setdefault(name,[]).extend(bodies)
    consumers=set();rows=[]
    for name,registered in sorted(sites.items()):
        paths=[p for p in sources if p.relative_to(SHADERS).as_posix().partition('/')[2]==name]
        assert paths,('unresolved registration',name)
        for p in paths:
            local=sources[p]
            if 'main' not in local:continue
            spot=reaches_spots(local,shared)
            if spot:consumers.add(name)
            rows.append(dict(shader=p.relative_to(SHADERS).as_posix(),registration=registered,
                             classification='native-spot-consumer' if spot else 'no-reachable-native-spot-sample'))
    # Planted unknown consumer: a new registered root must enter the census set.
    assert reaches_spots(functions('void main(){float x=sampleSpotShadow(vec3(0),vec3(0),0,vec2(0));}'),shared)
    assert reaches_spots(functions('void main(){float x=texture(shadowMap19,vec3(0));}'),shared)
    assert not reaches_spots(functions('void main(){vec4 x=vec4(1);}'),shared)
    return consumers,rows

