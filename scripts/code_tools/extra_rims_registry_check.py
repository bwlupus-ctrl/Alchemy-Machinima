"""Exercise production actor-registry storage reuse with a native heap guard.

Four extra rims share the six identities with two ordinary actor-only lights.
The same pure production acquisition/fill functions used by the manager run
1000 warm frames. UUID value services use a fixed-size identity adapter; the
production registry/acquisition code is unmodified. A planted storage-discard
regression must fail the heap oracle.
"""
from pathlib import Path

from native_check_tools import check_directory, compile_run, viewer_native_flags

ROOT = Path(__file__).resolve().parents[2]


def run_unit(root=ROOT):
    header = (root / 'indra/newview/alcinelightrigmanager.h').read_text(encoding='utf8')
    begin = header.index('constexpr U8 ACTOR_ONLY_ORPHAN_BIT')
    end = header.index('constexpr F64 FOCUS_MARGIN_M', begin)
    source = r'''
#include "alcinelightrigmodel.h"
#include <algorithm>
#include <array>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <new>
// Fixed-size UUID value adapter avoids llcommon's own global new/delete
// overrides, so this executable can count its standard-container allocations.
struct LLUUID {
    std::array<U32,4> value{};
    explicit LLUUID(U32 id=0) { value[0]=id; }
    bool isNull()const { return value==std::array<U32,4>{}; }
    bool notNull()const { return !isNull(); }
    bool operator==(const LLUUID& other)const { return value==other.value; }
};
static bool tracking = false;
static size_t allocations = 0;
void* operator new(size_t bytes) {
    if (tracking) ++allocations;
    if (void* p=std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,size_t) noexcept { std::free(p); }
void operator delete[](void* p,size_t) noexcept { std::free(p); }
'''
    source += header[begin:end]
    source += r'''
void require(bool okay,const char* name) {
    if (!okay) { tracking=false;std::cerr<<name<<"\n";std::exit(1); }
}
LLUUID id(int value) { return LLUUID(value); }
int main() {
    std::array<LLUUID,7> actors, lights;
    for(int i=0;i<7;++i){actors[i]=id(i+1);lights[i]=id(i+21);}
    std::vector<ActorOnlyRig> inputs;
    ActorOnlyRegistry result;
    const auto frame=[&](int count) {
        size_t used=0;
        for(int i=0;i<count;++i) {
            auto& input=acquireActorOnlyRig(inputs,used);
            input.mEnabled=true;input.mActorOnlyAll=true;
            input.mProjectors[i<2?0:2]=lights[i];
            ActorOnlyIdentity person;
            person.mAvatarId=person.mFxOwner=actors[i];
            input.mRecipients.push_back(person);
        }
        inputs.resize(used);
        fillActorOnlyRegistry(inputs,false,result);
    };
    frame(7);
    require(result.mRecipients.size()==6 && result.mOverflow &&
        result.actorOnlyBits(lights[6])==ACTOR_ONLY_ORPHAN_BIT,"xr_registry_overflow_closed");
    frame(6);
    require(result.mRecipients.size()==6 && !result.mOverflow &&
        result.actorOnlyBits(lights[6])==0,"xr_registry_stale_light");
    // Warm every result/input vector once before measuring a stable live set.
    tracking=true;
    for(int n=0;n<1000;++n)frame(6);
    tracking=false;
    require(allocations==0,"xr_registry_frame_allocations");
    require(result.mRecipients.size()==6 && result.mLights.size()==6 &&
        result.mLiveRecipientBits==63,"xr_registry_recipients");
    for(int i=0;i<6;++i)
        require(result.actorOnlyBits(lights[i])==(1u<<i) &&
            result.byteForOwner(actors[i])==(1u<<i),"xr_registry_one_actor");
    const auto cold=buildActorOnlyRegistry(inputs,false);
    require(cold.mLights.size()==result.mLights.size() &&
        cold.mDrawBytes==result.mDrawBytes && cold.mLiveRecipientBits==result.mLiveRecipientBits,
        "xr_registry_feature_off_equivalence");
    for(const auto& light:cold.mLights)
        require(result.actorOnlyBits(light.mId)==light.mBits,"xr_registry_feature_off_equivalence");
    fillActorOnlyRegistry(inputs,true,result);
    require(result.mRecipients.empty() && result.mLights.empty() && result.mDrawBytes.empty() &&
        !result.mLiveRecipientBits && !result.mOverflow,"xr_registry_disabled_clear");
    frame(1);
    require(result.mRecipients.size()==1 && result.mLights.size()==1 &&
        result.actorOnlyBits(lights[5])==0,"xr_registry_stale_light");
    std::cout<<"PASS: 1000 mixed actor/4-extra registry frames allocate zero heap blocks; one-actor masks, overflow fail-closed, stale cleanup, cold-path equivalence\n";
}
'''
    flags, libraries = viewer_native_flags()
    directory = check_directory('extra_rims_registry')
    baseline = compile_run(directory, source, 'baseline', flags)
    output = baseline.stdout + baseline.stderr
    (directory / 'baseline.log').write_bytes(output.encode('utf8'))
    assert baseline.returncode == 0, output
    print(baseline.stdout, end='', flush=True)
    before = '    result.mRecipients.clear();'
    after = '    std::vector<ActorOnlyIdentity>().swap(result.mRecipients);'
    assert source.count(before) == 1, 'registry storage mutation anchor drift'
    planted = compile_run(directory, source.replace(before, after, 1), 'discard_storage', flags)
    output = planted.stdout + planted.stderr
    (directory / 'discard_storage.log').write_bytes(output.encode('utf8'))
    assert planted.returncode != 0 and 'xr_registry_frame_allocations' in output.splitlines(), output
    print('PASS: rejected planted xr_registry_frame_allocations at its named assertion', flush=True)


if __name__ == '__main__':
    run_unit()
