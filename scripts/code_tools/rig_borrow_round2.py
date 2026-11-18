"""Production ownership classification and UI services for borrow regressions.

The ownership functions are extracted whole, including exclusions and Shot-key
metadata filtering. Settings names/comments come from production settings.xml.
"""
import json
import xml.etree.ElementTree as ET
from rig_shadow_presets_check import function


def ownership_fixture(root):
    header = (root/'indra/newview/alpresetlook.h').read_text(encoding='utf8')
    source = (root/'indra/newview/alkitpresets.cpp').read_text(encoding='utf8')
    enum = header[header.index('enum Family'):header.index(';', header.index('enum Family'))+1]
    return ('namespace ALPresetLook {\n' + enum + '\n' +
            function(source, 'std::vector<std::string> select(') + '\n' +
            function(source, 'const std::vector<std::string>& owned(') + '\n}\n')


def history_services(root, source):
    source = source.replace('    T* get() const { return ptr; }',
                            '    T* get() const { return ptr; }\n    operator T*()const{return ptr;}')
    source = source.replace('struct LLControlVariable {', '''
struct LLControlVariable;
struct LLControlGroup {struct ApplyFunctor {virtual void apply(const std::string&,LLControlVariable*)=0;};};
struct LLControlVariable {
    std::string comment;const std::string& getComment()const{return comment;}
''')
    source = source.replace('struct HistorySettings {', '''struct HistorySettings {
    std::map<std::string,LLControlVariable> controls;
    bool getBOOL(const std::string&)const{return true;}void setBOOL(const std::string&,bool){}
    void applyToAll(LLControlGroup::ApplyFunctor* f){for(auto& [n,c]:controls)f->apply(n,&c);}
''')
    source = source.replace('getControl(const std::string&) { return {}; }',
                            'getControl(const std::string& n) {auto it=controls.find(n);return it==controls.end()?LLHandle<LLControlVariable>():LLHandle<LLControlVariable>(&it->second);}')
    settings = ET.parse(root/'indra/newview/app_settings/settings.xml').getroot()[0]
    entries = list(settings)
    seed = []
    for key, value in zip(entries[::2], entries[1::2]):
        pairs = list(value)
        comment = next((pairs[i+1].text or '' for i in range(0, len(pairs), 2)
                        if pairs[i].text == 'Comment'), '')
        seed.append('gSavedSettings.controls['+json.dumps(key.text)+'].comment='+json.dumps(comment)+';')
    source += '\nstruct SeedSettings {SeedSettings(){'+''.join(seed)+'}} seedSettings;\n'
    source = source.replace('struct ALKitShell : LLView {', '''
struct Popover {bool visible=true;std::string snapshot;bool getVisible()const{return visible;}void hide(){visible=false;}};
struct ALKitShell : LLView {
    Popover* mOpenPopover=nullptr;bool hidePopover();uuid_vec_t castSelection()const{return {LLUUID{"new-body"}};}
''')
    # Round-one fixtures extract the full production rekey/prune methods but
    # have no derived session lights. Keep those new services explicit rather
    # than stripping calls out of the production method bodies.
    return ('#include <cstring>\n#define LL_INFOS(...) std::cerr\n'
            'void pruneRemovedExtraRims() {}\n'
            'void rewriteExtraRimActorReferences(const LLUUID&,const LLUUID&) {}\n' + source)


