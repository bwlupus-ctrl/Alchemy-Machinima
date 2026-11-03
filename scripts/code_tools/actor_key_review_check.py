"""Review regressions: execute production preview predicate and footer transition with mutations."""
from pathlib import Path
from native_check_tools import compile_run
from relight_area_policy_check import method
import subprocess
NV=Path('indra/newview'); OUT=Path('.tmp/ckey_20261102/wp2/review')
OUT.mkdir(parents=True, exist_ok=True)
preview=(NV/'llsnapshotlivepreview.cpp').read_text()
layout=(NV/'llfloatersnapshot.cpp').read_text()
mover=(NV/'llactormover.cpp').read_text()
key=method(NV/'llsnapshotlivepreview.cpp','bool LLSnapshotLivePreview::keyCaptureCurrent() const')
start=layout.index('        if (!mSkipReshaping && !floaterp->isMinimized())',layout.index('void LLFloaterSnapshotBase::ImplBase::updateLayout'))
end=layout.index('\n    }\n\n    bool use_freeze_frame',start)
footer=layout[start:end]
alpha='(ALActorChromaKey::sActive && ALActorChromaKey::keyedThisFrame() && !(inst.mAlpha > 0.f))'
render=mover[mover.index('void LLActorMover::renderStudioGhosts()'):]
def require_alpha(source):
 assert alpha in source[:source.index('LLVOAvatar* av =')]
require_alpha(render)
try: require_alpha(render.replace(alpha,'false',1))
except AssertionError: pass
else: raise AssertionError('zero-alpha mutation survived')
head=r'''
#include <cassert>
#include <string>
#include <iostream>
using U64=unsigned long long;using S32=int;
struct ALActorChromaKey {inline static bool sActive=false;inline static U64 rev=1;inline static std::string refusal;
 static U64 revision(){return rev;} static const std::string& snapshotPlaceholder(){return refusal;}};
struct LLSnapshotLivePreview {U64 mKeyCaptureRevision=0;bool keyCaptureCurrent()const;};
struct Rect {int w=500,h=477;int getWidth(){return w;}int getHeight(){return h;}};
struct Floater {Rect r;bool minimized=false;int reshapes=0;Rect& getRect(){return r;}bool isMinimized(){return minimized;}
 void reshape(int w,int h){r.w=w;r.h=h;++reshapes;}};
struct Layout {bool mSkipReshaping=false,mKeyLayoutExpanded=false;int mKeyLayoutOriginalHeight=0;
 void update(Floater* floaterp) {
'''
checks=r'''
}};
int main(){
 LLSnapshotLivePreview p;assert(p.keyCaptureCurrent());ALActorChromaKey::sActive=true;
 assert(!p.keyCaptureCurrent());p.mKeyCaptureRevision=1;assert(p.keyCaptureCurrent());
 ++ALActorChromaKey::rev;assert(!p.keyCaptureCurrent());p.mKeyCaptureRevision=2;
 ALActorChromaKey::refusal="failed";assert(!p.keyCaptureCurrent());ALActorChromaKey::refusal.clear();
 Floater f;Layout l;ALActorChromaKey::sActive=false;l.update(&f);assert(f.r.h==477&&f.reshapes==0);
 ALActorChromaKey::sActive=true;l.mSkipReshaping=true;l.update(&f);assert(f.r.h==477);
 l.mSkipReshaping=false;f.minimized=true;l.update(&f);assert(f.r.h==477);
 f.minimized=false;l.update(&f);assert(f.r.h==573);l.update(&f);assert(f.r.h==573);
 ALActorChromaKey::sActive=false;f.minimized=true;l.update(&f);assert(f.r.h==573);
 f.minimized=false;l.update(&f);assert(f.r.h==477);l.update(&f);assert(f.reshapes==2);
 std::cout<<"PASS preview and footer controls\n";
}
'''
def execute(label,fn,section):
 src=head+section+checks+fn
 p=compile_run(OUT,src,label)
 return p.returncode,p.stdout
code,out=execute('review_positive',key,footer);assert code==0,out
for label,fn,section in [('stale',key.replace('mKeyCaptureRevision == ALActorChromaKey::revision()','true'),footer),('layout',key,footer.replace('!mSkipReshaping && !floaterp->isMinimized()','true'))]:
 code,out=execute('review_negative_'+label,fn,section);assert code!=0,(label,out)
print('PASS production preview/footer predicates; stale-revision, layout-guard and zero-alpha mutations rejected')

