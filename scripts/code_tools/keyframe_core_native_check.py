"""Execute WP1 production math, real history clock, B1 and semantic mutants."""
from pathlib import Path
import subprocess
from native_check_tools import check_directory, compile_run

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
SOURCES = ('alkeyframecore.cpp', 'alkeyframecodec.cpp', 'alkeyframehistoryadapter.cpp')


def production_run(directory, sources, fixture, name='clean', run=True):
    for file, source in sources.items():
        (directory / file).write_bytes(source.encode('utf-8'))
    source = '\n'.join('#include "' + s + '"' for s in sources) + '\n' + fixture
    return compile_run(directory, source, name=name,
                       flags=(f'/I"{NV}"', f'/I"{NV / "tests"}"', f'/I"{ROOT / "indra/llcommon"}"', '/O2', '/utf-8'), run=run)


def check():
    directory = check_directory('keyframe_core')
    originals = {s: (NV / s).read_text(encoding='utf-8') for s in SOURCES}
    fixture = (NV / 'tests/alkeyframecore_native.cpp').read_text(encoding='utf-8')
    clean = production_run(directory, originals, fixture)
    print(clean.stdout + clean.stderr, end='', flush=True)
    assert clean.returncode == 0, 'clean executable fixtures failed'
    core, codec, adapter = SOURCES
    negatives = [
        ('reject_large_aim', 'F8jump', 'F8jump crossing recovers within bound', [(core, 'double turn=angle(transport,Quat{});', 'double turn=angle(transport,Quat{});if(turn>150*degree) return presented;')]),
        ('snap_large_aim', 'F8jump', 'F8jump crossing smooth bound', [(core, 'if(turn>720*degree*dt) transport=', 'if(false) transport='), (core, 'if(turn>most) sources[j]=unit(slerp(lastPresented,sources[j],most/turn));', '')]),
        ('snap_large_aim_transport_only', 'F8jump', 'F8jump discontinuity smooth bound', [(core, 'if(turn>720*degree*dt) transport=', 'if(false) transport=')]),
        ('unbounded_final_presentation', 'F8jump', 'F8jump laid crossing smooth bound', [(core, 'if(turn>most) sources[j]=unit(slerp(lastPresented,sources[j],most/turn));', '')]),
        ('lose_gesture_boundary', 'F12cancel', 'F12cancel rekey keeps both history boundaries', [(core, 'if(gestureCurrent) mExpectedRevision=mGesture->revision;', '')]),
        ('raw_coincident_tangents', 'F2', 'F2 finite', [(core, 'if(nodes.empty() || length(v-nodes.back())>.001)', 'if(true)')]),
        ('uniform_u', 'F3', 'F3 cruise flat', [(core, 'arc.curve.at(arc.inverse(s))', 'arc.curve.at(u)')]),
        ('per_piece_slope_clamp', 'F5', 'F5 C1', [(core, 'auto a=c.knots[j-1],b=c.knots[j];double h=', 'auto a=c.knots[j-1],b=c.knots[j];a.slope=std::min(a.slope,(b.s-a.s)/(b.t-a.t));b.slope=std::min(b.slope,(b.s-a.s)/(b.t-a.t));double h=')]),
        ('unchecked_speed_A', 'F6', 'F6 fixed point', [(core, 'if(e<=candidate/4)', 'if(true)')]),
        ('pi_over_two_lift', 'F7', 'F7 ladder <=175deg', [(core, 'std::min(pi/4,(175*degree-delta[k])/2)', '(pi/2)')]),
        ('local_high_angle_zero', 'F7', 'F7 shared tangent', [
            (core, 'zero[i]=zero[i+1]=true;', 'zero[i]=zero[i];'),
            (core, 'omega[i]*(duration/6)', '(delta[i]>170*degree?Vec3{}:omega[i])*(duration/6)'),
            (core, 'omega[i+1]*(-duration/6)', '(delta[i]>170*degree?Vec3{}:omega[i+1])*(-duration/6)')]),
        ('hard_reference', 'F8', 'F8 max step', [(core, 'double p=std::abs(step.z);', 'double p=std::abs(step.z);if(p>std::cos(10*degree)) presented=levelAim(step,roll);')]),
        ('no_substeps', 'F8', 'F8 24/144 agreement', [(core, 'int n=int(std::ceil(dt/hmax));', 'int n=1;')]),
        ('full_h_last_step', 'F8time', 'F8 sum(h)==dt', [(core, 'double h=dt/n;', 'double h=hmax;')]),
        ('plain_CR_fov', 'F9', 'F9 overshoot', [(core, 'pchip(c.timing[i-1].duration,c.timing[i].duration,d(i-1),d(i))', '(d(i-1)+d(i))/2')]),
        ('reacquire_snap', 'F10', 'F10 no snap', [(core, 'Vec3 start=continuous?previous:sample;', 'if(!continuous) filtered=sample;Vec3 start=continuous?previous:sample;')]),
        ('private_history_clock', 'F12b', 'F12b interleave order', [(adapter, 'label,alNextUndoSequence()', 'label,1')]),
        ('drop_roll', 'F13', 'F13 round-trip', [(codec, 'values.push_back(num(*p));', 'values.push_back(num(&v==&k.values.back()?0:*p));')]),
    ]
    for name, selected, assertion, edits in negatives:
        mutated = dict(originals)
        for file, before, after in edits:
            assert mutated[file].count(before) == 1, (name, before)
            mutated[file] = mutated[file].replace(before, after)
        exe = production_run(directory, mutated, fixture, name=name, run=False)
        failed = subprocess.run([str(exe), selected], cwd=ROOT, text=True, capture_output=True)
        output = failed.stdout + failed.stderr
        assert failed.returncode == 1 and ('FAIL ' + assertion + '\n') in output, (name, failed.returncode, output)
        assert output.count('FAIL ') == 1, output
        print('PASS negative [exec] ' + name + ': ' + output.strip(), flush=True)
    # Restore generated sources and recompile/run the clean production tree after all mutants.
    clean = production_run(directory, originals, fixture, name='clean_after_negatives')
    print(clean.stdout + clean.stderr, end='', flush=True)
    assert clean.returncode == 0
    print('PASS clean WP1 after 18 semantic negatives; kit arbitration INCONCLUSIVE until in-world')


if __name__ == '__main__':
    check()
