"""Run the real shaft/slot TUT tests with MSVC, then reject planted regressions.

Uses this worktree's headers and the repo's TUT implementation. No viewer build,
login or server objects. Mutated headers and native products stay under .tmp.
"""
from pathlib import Path
import re
from native_check_tools import compile_run, viewer_native_flags

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / "indra/newview"
OUT = ROOT / ".tmp/shaftfix/native"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    flags, libraries = viewer_native_flags()
    flags = [f'/I"{OUT}"', f'/I"{ROOT / "indra/externals/tut"}"'] + flags
    # Only include spellings change, so isolated mutations can shadow production
    # headers without editing the real tree. Every test body is compiled intact.
    def test_source(name):
        source = (NV / "tests" / (name + "_test.cpp")).read_text(encoding="utf-8")
        source = source.replace('"../test/lltut.h"', '"' + (ROOT / "indra/test/lltut.h").as_posix() + '"')
        return re.sub(r'"\.\./(al[^"/]+\.h)"', r'"\1"', source)

    source = "\n".join(test_source(name) for name in ("alshaftdecision", "alshadowslotmath")) + r'''
#include <tut/tut_reporter.hpp>
namespace tut { test_runner_singleton runner; }
int main() {
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();
    return reporter.all_ok() ? 0 : 1;
}
'''
    original = (NV / "alshaftdecision.h").read_text(encoding="utf-8")
    header = OUT / "alshaftdecision.h"
    header.write_text(original, encoding="utf-8")
    result = compile_run(OUT, source, "shaft_tut", flags, libraries)
    print(result.stdout, flush=True)
    assert result.returncode == 0, result.stderr
    mutations = {
        "froxel_blocked_by_cone_master": ("if (s.froxel) return Reason::Froxel;", "if (s.froxel && s.master) return Reason::Froxel;"),
        "cancel_deletes_active_callback": ("done && callbacks == 0", "done"),
        "hidden_observer_runs": ("return visible || testing;", "return true;"),
        "missing_light_parameter_block": ("light.setIsLight(true);", "light.setIsLight(false);"),
        "no_flag_priority": ("userShaft ? FlaggedWorld : World", "userShaft ? World : World"),
        "rig_below_flagged": ("Rig = 3", "Rig = 1"),
        "actor_rig_above_world": ("ActorRig = 0", "ActorRig = 4"),
        "cast_shadows_ignored": ("!suppressed &&", "true &&"),
        "vcam_promoted": ("childFlag ? !childAuto", "childFlag ? true"),
        "root_overrides_auto_child": ("childFlag ? !childAuto : rootFlag && !rootAuto", "(childFlag && !childAuto) || (rootFlag && !rootAuto)"),
        "size_order_reversed": ("priority > ownerPriority", "priority < ownerPriority"),
        "zero_world_fills_empty": ("priority > 0.0", "priority >= 0.0"),
        "black_shader_accepted": ("level == 3", "level >= 1"),
        "status_master_ignored": ("if (!s.master)", "if (false)"),
        "status_no_draw_claims_drawing": ("if (s.drawn)", "if (true)"),
        "froxel_needs_cone_density": ("if (s.froxel) return Reason::Froxel;", "if (s.froxel && s.shaft) return Reason::Froxel;"),
    }
    for name, (before, after) in mutations.items():
        assert original.count(before) == 1, (name, "mutation drift")
        header.write_text(original.replace(before, after), encoding="utf-8")
        result = compile_run(OUT, source, name, flags, libraries)
        assert result.returncode != 0, "planted regression survived: " + name
        print("PASS rejected planted regression:", name, flush=True)
    header.write_text(original, encoding="utf-8")
    # Discriminate native retention with the combined production decision/slot tests.
    retention = (NV / "alshadowslotmath.h").read_text(encoding="utf-8")
    for name, before, after in (
        ("tier_retention_reversed", "return tier > challengerTier;", "return tier < challengerTier;"),
        ("retention_slot_thrash", "chosen[i] = owners[i].id; states[i] = {}; break;", "chosen[i] = ranked[i].id; states[i] = {}; break;"),
    ):
        assert retention.count(before) == 1
        target = OUT / "alshadowslotmath.h"
        target.write_text(retention.replace(before, after), encoding="utf-8")
        result = compile_run(OUT, source, name, flags, libraries)
        assert result.returncode != 0, "planted retention regression survived: " + name
        print("PASS rejected planted regression:", name, flush=True)
        target.unlink()
    print("PASS: real TUT shaft decisions and existing shadow-slot tests; 18 header/retention mutants rejected")
    from shaft_probe_check import main as probe_check
    probe_check()


def selftest():
    """Prove frozen engine identity and the newest exact-reversal allowance."""
    import subprocess
    from relight_volumetric_engine_check import function
    from director_actor_bar_check import reverse_shaft_slot_merge
    current = (NV / "pipeline.cpp").read_text(encoding="utf-8")
    base = subprocess.check_output(
        ["git", "show", "8c350a71b4e:indra/newview/pipeline.cpp"],
        encoding="utf-8").replace("\r\n", "\n")
    for name in ("renderProjectorVolumetricClassic", "renderProjectorVolumetricNew"):
        frozen = function(base, name)
        def identical(candidate):
            return function(candidate, name) == frozen
        assert identical(current), (name, "frozen body changed")
        for label, edit in (("ordinary edit", "    ; // planted edit\n"),
                            ("unmarked hook", "    ALShaftSelfTest::submitted();\n")):
            changed = current.replace(frozen, frozen.replace("{\n", "{\n" + edit, 1), 1)
            assert not identical(changed), (name, label, "mutation survived")
        print("PASS frozen base identity and planted edit/unmarked hook:", name)
    for name in ("pipeline.cpp", "pipeline.h"):
        source = (NV / name).read_text(encoding="utf-8")
        original = subprocess.check_output(
            ["git", "show", "8c350a71b4e:indra/newview/" + name],
            encoding="utf-8").replace("\r\n", "\n")
        assert reverse_shaft_slot_merge(source, name) == original
        try:
            reverse_shaft_slot_merge(source + "\n// planted unrelated edit\n", name)
        except AssertionError:
            pass
        else:
            raise AssertionError("allowance accepted unrelated edit: " + name)
        print("PASS exact reversal and rejected unrelated edit:", name)
    print("PASS: shaft slot merge integration selftest")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true",
                        help="verify frozen bodies and exact pipeline allowance, including planted edits")
    args = parser.parse_args()
    if args.selftest:
        selftest()
    else:
        main()
