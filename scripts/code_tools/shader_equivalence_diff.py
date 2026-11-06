#!/usr/bin/env python3
"""Compare ordered shader equivalence JSONL dumps, without third-party packages.

Compare cold-before with cold-after and warm-before with warm-after.
Driver-assigned attributes, uniform locations and sampler units still fail comparison; use
--separate-driver-fields to group their reports separately.
Comparisons certify the requested workload: --expect-passes N is required.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import sys

PROGRAM_FIELDS = {
    'type', 'name', 'attempt', 'requested_level', 'resolved_level', 'shader_group',
    'features', 'defines', 'global_defines', 'cache_key', 'eligible', 'used_binary', 'shader_files',
    'stages', 'attachments', 'link_outcome', 'attributes', 'attribute_mask',
    'uniforms', 'uniform_map', 'texture', 'active_texture_channels',
    'ubo_bindings', 'samplers', 'success', 'binary_outcome', 'program_binary_ran',
    'epoch_neutral_key', 'root_load_outcome', 'pass',
}
DRIVER_FIELDS = {'attributes', 'uniforms', 'uniform_map', 'texture', 'samplers', 'ubo_bindings'}
MISSING = object()


def load(path, expect_passes=None):
    with Path(path).open(encoding='utf-8') as stream:
        records = [json.loads(line) for line in stream if line.strip()]
    validate(records, path, expect_passes)
    return records


def validate(records, path='<fixture>', expect_passes=None):
    if any(not isinstance(record, dict) for record in records):
        raise ValueError(f'{path}: every record must be a JSON object')
    if not records or records[0].get('type') != 'run':
        raise ValueError(f'{path}: missing run header')
    required = {'type', 'vendor', 'renderer', 'version', 'epoch', 'cache_enabled'}
    if required - records[0].keys():
        raise ValueError(f'{path}: incomplete run header')
    if type(records[0]['epoch']) is not int or records[0]['epoch'] < 1:
        raise ValueError(f'{path}: invalid cache epoch')
    seen, passes = set(), set()
    active = None
    counts = {}
    stage_fields = {'file', 'stage', 'requested_level', 'resolved_level', 'source_level',
                    'source_hash', 'object_key', 'compile_outcome', 'success'}
    for record in records[1:]:
        kind = record.get('type')
        if kind == 'pass_begin':
            number = record.get('pass')
            if active is not None or type(number) is not int or number in passes or not record.get('name'):
                raise ValueError(f'{path}: invalid/nested/duplicate pass')
            passes.add(number)
            active = number
            counts = dict(program_attempts=0, module_attempts=0)
            continue
        if active is None or record.get('pass') != active:
            raise ValueError(f'{path}: record outside a completed pass')
        if kind == 'pass_complete':
            if any(type(record.get(k)) is not int or record[k] != v for k, v in counts.items()):
                raise ValueError(f'{path}: completion attempt counts do not match')
            active = None
        elif kind == 'program':
            if PROGRAM_FIELDS - record.keys():
                raise ValueError(f'{path}: incomplete program record: {record.get("name")}')
            outcome = record['binary_outcome']
            if (outcome not in {'disabled', 'miss', 'read_failure', 'driver_rejected', 'synthetic_rejected', 'accepted'}
                    or type(record['program_binary_ran']) is not bool
                    or record['program_binary_ran'] != (outcome in {'driver_rejected', 'accepted'})):
                raise ValueError(f'{path}: inconsistent binary execution evidence')
            key = record['name'], record['attempt']
            if key in seen:
                raise ValueError(f'{path}: duplicate program/attempt: {key}')
            seen.add(key)
            counts['program_attempts'] += 1
            if not isinstance(record['stages'], list) or not isinstance(record['attachments'], list):
                raise ValueError(f'{path}: invalid stage/attachment collection')
            for stage in record['stages']:
                if not isinstance(stage, dict) or stage_fields - stage.keys():
                    raise ValueError(f'{path}: incomplete stage record')
            for attachment in record['attachments']:
                if not isinstance(attachment, dict) or attachment.get('untracked') or attachment.get('outcome') not in {'attached', 'skipped', 'not_attempted'}:
                    raise ValueError(f'{path}: uncertifiable attachment')
                if attachment['outcome'] == 'attached' and any(attachment.get(k) is None for k in
                        ('object_key', 'source_hash', 'requested_level', 'resolved_level', 'source_level')):
                    raise ValueError(f'{path}: incomplete attachment provenance')
        elif kind == 'module_load':
            if stage_fields - record.keys():
                raise ValueError(f'{path}: incomplete module record')
            counts['module_attempts'] += 1
        else:
            raise ValueError(f'{path}: unknown record type: {kind}')
    if active is not None or not passes or not seen:
        raise ValueError(f'{path}: incomplete evidence (missing completed pass/program)')
    if expect_passes is not None and len(passes) != expect_passes:
        raise ValueError(f'{path}: expected {expect_passes} completed passes, found {len(passes)}')


def fields(left, right, path=''):
    """Yield every differing leaf, retaining array order and exact JSON types."""
    if isinstance(left, dict) and isinstance(right, dict):
        for key in sorted(left.keys() | right.keys()):
            yield from fields(left.get(key, MISSING), right.get(key, MISSING),
                              f'{path}.{key}' if path else key)
    elif isinstance(left, list) and isinstance(right, list):
        for index in range(max(len(left), len(right))):
            yield from fields(left[index] if index < len(left) else MISSING,
                              right[index] if index < len(right) else MISSING,
                              f'{path}[{index}]')
    elif type(left) is not type(right) or left != right:
        yield path, left, right


def compare(left, right):
    result = []
    cross_epoch = left[0]['epoch'] != right[0]['epoch']
    for index in range(max(len(left), len(right))):
        a = left[index] if index < len(left) else MISSING
        b = right[index] if index < len(right) else MISSING
        record = a if isinstance(a, dict) else b
        label = f"record {index} {record.get('type', '?')} {record.get('name', record.get('file', ''))}"
        if record.get('type') == 'program':
            label += f" attempt {record.get('attempt', '?')}"
        for difference in fields(a, b):
            field = difference[0]
            if cross_epoch and index == 0 and field == 'epoch':
                continue
            if (cross_epoch and field == 'cache_key' and isinstance(a, dict) and isinstance(b, dict)
                    and a.get('epoch_neutral_key') is not None
                    and a['epoch_neutral_key'] == b.get('epoch_neutral_key')):
                continue
            result.append((label, *difference))
    return result


def selftest():
    header = dict(type='run', vendor='vendor', renderer='renderer', version='4.6', epoch=2, cache_enabled=True)
    stage = dict(file='rootV.glsl', stage=35633, requested_level=3, resolved_level=2,
                 source_level=2, source_hash='assembled', object_key='key', compile_outcome='compiled', success=True)
    program = dict(type='program', name='test', attempt=1, requested_level=3,
                   resolved_level=2, shader_group=0, features={'hasGamma': True},
                   defines={'X': '1'}, global_defines={'GLOBAL': '1'}, cache_key='cache', epoch_neutral_key='neutral', eligible=True, used_binary=False,
                   shader_files=[{'file': 'rootV.glsl', 'stage': 35633}], stages=[stage],
                   attachments=[dict(stage, outcome='attached'), dict(stage, object_key='feature', outcome='attached')],
                   link_outcome='success', attributes={'position': 0}, attribute_mask=1.0,
                   uniforms=[{'name': 'diffuseMap', 'location': 2}], uniform_map={'diffuseMap': 2},
                   texture=[0], active_texture_channels=1, ubo_bindings={'GLTFJoints': 1},
                   samplers={'diffuseMap': 0}, success=True, binary_outcome='miss', program_binary_ran=False, root_load_outcome='attempted', **{'pass': 1})
    baseline = [header, dict(type='pass_begin', name='setShaders', **{'pass': 1}), program, dict(program, attempt=2),
                dict(type='pass_complete', program_attempts=2, module_attempts=0, **{'pass': 1})]
    validate(baseline)
    assert not compare(baseline, copy.deepcopy(baseline))
    count = 0

    def leaves(value, path=()):
        if isinstance(value, dict):
            for key, child in value.items():
                yield from leaves(child, path + (key,))
        elif isinstance(value, list):
            for index, child in enumerate(value):
                yield from leaves(child, path + (index,))
        else:
            yield path, value

    # Mutate every actual field class, including all nested stage fields.
    for index, record in ((0, baseline[0]), (2, baseline[2])):
        for path, value in leaves(record):
            if index == 0 and path == ('epoch',):
                continue # Epoch changes are checked with their keys below.
            changed = copy.deepcopy(baseline)
            target = changed[index]
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = not value if isinstance(value, bool) else (
                value + 1 if isinstance(value, (int, float)) else value + '_changed')
            assert compare(baseline, changed), (index, path)
            count += 1
    changed = copy.deepcopy(baseline)
    changed[2]['attachments'].reverse()
    assert compare(baseline, changed), 'reordered attachment survived'
    assert compare(baseline, baseline[:3] + baseline[4:]), 'dropped retry survived'
    assert compare(baseline, [header, baseline[1], baseline[3], baseline[2], baseline[4]]), 'program reorder survived'
    changed = copy.deepcopy(baseline)
    del changed[2]['features']['hasGamma']
    assert compare(baseline, changed), 'deleted feature survived'
    changed = copy.deepcopy(baseline)
    changed[2]['used_binary'] = True
    assert compare(baseline, changed)
    two_pass = copy.deepcopy(baseline)
    second = copy.deepcopy(baseline[1:])
    for record in second:
        record['pass'] = 2
        if record['type'] == 'program':
            record['attempt'] += 2
    two_pass.extend(second)
    validate(two_pass, expect_passes=2)
    try:
        validate(two_pass[:len(baseline)], expect_passes=2)
    except ValueError:
        pass
    else:
        raise AssertionError('two-pass completed prefix certified')
    changed = copy.deepcopy(baseline)
    changed[0]['epoch'] += 1
    for record in changed:
        if record['type'] == 'program':
            record['cache_key'] += '_new_epoch'
    assert not compare(baseline, changed), 'epoch-only transition failed'
    changed[2]['epoch_neutral_key'] += '_real_change'
    assert compare(baseline, changed), 'epoch plus real key change survived'
    changed = copy.deepcopy(baseline)
    changed[2]['cache_key'] += '_same_epoch_change'
    assert compare(baseline, changed), 'raw key change at same epoch survived'
    for invalid in ([header], baseline[:-1], baseline[:3],
                    baseline[:3] + baseline[4:]):
        try:
            validate(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError('incomplete evidence accepted')
    changed = copy.deepcopy(baseline)
    changed[2]['attachments'][0]['untracked'] = True
    try:
        validate(changed)
    except ValueError:
        pass
    else:
        raise AssertionError('untracked attachment certified')
    print(f'shader_equivalence_diff selftest: PASS ({count} field mutations; attachment/order/retry changes; incomplete/header-only/truncated/count/untracked/two-pass-prefix evidence rejected; epoch-only passes, epoch plus real key change fails)')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', nargs='?')
    parser.add_argument('after', nargs='?')
    parser.add_argument('--separate-driver-fields', action='store_true')
    parser.add_argument('--selftest', action='store_true')
    parser.add_argument('--expect-passes', type=int, help='required completed-pass count for each capture')
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.before or not args.after:
        parser.error('two dump paths are required')
    if args.expect_passes is None or args.expect_passes < 1:
        parser.error('certification requires --expect-passes N (N >= 1)')
    try:
        left, right = load(args.before, args.expect_passes), load(args.after, args.expect_passes)
        differences = compare(left, right)
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f'Invalid dump: {error}', file=sys.stderr)
        return 1
    if left[0]['epoch'] != right[0]['epoch']:
        print(f"Expected epoch transition: {left[0]['epoch']} -> {right[0]['epoch']}")
        for a, b in zip(left, right):
            if (a.get('type') == b.get('type') == 'program' and a['cache_key'] != b['cache_key']
                    and a['epoch_neutral_key'] is not None and a['epoch_neutral_key'] == b['epoch_neutral_key']):
                print(f"{a['name']} attempt {a['attempt']}: expected raw cache_key difference (epochs differ)")

    def show(value):
        return '<missing>' if value is MISSING else json.dumps(value, sort_keys=True, ensure_ascii=False)

    groups = [('', differences)]
    if args.separate_driver_fields:
        driver, other = [], []
        for difference in differences:
            top = difference[1].split('.', 1)[0].split('[', 1)[0]
            (driver if top in DRIVER_FIELDS else other).append(difference)
        groups = [('Program/source differences', other), ('Driver assignments and locations', driver)]
    for title, group in groups:
        if title:
            print(f'{title}: {len(group)}')
        for program, field, before, after in group:
            print(f'{program}: {field}: {show(before)} -> {show(after)}')
    print(f'{len(differences)} difference(s)')
    return int(bool(differences))


if __name__ == '__main__':
    raise SystemExit(main())
