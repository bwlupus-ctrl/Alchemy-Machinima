"""WP5/WP3 integration: execute the production UI guard snapshot adapter."""
from pathlib import Path
from native_check_tools import compile_run, check_directory
from relight_area_policy_check import method

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / 'indra/newview/alpanelactorkey.cpp'
source = path.read_text(encoding='utf-8')
adapter = source[source.index('struct GuardState'):source.index('std::string captureWarnings()')]
fixture = r'''
#include <string>
#include <cassert>
namespace SLKey { enum class State { Off, NoReShade, Arming, Keying, Refused }; }
struct LLReShadeBridge {
    struct Status {
        SLKey::State state = SLKey::State::Off;
        bool red = false, amber = false;
        std::string text, guarantee, overlay;
    } status;
    static LLReShadeBridge& instance() { static LLReShadeBridge bridge; return bridge; }
    const Status& keyStatus() const { return status; }
};
''' + adapter + r'''
int main() {
    auto& s = LLReShadeBridge::instance().status;
    auto off = readGuard();
    assert(!off.refused && off.pending);
    assert(off.status == "ReShade: awaiting key preflight");
    assert(off.guarantee == "Key exactness awaits guard preflight.");
    s = {SLKey::State::NoReShade, false, false, "ReShade: not loaded", "Key colour is exact.", ""};
    auto absent = readGuard();
    assert(!absent.refused && !absent.pending && absent.status == s.text && absent.guarantee == s.guarantee);
    s = {SLKey::State::Refused, true, true, "Effect activity: key refused", "Limited guarantee", "overlay open; OSD on"};
    auto refused = readGuard();
    assert(refused.refused && refused.pending && refused.guarantee == s.guarantee);
    assert(refused.status == s.text + "; " + s.overlay);
    s = {SLKey::State::Arming, false, true, "Waiting for DLLs", "Limited guarantee", ""};
    auto arming = readGuard(); assert(!arming.refused && arming.pending && arming.status == s.text);
    s = {SLKey::State::Keying, false, true, "Key bypass active", "Limited guarantee", "ReShade overlay not hidden"};
    auto keying = readGuard(); assert(!keying.refused && keying.pending && keying.guarantee == s.guarantee);
}
'''
result = compile_run(check_directory('actor_key_ui'), fixture)
assert result.returncode == 0, result.stdout + result.stderr
post = method(path, 'bool ALPanelActorKey::postBuild()')
retry = post[post.index('retry->setCommitCallback'):post.index('getChild<LLButton>("ak_selftest")')]
assert 'LLReShadeBridge::instance().retryKeyGuard();' in retry
assert 'mRevision.reset();' in retry and 'refreshReadouts();' in retry
assert 'gSavedSettings' not in retry and 'setEnabled(false)' not in post
draw = method(path, 'void ALPanelActorKey::draw()')
assert 'LLReShadeBridge::instance().keyStatus().revision' in draw
assert 'WP3 pending' not in source and 'TODO(WP3 integration)' not in source
print('PASS production guard adapter: unevaluated/off, absent, refused, arming, loaded keying, overlays and guarantee; real retry and guard revision wiring')