def registry_fixture(root):
    """Run the complete production registry construction, with renderer services doubled."""
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    header = (root/'indra/newview/alcinelightrigmanager.h').read_text(encoding='utf8')
    begin = header.index('constexpr U8 ACTOR_ONLY_ORPHAN_BIT')
    end = header.index('constexpr F64 FOCUS_MARGIN_M', begin)
    support = r'''
namespace RigRegistryRound2 {
const LLUUID gAgentID{"self"};
struct LLVOAvatar {
    LLUUID id;explicit LLVOAvatar(LLUUID v={}):id(v){}virtual ~LLVOAvatar()=default;
    bool isDead()const{return false;}const LLUUID& getID()const{return id;}
    LLUUID getActorFxOwnerId()const{return id;}bool isControlAvatar()const{return false;}
    const LLVOAvatar* getAttachedAvatar()const{return nullptr;}LLVOAvatar* asAvatar(){return this;}
};
struct LLGhostAvatar:LLVOAvatar {};
struct LLDirectorCast {
    std::map<LLUUID,LLVOAvatar> avatars;
    static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
    LLVOAvatar* resolve(const LLUUID& id,bool){auto it=avatars.find(id);return it==avatars.end()?nullptr:&it->second;}
};
struct ObjectList {LLVOAvatar* findObject(const LLUUID& id){return LLDirectorCast::instance().resolve(id,false);}} gObjectList;
struct Settings {bool controlExists(const char*)const{return false;}bool getBOOL(const char*)const{return false;}} gSavedSettings;
struct ALCineLightRig {
    bool mActorOnlyRequested=false,group=true;LLUUID anchor;uuid_vec_t members;U32 slots=3;
    std::string litGroup;
    const LLUUID& getAnchor()const{return anchor;}LLUUID getObjectTarget()const{return {};}
    bool bounceLightsSet()const{return false;}bool catchlightActorOnlyEffective()const{return false;}
    bool actorOnlyAll()const{return true;}bool lightActorOnly(S32)const{return true;}
    LLUUID catchlightId()const{return {};}LLUUID omniId(S32)const{return {};}
    LLUUID projectorId(S32 i)const{return i==0?LLUUID{"emitter"}:LLUUID();}
    const std::string& getLitGroup()const{return litGroup;}const uuid_vec_t& lastResolvedMembers()const{return members;}
    bool isGroupEnabled()const{return group;}U32 lastResolvedGroupSlots()const{return slots;}
    LLVOAvatar* resolveSlotAvatar()const{return LLDirectorCast::instance().resolve(anchor,false);}
};
'''
    support += header[begin:end]
    support += r'''
struct ALCineLightRigManager {
    using Slot=ALCineLightRigSlot;static constexpr S32 SLOT_COUNT=5;static constexpr Slot SLOT_SELF=Slot::SELF;
    ALCineLightRig mInstances[5];U32 mActorOnlyObjectSlots=0;ActorOnlyRegistry mActorOnlyRegistry;
    struct ActorOnlyRecipient {LLVOAvatar* mAvatar=nullptr;LLUUID mAvatarId,mFxOwner;U8 mBit=0;};
    std::vector<ActorOnlyRecipient> mActorOnlyRecipients;
    std::map<LLUUID,std::unique_ptr<ALCineLightRig>> mMirrors;
    std::vector<int> mExtraRims;
    std::vector<ActorOnlyRig> mExtraRimRegistryInputs;
    std::vector<LLVOAvatar*> mExtraRimRegistryAvatars;
    bool mActorOnlyOverflowLogged=false;static inline bool sAnyActorOnly=false;
    bool isSlotEnabled(Slot)const{return true;}
    LLUUID slotActor(Slot slot)const{return mInstances[static_cast<int>(slot)].anchor;}
    LLUUID groupSlotActor(Slot)const;void rebuildActorOnlyRegistry();void rebuildActorOnlyRegistryWrongSelf();
    // Extra-rim behavior has its own production contract/TUT gate. This
    // fixture continues to exercise the unchanged rig/group registry path.
    void appendExtraRimActorOnly(std::vector<ActorOnlyRig>&,std::vector<LLVOAvatar*>&,size_t&) {}
    void finishExtraRimActorOnlyRegistry() {}
};
'''
    support += function(manager, 'LLUUID ALCineLightRigManager::groupSlotActor(')
    build = function(manager, 'void ALCineLightRigManager::rebuildActorOnlyRegistry()')
    support += build
    support += build.replace('::rebuildActorOnlyRegistry()', '::rebuildActorOnlyRegistryWrongSelf()', 1).replace(
        'const LLUUID actor = groupSlotActor(', 'const LLUUID actor = slotActor(')
    support += r'''
void test(){
    ALCineLightRigManager m;auto& cast=LLDirectorCast::instance();
    for(LLUUID id:{gAgentID,LLUUID{"Alice"},LLUUID{"extra"}})cast.avatars.emplace(id,LLVOAvatar(id));
    m.mInstances[0].anchor={"Alice"};m.mInstances[1].anchor={"extra"};m.mInstances[0].mActorOnlyRequested=true;
    m.rebuildActorOnlyRegistry();
    const auto correct=[&](){const auto& r=m.mActorOnlyRegistry;
        return r.mRecipients.size()==2 && r.byteForOwner(gAgentID)!=0 && r.byteForOwner({"Alice"})==0 &&
            r.byteForOwner({"extra"})!=0 && r.actorOnlyBits({"emitter"})==3 && m.mActorOnlyRecipients.size()==2;};
    assert(correct());m.rebuildActorOnlyRegistryWrongSelf();assert(!correct());
    std::cout<<"PASS: complete production actor-only registry includes You and anchored extra, excludes anchored SELF actor; old SELF consumer rejected\n";
}
}
'''
    return support


def run_registry(root):
    """Execute this module's production-registry oracle when run directly."""
    from native_check_tools import check_directory, compile_run, viewer_native_flags
    from rig_shadow_presets_check import SHIM
    from rig_sway_recovery import extended_shim
    header = (root/'indra/newview/alcinelightrig.h').read_text(encoding='utf8')
    source = (extended_shim(SHIM) + '\n#include <memory>\n'
              '#define LL_INFOS(...) std::cerr\n#define LL_WARNS(...) std::cerr\n' +
              function(header, 'enum class ALCineLightRigSlot') + ';\n' +
              registry_fixture(root) + '\nint main(){RigRegistryRound2::test();}\n')
    flags, libraries = viewer_native_flags()
    result = compile_run(check_directory('rig_borrow_registry'), source,
                         'registry', flags=flags + ['/UNDEBUG'])
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout.strip(), flush=True)


if __name__ == '__main__':
    from pathlib import Path
    run_registry(Path(__file__).resolve().parents[2])
