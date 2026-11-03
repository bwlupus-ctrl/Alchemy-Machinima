"""WP0 TUT membership tests and WP1 production CoverPolicy contract (no viewer build)."""
from pathlib import Path
import subprocess
import sys
from native_check_tools import compile_run
from relight_area_policy_check import method

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
OUT = ROOT / '.tmp/ckey_20261102/native'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    tests = (NV / 'tests/alactorkeyset_test.cpp').read_text(encoding='utf-8')
    tests = tests.replace('#include "linden_common.h"', '').replace('#include "../test/lltut.h"', '''
#include <stdexcept>
#include <iostream>
namespace tut {
inline void ensure(const char* label, bool pass) { if (!pass) throw std::runtime_error(label); }
template<class D> struct test_group { struct object : D { template<int N> void test(); }; test_group(const char*){} };
}
''').replace('../alactorkeyset.h', 'alactorkeyset.h')
    tests += '\nint main(){try {tut::actor_key_object t;\n' + ''.join(f't.test<{i}>();' for i in range(1, 8)) + '''
std::cout << "PASS: production TUT keyed-set tests 1-7; All/Me/Cast/actor-only inputs, entity/overlay, U-DOMAIN, orphan clone, wearer fallback\\n";
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
'''
    result = compile_run(OUT, tests, 'keyed_sets', [f'/I"{NV}"'])
    print(result.stdout, end=''); result.check_returncode()
    # Negative control uses the real production template, changing its include-clones gate.
    header = (NV / 'alactorkeyset.h').read_text(encoding='utf-8')
    negative = tests.replace('#include "alactorkeyset.h"', header.replace(
        'return includeClones &&', 'return true &&'))
    result = compile_run(OUT, negative, 'keyed_sets_bad_clones')
    assert result.returncode, 'clone-exclusion negative control escaped'
    print('PASS negative control: removing IncludeClones gate rejected by U-DOMAIN')

    # Execute the unchanged production policy functions, with identity-only fixture types.
    (OUT / 'lluuid.h').write_text('''#pragma once
using F32=float;
struct LLUUID { int id=0; LLUUID(int v=0):id(v){} static const LLUUID null;
bool operator<(const LLUUID& r)const{return id<r.id;} bool operator!=(const LLUUID& r)const{return id!=r.id;}
bool operator==(const LLUUID& r)const{return id==r.id;} };
inline const LLUUID LLUUID::null{};
''')
    (OUT / 'v2math.h').write_text('''#pragma once
enum {VX=0,VY=1,VZ=2,VW=3};
struct LLVector2 { float mV[2]{}; void set(float a,float b){mV[0]=a;mV[1]=b;} };
''')
    source = '''#include <cassert>
#include <iostream>
#include "alcoverpolicy.h"
#include "alactorkeyset.h"
class LLRenderTarget {};
namespace ALLightResponse {struct Packed {struct {float mV[4]{};} mSil,mSilAux;};}
struct ALAvatarLightResponse {
ALLightResponse::Packed data; LLUUID seen[3];
static ALAvatarLightResponse& instance(){static ALAvatarLightResponse p;return p;}
const ALLightResponse::Packed* peek(LLUUID a,LLUUID b,LLUUID c){seen[0]=a;seen[1]=b;seen[2]=c;return &data;}
};
struct ALActorChromaKey {
inline static bool sActive=true; inline static ALActorKeySet<LLUUID> sKeyed;
static bool ownerKeyed(const LLUUID&,const LLUUID&);
static bool instanceKeyed(const LLUUID&);
};
'''
    for signature in ('bool ALActorChromaKey::ownerKeyed(', 'bool ALActorChromaKey::instanceKeyed(',
                      'bool silhouetteCover(', 'bool CoverPolicy::nativeCover(',
                      'bool CoverPolicy::proxyCover(', 'bool CoverPolicy::sharedCover('):
        source += method(NV / 'alactorchromakey.cpp', signature) + '\n'
    source += '''int main(){
LLRenderTarget target; CoverPolicy sil{target,-8,"sil",false},key{target,-8,"key",true};
auto& response=ALAvatarLightResponse::instance(); LLVector2 c;
for(float amount:{0.f,.3f,.7f,1.f}) for(int haze=0;haze<2;++haze) for(int rim=0;rim<2;++rim){
 response.data.mSil.mV[VW]=amount; response.data.mSilAux.mV[VY]=(float)haze;
 response.data.mSilAux.mV[VZ]=(float)rim;
 bool expected=amount>0 && (!haze || !rim);
 assert(sil.nativeCover(11,12,c)==expected);
 if(expected){assert(c.mV[VX]==(!haze?amount:0.f));assert(c.mV[VY]==(!rim?amount:0.f));}
 assert(sil.proxyCover(10,11,12,c)==expected);
 assert(response.seen[0]==LLUUID(10) && response.seen[1]==LLUUID(11) && response.seen[2]==LLUUID(12));
 assert(sil.sharedCover(10,11,12,c)==expected);
 ALActorChromaKey::sKeyed.owners={11}; ALActorChromaKey::sKeyed.instances={10};
 assert(key.nativeCover(11,0,c) && c.mV[VX]==1.f && c.mV[VY]==0.f);
 assert(key.proxyCover(10,0,0,c) && c.mV[VX]==1.f);
 assert(!key.proxyCover(99,11,11,c));
 assert(!key.sharedCover(10,11,12,c));
}
std::cout<<"PASS: WP1 silhouette-OFF policy entry, gains haze=.3/rim=.7 independent, I-SILPARITY equations/peek order, proxy-only U-DOMAIN and D54 shared refusal\\n";
}
'''
    result = compile_run(OUT, source, 'cover_policy', [f'/I"{OUT}"', f'/I"{NV}"'])
    print(result.stdout, end=''); result.check_returncode()

    # Execute production tick with fixture scene/registry inputs. In current
    # Clone QoL mDeleting starts a visible fade, not immediate removal.
    tick_source = '''#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>
#include "lluuid.h"
#include "alactorkeyset.h"
using U32=unsigned; using U64=unsigned long long;
U32 subjects=0; bool includeClones=true, includeAnimesh=true;
template<class T> struct LLCachedControl {
 LLCachedControl(int,const char* n,T):name(n){} const char* name; T operator()()const {
 if constexpr(std::is_same_v<T,bool>) return std::string(name)=="ActorKeyIncludeAnimesh" ? includeAnimesh : includeClones; else return subjects;
 } operator T()const{return operator()();}};
int gSavedSettings=0;
struct LLCharacter {virtual ~LLCharacter()=default; inline static std::vector<LLCharacter*> sInstances;};
struct LLVOAvatar:LLCharacter {
 LLUUID id,owner; bool ghost=false,dead=false; LLVOAvatar* wearer=nullptr;
 LLVOAvatar(int i,int o,bool g=false):id(i),owner(o),ghost(g){}
 bool isDead()const{return dead;} bool isVisible()const{return true;} bool isControlAvatar()const{return wearer!=nullptr;}
 bool isGhostAvatar()const{return ghost;} LLVOAvatar* getAttachedAvatar(){return wearer;}
 LLUUID getID()const{return id;} LLUUID getActorFxOwnerId()const{return owner;}
};
struct LLDirectorCast {
 std::vector<LLUUID> ids; static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 const auto& getIds(){return ids;} LLVOAvatar* resolve(LLUUID id){for(auto* c:LLCharacter::sInstances) if(auto* a=dynamic_cast<LLVOAvatar*>(c);a&&a->id==id)return a; return nullptr;}
};
struct ALGhostStudio {
 struct Instance {LLUUID mId,mSource,mEntityId;bool mDeleting=false,mSourceRemoved=false;};
 std::vector<Instance> instances;
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 const auto& getInstances(){return instances;}
 bool isPrivateOverlayRuntime(LLUUID id){return id==LLUUID(6) || id==LLUUID(7);}
 static LLVOAvatar* resolveSourceAvatar(const Instance& i){
 for(auto* c:LLCharacter::sInstances) if(auto* a=dynamic_cast<LLVOAvatar*>(c);a&&a->id==i.mSource)return a;
 return nullptr;}
};
struct Ptr { LLVOAvatar* get()const{return nullptr;} }; Ptr gAgentAvatarp;
struct ALCineLightRigManager { struct Recipient {Ptr mAvatar;}; static ALCineLightRigManager& instance(){static ALCineLightRigManager x;return x;} std::vector<Recipient> actorOnlyRecipients(){return {};} };
std::vector<LLVOAvatar*> selection;
struct ALActorChromaKey {
 inline static bool sActive=true; inline static ALActorKeySet<LLUUID> sKeyed;
 inline static U64 sRevision=1;
 static std::vector<LLVOAvatar*> gatherKeyRecipients();
 static void tick(); static bool keepsLive(const LLVOAvatar*);
 static bool ownerKeyed(const LLUUID&,const LLUUID&); static bool instanceKeyed(const LLUUID&);
};
'''
    for signature in ('std::vector<LLVOAvatar*> ALActorChromaKey::gatherKeyRecipients()', 'void ALActorChromaKey::tick()', 'bool ALActorChromaKey::keepsLive(',
                      'bool ALActorChromaKey::ownerKeyed(', 'bool ALActorChromaKey::instanceKeyed('):
        tick_source += method(NV / 'alactorchromakey.cpp', signature) + '\n'
    tick_source += '''int main(){
LLVOAvatar self(1,100),resident(2,200),entity(3,3,true),worn(4,400), cloneworn(5,500), helper(6,6,true), helperworn(7,700);
worn.wearer=&self; cloneworn.wearer=&entity; helperworn.wearer=&helper;
LLCharacter::sInstances={&self,&resident,&entity,&worn,&cloneworn,&helper,&helperworn}; selection={&self,&resident};
auto& studio=ALGhostStudio::instance();studio.instances={{30,1,3,true,false},{31,2,0,false,false}};
using K=ALActorChromaKey;K::tick();
assert(K::instanceKeyed(30)&&K::keepsLive(&entity)&&K::ownerKeyed(3,0));
assert(K::keepsLive(&cloneworn) && !K::keepsLive(&helperworn));
includeClones=false;K::tick();assert(!K::keepsLive(&cloneworn) && !K::ownerKeyed(3,0) && !K::ownerKeyed(500,0) && !K::instanceKeyed(30) && !K::keepsLive(&helperworn)); includeClones=true;K::tick();
auto revision=K::sRevision;K::tick();assert(K::sRevision==revision);
for(auto who:{0u,3u})for(bool deferred:{false,true}){
 subjects=who;includeClones=false;selection={&self};K::tick();
 assert(!K::keepsLive(&entity)&&!K::instanceKeyed(30));(void)deferred;
}
subjects=1;includeClones=true;selection.clear();LLDirectorCast::instance().ids={30};K::tick();
assert(K::instanceKeyed(30)&&K::keepsLive(&entity)&&!K::keepsLive(&self));
studio.instances[0].mSourceRemoved=true;K::tick();assert(K::instanceKeyed(30)&&K::keepsLive(&entity));
LLDirectorCast::instance().ids.clear();selection={&self};K::tick();assert(!K::instanceKeyed(30));
subjects=0;K::tick();assert(K::ownerKeyed(400,0)&&K::ownerKeyed(999,100));
studio.instances.clear();subjects=2;selection.clear();K::tick();
assert(K::sKeyed.avatars.empty()&&K::sKeyed.owners.empty()&&K::sKeyed.instances.empty());
revision=K::sRevision;K::sActive=false;selection={&self};K::tick();
assert(K::sRevision==revision&&!K::keepsLive(&self)&&!K::ownerKeyed(100,0)&&!K::instanceKeyed(30));
std::cout<<"PASS: actual tick/keep-live/revision, deleting-but-visible clone, source-removed clone, All/Me exclusion, cast instance, wearer, removal and off guard\\n";
}
'''
    result = compile_run(OUT, tick_source, 'key_tick', [f'/I"{OUT}"', f'/I"{NV}"'])
    print(result.stdout, end=''); result.check_returncode()
    negative = tick_source.replace('            // A control worn by an entity clone belongs', '            // A control worn by an entity clone belongs')
    # Remove the production wearer-domain filter from the included template.
    header_bad = header[header.index('            // A control worn'):header.index('            if (!keyed) continue;')]
    negative = tick_source.replace('#include "alactorkeyset.h"', header.replace(header_bad, ''))
    result = compile_run(OUT, negative, 'key_tick_bad_wearer', [f'/I"{OUT}"', f'/I"{NV}"'])
    assert result.returncode, 'clone-wearer negative control escaped'
    negative = tick_source.replace('        if (ALGhostStudio::instance().isPrivateOverlayRuntime(avatar->getID())) return;', '')
    result = compile_run(OUT, negative, 'key_tick_bad_helper', [f'/I"{OUT}"', f'/I"{NV}"'])
    assert result.returncode, 'private-helper negative control escaped'
    print('PASS negative controls: clone wearer and private helper rejection')
    negative = tick_source.replace('        // Retiring instances stay visible',
                                  '        if (inst.mDeleting) continue;\n        // Retiring instances stay visible')
    assert negative != tick_source, 'fade-out negative control anchor moved'
    result = compile_run(OUT, negative, 'key_tick_bad_retirement', [f'/I"{OUT}"', f'/I"{NV}"'])
    assert result.returncode, 'premature retirement negative control escaped'
    print('PASS negative control: dropping a fading-out clone from tick is rejected')
    # Exercise the real control overrides against an unselected budget-impostored wearer.
    control_source = r'''#include <cassert>
#include <iostream>
struct LLUUID {};
struct LLVOAvatar { bool muted=false, mNeedsImpostorUpdate=false; int mLastImpostorUpdateReason=0;
 virtual bool computeNeedsUpdate(){mNeedsImpostorUpdate=false;return false;}
 virtual bool isImpostor(){return true;} bool isVisuallyMuted(){return muted;}
 bool hasEffectiveActorFx(){return false;} LLUUID getActorFxOwnerId(){return {};}
 void computeUpdatePeriod(){} };
struct ALDerenderList {static bool keepsAvatarLive(LLVOAvatar*){return false;}};
struct ALActorChromaKey {inline static bool selected=true; static bool keepsLive(LLVOAvatar*){return selected;}};
struct LLDirectorCast {static LLDirectorCast& instance(){static LLDirectorCast c;return c;} bool containsStored(LLUUID){return false;}};
struct LLControlAvatar:LLVOAvatar {LLVOAvatar* wearer=nullptr; LLVOAvatar* getAttachedAvatar(){return wearer;}
 bool computeNeedsUpdate() override; bool isImpostor() override;};
''' + method(NV / 'llcontrolavatar.cpp', 'bool LLControlAvatar::computeNeedsUpdate()') + '\n' + method(NV / 'llcontrolavatar.cpp', 'bool LLControlAvatar::isImpostor()') + r'''
int main(){LLVOAvatar wearer; LLControlAvatar control; control.wearer=&wearer;
assert(control.computeNeedsUpdate() && !control.isImpostor());
control.muted=true; assert(!control.computeNeedsUpdate() && control.isImpostor());
control.muted=false; ALActorChromaKey::selected=false; assert(!control.computeNeedsUpdate() && control.isImpostor());
std::cout<<"PASS: actual control overrides keep selected worn animesh live; mute and unselected veto\n";}
'''
    result = compile_run(OUT, control_source, 'key_control')
    print(result.stdout, end=''); result.check_returncode()
    negative = control_source.replace('ALActorChromaKey::keepsLive(this) && !isVisuallyMuted()', 'false')
    result = compile_run(OUT, negative, 'key_control_bad')
    assert result.returncode, 'control scheduling negative control escaped'
    print('PASS negative control: removing control keep-live rejected')
    pipeline = (NV / 'pipeline.cpp').read_text(encoding='utf-8')
    prism = method(NV / 'pipeline.cpp', 'void LLPipeline::renderActorKeyPrismOccluders()')
    scope = prism[prism.index('    struct ScissorBoxScope'):prism.index('    mActorKeyCoverage.bindTarget();')]
    restore = '''#include <cassert>
#include <algorithm>
#include <iostream>
using GLint=int; constexpr int GL_SCISSOR_BOX=0;
int current[4]={};
void glGetIntegerv(int, int* out){std::copy(current,current+4,out);}
void glScissor(int x,int y,int w,int h){current[0]=x;current[1]=y;current[2]=w;current[3]=h;}
int main(){for(bool enabled:{false,true}){
glScissor(3,7,23,41); {
''' + scope + '''glScissor(24,24,16,16);
} assert(current[0]==3&&current[1]==7&&current[2]==23&&current[3]==41); (void)enabled;
} std::cout<<"PASS: actual key-local scissor-box RAII restores nondefault rectangle\\n";}
'''
    result = compile_run(OUT, restore, 'scissor_restore')
    print(result.stdout, end=''); result.check_returncode()
    # Debug resources are now displayed only by K2, after the post chain.
    # K1 must never overwrite beauty or its screen sidecars for diagnostics.
    assert 'ActorKeyDebugView' not in method(NV / 'pipeline.cpp', 'void LLPipeline::buildActorKeyMatte()')
    diagnostic = method(NV / 'pipeline.cpp', 'void LLPipeline::compositeActorKey()')
    assert '.screen.bindTarget()' not in diagnostic and '.screen.flush()' not in diagnostic
    assert 'keyDebugView' in diagnostic and 'lensCoverageValid()' in diagnostic
    for gate in ('if (ALActorChromaKey::sActive && source == ActorMaskViewSource::MAIN)',
                 'if (ALActorChromaKey::sActive && actorKeyMatteValid())',
                 'if (ALActorChromaKey::sActive && mRT == &mMainRT) releaseActorKeyTargets();',
                 'if (ALActorChromaKey::sActive) releaseActorKeyTargets();'):
        assert gate in pipeline, gate
    replay = method(NV / 'pipeline.cpp', 'bool LLPipeline::renderCover(')
    assert 'sSilFrameMask' not in replay and 'mSilCoverValid' not in replay and 'ensureSilCover' not in replay
    key_entry = method(NV / 'pipeline.cpp', 'void LLPipeline::buildActorKeyCoverageAndMatte()')
    assert key_entry.index('if (!ALActorChromaKey::sActive) return;') < key_entry.index('ScopedActorMaskState')
    assert key_entry.index('renderActorKeyPrismOccluders();') < key_entry.index('mActorKeyCoverageFrame = gFrameCount;') < key_entry.index('buildActorKeyMatte();')
    print('PASS: key-off call guards; replay has no silhouette gate; last-writer/stamp order; debug only at stamped K2, without screen target self-bind or sidecar writes')

    # Execute the production radius calculation at different target heights.
    matte = method(NV / 'pipeline.cpp', 'void LLPipeline::buildActorKeyMatte()')
    radius_code = matte[matte.index('    const F32 scale ='):matte.index('    glDisable(GL_SCISSOR_TEST);')]
    radius_fixture = r'''#include <cassert>
#include <cmath>
#include <algorithm>
using F32=float; using U32=unsigned;
template<class T>T llclamp(T v,T lo,T hi){return std::clamp(v,lo,hi);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
struct Setting{float v; float operator()(){return v;}};
float calculate(U32 h,float fv,float cv){Setting feather{fv},choke{cv};
''' + radius_code + r'''(void)raw;return radius;}
int main(){assert(std::abs(calculate(540,4,0)-2)<.001);assert(std::abs(calculate(1080,4,0)-4)<.001);assert(std::abs(calculate(2160,4,0)-8)<.001);assert(std::abs(calculate(540,0,-6)-3)<.001);}
'''
    result=compile_run(OUT,radius_fixture,'key_radius_height'); result.check_returncode()
    negative=radius_fixture.replace('h / 1080.f','1.f')
    assert negative != radius_fixture
    result=compile_run(OUT,negative,'key_radius_bad_height')
    assert result.returncode, 'target-height scaling negative control escaped'
    print('PASS: production radius scales with target height; fixed-scale mutation rejected')

    # Execute the exact I-MATRIX check, including release-mode fail-closed action.
    start = pipeline.index('    if (ALActorChromaKey::sActive && actorKeyMatteValid())')
    matrix_check = pipeline[start:pipeline.index('\n    // ReShade decouple:',start)]
    matrix_source = '''#include <cassert>
#include <iostream>
using U32=unsigned; int assertions=0;
#define llassert(x) do{if(!(x))++assertions;}while(0)
struct ALActorChromaKey {inline static bool sActive=true;};
struct Fixture {
int mActorKeyProjection=4,mActorKeyModelview=7; U32 mActorKeyMatteFrame=12;
bool actorKeyMatteValid(){return true;}
void check(int actor_path_projection,int actor_path_modelview){
''' + matrix_check + '''}
};
int main(){Fixture f; f.check(4,7); assert(f.mActorKeyMatteFrame==12&&assertions==0);
f.check(5,7); assert(f.mActorKeyMatteFrame==~0u&&assertions==1);
f.mActorKeyMatteFrame=12; f.check(4,8); assert(f.mActorKeyMatteFrame==~0u&&assertions==2);
std::cout<<"PASS: actual I-MATRIX compare preserves matching stamp and rejects mismatching projection (negative control)\\n";}
'''
    result = compile_run(OUT, matrix_source, 'matrix_check')
    print(result.stdout, end=''); result.check_returncode()
    # Execute the actual consumer stamp predicate, not a duplicate of its law.
    stamp_source = '''#include <cassert>
#include <iostream>
using U32=unsigned;U32 gFrameCount=12;
struct ALActorChromaKey {inline static bool sActive=true,blocked=false;
static bool suppressed(){return blocked;}};
struct Target {U32 w=64,h=32;bool complete=true;
U32 getWidth()const{return w;} U32 getHeight()const{return h;}bool isComplete()const{return complete;}};
struct LLPipeline {
 struct Pack {Target deferredScreen;} mMainRT,aux;Pack* mRT=&mMainRT;
 U32 mActorKeyCoverageFrame=12,mActorKeyMatteFrame=12;
 const void* mActorKeyMatteRT=&mMainRT;
 U32 mActorKeyMatteWidth=64,mActorKeyMatteHeight=32;Target mActorKeyMatte;
 bool actorKeyMatteValid()const;
};
''' + method(NV / 'pipeline.cpp', 'bool LLPipeline::actorKeyMatteValid() const') + '''
int main(){LLPipeline p;assert(p.actorKeyMatteValid());
p.mActorKeyCoverageFrame=11;assert(!p.actorKeyMatteValid());p.mActorKeyCoverageFrame=12;
p.mActorKeyMatteFrame=11;assert(!p.actorKeyMatteValid());p.mActorKeyMatteFrame=12;
p.mActorKeyMatteRT=&p.aux;assert(!p.actorKeyMatteValid());p.mActorKeyMatteRT=&p.mMainRT;
p.mRT=&p.aux;assert(!p.actorKeyMatteValid());p.mRT=&p.mMainRT;
p.mActorKeyMatteWidth=63;assert(!p.actorKeyMatteValid());p.mActorKeyMatteWidth=64;
p.mActorKeyMatteHeight=31;assert(!p.actorKeyMatteValid());p.mActorKeyMatteHeight=32;
p.mActorKeyMatte.complete=false;assert(!p.actorKeyMatteValid());p.mActorKeyMatte.complete=true;
ALActorChromaKey::blocked=true;assert(!p.actorKeyMatteValid());ALActorChromaKey::blocked=false;
ALActorChromaKey::sActive=false;assert(!p.actorKeyMatteValid());
std::cout<<"PASS: actual matte validity rejects stale coverage/matte, wrong owner/target/width/height, incomplete target, suppression and key off\\n";}
'''
    result = compile_run(OUT, stamp_source, 'stamp_check')
    print(result.stdout, end=''); result.check_returncode()
    # Unrelated production paths must remain byte-identical, not broadly re-pinned.
    for path in ('alcinelightrigmanager.cpp', 'alcinelightrigmanager.h',
                 'llactormaskeligibility.cpp', 'llactormaskeligibility.h', 'alghoststudio.cpp',
                 'alcloneqol.h', 'alpovblink.cpp', 'lldirectorcast.cpp'):
        baseline = subprocess.check_output(['git', 'show', 'ab2329ba39f:indra/newview/' + path])
        assert baseline.replace(b'\r\n', b'\n') == (NV / path).read_bytes().replace(b'\r\n', b'\n'), path
    print('PASS: no key changes to main light registry/eligibility, Studio QoL/interplay/retirement, blink or cast')
    from director_actor_bar_check import actor_key_baseline
    for name in ('llactormover.cpp', 'llactormover.h', 'pipeline.cpp', 'pipeline.h'):
        current = (NV / name).read_text(encoding='utf-8')
        actor_key_baseline(current, name)
        try:
            actor_key_baseline(current + '\n// planted unrelated change\n', name)
        except AssertionError:
            pass
        else:
            raise AssertionError('WP01 allow-list hid an unrelated edit: ' + name)
    print('PASS: exact actor-bar WP01 reversal; unrelated-change negative controls rejected in all four guarded files')


if __name__ == '__main__':
    main()
