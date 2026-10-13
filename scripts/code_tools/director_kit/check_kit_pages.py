#!/usr/bin/env python3
"""Validate scripts/code_tools/director_kit/kit_pages.json against spec section 4.7.

Stdlib only. Exit 0 when every rule passes, 1 otherwise.

Name resolution: a widget name referenced by the config must exist in
director_mockup_data.json, in doc/director_kit/page_sources.json, or as a
name= attribute in the page's own v1 source XML (v1_sources). The v1 XML is
needed because several 4.7 names (cine_setup_combo, cine_group_status, ...)
are v1 widgets that the mockup data and page_sources do not carry.

add_rows row widgets are new kit rows that host v1 nodes, so they are checked
for resolution and for duplication inside the page, not for prior existence.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_CONFIG = os.path.join(HERE, "kit_pages.json")
DEFAULT_DATA = os.path.join(HERE, "director_mockup_data.json")
DEFAULT_POLICY = os.path.join(HERE, "gate_policy.json")
DEFAULT_PAGE_SOURCES = os.path.join(REPO, "doc", "director_kit", "page_sources.json")
V1_XUI_DIR = os.path.join(REPO, "indra", "newview", "skins", "default", "xui", "en")

ROW_KINDS = {"slider", "number", "toggle", "combo", "color", "dial", "arc",
             "vec2", "vec3", "readout", "button", "text", "list", "link"}
PRESET_MODES = {"widgets", "provider"}
TOP_KEYS = {"version", "p0_pages", "launcher_targets", "parity_allow"}
PAGE_REQUIRED = {"v1_sources", "root_class", "root_name", "preset",
                 "selector_settings", "card_enables", "add_rows", "label_names"}
PAGE_OPTIONAL = {"undo_context", "who_adapter", "mode_adapter", "mode_on",
                 "option_enables", "row_overrides", "chip_mode", "card_copy_types"}
LAUNCHER_REQUIRED = {"target", "target_tab", "button_label", "help"}


class Checker:
    def __init__(self):
        self.errors = []

    def err(self, where, msg):
        self.errors.append("%s: %s" % (where, msg))

    def ok(self, cond, where, msg):
        if not cond:
            self.err(where, msg)
        return cond


def load_json(path, label):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError) as e:
        print("FAIL: cannot load %s (%s): %s" % (label, path, e))
        sys.exit(1)


def collect_data(data):
    """Return (pages_by_key, widget_names) from director_mockup_data.json."""
    pages = {}
    names = set()
    for g in data.get("groups", []):
        gname = g.get("name")
        for p in g.get("pages", []):
            key = "%s/%s" % (gname, p.get("name"))
            pages[key] = {"group": gname, "page": p}
            if isinstance(p.get("master"), dict) and p["master"].get("w"):
                names.add(p["master"]["w"])
            if isinstance(p.get("modes"), dict) and p["modes"].get("widget"):
                names.add(p["modes"]["widget"])
            for pk in (p.get("who") or {}).get("pickers", []) or []:
                names.add(pk)
            for c in p.get("cards", []):
                if c.get("ew"):
                    names.add(c["ew"])
                for r in c.get("rows", []) or []:
                    if r.get("w"):
                        names.add(r["w"])
                    if r.get("ew"):
                        names.add(r["ew"])
                for o in c.get("opts", []) or []:
                    if o.get("ew"):
                        names.add(o["ew"])
                    for r in o.get("rows", []) or []:
                        if r.get("w"):
                            names.add(r["w"])
                        if r.get("ew"):
                            names.add(r["ew"])
    return pages, names


def collect_page_sources(ps):
    """Widget-name strings from page_sources.json (any row-level widget field)."""
    keys = {"widget", "matched_widget", "enable_widget", "ew", "w"}
    names = set()

    def walk(node):
        if isinstance(node, dict):
            for k, v in node.items():
                if k in keys and isinstance(v, str):
                    names.add(v)
                else:
                    walk(v)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(ps)
    return names


def v1_names(files):
    """name= attribute values from the given v1 XML files; missing files -> error list."""
    names, missing = set(), []
    for fn in files:
        path = os.path.join(V1_XUI_DIR, fn)
        if not os.path.isfile(path):
            missing.append(fn)
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            names.update(re.findall(r'\bname="([^"]+)"', f.read()))
    return names, missing


def card_map(page):
    return {c.get("id"): c for c in page.get("cards", [])}


def page_rows(page):
    """Map row widget name -> (card id, option label or None, row dict)."""
    out = {}
    for c in page.get("cards", []):
        for r in c.get("rows", []) or []:
            if r.get("w"):
                out.setdefault(r["w"], (c.get("id"), None, r))
        for o in c.get("opts", []) or []:
            for r in o.get("rows", []) or []:
                if r.get("w"):
                    out.setdefault(r["w"], (c.get("id"), o.get("label"), r))
    return out


def check_page(chk, key, entry, data_pages, data_names, ps_names, pages_src_cfg):
    where = "p0_pages[%s]" % key
    if not chk.ok(isinstance(entry, dict), where, "entry must be an object"):
        return
    keys = set(entry)
    for missing in sorted(PAGE_REQUIRED - keys):
        chk.err(where, "missing required key '%s'" % missing)
    for extra in sorted(keys - PAGE_REQUIRED - PAGE_OPTIONAL):
        chk.err(where, "unknown key '%s'" % extra)

    if key not in data_pages:
        chk.err(where, "page not found in director_mockup_data.json")
        return
    if key == "capture_group/capture_settings_tab":
        chk.err(where, "Empty Capture settings page is dropped (USER_DECISIONS_20261010 item 2)")
    dpage = data_pages[key]["page"]
    cards = card_map(dpage)
    rows = page_rows(dpage)

    # v1 sources and name resolution scope for this page
    vfiles = entry.get("v1_sources", [])
    chk.ok(isinstance(vfiles, list) and len(vfiles) > 0 and
           all(isinstance(x, str) and x.endswith(".xml") for x in vfiles),
           where, "v1_sources must be a non-empty list of .xml names")
    v1, missing = v1_names(vfiles if isinstance(vfiles, list) else [])
    for m in missing:
        chk.err(where, "v1 source not found under xui/en: %s" % m)

    def resolves(name):
        return name in data_names or name in ps_names or name in v1

    for k_ in ("root_class", "root_name"):
        chk.ok(isinstance(entry.get(k_), str) and entry.get(k_), where,
               "%s must be a non-empty string" % k_)
    for k_ in ("undo_context", "who_adapter", "mode_adapter", "mode_on"):
        if k_ in entry:
            chk.ok(isinstance(entry[k_], str) and entry[k_], where,
                   "%s must be a non-empty string" % k_)
    if "chip_mode" in entry:
        chk.ok(entry["chip_mode"] in {"auto", "filter"}, where,
               "chip_mode must be 'auto' or 'filter'")

    # modes / who consistency with the data (who_adapter and mode_adapter come from config)
    if dpage.get("who"):
        chk.ok("who_adapter" in entry, where, "data has a who row; who_adapter required")
    if dpage.get("modes"):
        chk.ok("mode_adapter" in entry and "mode_on" in entry, where,
               "data has modes; mode_adapter and mode_on required")
        if "mode_on" in entry:
            opts = dpage["modes"].get("options", [])
            chk.ok(entry["mode_on"] in opts, where,
                   "mode_on '%s' is not one of %s" % (entry["mode_on"], opts))

    # preset
    preset = entry.get("preset")
    if chk.ok(isinstance(preset, dict), where, "preset must be an object"):
        pm = preset.get("mode")
        chk.ok(pm in PRESET_MODES, where, "preset.mode must be one of %s" % sorted(PRESET_MODES))
        if pm == "widgets":
            pk = set(preset)
            chk.ok(pk == {"mode", "combo", "save", "delete"}, where,
                   "preset(mode=widgets) must have exactly mode, combo, save, delete")
            for pk_ in ("combo", "save", "delete"):
                n = preset.get(pk_)
                chk.ok(isinstance(n, str) and resolves(n), where,
                       "preset.%s '%s' does not resolve" % (pk_, n))
        elif pm == "provider":
            chk.ok(set(preset) == {"mode", "provider"}, where,
                   "preset(mode=provider) must have exactly mode, provider")
            chk.ok(isinstance(preset.get("provider"), str) and preset.get("provider"), where,
                   "preset.provider must be a non-empty string")

    # selector_settings: config fallback for sel_setting of selector cards
    sel = entry.get("selector_settings")
    if chk.ok(isinstance(sel, dict), where, "selector_settings must be an object"):
        for cid, setting in sel.items():
            c = cards.get(cid)
            chk.ok(c is not None and c.get("k") == "selector", where,
                   "selector_settings '%s' is not a selector card in the page" % cid)
            chk.ok(isinstance(setting, str) and setting, where,
                   "selector_settings['%s'] must be a non-empty string" % cid)
        for cid, c in cards.items():
            if c.get("k") == "selector" and not c.get("sel_setting"):
                chk.ok(cid in sel, where,
                       "selector card '%s' has no sel_setting and no selector_settings entry" % cid)

    # card_enables: fallback for enable_widget when data ew is missing
    ce = entry.get("card_enables")
    if chk.ok(isinstance(ce, dict), where, "card_enables must be an object"):
        for cid, widget in ce.items():
            c = cards.get(cid)
            chk.ok(c is not None, where, "card_enables card '%s' not in page" % cid)
            chk.ok(isinstance(widget, str) and resolves(widget), where,
                   "card_enables '%s' -> '%s' does not resolve" % (cid, widget))
            if c is not None and c.get("ew") and c.get("ew") != widget:
                chk.err(where, "card_enables '%s' conflicts with data ew '%s'" % (cid, c["ew"]))

    # card_copy_types: repeated-item cards that expose the shared copy/paste menu
    copy_types = entry.get("card_copy_types", {})
    if chk.ok(isinstance(copy_types, dict), where, "card_copy_types must be an object"):
        for cid, copy_type in copy_types.items():
            chk.ok(cid in cards, where, "card_copy_types card '%s' not in page" % cid)
            chk.ok(isinstance(copy_type, str) and copy_type, where,
                   "card_copy_types['%s'] must be a non-empty string" % cid)

    # option_enables: keyed by selector card, then option label
    oe = entry.get("option_enables", {})
    if chk.ok(isinstance(oe, dict), where, "option_enables must be an object"):
        for cid, opts_map in oe.items():
            c = cards.get(cid)
            if not chk.ok(c is not None and c.get("k") == "selector", where,
                          "option_enables '%s' is not a selector card" % cid):
                continue
            labels = {o.get("label"): o for o in c.get("opts", []) or []}
            chk.ok(isinstance(opts_map, dict), where,
                   "option_enables['%s'] must be an object" % cid)
            for label, widget in (opts_map or {}).items():
                chk.ok(label in labels, where,
                       "option_enables '%s'.'%s' is not an option label" % (cid, label))
                chk.ok(isinstance(widget, str) and resolves(widget), where,
                       "option_enables '%s'.'%s' -> '%s' does not resolve" % (cid, label, widget))
                o = labels.get(label)
                if o is not None and o.get("ew") and o.get("ew") != widget:
                    chk.err(where, "option_enables '%s'.'%s' conflicts with data ew '%s'"
                            % (cid, label, o["ew"]))

    # row_overrides: every key must be a row in the data (no silent skip)
    ro = entry.get("row_overrides", {})
    if chk.ok(isinstance(ro, dict), where, "row_overrides must be an object"):
        for rw, patch in ro.items():
            chk.ok(rw in rows, where, "row_overrides key '%s' is not a row in the data" % rw)
            if not chk.ok(isinstance(patch, dict) and patch, where,
                          "row_overrides['%s'] must be a non-empty object" % rw):
                continue
            for pk_, pv in patch.items():
                if pk_ == "w":
                    chk.ok(isinstance(pv, str) and resolves(pv), where,
                           "row_overrides '%s'.w '%s' does not resolve" % (rw, pv))
                elif pk_ == "k":
                    chk.ok(pv in ROW_KINDS or pv == "gobo", where,
                           "row_overrides '%s'.k '%s' is not a known kind" % (rw, pv))
                elif pk_ == "a":
                    chk.ok(pv in (0, 1) and not isinstance(pv, bool), where,
                           "row_overrides '%s'.a must be 0 or 1" % rw)
                elif pk_ == "mirror_setting":
                    chk.ok(isinstance(pv, str) and pv, where,
                           "row_overrides '%s'.mirror_setting must be a non-empty string" % rw)
                elif pk_ == "shortcut_of":
                    chk.ok(isinstance(pv, str) and re.fullmatch(r"[^/]+/[^/]+", pv) is not None,
                           where, "row_overrides '%s'.shortcut_of must be '<page>/<row>'" % rw)
                else:
                    chk.err(where, "row_overrides '%s' has unknown key '%s'" % (rw, pk_))

    # add_rows
    ar = entry.get("add_rows")
    if chk.ok(isinstance(ar, list), where, "add_rows must be a list"):
        seen_w = set()
        for i, item in enumerate(ar):
            iw = "%s add_rows[%d]" % (where, i)
            if not chk.ok(isinstance(item, dict), iw, "must be an object"):
                continue
            for extra in sorted(set(item) - {"card", "after", "option", "first", "row"}):
                chk.err(iw, "unknown key '%s'" % extra)
            card_id = item.get("card")
            c = cards.get(card_id)
            chk.ok(c is not None, iw, "card '%s' not in page" % card_id)
            if "option" in item:
                labels = [o.get("label") for o in (c or {}).get("opts", []) or []]
                chk.ok(item["option"] in labels, iw,
                       "option '%s' is not an option of card '%s'" % (item["option"], card_id))
            if "first" in item:
                chk.ok(isinstance(item["first"], bool), iw, "first must be a bool")
            if "after" in item:
                a = item["after"]
                ar_row = rows.get(a)
                chk.ok(ar_row is not None, iw,
                       "after '%s' is not a row in the data (generator error)" % a)
                if ar_row is not None:
                    chk.ok(ar_row[0] == card_id, iw,
                           "after '%s' lives in card '%s', not '%s'" % (a, ar_row[0], card_id))
            row = item.get("row")
            if not chk.ok(isinstance(row, dict), iw, "row must be an object"):
                continue
            chk.ok(row.get("k") in ROW_KINDS, iw, "row.k '%s' is not a known kind" % row.get("k"))
            for req in ("w", "l"):
                chk.ok(isinstance(row.get(req), str) and row.get(req), iw,
                       "row.%s must be a non-empty string" % req)
            if "h" in row:
                chk.ok(isinstance(row["h"], str), iw, "row.h must be a string")
            if "a" in row:
                chk.ok(row["a"] in (0, 1) and not isinstance(row["a"], bool), iw,
                       "row.a must be 0 or 1")
            if "hidden" in row:
                chk.ok(row["hidden"] in (0, 1) and not isinstance(row["hidden"], bool), iw,
                       "row.hidden must be 0 or 1")
            w = row.get("w")
            if isinstance(w, str) and w:
                chk.ok(resolves(w), iw, "row.w '%s' does not resolve (data, page_sources or v1)" % w)
                chk.ok(w not in rows and w not in seen_w, iw,
                       "row.w '%s' duplicates an existing or added row" % w)
                seen_w.add(w)

    # label_names: row widget -> label widget, both must resolve
    ln = entry.get("label_names")
    if chk.ok(isinstance(ln, dict), where, "label_names must be an object"):
        for rw, lw in ln.items():
            chk.ok(rw in rows, where, "label_names key '%s' is not a row in the data" % rw)
            chk.ok(isinstance(lw, str) and resolves(lw), where,
                   "label_names '%s' -> '%s' does not resolve" % (rw, lw))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--config", default=DEFAULT_CONFIG)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--page-sources", default=DEFAULT_PAGE_SOURCES)
    ap.add_argument("--policy", default=DEFAULT_POLICY)
    args = ap.parse_args(argv)

    cfg = load_json(args.config, "kit_pages.json")
    data = load_json(args.data, "director_mockup_data.json")
    ps = load_json(args.page_sources, "page_sources.json")
    policy = load_json(args.policy, "gate_policy.json")

    chk = Checker()
    data_pages, data_names = collect_data(data)
    ps_names = collect_page_sources(ps)

    if chk.ok(isinstance(cfg, dict), "kit_pages.json", "top level must be an object"):
        for extra in sorted(set(cfg) - TOP_KEYS):
            chk.err("kit_pages.json", "unknown top-level key '%s'" % extra)
        for missing in sorted(TOP_KEYS - set(cfg)):
            chk.err("kit_pages.json", "missing top-level key '%s'" % missing)
        chk.ok(cfg.get("version") == 1 and not isinstance(cfg.get("version"), bool),
               "kit_pages.json", "version must be 1")

        p0 = cfg.get("p0_pages", {})
        if chk.ok(isinstance(p0, dict), "p0_pages", "must be an object"):
            for key, entry in p0.items():
                check_page(chk, key, entry, data_pages, data_names, ps_names, None)

        lt = cfg.get("launcher_targets", {})
        if chk.ok(isinstance(lt, dict), "launcher_targets", "must be an object"):
            for key, entry in lt.items():
                w = "launcher_targets[%s]" % key
                chk.ok(key in data_pages, w, "page not found in director_mockup_data.json")
                chk.ok(key not in p0, w, "page is both a P0 page and a launcher page")
                chk.ok(key != "capture_group/capture_settings_tab", w,
                       "Empty Capture settings page is dropped (USER_DECISIONS_20261010 item 2)")
                if chk.ok(isinstance(entry, dict), w, "must be an object"):
                    for extra in sorted(set(entry) - LAUNCHER_REQUIRED):
                        chk.err(w, "unknown key '%s'" % extra)
                    for req in sorted(LAUNCHER_REQUIRED - set(entry)):
                        chk.err(w, "missing required key '%s'" % req)
                    chk.ok(entry.get("target") in ("lightbox", "floater"), w,
                           "target must be 'lightbox' or 'floater'")
                    for req in ("target_tab", "button_label", "help"):
                        chk.ok(isinstance(entry.get(req), str) and entry.get(req), w,
                               "%s must be a non-empty string" % req)

        pa = cfg.get("parity_allow", {})
        if chk.ok(isinstance(pa, dict), "parity_allow", "must be an object"):
            for key, reason in pa.items():
                chk.ok(isinstance(reason, str) and reason, "parity_allow[%s]" % key,
                       "reason must be a non-empty string")

        # Entry counts for the report
        n_pages = len(p0) if isinstance(p0, dict) else 0
        n_cards = sum(len(card_map(data_pages[k]["page"])) for k in p0 if k in data_pages)
        n_add = sum(len(e.get("add_rows", [])) for e in p0.values() if isinstance(e, dict)
                    and isinstance(e.get("add_rows"), list))
        n_launch = len(lt) if isinstance(lt, dict) else 0
        n_parity = len(pa) if isinstance(pa, dict) else 0
        print("entries: p0_pages=%d (data cards=%d, add_rows=%d) launcher_targets=%d parity_allow=%d"
              % (n_pages, n_cards, n_add, n_launch, n_parity))

    policy_keys = {"version", "pending_settings", "pending_homes", "shared_writers",
                   "same_page_mode_writers", "xui_legacy_v1_findings",
                   "help_audit_allow", "code_kind_exceptions", "retired_cpp_targets",
                   "pending_cpp_targets"}
    if chk.ok(isinstance(policy, dict), "gate_policy.json", "top level must be an object"):
        for extra in sorted(set(policy) - policy_keys):
            chk.err("gate_policy.json", "unknown top-level key '%s'" % extra)
        for missing in sorted(policy_keys - set(policy)):
            chk.err("gate_policy.json", "missing top-level key '%s'" % missing)
        chk.ok(policy.get("version") == 1 and not isinstance(policy.get("version"), bool),
               "gate_policy.json", "version must be 1")
        baseline = policy.get("xui_legacy_v1_findings", [])
        if chk.ok(isinstance(baseline, list) and all(isinstance(x, str) for x in baseline),
                  "xui_legacy_v1_findings", "must be a list of exact string keys"):
            chk.ok(len(baseline) == len(set(baseline)), "xui_legacy_v1_findings",
                   "keys must be unique")
            for key in baseline:
                chk.ok(re.fullmatch(r"panel_dpage_[^|*?\[\]]+\.xml\|[a-z-]+\|[^|*?\[\]]+", key) is not None,
                       "xui_legacy_v1_findings", "invalid exact key '%s'" % key)
        for section in sorted(policy_keys - {"version", "xui_legacy_v1_findings"}):
            entries = policy.get(section, {})
            if not chk.ok(isinstance(entries, dict), section, "must be an object"):
                continue
            for name, entry in entries.items():
                where = "%s[%s]" % (section, name)
                if not chk.ok(isinstance(entry, dict), where, "must be an object"):
                    continue
                required = {"reason", "citation", "expires"}
                if section == "help_audit_allow": required.add("text")
                if section in {"shared_writers", "same_page_mode_writers"}: required.add("attrs")
                if section == "pending_cpp_targets": required.update({"file", "symbol"})
                for missing in sorted(required - set(entry)):
                    chk.err(where, "missing required key '%s'" % missing)
                chk.ok(re.fullmatch(r"WP(?:[1-9]|1[0-2])", str(entry.get("expires", ""))) is not None,
                       where, "expires must be WP1..WP12")
                for field in ("reason", "citation"):
                    chk.ok(isinstance(entry.get(field), str) and entry.get(field), where,
                           "%s must be a non-empty string" % field)
                if "attrs" in required:
                    chk.ok(isinstance(entry.get("attrs"), list) and entry.get("attrs") and
                           all(isinstance(x, str) and x for x in entry.get("attrs", [])),
                           where, "attrs must be a non-empty string list")
                if section == "pending_cpp_targets":
                    chk.ok(isinstance(entry.get("file"), str) and
                           re.fullmatch(r"indra/newview/[A-Za-z0-9_]+\.cpp", entry.get("file", "")) is not None,
                           where, "file must name one exact indra/newview C++ source")
                    chk.ok(isinstance(entry.get("symbol"), str) and
                           re.fullmatch(r"[A-Za-z_]\w*", entry.get("symbol", "")) is not None,
                           where, "symbol must name one exact C++ owner")

    if chk.errors:
        for e in chk.errors:
            print("FAIL: " + e)
        print("check_kit_pages: %d error(s)" % len(chk.errors))
        return 1
    print("check_kit_pages: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
