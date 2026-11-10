"""Gate for "an actor's gaze can aim at any nearby avatar" (Gaze page Nearby section).

Two halves, each with planted negatives that must fail:

1. NATIVE: the pure selection rules in indra/newview/algazenearby.h run natively (a tiny LLUUID
   stand-in; no viewer build). Mutated copies of the header must fail behaviourally.
2. SOURCE: structural rules on alpanellensgaze.cpp, llactormover.cpp, lldirectorcast.cpp and
   the header, checked on the real text and again on mutated text that must be rejected:
     R1  the nearby list/labels/status never show or log a UUID
     R2  refresh paths never write the model (a non-cast target is not reset to tangent)
     R3  gaze resolution of a cast-ref never needs cast membership; the tangent reset on identity
         rewrite stays gated on the retired id
     R4  scene save/load keeps the id through the same canonical paths as cast targets
     R5  no per-frame name lookup, avatar walk or allocation: polling is clock-gated in draw()
     R6  the Hot target combo rebuilds Nearby only inside its revision-gated rebuild

Run: python scripts/code_tools/director_gaze_nearby_check.py [--no-native]
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'


def read(name):
    return (NV / name).read_text(encoding='utf-8')


def body(text, signature):
    start = text.index(signature)
    i = text.index('{', start) + 1
    depth = 1
    while depth:
        c = text[i]
        depth += (c == '{') - (c == '}')
        i += 1
    return text[start:i]


def case_blocks(text, marker):
    """Every `case X:` block (up to the next `case`) that starts with `marker`."""
    out = []
    pos = 0
    while True:
        at = text.find(marker, pos)
        if at < 0:
            return out
        end = re.search(r'\bcase\s', text[at + len(marker):])
        out.append(text[at: at + len(marker) + (end.start() if end else 400)])
        pos = at + len(marker)


def call_args(text, call):
    """Top-level argument lists of every `call(` in text."""
    result = []
    pos = 0
    while True:
        at = text.find(call, pos)
        if at < 0:
            return result
        i = at + len(call)
        depth, arg, args = 1, '', []
        while depth:
            c = text[i]
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
            if depth == 0:
                break
            if c == ',' and depth == 1:
                args.append(arg.strip())
                arg = ''
            else:
                arg += c
            i += 1
        args.append(arg.strip())
        result.append(args)
        pos = i


MODEL_WRITES = ('setGazeTarget(', 'setEyeGazeTarget(', 'setGazeCastTarget(', 'setGazeTargetMode(',
                'setGazeTargetConfig(', 'clearEyeGazeTarget(', 'setHotGazeSlot(', 'GAZE_TANGENT',
                'GazeTarget::MOTION', 'editDetailTargetsFor(', 'editGazeTargetsFor(')
NAME_WORK = ('LLAvatarNameCache', 'getFullname', 'sInstances', 'collectNearbyPeople(', 'personName(',
             'knownPersonName(')
LOGGING = ('LL_INFOS', 'LL_WARNS', 'LL_DEBUGS', 'LL_ERRS', 'LL_CONT')
MEMBERSHIP = ('contains(', 'getMember(', 'getStoredMember(', 'containsStored(', 'getIds(', 'getCast()',
              'mMemberIndex', 'membersInGroup(')


def draw_cues_for_count(cues):
    return body(cues, 'void ALFloaterGazeCues::draw()')


def check(gaze, mover, cast, header, cmake, hot_slots, cues, name_header, cues_panel, cues_h, cues_panel_h):
    bad = []

    def need(ok, tag, what):
        if not ok:
            bad.append(tag + ' ' + what)

    f = {name: body(gaze, sig) for name, sig in {
        'draw': 'void ALPanelLensGaze::draw()',
        'castcombo': 'void ALPanelLensGaze::refreshCastCombo(',
        'poll': 'void ALPanelLensGaze::pollNearby(',
        'livename': 'bool ALPanelLensGaze::liveNearbyName(',
        'revoked': 'bool nearbyNamesRevoked(',
        'castrevoked': 'bool ALPanelLensGaze::castNamesRevoked(',
        'rewrite': 'void ALPanelLensGaze::rewriteActorReferences(',
        'nearbyfor': 'void ALPanelLensGaze::nearbyFor(',
        'hotappend': 'void ALPanelLensGaze::appendHotNearbyChoices(',
        'hotrefresh': 'void ALPanelLensGaze::refreshHotControls(',
        'hotcommit': 'void ALPanelLensGaze::commitHotControls(',
        'live': 'void ALPanelLensGaze::refreshLiveControls(',
        'controls': 'void ALPanelLensGaze::refreshControls(',
        'collect': 'void collectNearbyPeople(',
        'personname': 'std::string personName(',
        'knownname': 'std::string knownPersonName(',
        'actorname': 'std::string actorName(',
        'castcommit': 'void ALPanelLensGaze::onCastCommit(',
    }.items()}

    # R1: names only, never a UUID, in labels, fallbacks, status text and logs.
    need('asString().substr(' not in gaze, 'R1', 'a UUID prefix is used as a label in alpanellensgaze.cpp')
    need('return id.asString().substr(' not in cues, 'R1', 'the Gaze Cues actor label falls back to a UUID prefix')
    for name in ('collect', 'personname', 'knownname', 'actorname', 'nearbyfor', 'hotappend', 'poll'):
        need('asString' not in re.sub(r'//[^\n]*', '', f[name]), 'R1', name + ' builds a name from an id')
        need(not any(t in f[name] for t in LOGGING), 'R1', name + ' logs')
    for name in ('castcombo', 'hotappend', 'hotrefresh'):
        for args in call_args(f[name], '->add('):
            label = args[0]
            need('asString' not in label and 'mId' not in label and 'getID' not in label,
                 'R1', name + ' labels a combo row with an id: ' + ' '.join(label.split()))
    need(not any(t in f['castcombo'] for t in LOGGING), 'R1', 'castcombo logs')
    status = '\n'.join(case_blocks(mover, '        case GAZE_CAST:\n        {\n            std::string name;'))
    need('ALGazeNearby::nonCastTargetName(g.mCastTarget)' in status and 'asString' not in status and
         '"a person"' in status and 'findInstanceByRuntime' in status,
         'R1', 'gaze status does not name a non-cast target by display name (clone keeps cast wording)')
    # Display names only: no user-name / legacy-name fallback anywhere a name is shown.
    need('getUserName' not in gaze, 'R1', 'a user-name fallback is used for a person')
    for name in ('personname', 'knownname', 'collect', 'livename', 'castcombo', 'castcommit', 'nearbyfor', 'poll',
                 'hotappend', 'hotcommit'):
        need(not any(t in f[name] for t in ('getFullname', 'getCompleteName', 'getUserName')), 'R1',
             name + ' uses a legacy/user name (display names only; the cast rows keep their existing actorName)')
    need('getUserName' not in status and 'getFullname' not in status, 'R1', 'the status line falls back to a user/legacy name')
    need('return ALGazeNearby::personDisplayName(id);' in f['personname'], 'R1',
         'personName is not the shared display-name rule')
    need('return name.getDisplayName(true);' in name_header and 'RlvActions::canShowName' in name_header and
         'getUserName' not in name_header and 'getFullname' not in name_header and
         'getCompleteName' not in name_header and 'asString' not in name_header and
         not any(t in name_header for t in LOGGING), 'R1',
         'the shared person-name rule is not display-name only, RLV-gated and id-free')
    # Cast members keep their existing behaviour (stored name, else the complete name); only
    # non-cast ids go through the shared display-name/RLV/@shownearby rule.
    an = f['actorname']
    need(gaze.count('getCompleteName') == 1 and 'getCompleteName' in an and 'getMember(id)' in an and
         'ALGazeNearby::nonCastTargetName(id)' in an and an.index('getCompleteName') < an.index('nonCastTargetName'),
         'R1', 'alpanellensgaze actorName: cast members must keep the complete-name fallback, non-cast ids the shared rule')
    need('return RlvActions::canShowNearbyAgents();' in name_header, 'R1', 'nearbyAllowed is not the RLV @shownearby predicate')
    # Gaze Cues (a Nearby target reaches it): same rule in the label, the editor row and both lists.
    cues_actor = body(cues, 'std::string actorName(')
    need(cues.count('getCompleteName') == 1 and cues.count('LLAvatarNameCache::get') == 1 and
         'getMember(id)' in cues_actor and 'getCompleteName' in cues_actor and
         'ALGazeNearby::nonCastTargetName(id)' in cues_actor and 'return "You";' in cues_actor and
         cues_actor.index('getCompleteName') < cues_actor.index('nonCastTargetName'), 'R1',
         'the Gaze Cues actor label: cast members keep the complete-name fallback, non-cast ids the shared rule')
    extra = body(cues, 'std::string ALFloaterGazeCues::extraTargetLabel(')
    need('ALGazeNearby::nonCastTargetName(ref)' in extra and 'ALGazeNearby::awayLabel(name)' in extra, 'R1',
         'the cue editor extra row is not labelled by display name / away')
    refresh = body(cues, 'void ALFloaterGazeCues::refreshCastCombo(')
    need('mCastTarget->add(mExtraLabel, LLSD(mExtraRef.asString()))' in refresh and
         'if (!force && !relabel && !context_changed)' in refresh and
         'const bool rows_changed = force || context_changed || label != mExtraLabel || extra_ref != mExtraRef;' in refresh and
         'mCastTarget->hideList()' in refresh, 'R2',
         'the cue editor does not offer, relabel and redact the saved non-cast target row (id stored beside label)')
    need("const std::string keep_value = force ? std::string() : mCastTarget->getSelectedValue().asString();" in refresh and
         'if (!force && selectComboByValue(mCastTarget, keep_value))' in refresh, 'R2',
         'a label refresh no longer keeps the user\'s current combo value')
    sync = body(cues, 'bool selectComboByValue(')
    need('combo->setCurrentByIndex(combo->getCurrentIndex());' in sync and cues.count('setSelectedByValue(') == 1 and
         'selectComboByValue(mCastTarget, mEditorTarget.mCastRef.asString())' in refresh and
         'selectComboByValue(mCastTarget, cue.mTarget.mCastRef.asString())' in body(cues, 'void ALFloaterGazeCues::loadEditor('),
         'R2', 'a cue-combo selection by value does not sync the combo\'s remembered index (hideList would re-select a stale row)')
    relabel = body(cues, 'bool ALFloaterGazeCues::relabelRows(')
    need('item->getValue().asInteger()' in relabel and 'cues[cue_index]' in relabel and
         'list->setNeedsSort(true)' in relabel and 'cue_index >= static_cast<S32>(cues.size())' in relabel, 'R2',
         'relabelRows does not follow each row\'s own cue index (rows may be sorted)')
    need('targetLabel(cue)' in body(cues, 'std::string ALFloaterGazeCues::targetLabelSignature('), 'R5',
         'the cue label signature omits the rendered labels')
    need(re.sub(r'//[^\n]*', '', draw_cues_for_count(cues)).count('refreshCueList(true)') == 1, 'R2',
         'the cue editor label tick may reload the cue list more than in the mismatch fallback')
    need(not any(t in relabel for t in ('deleteAllItems', 'selectNthItem', 'loadEditor', 'removeall', 'addElement')), 'R2',
         'relabelRows clears, reselects or reloads instead of updating labels in place')
    need('refreshCastCombo(true); // offers the cue' in body(cues, 'void ALFloaterGazeCues::loadEditor('), 'R2',
         'loading a cue does not offer its saved non-cast target before selecting it')
    draw_cues = body(cues, 'void ALFloaterGazeCues::draw()')
    need('mNameClock.due()' in draw_cues and 'refreshCastCombo(false, names_due)' in draw_cues and
         'relabelRows(mCueList, cues, 1)' in draw_cues and
         'targetLabelSignature' in draw_cues and 'mCueLabelSignature' in draw_cues, 'R5',
         'the cue editor does not redact/heal names on the 2 s tick')
    need('loadEditor' not in re.sub(r'//[^\n]*', '', draw_cues) and 'refreshCueList(names_due' not in draw_cues, 'R2',
         'the cue editor label tick reloads the editor (it would discard pending edits)')
    pdraw = body(cues_panel, 'void ALPanelGazeCues::draw()')
    need('mNameClock.due()' in pdraw and 'targetLabelSignature' in pdraw and 'mLabelSignature' in pdraw and
         'ALFloaterGazeCues::relabelRows(mCueList, cues, 1)' in pdraw, 'R5',
         'the Director cue page does not redact/heal names on the 2 s tick')
    need('ALLiveReadoutClock mNameClock{0.5f};' in cues_h and 'ALLiveReadoutClock mNameClock{0.5f};' in cues_panel_h,
         'R5', 'a cue name clock is no longer 2 s')
    need('canShowNearbyAgents' in name_header and 'return nearbyAllowed() ? personDisplayName(id) : std::string();' in name_header,
         'R1', 'the shared rule does not withhold a non-cast target name under @shownearby')
    need(all('RlvActions::canShowName' in text for text in
             (f['collect'], name_header, f['livename'], f['revoked'])), 'R1',
         'RLV name hiding is not honoured by the nearby list, the away label, the status line and the commit')
    need('asString' not in header and not any(t in header for t in LOGGING), 'R1',
         'algazenearby.h handles ids as text or logs')
    need('LLUUID::validate(e.mName)' in header and 'LLUUID::validate(name)' in header, 'R1',
         'the pure selection no longer rejects a UUID-looking name')
    need('entry.mName.empty()' in f['collect'] and 'continue; // no display name cached yet' in f['collect'], 'R1',
         'a person with no cached display name is no longer skipped')

    # R2: refresh paths read; they never reset or rewrite a gaze target.
    for name in ('draw', 'castcombo', 'poll', 'nearbyfor', 'hotappend', 'hotrefresh', 'collect'):
        for token in MODEL_WRITES:
            need(token not in f[name], 'R2', name + ' writes the model via ' + token)
        need(not re.search(r'mCastRef\s*=[^=]', f[name]) and not re.search(r'mCastTarget\s*=[^=]', f[name]),
             'R2', name + ' assigns a cast reference')
    # Disabled header: a non-text sentinel value, added disabled, never the default row (chosen by
    # VALUE), and any commit that reaches it is refused without touching the saved target.
    header_add = 'mCast->add(ALGazeNearby::HEADER_LABEL, LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, false)'
    need(header_add in f['castcombo'], 'R2', 'the Gaze Nearby header is not a disabled sentinel row')
    need('setValueIfChanged(mCast, LLSD(first_value), true)' in f['castcombo'] and
         'setCurrentByIndex' not in f['castcombo'] and 'if (first_value.empty()) first_value' in f['castcombo'],
         'R2', 'the cast combo may select the disabled Nearby header (default must be chosen by value)')
    need('const std::string known = knownPersonName(current);' in f['castcombo'] and
         'ALGazeNearby::awayLabel(known)' in f['castcombo'] and
         '!listedAmong(nearby, current)' in f['castcombo'], 'R2',
         'the current target is no longer kept visible as "(away)" only when absent')
    need('if (mAwayPoll != mPollGeneration || mAwayFor != current)' in f['castcombo'], 'R5',
         'the away-label name lookup is not cached per poll and target (it would run per frame)')
    need('nearbyFor(mCastProjection, target_actor, current);' in f['castcombo'], 'R2',
         'the saved target is not kept in the projection before the cap')
    need('!ALGazeNearby::sameMembers(nearby, mCastNearbyShown) || away_label != mCastAwayShown' in f['castcombo'],
         'R2', 'the Gaze combo no longer rebuilds when its projection or away label changes')
    need('displayed != mCastComboActor || current != mCastComboSelection || mCastRestore ||' in f['castcombo'] and
         'mCastRestore = false;' in f['castcombo'], 'R2',
         'a refused commit can no longer force the Gaze combo back to the saved target')
    need('if (force && mCast->isListVisible())' in f['castcombo'] and 'mCast->hideList()' in f['castcombo'], 'R2',
         'a forced rebuild no longer closes an open list first')
    need('mCastAwayId = away_label.empty() ? LLUUID::null : current;' in f['castcombo'] and
         'mCastAwayNamed = away_named;' in f['castcombo'], 'R2', 'the away row is not tracked for RLV redaction')
    commit = f['castcommit']
    need('t.mCastRef = cast_target;' in commit and 'contains' not in commit and 'getMember' not in commit,
         'R2', 'onCastCommit no longer stores any picked id as-is')
    need('if (value == ALGazeNearby::HEADER_VALUE)' in commit and commit.count('mCastRestore = true;') == 2 and
         commit.count('refreshCastCombo(true);') == 2 and
         commit.index('HEADER_VALUE') < commit.index('editDetailTargetsFor'), 'R2',
         'onCastCommit does not refuse (and restore after) the Nearby header / a refused pick before storing')
    need('listedAmong(mCastNearbyShown, cast_target) &&\n        !liveNearbyName(cast_target, live_name)' in commit and
         'const LLUUID cast_target = comboValueId(value);' in commit, 'R2',
         'a Gaze Nearby pick is not validated against the live poll (present + RLV-visible) before writing')
    # Sentinel isolation: the header value is never parsed as an id by any conversion of these combos.
    need('getSelectedValue().asUUID()' not in gaze and 'mHotActor->getValue().asUUID()' not in gaze and
         'LLUUID(panel->mCast' not in gaze and 'LLUUID(value)' not in commit, 'R2',
         'a Gaze/Hot combo value is converted to an id without the sentinel guard')
    need('comboValueId(panel->mCast->getSelectedValue().asString())' in f['rewrite'] and
         'comboValueId(mHotActor->getValue().asString())' in f['hotcommit'], 'R2',
         'rewriteActorReferences / Hot commit do not isolate the header sentinel')
    need('value == ALGazeNearby::HEADER_VALUE' in body(gaze, 'LLUUID comboValueId('), 'R2',
         'comboValueId no longer rejects the header sentinel')
    need('(mCastAwayNamed && !RlvActions::canShowName(RlvActions::SNC_DEFAULT, mCastAwayId))' in f['castrevoked'] and
         'return !RlvActions::canShowName(RlvActions::SNC_DEFAULT, e.mId);' in f['revoked'], 'R2',
         'RLV revocation detection is inverted or incomplete')
    need(f['nearbyfor'].count('{ return !RlvActions::canShowName(RlvActions::SNC_DEFAULT, e.mId); }') == 2, 'R5',
         'a projection filter no longer drops names RLV hides')
    need('ALGazeNearby::nearbyAllowed()' in f['collect'] and 'ALGazeNearby::nearbyAllowed()' in f['livename'] and
         'ALGazeNearby::nearbyAllowed()' in f['revoked'] and f['nearbyfor'].count('ALGazeNearby::nearbyAllowed()') == 2 and
         '(mCastAwayNamed && !ALGazeNearby::nearbyAllowed())' in f['castrevoked'] and
         'ALGazeNearby::nonCastTargetName(id)' in f['knownname'], 'R1',
         '@shownearby is not honoured (collection, projections, live commits, revocation, saved-target label)')
    need('mCastAwayNamed' in f['castrevoked'] and 'RlvActions::canShowName' in f['castrevoked'] and
         'nearbyNamesRevoked(mCastNearbyShown)' in f['castrevoked'], 'R2',
         'RLV revocation ignores the displayed away row')

    # R3: resolving a cast-ref needs no membership; the tangent reset stays gated.
    blocks = case_blocks(mover, '\n            case GAZE_CAST:') + \
        case_blocks(mover, '\n            case GazeTarget::CAST_MEMBER:') + \
        case_blocks(mover, '\n    case GazeTarget::CAST_MEMBER:')
    resolving = [b for b in blocks if 'resolve(' in b]
    need(len(resolving) >= 3, 'R3', 'expected three resolving cast-ref blocks, found %d' % len(resolving))
    for b in resolving:
        for token in MEMBERSHIP:
            need(token not in b, 'R3', 'a cast-ref block gates on membership via ' + token)
    need('if (g.mCastTarget == old_id)\n        {\n            g.mCastTarget = new_id;\n'
         '            if (new_id.isNull() && g.mTarget == GAZE_CAST) g.mTarget = GAZE_TANGENT;' in mover, 'R3',
         'the tangent reset on identity rewrite lost its retired-id gate')
    need('if (target.mCastRef == old_id)\n    {\n        target.mCastRef = new_id;\n'
         '        if (new_id.isNull() && target.mMode == GazeTarget::CAST_MEMBER)' in mover, 'R3',
         'rewriteGazeTarget lost its retired-id gate')

    # R4: every saved/loaded cast_ref goes through the canonical scene id paths.
    lines = cast.split('\n')
    writes = reads = 0
    for i, line in enumerate(lines):
        if '"cast_ref"' not in line:
            continue
        window = '\n'.join(lines[max(0, i - 3): i + 4])
        for token in MEMBERSHIP:
            need(token not in window, 'R4', 'cast_ref near line %d is gated by %s' % (i + 1, token))
        if '] =' in line.split('"cast_ref"')[1][:4]:
            writes += 1
            need('savedSceneActorId' in line or 'saved_id' in line, 'R4',
                 'cast_ref write at line %d bypasses the saved-id path' % (i + 1))
        if 'has("cast_ref")' in line:
            reads += 1
            need('canonicalSceneActorId' in window, 'R4',
                 'cast_ref read at line %d bypasses the canonical id path' % (i + 1))
    need(writes >= 4 and reads >= 4, 'R4', 'cast_ref writes/reads found: %d/%d' % (writes, reads))
    need('s["cast_ref"] = slot.mTarget.mCastRef' in hot_slots and 'persistable(slot.mTarget.mCastRef)' in hot_slots,
         'R4', 'hot slots no longer persist the target id through the persistable gate')

    # R5: no per-frame name lookup, avatar walk or allocation.
    allowed = ('    if (mNearbyClock.due())\n    {\n        pollNearby();\n'
               '        refreshCastCombo(castNamesRevoked());\n'
               '        refreshHotControls(nearbyNamesRevoked(mHotNearbyShown));\n    }')
    need(allowed in f['draw'], 'R5', 'draw() does not poll nearby people (and refresh both pickers) on the 2 s clock tick')
    stripped = f['draw'].replace(allowed, '')
    for token in NAME_WORK + ('pollNearby(', 'nearbyFor(', 'nearbyNamesRevoked(', 'castNamesRevoked(', 'actorName(',
                              'RlvActions'):
        need(token not in stripped, 'R5', 'draw() reaches ' + token + ' outside the clock gate')
    need('static_cast<F64>(far_clip)' in f['nearbyfor'] and '"RenderFarClip"' in f['nearbyfor'] and
         'const F32 far_clip = draw_distance;' in f['nearbyfor'], 'R5',
         'the nearby range is no longer the draw distance')
    # Projections are cached between polls (no per-frame copy/sort while a slider drags) and
    # names RLV hides are dropped on every call, cached or not.
    need('if (proj.mPoll != mPollGeneration || proj.mCastRevision != cast_revision || proj.mActor != actor ||\n'
         '        proj.mKeep != keep || proj.mFar != far_clip)' in f['nearbyfor'], 'R5',
         'the nearby projection is not cached between polls')
    need(f['nearbyfor'].count('RlvActions::canShowName') == 2, 'R5',
         'a cached or fresh projection may still list a name RLV hides')
    need('++mPollGeneration;' in f['poll'], 'R5', 'a poll no longer invalidates the cached projections')
    for name in ('live', 'controls', 'hotrefresh', 'hotappend', 'castcombo', 'hotcommit', 'castcommit', 'nearbyfor'):
        for token in ('LLAvatarNameCache', 'getFullname', 'sInstances', 'collectNearbyPeople(', 'pollNearby('):
            need(token not in re.sub(r'//[^\n]*', '', f[name]), 'R5', name + ' reaches ' + token)
    need(len(re.findall(r'collectNearbyPeople\(', gaze)) == 2, 'R5',
         'collectNearbyPeople must be defined once and called only from pollNearby')
    need(len(re.findall(r'\bpollNearby\(\)', gaze)) == 2, 'R5',
         'pollNearby must be defined once and called only from the clock-gated draw()')
    need(len(re.findall(r'\bnearbyNamesRevoked\(', gaze)) == 3, 'R5', 'nearbyNamesRevoked has an unexpected caller')
    need('ALLiveReadoutClock mNearbyClock{0.5f};' in read('alpanellensgaze.h'), 'R5',
         'the nearby poll clock is no longer 0.5 Hz')
    # personName/knownPersonName are reached only from rebuild paths and the poll
    need(len(re.findall(r'\bknownPersonName\(', gaze)) == 2, 'R5', 'knownPersonName has an unexpected caller')

    # R6: Hot nearby choices exist only inside the revision-gated rebuild.
    rebuild = body(f['hotrefresh'], 'if (rebuild_choices)')
    need('appendHotNearbyChoices(' in rebuild, 'R6', 'Hot nearby choices are not built in the rebuild block')
    need(f['hotrefresh'].count('appendHotNearbyChoices(') == 1, 'R6', 'appendHotNearbyChoices called elsewhere')
    need('appendHotNearbyChoices(hot_nearby)' in rebuild, 'R6', 'the Hot rebuild does not use the per-slot projection')
    need('rebuild_choices = gate_changed || nearby_changed || mHotRestore;' in f['hotrefresh'] and
         'mHotRestore = false;' in f['hotrefresh'] and
         '!ALGazeNearby::sameMembers(hot_nearby, mHotNearbyShown)' in f['hotrefresh'], 'R6',
         'the Hot combo no longer rebuilds when its nearby projection changes')
    need('static_cast<U64>(mHotSlotIndex)' in f['hotrefresh'] and 'slot.mWho.getCRC32()' in f['hotrefresh'] and
         'std::hash<std::string>()(slot.mGroup)' in f['hotrefresh'], 'R6',
         'the Hot choice gate does not include slot index, Who and group')
    need('!alIsBeingEdited(mHotWho) && !alIsBeingEdited(mHotActor)' in f['hotrefresh'], 'R6',
         'the Hot focus guard is gone')
    need('nearbyFor(mHotProjection, slot.mGroup.empty() ? slot.mWho : LLUUID::null,' in f['hotrefresh'] and
         'slot.mTarget.mCastRef : LLUUID::null' in f['hotrefresh'], 'R6',
         'the Hot projection is not for the slot Who with the saved target kept')
    hc = f['hotcommit']
    need('hotNearbyOffered(actor) && liveNearbyName(actor, nearby_name)' in hc and
         '(nearby_pick ? nearby_name : mHotActor->getSelectedItemLabel())' in hc, 'R6',
         'Hot commit does not validate a nearby pick against the live record and store the live name')
    need('if (source == mHotActor && mHotActor->getValue().asString() == ALGazeNearby::HEADER_VALUE)' in hc and
         hc.index('HEADER_VALUE') < hc.index('slot.mTarget.mCastRef = actor'), 'R6',
         'Hot commit does not refuse the Nearby header')
    need('if (force_choices)\n        {\n            // Open lists must not re-select' in f['hotrefresh'], 'R6',
         'a forced Hot rebuild no longer closes open lists first')
    need(hc.count('mHotRestore = true;') == 2 and 'refreshHotControls(mHotRestore);' in hc and
         'if (mHotWho->isListVisible()) mHotWho->hideList();' in f['hotrefresh'], 'R6',
         'a refused Hot commit does not force the combo back to the saved target (and close open lists)')
    need('mHotActor->add(ALGazeNearby::HEADER_LABEL, LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, false)'
         in f['hotappend'], 'R6', 'the Hot Nearby header is not a disabled sentinel row')

    need('algazenearby.h' in cmake, 'build', 'algazenearby.h is not listed in CMakeLists.txt')
    need('algazenearbyname.h' in cmake, 'build', 'algazenearbyname.h is not listed in CMakeLists.txt')
    return bad


def mutations(gaze, mover, cast, header, cues, name_header, cues_panel, cues_h, cues_panel_h):
    """(name, expected tag, sources) - each must be rejected by check()."""
    def sub(text, old, new, count=1):
        assert text.count(old) == count, (old, text.count(old))
        return text.replace(old, new)
    out = []
    out.append(('uuid_prefix_label', 'R1', dict(gaze=sub(gaze,
        '    return name.empty() ? std::string("Unknown") : name;', '    return id.asString().substr(0, 8);'))))
    out.append(('nearby_row_labelled_by_id', 'R1', dict(gaze=sub(gaze,
        'mCast->add(person.mName, LLSD(person.mId.asString()));',
        'mCast->add(person.mId.asString(), LLSD(person.mId.asString()));'))))
    out.append(('away_label_from_id', 'R2', dict(gaze=sub(gaze,
        'ALGazeNearby::awayLabel(known)',
        'ALGazeNearby::awayLabel(current.asString())'))))
    out.append(('away_lookup_per_frame', 'R5', dict(gaze=sub(gaze,
        'if (mAwayPoll != mPollGeneration || mAwayFor != current)', 'if (true)'))))
    out.append(('rewrite_parses_sentinel', 'R2', dict(gaze=sub(gaze,
        'comboValueId(panel->mCast->getSelectedValue().asString())',
        'LLUUID(panel->mCast->getSelectedValue().asString())'))))
    out.append(('hot_commit_parses_sentinel', 'R2', dict(gaze=sub(gaze,
        'comboValueId(mHotActor->getValue().asString())', 'mHotActor->getValue().asUUID()'))))
    out.append(('combo_value_id_accepts_sentinel', 'R2', dict(gaze=sub(gaze,
        '    if (value.empty() || value == ALGazeNearby::HEADER_VALUE)\n', '    if (value.empty())\n'))))
    out.append(('cast_header_restore_missing', 'R2', dict(gaze=sub(gaze,
        '        mCastRestore = true;\n        refreshCastCombo(true);\n        return;\n    }\n    const LLUUID cast_target',
        '        refreshCastCombo(true);\n        return;\n    }\n    const LLUUID cast_target'))))
    out.append(('cast_pick_not_live_checked', 'R2', dict(gaze=sub(gaze,
        '    if (cast_target.notNull() && listedAmong(mCastNearbyShown, cast_target) &&\n        !liveNearbyName(cast_target, live_name))',
        '    if (cast_target.notNull() && listedAmong(mCastNearbyShown, cast_target) &&\n        !RlvActions::canShowName(RlvActions::SNC_DEFAULT, cast_target))'))))
    out.append(('away_revocation_ignored', 'R2', dict(gaze=sub(gaze,
        '    return nearbyNamesRevoked(mCastNearbyShown) ||\n        (mCastAwayNamed && !RlvActions::canShowName(RlvActions::SNC_DEFAULT, mCastAwayId)) ||\n        (mCastAwayNamed && !ALGazeNearby::nearbyAllowed());',
        '    return nearbyNamesRevoked(mCastNearbyShown);'))))
    out.append(('restore_flag_not_stale', 'R2', dict(gaze=sub(gaze,
        'current != mCastComboSelection || mCastRestore ||', 'current != mCastComboSelection ||'))))
    out.append(('forced_rebuild_keeps_open_list', 'R2', dict(gaze=sub(gaze,
        '        if (force && mCast->isListVisible())\n        {\n            mCast->hideList(); // an open list must not re-select a stale row index afterwards\n        }\n', ''))))
    out.append(('projection_keeps_hidden_names', 'R5', dict(gaze=sub(gaze,
        '        proj.mList = mNearbyPeople;\n        // A name RLV hides never enters a projection, so a rebuild inside the poll window\n        // cannot re-list it.\n        proj.mList.erase(std::remove_if(proj.mList.begin(), proj.mList.end(), [](const ALGazeNearby::Entry& e)\n            { return !RlvActions::canShowName(RlvActions::SNC_DEFAULT, e.mId); }), proj.mList.end());\n',
        '        proj.mList = mNearbyPeople;\n'))))
    out.append(('projection_not_cached', 'R5', dict(gaze=sub(gaze,
        'if (proj.mPoll != mPollGeneration || proj.mCastRevision != cast_revision', 'if (true || proj.mCastRevision != cast_revision'))))
    out.append(('poll_keeps_old_cache', 'R5', dict(gaze=sub(gaze, '    ++mPollGeneration;\n', ''))))
    out.append(('cues_uuid_prefix', 'R1', dict(cues=sub(cues,
        '    return name.empty() ? std::string("Unknown") : name;',
        'return id.asString().substr(0, 8);'))))
    out.append(('nearby_logs_name', 'R1', dict(gaze=sub(gaze,
        '    std::vector<ALGazeNearby::Entry> fresh;\n    fresh.reserve',
        '    LL_INFOS("Gaze") << "poll" << LL_ENDL;\n    std::vector<ALGazeNearby::Entry> fresh;\n    fresh.reserve'))))
    out.append(('status_shows_generic', 'R1', dict(mover=sub(mover,
        'name = ALGazeNearby::nonCastTargetName(g.mCastTarget);', 'name = g.mCastTarget.asString();'))))
    out.append(('status_ignores_shownearby', 'R1', dict(mover=sub(mover,
        'name = ALGazeNearby::nonCastTargetName(g.mCastTarget);', 'name = ALGazeNearby::personDisplayName(g.mCastTarget);'))))
    out.append(('rlv_hidden_person_offered', 'R1', dict(gaze=sub(gaze,
        ' ||\n            !RlvActions::canShowName(RlvActions::SNC_DEFAULT, avatar->getID()))', ')'))))
    out.append(('nearby_allowed_ignored', 'R1', dict(name_header=sub(name_header,
        '    return RlvActions::canShowNearbyAgents();', '    return true;'))))
    out.append(('collect_ignores_shownearby', 'R1', dict(gaze=sub(gaze,
        '    if (!ALGazeNearby::nearbyAllowed())\n    {\n        return; // RLV @shownearby', '    if (false)\n    {\n        return; // RLV @shownearby'))))
    out.append(('live_pick_ignores_shownearby', 'R1', dict(gaze=sub(gaze,
        '    if (!ALGazeNearby::nearbyAllowed()) return false; // RLV @shownearby: no Nearby pick is live\n', ''))))
    out.append(('revocation_ignores_shownearby', 'R1', dict(gaze=sub(gaze,
        '    if (!shown.empty() && !ALGazeNearby::nearbyAllowed())\n    {\n        return true;\n    }\n', ''))))
    out.append(('projection_ignores_shownearby', 'R1', dict(gaze=sub(gaze,
        '        if (!ALGazeNearby::nearbyAllowed()) proj.mList.clear(); // @shownearby: nobody is offered\n', ''))))
    out.append(('away_label_names_under_shownearby', 'R1', dict(gaze=sub(gaze,
        '    return ALGazeNearby::nonCastTargetName(id);\n}', '    return personName(id);\n}'))))
    out.append(('away_revocation_ignores_shownearby', 'R1', dict(gaze=sub(gaze,
        ' ||\n        (mCastAwayNamed && !ALGazeNearby::nearbyAllowed());', ';'))))
    out.append(('cue_label_ignores_shownearby', 'R1', dict(cues=sub(cues,
        '    const std::string name = ALGazeNearby::nonCastTargetName(ref);\n', '    const std::string name = ALGazeNearby::personDisplayName(ref);\n'))))
    out.append(('cue_tick_reloads_editor', 'R2', dict(cues=sub(cues,
        '                refreshCueList(true); // the row set itself differs: a real model reload',
        '                refreshCueList(true); loadEditor(cues[0]);'))))
    out.append(('cue_tick_clears_rows', 'R2', dict(cues=sub(cues,
        '    for (S32 row = 0; row < list->getItemCount(); ++row)\n    {\n        LLScrollListItem* item',
        '    list->deleteAllItems();\n    for (S32 row = 0; row < list->getItemCount(); ++row)\n    {\n        LLScrollListItem* item'))))
    out.append(('cue_relabel_by_position', 'R2', dict(cues=sub(cues,
        'cell->setValue(LLSD(targetLabel(cues[cue_index])));', 'cell->setValue(LLSD(targetLabel(cues[0])));'))))
    out.append(('cue_signature_without_labels', 'R5', dict(cues=sub(cues,
        '        signature += targetLabel(cue);', '        signature += "x";'))))
    out.append(('cue_combo_index_unsynced', 'R2', dict(cues=sub(cues,
        '    combo->setCurrentByIndex(combo->getCurrentIndex());\n', ''))))
    out.append(('cue_no_resort', 'R2', dict(cues=sub(cues, '        list->setNeedsSort(true); //', '        //'))))
    out.append(('nearby_allowed_reversed', 'R1', dict(name_header=sub(name_header,
        '    return RlvActions::canShowNearbyAgents();', '    return !RlvActions::canShowNearbyAgents();'))))
    out.append(('cue_tick_extra_reload', 'R2', dict(cues=sub(cues,
        '            if (relabelRows(mCueList, cues, 1))\n            {\n                mCueLabelSignature = signature;\n            }',
        '            refreshCueList(true);\n            if (relabelRows(mCueList, cues, 1))\n            {\n                mCueLabelSignature = signature;\n            }'))))
    out.append(('cue_combo_keep_dropped', 'R2', dict(cues=sub(cues,
        'if (!force && selectComboByValue(mCastTarget, keep_value))', 'if (false)'))))
    out.append(('cue_extra_row_wrong_id', 'R2', dict(cues=sub(cues,
        'mCastTarget->add(mExtraLabel, LLSD(mExtraRef.asString()));', 'mCastTarget->add(mExtraLabel, LLSD(mEditorTarget.mCastRef.asString()));'))))
    out.append(('cue_panel_tick_clears', 'R5', dict(cues_panel=sub(cues_panel,
        'if (ALFloaterGazeCues::relabelRows(mCueList, cues, 1))', 'if (false)'))))
    out.append(('nameless_person_offered', 'R1', dict(gaze=sub(gaze,
        '            continue; // no display name cached yet: never offer a nameless row',
        '            entry.mName = "Someone";'))))
    out.append(('user_name_fallback', 'R1', dict(gaze=sub(gaze,
        '    return ALGazeNearby::personDisplayName(id); // RLV', '    LLAvatarName n; LLAvatarNameCache::get(id, &n); return n.getUserName(); // RLV'))))
    out.append(('shared_name_user_fallback', 'R1', dict(name_header=sub(name_header,
        '    return name.getDisplayName(true);', '    return name.getUserName();'))))
    out.append(('shared_name_ignores_rlv', 'R1', dict(name_header=sub(name_header,
        '!RlvActions::canShowName(RlvActions::SNC_DEFAULT, id) || ', ''))))
    out.append(('cues_actor_complete_name', 'R1', dict(cues=sub(cues,
        '    const std::string name = ALGazeNearby::nonCastTargetName(id);\n    return name.empty() ? std::string("Unknown") : name;',
        '    LLAvatarName n; LLAvatarNameCache::get(id, &n); return n.getCompleteName();'))))
    out.append(('gaze_actor_complete_name', 'R1', dict(gaze=sub(gaze,
        '    const std::string name = ALGazeNearby::nonCastTargetName(id);\n    return name.empty() ? std::string("Unknown") : name;',
        '    LLAvatarName n; LLAvatarNameCache::get(id, &n); return n.getCompleteName();'))))
    out.append(('cue_extra_row_dropped', 'R2', dict(cues=sub(cues,
        'mCastTarget->add(mExtraLabel, LLSD(mExtraRef.asString()))', '(void)0'))))
    out.append(('cue_load_no_offer', 'R2', dict(cues=sub(cues,
        '    refreshCastCombo(true); // offers the cue', '    (void)0; // offers the cue'))))
    out.append(('cue_editor_no_tick', 'R5', dict(cues=sub(cues,
        'refreshCastCombo(false, names_due);', 'refreshCastCombo();'))))
    out.append(('cue_panel_no_tick', 'R5', dict(cues_panel=sub(cues_panel,
        '    if (mNameClock.due())\n    {\n        const LLDirectorCast::GazeCueList& cues',
        '    if (false)\n    {\n        const LLDirectorCast::GazeCueList& cues'))))
    out.append(('cue_clock_per_frame', 'R5', dict(cues_h=sub(cues_h,
        'ALLiveReadoutClock mNameClock{0.5f}; // Hz: every 2 s', 'ALLiveReadoutClock mNameClock{60.f}; // Hz: every 2 s'))))
    out.append(('legacy_name_fallback', 'R1', dict(gaze=sub(gaze,
        '            continue; // no display name cached yet: never offer a nameless row',
        '            entry.mName = avatar->getFullname();'))))
    out.append(('status_user_name_fallback', 'R1', dict(mover=sub(mover,
        '                    name = ALGazeNearby::nonCastTargetName(g.mCastTarget);',
        '                    name = ALGazeNearby::nonCastTargetName(g.mCastTarget);\n                    if (name.empty()) name = LLAvatarName().getUserName();'))))
    out.append(('status_clone_loses_cast_wording', 'R1', dict(mover=sub(mover,
        'if (studio.getInstance(g.mCastTarget) || studio.findInstanceByRuntime(g.mCastTarget))',
        'if (false)'))))
    out.append(('rlv_commit_unchecked', 'R1', dict(gaze=sub(gaze,
        '            if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, id)) return false;\n', ''))))
    out.append(('refresh_resets_to_tangent', 'R2', dict(gaze=sub(gaze,
        '        mCastAwayShown = away_label;\n',
        '        mCastAwayShown = away_label;\n'
        '        if (current.notNull() && !cast.contains(current)) mover_reset_target(GAZE_TANGENT);\n'))))
    out.append(('refresh_writes_model', 'R2', dict(gaze=sub(gaze,
        '        mCastAwayShown = away_label;\n',
        '        mCastAwayShown = away_label;\n'
        '        cast.setGazeTarget(displayed, LLActorMover::GazeTarget());\n'))))
    out.append(('header_enabled', 'R2', dict(gaze=sub(gaze,
        'LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, false);\n            for (const ALGazeNearby::Entry& person : nearby)\n            {\n                mCast->add(',
        'LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, true);\n            for (const ALGazeNearby::Entry& person : nearby)\n            {\n                mCast->add('))))
    out.append(('header_value_empty', 'R2', dict(gaze=sub(gaze,
        'mCast->add(ALGazeNearby::HEADER_LABEL, LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, false)',
        'mCast->add(ALGazeNearby::HEADER_LABEL, LLSD(std::string()), ADD_BOTTOM, false)'))))
    out.append(('default_row_by_index', 'R2', dict(gaze=sub(gaze,
        'setValueIfChanged(mCast, LLSD(first_value), true);', 'mCast->setCurrentByIndex(0);'))))
    out.append(('cast_commit_header_unguarded', 'R2', dict(gaze=sub(gaze,
        '    if (value == ALGazeNearby::HEADER_VALUE)\n', '    if (false)\n'))))
    out.append(('cast_projection_not_compared', 'R2', dict(gaze=sub(gaze,
        '!ALGazeNearby::sameMembers(nearby, mCastNearbyShown) || away_label != mCastAwayShown;',
        'false;'))))
    out.append(('cast_keep_dropped', 'R2', dict(gaze=sub(gaze,
        'nearbyFor(mCastProjection, target_actor, current);', 'nearbyFor(mCastProjection, target_actor, LLUUID::null);'))))
    out.append(('away_when_present', 'R2', dict(gaze=sub(gaze,
        ' && !listedAmong(nearby, current)', ''))))
    out.append(('hot_refresh_rewrites_ref', 'R2', dict(gaze=sub(gaze,
        '    const bool target_available = slot.mHasTarget &&',
        '    if (!actor_available) { auto fix = slot; fix.mTarget.mCastRef = LLUUID::null; cast.setHotGazeSlot(0, fix); }\n'
        '    const bool target_available = slot.mHasTarget &&'))))
    out.append(('commit_requires_membership', 'R2', dict(gaze=sub(gaze,
        '{ t.mCastRef = cast_target; });', '{ if (LLDirectorCast::instance().contains(cast_target)) t.mCastRef = cast_target; });'))))
    out.append(('resolve_requires_membership', 'R3', dict(mover=sub(mover,
        '                LLVOAvatar* tgt = (g.mCastTarget.notNull() && g.mCastTarget != av->getID())\n                    ? LLDirectorCast::instance().resolve(g.mCastTarget) : nullptr;',
        '                LLVOAvatar* tgt = (g.mCastTarget.notNull() && g.mCastTarget != av->getID() &&\n'
        '                    LLDirectorCast::instance().contains(g.mCastTarget))\n                    ? LLDirectorCast::instance().resolve(g.mCastTarget) : nullptr;'))))
    out.append(('hot_resolve_requires_membership', 'R3', dict(mover=sub(mover,
        '        LLVOAvatar* body = LLDirectorCast::instance().resolve(target.mCastRef);',
        '        LLVOAvatar* body = LLDirectorCast::instance().getMember(target.mCastRef) ? LLDirectorCast::instance().resolve(target.mCastRef) : nullptr;'))))
    out.append(('rewrite_resets_every_cast_ref', 'R3', dict(mover=sub(mover,
        '        if (g.mCastTarget == old_id)\n        {\n            g.mCastTarget = new_id;\n            if (new_id.isNull() && g.mTarget == GAZE_CAST) g.mTarget = GAZE_TANGENT;',
        '        if (true)\n        {\n            g.mCastTarget = new_id;\n            if (new_id.isNull() && g.mTarget == GAZE_CAST) g.mTarget = GAZE_TANGENT;'))))
    out.append(('scene_load_drops_non_cast', 'R4', dict(cast=sub(cast,
        '        target.mCastRef = canonicalSceneActorId(\n            data["cast_ref"].asUUID(), actor_ids);\n    }\n    if (data.has("fixed_point"))\n    {\n        target.mFixedPoint = ll_vector3d_from_sd(data["fixed_point"]);\n    }\n    if (data.has("object_ref"))\n    {\n        target.mObjectRef = data["object_ref"].asUUID();\n    }\n    readGazeExpression(data, target);',
        '        target.mCastRef = LLDirectorCast::instance().contains(data["cast_ref"].asUUID()) ? canonicalSceneActorId(\n            data["cast_ref"].asUUID(), actor_ids) : LLUUID::null;\n    }\n    if (data.has("fixed_point"))\n    {\n        target.mFixedPoint = ll_vector3d_from_sd(data["fixed_point"]);\n    }\n    if (data.has("object_ref"))\n    {\n        target.mObjectRef = data["object_ref"].asUUID();\n    }\n    readGazeExpression(data, target);'))))
    out.append(('scene_save_drops_non_cast', 'R4', dict(cast=sub(cast,
        '            gaze_sd["cast_ref"] = saved_id(m.mGazeTarget.mCastRef);',
        '            gaze_sd["cast_ref"] = contains(m.mGazeTarget.mCastRef) ? saved_id(m.mGazeTarget.mCastRef) : LLUUID::null;'))))
    out.append(('per_frame_poll', 'R5', dict(gaze=sub(gaze,
        '    if (mNearbyClock.due())\n    {\n        pollNearby();', '    if (true)\n    {\n        pollNearby();'))))
    out.append(('draw_looks_up_names', 'R5', dict(gaze=sub(gaze,
        '    LLDirectorCast& cast = LLDirectorCast::instance();\n    LLActorMover& mover = LLActorMover::instance();\n    if (mStructureGate.changed(',
        '    LLDirectorCast& cast = LLDirectorCast::instance();\n    LLActorMover& mover = LLActorMover::instance();\n    (void)actorName(gAgentID);\n    if (mStructureGate.changed('))))
    out.append(('draw_distance_replaced', 'R5', dict(gaze=sub(gaze,
        'static_cast<F64>(far_clip), keep);', '64.0, keep);'))))
    out.append(('per_frame_name_lookup', 'R5', dict(gaze=sub(gaze,
        '    std::string status;\n    mover.getGazeStatus(actor, status);',
        '    std::string status;\n    LLAvatarName per_frame; LLAvatarNameCache::get(actor, &per_frame);\n    mover.getGazeStatus(actor, status);'))))
    out.append(('per_frame_avatar_walk', 'R5', dict(gaze=sub(gaze,
        '    mHotNearbyOffered.clear();\n    mHotNearbyShown = nearby;',
        '    mHotNearbyOffered.clear();\n    for (LLCharacter* c : LLCharacter::sInstances) (void)c;\n    mHotNearbyShown = nearby;'))))
    out.append(('cast_combo_walks_avatars', 'R5', dict(gaze=sub(gaze,
        '    nearbyFor(mCastProjection, target_actor, current);\n',
        '    collectNearbyPeople(mCastProjection.mList);\n    nearbyFor(mCastProjection, target_actor, current);\n'))))
    out.append(('hot_nearby_outside_rebuild', 'R6', dict(gaze=sub(gaze,
        '        appendHotNearbyChoices(hot_nearby);\n', '')
        .replace('    setValueIfChanged(mHotSlot, mHotSlotIndex);',
                 '    appendHotNearbyChoices(hot_nearby);\n    setValueIfChanged(mHotSlot, mHotSlotIndex);', 1))))
    out.append(('hot_projection_ignored', 'R6', dict(gaze=sub(gaze,
        'rebuild_choices = gate_changed || nearby_changed || mHotRestore;', 'rebuild_choices = gate_changed || mHotRestore;'))))
    out.append(('hot_restore_not_forced', 'R6', dict(gaze=sub(gaze,
        'rebuild_choices = gate_changed || nearby_changed || mHotRestore;', 'rebuild_choices = gate_changed || nearby_changed;'))))
    out.append(('hot_refusal_restore_missing', 'R6', dict(gaze=sub(gaze,
        '            mHotRestore = true; // a refused pick', '            (void)0; // a refused pick'))))
    out.append(('hot_gate_ignores_slot', 'R6', dict(gaze=sub(gaze,
        'static_cast<U64>(mHotSlotIndex),', '0,'))))
    out.append(('hot_focus_guard_bypassed', 'R6', dict(gaze=sub(gaze,
        'if (force_choices || (!alIsBeingEdited(mHotWho) && !alIsBeingEdited(mHotActor)))', 'if (true)'))))
    out.append(('hot_commit_stale_offer', 'R6', dict(gaze=sub(gaze,
        'hotNearbyOffered(actor) && liveNearbyName(actor, nearby_name)', 'hotNearbyOffered(actor)'))))
    out.append(('hot_commit_stores_row_label', 'R6', dict(gaze=sub(gaze,
        '(nearby_pick ? nearby_name : mHotActor->getSelectedItemLabel())', 'mHotActor->getSelectedItemLabel()'))))
    out.append(('hot_header_unguarded', 'R6', dict(gaze=sub(gaze,
        'if (source == mHotActor && mHotActor->getValue().asString() == ALGazeNearby::HEADER_VALUE)', 'if (false)'))))
    out.append(('hot_header_enabled', 'R6', dict(gaze=sub(gaze,
        'LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, false); // ui-draw-lint', 'LLSD(std::string(ALGazeNearby::HEADER_VALUE)), ADD_BOTTOM, true); // ui-draw-lint'))))
    out.append(('header_logs_id', 'R1', dict(header=sub(header,
        'inline std::string awayLabel(const std::string& name)\n{',
        'inline std::string awayLabel(const std::string& name)\n{\n    LL_INFOS("Gaze") << name << LL_ENDL;'))))
    return out


# ---------------------------------------------------------------------------------------------
# NATIVE half
PRELUDE = r'''
#define LL_LLUUID_H
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
typedef double F64; typedef float F32;
struct LLUUID {
  std::string v; LLUUID() = default; LLUUID(const char* s) : v(s) {}
  bool isNull() const { return v.empty(); }
  bool operator==(const LLUUID& o) const { return v == o.v; } bool operator!=(const LLUUID& o) const { return v != o.v; }
  bool operator<(const LLUUID& o) const { return v < o.v; }
  static bool validate(const std::string& s) {
    if (s.size() != 36) return false;
    for (size_t i = 0; i < 36; ++i) {
      const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
      if (dash ? s[i] != '-' : !isxdigit((unsigned char)s[i])) return false;
    }
    return true; }
};
typedef std::vector<LLUUID> uuid_vec_t;
'''

TEST = r'''
using namespace ALGazeNearby;
static void require(bool ok, const char* what) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(2); } }
static Entry person(const char* id, const char* name, F64 x) { Entry e; e.mId = LLUUID(id); e.mName = name; e.mPos[0] = x; return e; }
int main() {
  const F64 actor_at[3] = {100, 0, 0}, user_at[3] = {0, 0, 0};
  const uuid_vec_t cast = {LLUUID("cast1")};
  const LLUUID actor("actor");
  // order: nearest the ACTOR first (not nearest the user), within the user's draw distance
  {
    std::vector<Entry> p = {person("a", "Far", 0), person("b", "Near", 99), person("c", "Mid", 90)};
    select(p, cast, actor, actor_at, user_at, 256);
    require(p.size() == 3 && p[0].mName == "Near" && p[1].mName == "Mid" && p[2].mName == "Far", "nearest the actor first");
  }
  // range: beyond draw distance dropped, exactly at it kept
  {
    std::vector<Entry> p = {person("a", "Edge", 64), person("b", "Out", 64.5), person("c", "In", 10)};
    select(p, cast, actor, actor_at, user_at, 64);
    bool edge = false, out = false;
    for (auto& e : p) { edge |= e.mName == "Edge"; out |= e.mName == "Out"; }
    require(edge && !out, "draw distance range");
  }
  // cap at 16, keeping the 16 nearest
  {
    std::vector<Entry> p;
    for (int i = 0; i < 40; ++i) p.push_back(person(("id" + std::to_string(i)).c_str(), ("P" + std::to_string(i)).c_str(), 100 - i));
    select(p, cast, actor, actor_at, user_at, 1000);
    require(p.size() == 16, "cap 16");
    require(p[0].mName == "P0" && p[15].mName == "P15", "cap keeps the nearest");
  }
  // exclusions: actor, cast members, nameless, UUID-named, null id
  {
    std::vector<Entry> p = {person("actor", "Actor Self", 100), person("cast1", "Cast One", 100),
      person("n", "", 100), person("u", "123e4567-e89b-12d3-a456-426614174000", 100), person("", "Null", 100),
      person("ok", "Real Person", 100)};
    select(p, cast, actor, actor_at, user_at, 1000);
    require(p.size() == 1 && p[0].mName == "Real Person", "exclusions");
    for (auto& e : p) require(!LLUUID::validate(e.mName) && e.mName != e.mId.v, "no UUID shown");
  }
  // the saved target (keep): a PRESENT keeper stays listed past the range and past the cap; an
  // absent one is not invented; cast/actor exclusions still win over keep
  {
    std::vector<Entry> p = {person("k", "Keeper", 90), person("a", "A", 1)};
    select(p, cast, actor, actor_at, user_at, 64, LLUUID("k"));
    bool keeper = false; for (auto& e : p) keeper |= e.mName == "Keeper";
    require(keeper, "keep beyond the range stays listed");
    p = {person("a", "A", 1)};
    select(p, cast, actor, actor_at, user_at, 64, LLUUID("k"));
    require(p.size() == 1 && p[0].mName == "A", "an absent keep is not invented");
    std::vector<Entry> q;
    for (int i = 0; i < 40; ++i) q.push_back(person(("id" + std::to_string(i)).c_str(), ("P" + std::to_string(i)).c_str(), 100 - i));
    q.push_back(person("far", "Far Keeper", 0));
    select(q, cast, actor, actor_at, user_at, 1000, LLUUID("far"));
    require(q.size() == 16 && q.back().mName == "Far Keeper" && q[0].mName == "P0", "keep beyond the cap replaces the last row");
    q = {person("cast1", "Cast One", 100), person("actor", "Actor Self", 100)};
    select(q, cast, actor, actor_at, user_at, 1000, LLUUID("cast1"));
    require(q.empty(), "keep never overrides the cast/actor exclusions");
  }
  // the header row value is a non-text sentinel: not empty (the placeholder), not a UUID
  require(HEADER_VALUE[0] != 0 && !LLUUID::validate(HEADER_VALUE) && std::string(HEADER_LABEL) == "Nearby", "header sentinel");
  // deterministic ties: name, then id
  {
    std::vector<Entry> p = {person("2", "Same", 100), person("1", "Same", 100), person("3", "Alpha", 100)};
    select(p, cast, actor, actor_at, user_at, 1000);
    require(p[0].mName == "Alpha" && p[1].mId.v == "1" && p[2].mId.v == "2", "tie order");
  }
  // membership compare ignores movement, sees names and members
  {
    std::vector<Entry> a = {person("1", "One", 0), person("2", "Two", 0)}, b = a;
    b[0].mPos[0] = 50; b[1].mPos[2] = 9;
    require(sameMembers(a, b), "movement must not rebuild a combo");
    b[1].mName = "Renamed"; require(!sameMembers(a, b), "rename must rebuild");
    b = a; b.pop_back(); require(!sameMembers(a, b), "leaving must rebuild");
  }
  // away label: name, else a generic word; never a UUID
  {
    require(awayLabel("Bob") == "Bob (away)", "away label");
    require(awayLabel("") == "Unknown person (away)", "away label for unknown");
    require(awayLabel("123e4567-e89b-12d3-a456-426614174000") == "Unknown person (away)", "away label never a UUID");
  }
  std::puts("PASS: nearby selection rules");
  return 0;
}
'''

HEADER_MUTANTS = {
    'uuid_named_person_offered': ('            LLUUID::validate(e.mName) ||\n', ''),
    'cap_removed': ('        else\n        {\n            people.resize(cap);\n        }', '        else\n        {\n        }'),
    'keep_range_ignored': ('e.mId != keep && ', ''),
    'keep_cap_dropped': ('if (!keep.isNull() && at != people.end()', 'if (false && at != people.end()'),
    'header_value_empty': ('constexpr const char* HEADER_VALUE = "nearby-header";', 'constexpr const char* HEADER_VALUE = "";'),
    'header_value_uuid': ('constexpr const char* HEADER_VALUE = "nearby-header";', 'constexpr const char* HEADER_VALUE = "123e4567-e89b-12d3-a456-426614174000";'),
    'order_removed': ('        if (da != db) return da < db;\n', '        if (da != db) return false;\n'),
    'cast_member_duplicated': ('            std::find(cast_ids.begin(), cast_ids.end(), e.mId) != cast_ids.end() ||\n', ''),
    'range_ignored': ('            (e.mId != keep && distanceSquared(e.mPos, range_ref) > range_sq);', '            false;'),
    'nameless_offered': (' || e.mName.empty() ||', ' ||'),
    'actor_offered': ('e.mId == actor || ', ''),
    'away_label_uuid': ('(name.empty() || LLUUID::validate(name) ? std::string("Unknown person") : name)', 'name'),
    'movement_rebuilds': ('a[i].mId != b[i].mId || a[i].mName != b[i].mName', 'a[i].mId != b[i].mId || a[i].mName != b[i].mName || a[i].mPos[0] != b[i].mPos[0]'),
    'order_from_user': ('const F64 da = distanceSquared(a.mPos, order_ref), db = distanceSquared(b.mPos, order_ref);',
                        'const F64 da = distanceSquared(a.mPos, range_ref), db = distanceSquared(b.mPos, range_ref);'),
}


def native(header):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from native_check_tools import check_directory, compile_run
    out = check_directory('gaze_nearby')
    inline = header.replace('#include "lluuid.h"\n', '')

    def run(name, text):
        result = compile_run(out, PRELUDE + text + TEST, name)
        return result

    result = run('nearby_real', inline)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout.strip())
    for name, (old, new) in HEADER_MUTANTS.items():
        assert inline.count(old) == 1, ('mutant target not found exactly once', name, inline.count(old))
        try:
            result = run('nearby_mutant_' + name, inline.replace(old, new))
        except RuntimeError:
            raise AssertionError('mutant did not compile (fix the control): ' + name)
        assert result.returncode == 2, ('native mutant survived: ' + name, result.stdout, result.stderr)
        print('PASS: native negative control rejected: ' + name + ' (' + result.stderr.strip() + ')')


def main():
    real = dict(gaze=read('alpanellensgaze.cpp'), mover=read('llactormover.cpp'), cast=read('lldirectorcast.cpp'),
                header=read('algazenearby.h'), cmake=read('CMakeLists.txt'), hot_slots=read('algazehotslots.h'),
                cues=read('alfloatergazecues.cpp'), name_header=read('algazenearbyname.h'),
                cues_panel=read('alpanelgazecues.cpp'), cues_h=read('alfloatergazecues.h'),
                cues_panel_h=read('alpanelgazecues.h'))
    bad = check(**real)
    assert not bad, '\n'.join(bad)
    print('PASS: source rules R1-R6 hold on the real tree')
    for name, tag, changed in mutations(real['gaze'], real['mover'], real['cast'], real['header'], real['cues'], real['name_header'], real['cues_panel'], real['cues_h'], real['cues_panel_h']):
        failures = check(**{**real, **changed})
        assert any(item.startswith(tag) for item in failures), ('planted negative survived', name, tag, failures)
        print('PASS: planted negative rejected: ' + name + ' -> ' + [i for i in failures if i.startswith(tag)][0][:110])
    if '--no-native' not in sys.argv:
        native(real['header'])


if __name__ == '__main__':
    main()
