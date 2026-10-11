#!/usr/bin/env python3
"""Per-control wiring table for the Director UI: ORIG vs V1 vs V2.

READ-ONLY analysis. Writes only:
  doc/director_kit/wiring/wiring_table.csv
  doc/director_kit/wiring/WIRING_SUMMARY.md

Versions:
  ORIG = git commit 9c25f75491a (original v1 Director; read via git blobs)
  V1   = working tree floater_director.xml + owner panel XUIs (page owners from page_sources.json)
  V2   = working tree floater_director_v2.xml + panel_dpage_*.xml (kit_page page="group/page")
         + director_kit_index.xml (TSV inside CDATA, cross-check only)

Per control (one mockup row = one page x card x row) the script records the XUI/kit element,
its bound settings, its commit/init/validate callbacks, and C++ references found in the
top-level indra/newview/*.cpp of the same version (ORIG cpp read from the commit).

Status values:
  WIRED_SETTING   a bound setting (control_name / setting / sel_setting ...) is declared in app_settings/settings*.xml
  WIRED_CALLBACK  a callback function name from the XUI is referenced in C++ (not only its definition)
  WIRED_CPP       a getChild-style C++ access to the widget name inside an owner class of the page
  UNWIRED         present in the XUI but none of the above
  MISSING         widget not present in that version's page files
  V2_SUSPECT_FLOATER_SCOPED  (V2 only) the only C++ support is LLFloaterDirector (v1 path, not
                  DirectorUIV2-aware) or a panel class whose XML is not embedded in the V2 page

Usage:  python -I scripts/code_tools/director_wiring_table.py
Stdlib only.
"""
import bisect
import csv
import json
import os
import re
import subprocess
import sys
from collections import Counter, OrderedDict, defaultdict

ORIG_COMMIT = '9c25f75491a'
XUI_REL = 'indra/newview/skins/default/xui/en'
CPP_REL = 'indra/newview'
SETTINGS_REL = 'indra/newview/app_settings'
MOCKUP_REL = 'scripts/code_tools/director_kit/director_mockup_data.json'
PAGE_SOURCES_REL = 'doc/director_kit/page_sources.json'
KIT_INDEX_REL = XUI_REL + '/director_kit_index.xml'
OUT_REL = 'doc/director_kit/wiring'
V1_FLOATER = XUI_REL + '/floater_director.xml'
V2_FLOATER = XUI_REL + '/floater_director_v2.xml'
FLOATER_CLASS = 'LLFloaterDirector'
NONINTERACTIVE_KINDS = {'readout', 'text'}

SETTING_ATTRS = ('control_name', 'setting', 'sel_setting', 'mirror_setting', 'master_setting',
                 'enable_setting', 'mode_setting')
CALLBACK_ATTRS = ('commit_callback.function', 'commit_callback', 'init_callback.function',
                  'init_callback', 'validate_callback.function', 'validate_callback',
                  'callback.function', 'function')
KIT_CALLBACK_ATTRS = ('action',)

ACCESS_RE = re.compile(r'getChild|findChild|childSet|childGet|childShow|childHide|childEnable|'
                       r'childDisable|childIs|childHas|getChildView|setCommitCallback|'
                       r'childSetAction|findControl|Registrar|commit\.add')
FUNC_RE = re.compile(r'^(?:[A-Za-z_][\w:<>\*&,]*\s+)*?(~?[A-Za-z_]\w*)::(~?[A-Za-z_]\w*)\s*\(')
TOKEN_RE = re.compile(r'"((?:[^"\\]|\\.)*)"|([A-Za-z_][\w.]*)')
PANEL_REG_RE = re.compile(r'LLPanelInjector\s*<\s*(\w+)\s*>\s*\w+\s*\(\s*"([^"]+)"\s*\)')
SETTING_DECL_RE = re.compile(
    r'<key>([A-Za-z_]\w*)</key>\s*<map>\s*<key>(?:Comment|Persist|Type|Value|Group)</key>')
COMMENT_RE = re.compile(r'<!--.*?-->', re.S)
TAG_RE = re.compile(r'<([A-Za-z_][\w.\-]*)((?:\s+[A-Za-z_][\w.:\-]*\s*=\s*"[^"]*")*)\s*/?>')
ATTR_RE = re.compile(r'([A-Za-z_][\w.:\-]*)\s*=\s*"([^"]*)"')
DPAGE_RE = re.compile(r'<kit_page\b[^>]*\bpage="([^"]+)"')

WIRED = ('WIRED_SETTING', 'WIRED_CALLBACK', 'WIRED_CPP')


def repo_root():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, '..', '..'))


def newline_index(text):
    return [m.start() for m in re.finditer('\n', text)]


def line_of(nl, pos):
    return bisect.bisect_left(nl, pos) + 1


def blank_keep_lines(m):
    return re.sub(r'[^\n]', ' ', m.group(0))


