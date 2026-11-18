"""Round-4 idle/gesture/registry contracts, each with a named planted negative.

Recipient, emitter, brightness, scene and body-lifetime behavior is executed by
the engine, model, round2 and round3 native gates alongside these contracts.
"""
from pathlib import Path
from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]


def run_unit(root=ROOT):
    ui = (root/'indra/newview/alpanelrigrim.cpp').read_text(encoding='utf8')
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    def check(ui, manager):
        idle = function(ui, 'void ALPanelRigRim::idle(')
        assert 0 <= idle.find('if (!panel->mExtraRefreshClock.due()) return;') < idle.index('isWidgetOnScreen('), 'xr_idle_clock_first'
        refresh = function(ui, 'void ALExtraRimUI::refresh(')
        assert 0 <= refresh.find('if (mTrim && (mTrim->isMouseHeldDown() || mTrim->isWheelSettlePending())) return;') < refresh.index('mRevision = rigs.extraRimRevision();'), 'xr_pending_trim_no_rebuild'
        rewrite = function(manager, 'void ALCineLightRigManager::rewriteExtraRimActorReferences(')
        assert 'mExtraRimRegistryDirty = true;' in rewrite.split('it = mExtraRims.erase(it);', 1)[1], 'xr_rewrite_drop_dirty'
        update = function(manager, 'void ALCineLightRigManager::updateExtraRims(')
        assert update.count('const auto recipients = extraRimRecipientCache();') == 1 and update.index('extraRimRecipientCache();') < update.index('for (auto& extra : mExtraRims)'), 'xr_recipient_tick_cache'
        assert 'extra.mAtMaxBrightness = rim->getLightIntensity() * std::clamp(extra.mTrim, 0.f, 2.f) > 1.f;' in update, 'xr_max_brightness_product'
    check(ui, manager)
    for name, u, m in [
        ('xr_idle_clock_first', ui.replace('if (!panel->mExtraRefreshClock.due()) return;', ''), manager),
        ('xr_pending_trim_no_rebuild', ui.replace('if (mTrim && (mTrim->isMouseHeldDown() || mTrim->isWheelSettlePending())) return;', ''), manager),
        ('xr_rewrite_drop_dirty', ui, manager.replace('it = mExtraRims.erase(it);\n            ++mExtraRimRevision;\n            mExtraRimRegistryDirty = true;', 'it = mExtraRims.erase(it);\n            ++mExtraRimRevision;', 1)),
        ('xr_recipient_tick_cache', ui, manager.replace('const auto recipients = extraRimRecipientCache();', 'ExtraRimRecipientCache recipients;')),
        ('xr_max_brightness_product', ui, manager.replace('rim->getLightIntensity() * std::clamp(extra.mTrim, 0.f, 2.f) > 1.f;', 'false;')),
    ]:
        try:
            check(u, m)
        except AssertionError as error:
            assert str(error) == name, error
        else:
            raise AssertionError(name+' escaped')
        print('PASS: rejected planted '+name+' at its named source assertion', flush=True)


if __name__ == '__main__':
    run_unit()
