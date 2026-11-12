"""Native lazy-page/draw-heartbeat fixture; compile production paths verbatim."""
from pathlib import Path
from native_check_tools import compile_run

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
OUT = ROOT / '.tmp/shaftfix/visibility-native'


def main():
    from shaft_probe_check import block
    OUT.mkdir(parents=True, exist_ok=True)
    pipe = (NV / 'pipeline.cpp').read_text(encoding='utf-8')
    page = (NV / 'alkitpage.cpp').read_text(encoding='utf-8')
    panel = (NV / 'alpanelprojectorshafts.cpp').read_text(encoding='utf-8')
    lazy = (NV / 'alkitlazypage.cpp').read_text(encoding='utf-8')
    ui = (NV / 'aluirefresh.h').read_text(encoding='utf-8')
    # Same real factory/construction ordering that made postBuild registration fail.
    construction = (ROOT / 'indra/llui/llpanel.cpp').read_text(encoding='utf-8')
    construction = block(construction, 'bool LLPanel::initPanelXML(')
    assert construction.index('LLUICtrlFactory::createChildren(') < construction.index('parent->addChild(this, tab_group)') < construction.index('postBuild();')
    shell = (NV / 'alkitshell.cpp').read_text(encoding='utf-8')
    assert 'lazy->ensureBuilt(); // Resolve without selecting or navigating away from the input page.' in shell
    parts = {
        'HEARTBEAT': (NV / 'alshaftstatusconsumer.h').read_text(encoding='utf-8'),
        'ON_SCREEN': block(ui, 'inline bool isWidgetOnScreen('),
        'LAZY_BUILD': block(lazy, 'LLPanel* ALKitLazyPage::ensureBuilt()'),
        'PAGE_STATUS_INIT': 'mShaftStatus = findChild<LLTextBox>("shaft_status_text");',
        'PAGE_DRAW': block(block(page, 'void ALKitPage::draw()'), 'if (mShaftStatus'),
        'PANEL_DRAW': block(block(panel, 'void ALPanelProjectorShafts::draw()'), 'if (mShaftStatus'),
        'OBSERVER_GATE': block(pipe, 'void LLPipeline::observeProjectorShafts(').splitlines()[2],
    }
    for source, sig in ((page, 'bool ALKitPage::postBuild()'), (panel, 'bool ALPanelProjectorShafts::postBuild()')):
        init = block(source, sig)
        assert parts['PAGE_STATUS_INIT'] in init
        assert 'ALShaftStatus::' not in init and 'mShaftConsumer' not in init
        assert 'ALShaftStatus::' not in block(source, 'void ' + ('ALKitPage' if source == page else 'ALPanelProjectorShafts') + '::onVisibilityChange(')
    template = (Path(__file__).parent / 'fixtures/shaft_visibility.cpp.in').read_text(encoding='utf-8')

    def compose(values):
        result = template
        for key, value in values.items():
            result = result.replace('@@' + key + '@@', value)
        assert '@@' not in result
        return result

    flags = ['/MT', '/Od', '/UNDEBUG', f'/I"{NV}"']
    result = compile_run(OUT, compose(parts), 'shaft_visibility', flags)
    print(result.stdout, flush=True)
    assert result.returncode == 0, result.stderr
    mutants = (
        ('visibility_never_expires', 'HEARTBEAT', 'frame - lastDrawFrame <= 1u', 'true'),
        ('visibility_accepts_two_old_frames', 'HEARTBEAT', '<= 1u', '<= 2u'),
        ('visibility_registered_at_build', 'PAGE_STATUS_INIT', ';', '; ALShaftStatus::markDrawn(gFrameCount);'),
        ('observer_ignores_visibility', 'OBSERVER_GATE', 'ALShaftStatus::recentlyDrawn(gFrameCount)', 'true'),
        ('page_clipped_row_still_counts', 'PAGE_DRAW', 'isWidgetOnScreen(mShaftStatus)', 'true'),
        ('panel_clipped_row_still_counts', 'PANEL_DRAW', 'isWidgetOnScreen(mShaftStatus)', 'true'),
        ('page_heartbeat_throttled', 'PAGE_DRAW', 'ALShaftStatus::markDrawn(gFrameCount);', 'if (mShaftStatusClock.due()) ALShaftStatus::markDrawn(gFrameCount);'),
        ('panel_heartbeat_throttled', 'PANEL_DRAW', 'ALShaftStatus::markDrawn(gFrameCount);', 'if (mStatusClock.due()) ALShaftStatus::markDrawn(gFrameCount);'),
    )
    for name, key, before, after in mutants:
        assert parts[key].count(before) == 1, (name, 'mutation drift')
        changed = dict(parts)
        changed[key] = changed[key].replace(before, after)
        result = compile_run(OUT, compose(changed), name, flags)
        assert result.returncode != 0, 'planted regression survived: ' + name
        print('PASS rejected planted visibility regression:', name, flush=True)
    print('PASS real lazy ensureBuilt / page + shared-panel draw paths; 8 visibility mutants rejected')


if __name__ == '__main__':
    main()