class Repo(object):
    def __init__(self, root):
        self.root = root
        self._blob = {}

    def _git(self, args, data=None):
        r = subprocess.run(['git', '-C', self.root] + args, input=data,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if r.returncode != 0:
            raise RuntimeError('git %s failed: %s' % (' '.join(args), r.stderr[:300]))
        return r.stdout

    def wt_text(self, rel):
        p = os.path.join(self.root, *rel.split('/'))
        if not os.path.isfile(p):
            return None
        with open(p, 'rb') as f:
            return f.read().decode('utf-8', 'replace')

    def blob_text(self, commit, rel):
        key = (commit, rel)
        if key not in self._blob:
            r = subprocess.run(['git', '-C', self.root, 'cat-file', '-p', commit + ':' + rel],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self._blob[key] = r.stdout.decode('utf-8', 'replace') if r.returncode == 0 else None
        return self._blob[key]

    def list_dir(self, commit, rel_dir):
        if commit is None:
            d = os.path.join(self.root, *rel_dir.split('/'))
            if not os.path.isdir(d):
                return []
            return [rel_dir + '/' + n for n in sorted(os.listdir(d))]
        out = self._git(['ls-tree', '--name-only', commit, rel_dir + '/']).decode('utf-8', 'replace')
        return [l.strip() for l in out.splitlines() if l.strip()]

    def blobs_batch(self, commit, rels):
        """Read many blobs at a commit with one cat-file --batch call (order preserved)."""
        if not rels:
            return {}
        req = ''.join(commit + ':' + r + '\n' for r in rels).encode('utf-8')
        data = self._git(['cat-file', '--batch'], data=req)
        out = {}
        pos = 0
        for rel in rels:
            nl = data.index(b'\n', pos)
            parts = data[pos:nl].decode('utf-8', 'replace').split()
            pos = nl + 1
            if len(parts) == 3 and parts[1] == 'blob':
                size = int(parts[2])
                out[rel] = data[pos:pos + size].decode('utf-8', 'replace')
                pos += size + 1
            else:
                out[rel] = None
        return out


class Source(object):
    """One version's file source. commit None = working tree."""

    def __init__(self, repo, commit, label):
        self.repo = repo
        self.commit = commit
        self.label = label
        self.cache = {}

    def text(self, rel):
        if rel not in self.cache:
            if self.commit is None:
                self.cache[rel] = self.repo.wt_text(rel)
            else:
                self.cache[rel] = self.repo.blob_text(self.commit, rel)
        return self.cache[rel]

    def listdir(self, rel_dir):
        return self.repo.list_dir(self.commit, rel_dir)


def parse_xml(text):
    clean = COMMENT_RE.sub(blank_keep_lines, text)
    nl = newline_index(clean)
    elems = []
    for m in TAG_RE.finditer(clean):
        attrs = {}
        for k, v in ATTR_RE.findall(m.group(2)):
            attrs.setdefault(k, v)
        elems.append((m.group(1), line_of(nl, m.start()), attrs))
    return elems


def load_settings(src):
    decl = {}
    for rel in src.listdir(SETTINGS_REL):
        base = rel.rsplit('/', 1)[-1]
        if not (base.startswith('settings') and base.endswith('.xml')):
            continue
        text = src.text(rel)
        if text is None:
            continue
        nl = newline_index(text)
        for m in SETTING_DECL_RE.finditer(text):
            decl.setdefault(m.group(1), (rel, line_of(nl, m.start())))
    return decl


class Hit(object):
    __slots__ = ('rel', 'line', 'kind', 'access', 'is_def', 'cls', 'fn', 'fline')

    def __init__(self, rel, line, kind, access, is_def, cls, fn, fline):
        self.rel = rel
        self.line = line
        self.kind = kind
        self.access = access
        self.is_def = is_def
        self.cls = cls
        self.fn = fn
        self.fline = fline

    def where(self):
        base = self.rel.rsplit('/', 1)[-1]
        if self.cls:
            return '%s:%d %s::%s' % (base, self.line, self.cls, self.fn)
        return '%s:%d %s' % (base, self.line, self.fn)


class CppIndex(object):
    """Index of string-literal and identifier occurrences of needed names in top-level cpp files."""

    def __init__(self, src, needed):
        self.src = src
        self.files = {}          # rel -> (start_lines, starts[(line, cls, fn)], v2aware_start_lines)
        self.hits = defaultdict(list)
        self.panel_reg = {}      # panel file stem -> C++ class
        self.nfiles = 0
        rels = [r for r in src.listdir(CPP_REL) if r.endswith('.cpp') and '/' not in r[len(CPP_REL) + 1:]]
        if src.commit is None:
            texts = {r: src.text(r) for r in rels}
        else:
            texts = src.repo.blobs_batch(src.commit, rels)
        for rel in rels:
            text = texts.get(rel)
            if not text:
                continue
            self.nfiles += 1
            self._index_file(rel, text, needed)

    def _index_file(self, rel, text, needed):
        lines = text.split('\n')
        starts = []
        for i, line in enumerate(lines, 1):
            m = FUNC_RE.match(line)
            if m and not line.rstrip().endswith(';'):
                starts.append((i, m.group(1), m.group(2)))
        start_lines = [s[0] for s in starts]
        v2aware = set()
        if 'DirectorUIV2' in text:
            bounds = start_lines + [len(lines) + 1]
            for k, (sl, _c, _f) in enumerate(starts):
                body = '\n'.join(lines[sl - 1:bounds[k + 1] - 1])
                if 'DirectorUIV2' in body:
                    v2aware.add(sl)
        self.files[rel] = (start_lines, starts, v2aware)
        for m in PANEL_REG_RE.finditer(text):
            self.panel_reg.setdefault(m.group(2), m.group(1))
        acc = [bool(ACCESS_RE.search(l)) for l in lines]
        for i, line in enumerate(lines, 1):
            code = line.split('//', 1)[0] if '//' in line else line
            if not code.strip():
                continue
            is_def = bool(FUNC_RE.match(line))
            access = any(acc[j] for j in range(max(0, i - 4), min(len(lines), i + 1)))
            for m in TOKEN_RE.finditer(code):
                if m.group(1) is not None:
                    name, kind = m.group(1), 'lit'
                else:
                    name, kind = m.group(2), 'id'
                if name not in needed:
                    continue
                k = bisect.bisect_right(start_lines, i) - 1
                if k >= 0:
                    sl, cls, fn = starts[k][0], starts[k][1], starts[k][2]
                else:
                    sl, cls, fn = 0, '', '(file scope)'
                self.hits[name].append(Hit(rel, i, kind, access, is_def, cls, fn, sl))


class Ctx(object):
    """Everything needed to evaluate one version."""

    def __init__(self, name, src, decl, cpp, closures_roots, is_v2, owner_classes, dpage_by_page=None):
        self.name = name
        self.src = src
        self.decl = decl
        self.cpp = cpp
        self.roots_for = closures_roots      # page -> list of root rel paths
        self.is_v2 = is_v2
        self.owner_classes = owner_classes   # page -> set(class)
        self.dpage_by_page = dpage_by_page or {}
        self.elem_cache = {}
        self.closure_cache = {}
        self.index_cache = {}
        self.panel_types = set(cpp.panel_reg.values()) if cpp is not None else set()

    def elems(self, rel):
        if rel not in self.elem_cache:
            t = self.src.text(rel)
            self.elem_cache[rel] = None if t is None else parse_xml(t)
        return self.elem_cache[rel]

    def closure(self, page):
        if page in self.closure_cache:
            return self.closure_cache[page]
        order, seen = [], set()
        queue = [(r, 0) for r in self.roots_for(page)]
        i = 0
        while i < len(queue):
            rel, depth = queue[i]
            i += 1
            if rel in seen:
                continue
            seen.add(rel)
            els = self.elems(rel)
            if els is None:
                continue
            order.append(rel)
            if depth < 6:
                for _tag, _line, attrs in els:
                    fn = attrs.get('filename')
                    if fn:
                        queue.append((XUI_REL + '/' + fn, depth + 1))
        self.closure_cache[page] = order
        return order

    def index(self, page):
        """(by_name, by_widget, by_setting, by_tag) for a page closure.

        by_name: explicit name attribute. by_widget: kit row widget= attribute.
        by_setting: every bound setting attribute value (alias fallback).
        by_tag: elements without a name attribute, keyed by tag (LLView default name)."""
        if page in self.index_cache:
            return self.index_cache[page]
        by_name = defaultdict(list)
        by_widget = defaultdict(list)
        by_setting = defaultdict(list)
        by_tag = defaultdict(list)
        for rel in self.closure(page):
            for tag, line, attrs in self.elems(rel):
                if 'name' in attrs:
                    by_name[attrs['name']].append((rel, line, tag, attrs))
                else:
                    by_tag[tag].append((rel, line, tag, attrs))
                if 'widget' in attrs:
                    by_widget[attrs['widget']].append((rel, line, tag, attrs))
                for k in SETTING_ATTRS:
                    if attrs.get(k):
                        by_setting[attrs[k]].append((rel, line, tag, attrs))
        self.index_cache[page] = (by_name, by_widget, by_setting, by_tag)
        return self.index_cache[page]

    def inst_classes(self, page):
        """V2: panel classes whose XML is embedded in the page closure (+ the floater class)."""
        inst = {FLOATER_CLASS}
        for rel in self.closure(page):
            stem = rel.rsplit('/', 1)[-1]
            if stem.endswith('.xml'):
                stem = stem[:-4]
            if stem in self.cpp.panel_reg:
                inst.add(self.cpp.panel_reg[stem])
        return inst


def eval_row(ctx, page, widget, setting=''):
    """Evaluate one widget on one page in one version. Returns an evidence dict.

    Lookup order: XUI name (tag if unnamed) -> kit row widget= -> alias by bound setting
    (the mockup row's setting matches a control_name/setting on the same page)."""
    res = {'present': False, 'source': '', 'file': '', 'line': '', 'tag': '', 'dupes': 0,
           'bound': [], 'declared': [], 'undeclared': [], 'callbacks': [], 'cb_hits': [],
           'cpp_owner': [], 'cpp_other': [], 'lit_only': [], 'kit_cpp': [], 'kit_action_hits': [],
           'status': 'MISSING', 'basis': '', 'evidence': '', 'suspect': False, 'support': []}
    by_name, by_widget, by_setting, by_tag = ctx.index(page)
    cands = by_name.get(widget)
    source = 'xui'
    if not cands:
        cands = by_widget.get(widget)
        source = 'kit'
    if not cands and setting:
        cands = by_setting.get(setting)
        source = 'alias'
    if not cands:
        cands = by_tag.get(widget)
        source = 'xui-unnamed'
    if not cands:
        return res
    rel, line, tag, attrs = cands[0]
    is_kit = source == 'kit' or (source == 'alias' and tag.startswith('kit_'))
    kind = 'kit' if is_kit else 'xui'
    if source == 'alias':
        src_label = 'alias-kit' if is_kit else 'alias-xui'
    else:
        src_label = source
    res.update(present=True, source=src_label, file=rel, line=str(line), tag=tag, dupes=len(cands))
    res['bound'] = [attrs[k] for k in SETTING_ATTRS if attrs.get(k)]
    res['declared'] = [s for s in res['bound'] if s in ctx.decl]
    res['undeclared'] = [s for s in res['bound'] if s not in ctx.decl]
    cb_names = [attrs[k].split()[0] for k in CALLBACK_ATTRS if attrs.get(k) and attrs[k].split()]
    if kind == 'kit':
        cb_names += [attrs[k].split()[0] for k in KIT_CALLBACK_ATTRS if attrs.get(k) and attrs[k].split()]
    res['callbacks'] = sorted(set(cb_names))
    for cb in res['callbacks']:
        hs = [h for h in ctx.cpp.hits.get(cb, []) if not h.is_def]
        res['cb_hits'].extend(hs)
    ename = attrs.get('name') or attrs.get('widget') or tag
    lookup = [widget] if ename == widget else [widget, ename]
    lit = [h for n in lookup for h in ctx.cpp.hits.get(n, []) if h.kind == 'lit']
    owners = ctx.owner_classes.get(page, set())
    res['cpp_owner'] = [h for h in lit if h.access and h.cls in owners]
    res['cpp_other'] = [h for h in lit if h.access and h.cls not in owners]
    res['lit_only'] = [h for h in lit if not h.access]
    res['kit_cpp'] = [h for h in lit if h.rel.startswith(CPP_REL + '/alkit')]
    if kind == 'kit':
        res['kit_action_hits'] = [h for h in res['cb_hits']]

    parts = []
    if res['declared']:
        parts.append('SETTING')
    if res['cb_hits']:
        parts.append('CALLBACK')
    if res['cpp_owner'] or (kind == 'kit' and res['kit_cpp']):
        parts.append('CPP')

    if kind == 'kit':
        if res['declared']:
            status = 'WIRED_SETTING'
        elif res['cb_hits'] or res['kit_cpp']:
            status = 'WIRED_CPP'
        else:
            status = 'UNWIRED'
        support = res['cb_hits'] + res['kit_cpp']
    else:
        if res['declared']:
            status = 'WIRED_SETTING'
        elif res['cb_hits']:
            status = 'WIRED_CALLBACK'
        elif res['cpp_owner']:
            status = 'WIRED_CPP'
        else:
            status = 'UNWIRED'
        support = res['cb_hits'] + res['cpp_owner']

    if ctx.is_v2 and kind == 'xui' and status in ('WIRED_CALLBACK', 'WIRED_CPP') and support:
        def floater_scoped(h):
            return h.cls == FLOATER_CLASS and h.fline not in ctx.cpp.files.get(h.rel, ([], [], set()))[2]

        def not_inst(h):
            return h.cls in ctx.cpp.panel_reg.values() and h.cls not in ctx.inst_classes(page)

        if all(floater_scoped(h) or not_inst(h) for h in support):
            status = 'V2_SUSPECT_FLOATER_SCOPED'
            res['suspect'] = True
    res['support'] = support
    res['status'] = status
    res['basis'] = '+'.join(parts) if parts else ('LIT_ONLY' if res['lit_only'] else '')

    ev = []
    if res['declared']:
        ev.append('setting %s (%s)' % (','.join(res['declared']), ctx.decl[res['declared'][0]][0].rsplit('/', 1)[-1]))
    if res['undeclared']:
        ev.append('undeclared setting %s' % ','.join(res['undeclared']))
    for cb in res['callbacks']:
        hs = [h for h in res['cb_hits']]
        if hs:
            ev.append('cb %s -> %s' % (cb, hs[0].where()))
        else:
            ev.append('cb %s (no C++ ref)' % cb)
    for h in res['cpp_owner'][:3]:
        ev.append('cpp %s' % h.where())
    if res['cpp_other'] and not res['cpp_owner']:
        ev.append('cpp other-class %s' % res['cpp_other'][0].where())
    if res['lit_only'] and not res['cpp_owner']:
        ev.append('literal-only %s' % res['lit_only'][0].where())
    if kind == 'kit':
        ev.append('kit row, kit-cpp refs=%d' % len(res['kit_cpp']))
    res['evidence'] = '; '.join(ev)
    return res


def explicit_names(ctx, page):
    """name -> (file, line, tag) for elements with an explicit name attribute in the page closure."""
    out = {}
    for rel in ctx.closure(page):
        for tag, line, attrs in ctx.elems(rel):
            n = attrs.get('name')
            if n and n not in out:
                out[n] = (rel, line, tag)
    return out


def build_catalog(mock, ps_pages):
    """Control catalog: every mockup row, plus page_sources rows whose (page, widget) the mockup
    does not carry (page_sources expands option-indexed rows). Duplicate (page, widget) collapse."""
    rows = []
    pages = OrderedDict()
    for g in mock['groups']:
        for p in g['pages']:
            pages[p['name']] = {'key': p['name'], 'label': p.get('label', ''),
                                'group': g['name'], 'group_label': g.get('label', '')}
            for ci, c in enumerate(p['cards']):
                for ri, r in enumerate(c['rows']):
                    kind = r.get('k', '')
                    rows.append({
                        'page': p['name'], 'group': g['name'], 'card': c['id'],
                        'card_title': c.get('t', ''), 'row': ri, 'card_idx': ci,
                        'label': r.get('l', ''), 'kind': kind, 'widget': r.get('w', ''),
                        'setting': r.get('s', ''), 'catalog': 'mockup',
                        'interactive': (kind not in NONINTERACTIVE_KINDS) and bool(r.get('w')),
                    })
    seen = set((r['page'], r['widget']) for r in rows)
    extra = 0
    for pk in pages:
        for prow in ps_pages.get(pk, {}).get('rows', []):
            w = prow.get('widget') or ''
            if not w or (pk, w) in seen:
                continue
            seen.add((pk, w))
            kind = prow.get('kind', '') or ''
            rows.append({
                'page': pk, 'group': pages[pk]['group'], 'card': prow.get('card', '') or '',
                'card_title': prow.get('card_title', '') or '', 'row': prow.get('index', ''),
                'card_idx': -1, 'label': prow.get('label', '') or '', 'kind': kind, 'widget': w,
                'setting': prow.get('declared_setting') or '', 'catalog': 'page_sources',
                'interactive': (kind not in NONINTERACTIVE_KINDS) and bool(w),
            })
            extra += 1
    return rows, pages, extra


def parse_kit_index(text):
    """Cross-check table from director_kit_index.xml (TSV inside CDATA, 15 fields).

    Returns {(page, widget, label): (setting, kind)} and {(page, widget): (setting, kind)} (first)."""
    full, pw = {}, {}
    i = text.find('CDATA[')
    j = text.rfind(']]>')
    if i < 0 or j < 0:
        return full, pw
    for line in text[i + 6:j].split('\n'):
        f = line.split('\t')
        if len(f) < 12 or f[1] == '' or f[8] == '':
            continue
        val = (f[11], f[10])
        full.setdefault((f[1], f[8], f[9]), val)
        pw.setdefault((f[1], f[8]), val)
    return full, pw


def main():
    root = repo_root()
    repo = Repo(root)
    out_dir = os.path.join(root, *OUT_REL.split('/'))
    os.makedirs(out_dir, exist_ok=True)

    mock = json.loads(repo.wt_text(MOCKUP_REL))
    ps = json.loads(repo.wt_text(PAGE_SOURCES_REL))
    ps_pages = {p['key']: p for p in ps['pages']}
    rows, pages, n_extra = build_catalog(mock, ps_pages)
    n_mock = sum(1 for r in rows if r['catalog'] == 'mockup')

    orig = Source(repo, ORIG_COMMIT, 'ORIG')
    v1 = Source(repo, None, 'V1')
    v2 = Source(repo, None, 'V2')

    # page -> owner classes (from page_sources, derived from the current tree)
    owner_classes = {k: {c['class'] for c in p.get('owner_classes', [])} for k, p in ps_pages.items()}

    # V2: map each page to its panel_dpage file via kit_page page="group/page"
    dpage_by_page = {}
    for rel in v2.listdir(XUI_REL):
        base = rel.rsplit('/', 1)[-1]
        if not (base.startswith('panel_dpage_') and base.endswith('.xml')):
            continue
        m = DPAGE_RE.search(v2.text(rel) or '')
        if m:
            key = m.group(1).split('/')[-1]
            dpage_by_page[key] = rel

    def roots_orig(page):
        rs = []
        for r in ps_pages.get(page, {}).get('owner_roots', []):
            if orig.text(r['file']) is not None:
                rs.append(r['file'])
        if orig.text(V1_FLOATER) is not None:
            rs.append(V1_FLOATER)
        return list(OrderedDict.fromkeys(rs))

    def roots_v1(page):
        rs = []
        for r in ps_pages.get(page, {}).get('owner_roots', []):
            if v1.text(r['file']) is not None:
                rs.append(r['file'])
        rs.append(V1_FLOATER)
        return list(OrderedDict.fromkeys(rs))

    def roots_v2(page):
        rs = []
        if page in dpage_by_page:
            rs.append(dpage_by_page[page])
        rs.append(V2_FLOATER)
        return rs

    # Page-level mapping: pages in mockup order
    page_keys = list(pages.keys())
    # Note: page_sources key 'move_tab' etc. Use the mockup page key for lookups.
    ctx_orig_tmp = Ctx('ORIG', orig, {}, None, roots_orig, False, owner_classes)
    ctx_v1_tmp = Ctx('V1', v1, {}, None, roots_v1, False, owner_classes)
    ctx_v2_tmp = Ctx('V2', v2, {}, None, roots_v2, True, owner_classes, dpage_by_page)

    # Settings declarations (needed later)
    decl_orig = load_settings(orig)
    decl_v1 = load_settings(v1)

    # Collect needed names from all XUI closures plus the mockup widgets
    needed = set(r['widget'] for r in rows if r['widget'])
    for ctx in (ctx_orig_tmp, ctx_v1_tmp, ctx_v2_tmp):
        for pk in page_keys:
            by_name, by_widget, _by_setting, _by_tag = ctx.index(pk)
            needed.update(by_name.keys())
            needed.update(by_widget.keys())
            for name, lst in by_name.items():
                for _rel, _line, _tag, attrs in lst:
                    for k in CALLBACK_ATTRS + KIT_CALLBACK_ATTRS:
                        if attrs.get(k) and attrs[k].split():
                            needed.add(attrs[k].split()[0])
    print('needed names: %d' % len(needed), file=sys.stderr)

    cpp_orig = CppIndex(orig, needed)
    cpp_v1 = CppIndex(v1, needed)
    print('cpp files: ORIG=%d WT=%d; panel registrations ORIG=%d WT=%d' % (
        cpp_orig.nfiles, cpp_v1.nfiles, len(cpp_orig.panel_reg), len(cpp_v1.panel_reg)), file=sys.stderr)

    ctx_o = Ctx('ORIG', orig, decl_orig, cpp_orig, roots_orig, False, owner_classes)
    ctx_1 = Ctx('V1', v1, decl_v1, cpp_v1, roots_v1, False, owner_classes)
    ctx_2 = Ctx('V2', v2, decl_v1, cpp_v1, roots_v2, True, owner_classes, dpage_by_page)

    # V2 launcher stubs: the dpage only holds an "Open ... in Lightbox" button (kind="launcher"
    # or card_*_open). Their controls live in the V1 owner floater(s), which V2 reaches through
    # the launcher. Rows missing from the stub dpage are resolved there and marked redirect-*.
    stub_pages = set()
    for pk, rel in dpage_by_page.items():
        t = v2.text(rel) or ''
        if re.search(r'kind="launcher"|name="card_[A-Za-z0-9_]*_open"', t):
            stub_pages.add(pk)

    def roots_redirect(page):
        return [r['file'] for r in ps_pages.get(page, {}).get('owner_roots', [])
                if v1.text(r['file']) is not None]
    ctx_r = Ctx('V2-REDIRECT', v1, decl_v1, cpp_v1, roots_redirect, False, owner_classes)

    kit_full, kit_index = parse_kit_index(repo.wt_text(KIT_INDEX_REL) or '')

    # Evaluate every catalog row in each version
    results = []
    for r in rows:
        pg, w = r['page'], r['widget']
        eo = eval_row(ctx_o, pg, w, r['setting'])
        e1 = eval_row(ctx_1, pg, w, r['setting'])
        e2 = eval_row(ctx_2, pg, w, r['setting'])
        if not e2['present'] and pg in stub_pages and r['interactive']:
            rr = eval_row(ctx_r, pg, w, r['setting'])
            if rr['present']:
                rr['source'] = 'redirect-' + rr['source']
                rr['via'] = 'launcher stub -> ' + rr['file'].rsplit('/', 1)[-1]
                e2 = rr
        idx = kit_full.get((pg, w, r['label'])) or kit_index.get((pg, w))
        results.append((r, eo, e1, e2, idx))

    # V2 relocation: a row missing on its page but present on another V2 page (the mockup lists
    # some controls on two pages). Such a row is relocated, not lost.
    # Matched by widget name or by bound setting (the mockup often renames the widget, e.g.
    # shot_tab CinematicCamVertigoDirection is zolly_direction on moves_tab, same setting).
    v2_pages_of = defaultdict(set)
    for r, eo, e1, e2, idx in results:
        if e2['present']:
            v2_pages_of[('w', r['widget'])].add(r['page'])
            if r['setting']:
                v2_pages_of[('s', r['setting'])].add(r['page'])
    for r, eo, e1, e2, idx in results:
        if e2['status'] in ('MISSING', 'UNWIRED'):
            others = set(v2_pages_of.get(('w', r['widget']), set()))
            if r['setting']:
                others |= v2_pages_of.get(('s', r['setting']), set())
            others = sorted(others - {r['page']})
            if others:
                e2['reloc'] = ','.join(others)

    # V1 regression: names in the ORIG closure but not in the V1 closure, per page
    removed = []
    for pk in page_keys:
        bo = explicit_names(ctx_o, pk)
        b1 = explicit_names(ctx_1, pk)
        for name in sorted(set(bo) - set(b1)):
            rel, line, tag = bo[name]
            refs = [h for h in cpp_v1.hits.get(name, []) if h.kind == 'lit']
            removed.append((pk, name, tag, '%s:%d' % (rel.rsplit('/', 1)[-1], line), len(refs),
                            refs[0].where() if refs else ''))

    global PAGE_LABELS
    PAGE_LABELS = {k: v['label'] for k, v in pages.items()}
    write_csv(os.path.join(out_dir, 'wiring_table.csv'), results)
    summary = build_summary(results, removed, pages, ps_pages, decl_v1, kit_index, dpage_by_page,
                            ctx_2, cpp_v1, cpp_orig, rows)
    with open(os.path.join(out_dir, 'WIRING_SUMMARY.md'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(summary)

    tot = Counter_totals(results)
    print(json.dumps(tot, indent=1))


def Counter_totals(results):
    out = {}
    for label, idx in (('ORIG', 1), ('V1', 2), ('V2', 3)):
        c = defaultdict(int)
        for r, *ev in [(x[0], x[1], x[2], x[3]) for x in results]:
            if not r['interactive']:
                continue
            c[ev[idx - 1]['status']] += 1
        out[label] = dict(c)
    return out


def wired(status):
    return status in WIRED


def is_loss(x):
    """Wired in ORIG or V1, UNWIRED or MISSING in V2, and not relocated to another V2 page."""
    return ((wired(x[1]['status']) or wired(x[2]['status']))
            and x[3]['status'] in ('UNWIRED', 'MISSING') and not x[3].get('reloc'))


def write_csv(path, results):
    cols = ['page', 'page_label', 'group', 'card', 'card_title', 'row', 'label', 'kind', 'interactive',
            'catalog', 'widget', 'mockup_setting']
    for v in ('orig', 'v1', 'v2'):
        cols += ['%s_status' % v, '%s_basis' % v, '%s_source' % v, '%s_xui' % v, '%s_bound' % v,
                 '%s_callbacks' % v, '%s_evidence' % v]
    cols += ['v2_suspect', 'v2_index_setting', 'v2_index_match', 'v2_kit_cpp_refs', 'v1_regression',
             'v2_relocated_to', 'v2_loss', 'v2_loss_detail', 'v2_setting_mismatch']
    with open(path, 'w', encoding='utf-8', newline='') as f:
        wr = csv.writer(f)
        wr.writerow(cols)
        for r, eo, e1, e2, idx in results:
            row = [r['page'], PAGE_LABELS.get(r['page'], ''), r['group'], r['card'], r['card_title'],
                   r['row'], r['label'], r['kind'], 'Y' if r['interactive'] else 'N', r['catalog'],
                   r['widget'], r['setting']]
            for e in (eo, e1, e2):
                row += [e['status'], e['basis'], e['source'],
                        ('%s:%s <%s>' % (e['file'].rsplit('/', 1)[-1], e['line'], e['tag'])) if e['present'] else '',
                        ','.join(e['bound']), ','.join(e['callbacks']), e['evidence']]
            idx_setting = idx[0] if idx else ''
            idx_kind = idx[1] if idx else ''
            v2_bound = e2['bound'][0] if e2['bound'] else ''
            mismatch = 'Y' if (r['setting'] and idx and idx_setting and idx_setting != r['setting']) else ''
            if e2['bound'] and idx_setting and e2['bound'][0] != idx_setting:
                mismatch = 'Y'
            reg = 'Y' if (r['interactive'] and wired(eo['status']) and not wired(e1['status'])) else ''
            base_wired = r['interactive'] and (wired(eo['status']) or wired(e1['status']))
            loss = ''
            loss_kind = ''
            if base_wired and e2['status'] in ('UNWIRED', 'MISSING'):
                if e2.get('reloc'):
                    loss = 'RELOCATED'
                    loss_kind = 'V2 page(s): ' + e2['reloc']
                else:
                    loss = 'Y'
                    loss_kind = e2['status']
            elif base_wired and e2['status'] == 'V2_SUSPECT_FLOATER_SCOPED':
                loss = 'SUSPECT'
                loss_kind = e2['status']
            row += ['Y' if e2['suspect'] else '', idx_setting, 'Y' if idx else '',
                    len(e2['kit_cpp']), reg, e2.get('reloc', ''), loss, loss_kind, mismatch]
            wr.writerow(row)


PAGE_LABELS = {}


def build_summary(results, removed, pages, ps_pages, decl, kit_index, dpage_by_page, ctx_2,
                  cpp_v1, cpp_orig, rows):
    global PAGE_LABELS
    PAGE_LABELS = {k: v['label'] for k, v in pages.items()}
    lines = []
    w = lines.append
    w('# Director wiring summary (ORIG vs V1 vs V2)')
    w('')
    w('Generated by `scripts/code_tools/director_wiring_table.py` (stdlib, `python -I`). Read-only.')
    w('Per-control rows: `doc/director_kit/wiring/wiring_table.csv`.')
    w('')
    w('Versions: **ORIG** = commit 9c25f75491a (git blobs); **V1** = working tree `floater_director.xml` '
      '+ owner panels; **V2** = working tree `floater_director_v2.xml` + `panel_dpage_*.xml` + kit rows.')
    w('')
    inter = [x for x in results if x[0]['interactive']]
    w('Interactive controls: %d (of %d catalog rows; readout/text rows excluded from counts).' % (
        len(inter), len(results)))
    w('')

    def cnt(idx, pred):
        return sum(1 for x in inter if pred(x[idx]))

    w('## Totals')
    w('')
    w('| Version | Wired | WIRED_SETTING | WIRED_CALLBACK | WIRED_CPP | Suspect (V2) | UNWIRED | MISSING |')
    w('|---|---:|---:|---:|---:|---:|---:|---:|')
    for label, idx in (('ORIG', 1), ('V1', 2), ('V2', 3)):
        def st(s):
            return cnt(idx, lambda e, s=s: e['status'] == s)
        wired_n = cnt(idx, lambda e: wired(e['status']))
        w('| %s | %d | %d | %d | %d | %d | %d | %d |' % (
            label, wired_n, st('WIRED_SETTING'), st('WIRED_CALLBACK'), st('WIRED_CPP'),
            st('V2_SUSPECT_FLOATER_SCOPED'), st('UNWIRED'), st('MISSING')))
    w('')

    # Per page table
    w('## Per page: wired counts (ORIG -> V1 -> V2)')
    w('')
    w('| Page | Group | Interactive | ORIG wired | V1 wired | V2 wired | V2 suspect | V2 unwired | V2 missing | V2 loss |')
    w('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|')
    per_page = OrderedDict()
    for pk, pinfo in pages.items():
        ps_row_count = ps_pages.get(pk, {}).get('row_count', '')
        sel = [x for x in inter if x[0]['page'] == pk]
        n = len(sel)
        o = sum(1 for x in sel if wired(x[1]['status']))
        v = sum(1 for x in sel if wired(x[2]['status']))
        v2w = sum(1 for x in sel if wired(x[3]['status']))
        sus = sum(1 for x in sel if x[3]['status'] == 'V2_SUSPECT_FLOATER_SCOPED')
        un = sum(1 for x in sel if x[3]['status'] == 'UNWIRED')
        mi = sum(1 for x in sel if x[3]['status'] == 'MISSING')
        loss = sum(1 for x in sel if is_loss(x))
        per_page[pk] = (n, o, v, v2w, sus, un, mi, loss)
        w('| %s (%s) | %s | %d | %d | %d | %d | %d | %d | %d | %d |' % (
            pk, pinfo['label'], pinfo['group_label'], n, o, v, v2w, sus, un, mi, loss))
    w('')

    # Row count reconciliation
    w('## Reconciliation with page_sources.json')
    w('')
    n_mock = sum(1 for r in rows if r['catalog'] == 'mockup')
    n_ps = sum(1 for r in rows if r['catalog'] == 'page_sources')
    w('Catalog = %d mockup rows + %d page_sources-only (page, widget) rows = %d controls. '
      'page_sources.json lists %d row entries in total; it expands option-indexed rows (repeated widgets '
      'are collapsed here by page + widget).' % (
          n_mock, n_ps, len(rows), sum(p.get('row_count', 0) for p in ps_pages.values())))
    extra_inter = sum(1 for r in rows if r['catalog'] == 'page_sources' and r['interactive'])
    w('Of the page_sources-only rows, %d are interactive.' % extra_inter)
    w('')

    # V2 losses
    w('## V2 losses: wired in ORIG or V1, UNWIRED or MISSING in V2')
    w('')
    losses = [x for x in inter if is_loss(x)]
    relocs = [x for x in inter if (wired(x[1]['status']) or wired(x[2]['status']))
              and x[3]['status'] in ('UNWIRED', 'MISSING') and x[3].get('reloc')]
    w('Total: %d (excludes %d rows that are present on another V2 page, listed below)' % (
        len(losses), len(relocs)))
    w('')
    group_loss = OrderedDict()
    for x in losses:
        group_loss.setdefault(x[0]['page'], []).append(x)
    for pk, lst in group_loss.items():
        w('### %s (%d)' % (pk, len(lst)))
        w('')
        w('| Widget | Label | ORIG | V1 | V2 | V2 evidence |')
        w('|---|---|---|---|---|---|')
        for x in lst:
            r = x[0]
            w('| `%s` | %s | %s | %s | %s | %s |' % (
                r['widget'], r['label'].replace('|', '/'), x[1]['status'], x[2]['status'],
                x[3]['status'], (x[3]['evidence'] or '-').replace('|', '/')[:160]))
        w('')
    w('')

    w('### Relocated in V2 (missing on this page, present on another V2 page)')
    w('')
    if relocs:
        for x in relocs:
            w('- %s `%s` (%s): V2 page(s) %s' % (x[0]['page'], x[0]['widget'], x[0]['label'], x[3]['reloc']))
    else:
        w('None.')
    w('')

    # Suspects
    w('## V2 suspects: V2_SUSPECT_FLOATER_SCOPED (only support is LLFloaterDirector v1 path or a non-instantiated panel class)')
    w('')
    sus = [x for x in inter if x[3]['status'] == 'V2_SUSPECT_FLOATER_SCOPED']
    w('Total: %d (of which previously wired in ORIG or V1: %d)' % (
        len(sus), sum(1 for x in sus if wired(x[1]['status']) or wired(x[2]['status']))))
    w('')
    sus_by_page = OrderedDict()
    for x in sus:
        sus_by_page.setdefault(x[0]['page'], []).append(x)
    for pk, lst in sus_by_page.items():
        w('- **%s**: %s' % (pk, ', '.join('`%s`' % x[0]['widget'] for x in lst)))
    w('')

    # V1 regressions
    w('## V1 regressions (wired in ORIG, not wired in V1)')
    w('')
    regs = [x for x in inter if wired(x[1]['status']) and not wired(x[2]['status'])]
    w('Total: %d' % len(regs))
    for x in regs:
        r = x[0]
        w('- %s `%s` ORIG=%s V1=%s (%s)' % (r['page'], r['widget'], x[1]['status'], x[2]['status'],
                                            x[2]['evidence'] or 'no evidence'))
    w('')
    w('### Element-name removals ORIG -> V1 (whole page closure, not only mockup rows)')
    w('')
    w('Names present in the ORIG page closure but absent from the V1 closure: %d entries, %d distinct names.' % (
        len(removed), len(set(x[1] for x in removed))))
    if removed:
        w('')
        w('| Name | Tag | ORIG file:line | Pages | WT C++ literal refs | first ref |')
        w('|---|---|---|---|---:|---|')
        grouped = OrderedDict()
        for pk, name, tag, where, nref, first in removed:
            grouped.setdefault((name, tag, where, nref, first), []).append(pk)
        for (name, tag, where, nref, first), pks in grouped.items():
            w('| `%s` | %s | %s | %d page(s): %s | %d | %s |' % (
                name, tag, where, len(pks), ', '.join(pks[:4]) + (' ...' if len(pks) > 4 else ''), nref, first))
    w('')

    # Structural checks
    w('## Structural checks')
    w('')
    settings_undeclared = sum(1 for x in inter if x[2]['undeclared'])
    w('- Interactive rows whose V1 XUI control_name/setting is not declared in app_settings/settings*.xml: %d '
      '(may be declared in C++ instead; not counted as WIRED_SETTING).' % settings_undeclared)
    src_counts = Counter(x[3]['source'] or 'none' for x in inter)
    w('- V2 resolution by source: ' + ', '.join('%s=%d' % (k, v) for k, v in sorted(src_counts.items())) + '.')
    w('  `xui` = named widget in the V2 dpage closure; `kit` = kit row `widget=`; `alias-*` = matched by bound '
      'setting on the same page; `redirect-*` = V2 launcher stub, control found in the V1 owner floater it opens.')
    stub_list = sorted(set(x[0]['page'] for x in inter if x[3]['source'].startswith('redirect')))
    w('- Launcher-stub pages (V2 opens V1 floater): ' + (', '.join(stub_list) or 'none') + '.')
    mockup_only = [x for x in inter if x[1]['status'] == 'MISSING' and x[2]['status'] == 'MISSING'
                   and x[3]['status'] == 'MISSING']
    w('- Interactive rows missing in ALL three versions (mockup-only, never implemented): %d.' % len(mockup_only))
    idx_missing = sum(1 for x in inter if (x[0]['page'], x[0]['widget']) not in kit_index)
    w('- Rows absent from the director_kit_index.xml TSV (page, widget): %d of %d interactive.' % (
        idx_missing, len(inter)))
    mism = [x for x in inter if x[4] and x[3]['bound'] and x[4][0] and x[3]['bound'][0] != x[4][0]]
    w('- V2 bound setting differs from the kit index TSV: %d' % len(mism))
    w('- V2 pages without a panel_dpage file: %s' % (
        ', '.join(sorted(set(pages.keys()) - set(dpage_by_page.keys()))) or 'none'))
    w('')

    w('## Caveats')
    w('')
    w('- Static analysis only. A C++ match inside a FLOATER-owned postBuild (LLFloaterDirector) does not prove the '
      'V2 copy is wired; such V2 controls are flagged `V2_SUSPECT_FLOATER_SCOPED` unless the enclosing function '
      'mentions `DirectorUIV2`. This flag is conservative and needs in-world confirmation.')
    w('- A panel class counts as instantiated for a V2 page only when its XML is embedded in that page closure '
      '(`LLPanelInjector` registration name = file stem).')
    w('- Kit rows (V2 `kit_row`/`kit_switch` with `widget=`) have no per-widget C++; WIRED_SETTING means the bound '
      'setting is declared (kit binding is generic in alkit*.cpp), not that the in-world behaviour was checked.')
    w('- Control universe = the mockup rows (%d interactive) per page, matched to each version\'s XML. Widgets that '
      'exist in a version XUI but are not in the mockup are not enumerated.' % len(inter))
    w('- ORIG page owners are the HEAD owner files that exist at ORIG; ORIG floater_director.xml is always searched. '
      'ORIG C++ is read from the commit; V1/V2 C++ is the working tree.')
    w('- Callbacks count as WIRED only when the name is referenced (not just defined) in top-level indra/newview/*.cpp. '
      'Registry names are matched as literals or identifiers.')
    w('- Widget literal hits require a getChild/findChild/childSet/setCommitCallback/Registrar token within 3 lines '
      'before (or 1 line after) the literal, and the enclosing class must be one of the page owner classes from page_sources.json.')
    w('- V2 launcher stubs (a dpage whose only control is an "Open ... in Lightbox" style button) host their rows in '
      'the V1 owner floater they open. Those rows are resolved there (`redirect-*`), so their V2 status is the V1 '
      'wiring of the same XML, not a separate V2 implementation.')
    w('- A row missing on its own V2 page but present on another V2 page is "relocated" and is not counted as a loss.')
    w('- MISSING means no element with that name, kit widget, bound setting or unnamed tag exists in that version\'s '
      'page closure. Rows missing in all three versions are mockup-only and are not counted as losses.')
    w('')
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    sys.exit(main())
