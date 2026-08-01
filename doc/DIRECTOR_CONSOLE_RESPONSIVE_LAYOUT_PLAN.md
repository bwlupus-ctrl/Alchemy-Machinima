# Director Console → Responsive Layout — implementation plan (Opus analyst, 2026-07-31)

User wants the Director Console (fixed absolute positioning, overlapping on some tabs) to reflow "like a webpage" on resize, all tabs. Verdict from analysis: **do NOT wrap-every-control in layout_stack** (high effort, high risk, worse on ~half the panels). Use a **HYBRID** with a working precedent in-tree (`panel_ghost_studio.xml`).

## Recommended strategy
1. **Frame the floater + each tab body with `follows`-anchoring + a fill container** (like panel_ghost_studio.xml: fixed top `follows="left|top|right"`, middle body `follows="all"`, bottom status `follows="left|bottom|right"`).
2. **Wrap tall fixed-content panels in a `scroll_container`** so nothing is ever unreachable (Path/Weather/most Camera sub-tabs already do; normalize the rest).
3. **Use `LLLayoutStack` surgically** where real reflow is wanted: (a) Cinematic params-over-orbit, (b) horizontal button rows that should share width, (c) later, the outer floater frame.

## HARD CONSTRAINT (design around, don't fight)
`panel_cinecam_params.xml` (1696L) = fixed header + a **50-way mode CARD STACK**: all 50 `panel_mode_*` declared at the SAME rect (`left0 top317 320x98 visible=false`); C++ `updateModePanel()` (alpanelcinecamparams.cpp:479) only `setVisible` (never reshape). A vertical layout_stack lays children SEQUENTIALLY → feeding it 50 cards = ~4900px column. **So panel_cinecam_params stays fixed-height internally, lives in a scroll_container — a "keep+scroll" panel, never "reflow".** (findChild is recursive, so wrapping OTHER panels' controls in layout_panels never breaks C++ name lookups.) Same caution: panel_path_editor's auto-hiding gaze rows + its floating `suspend_banner` overlay (keep absolute, drawn last); the card-stack pattern.

## Root layout (floater_director.xml)
`can_resize`, h650 min_height560 min_width658 w658, save_rect. Outer tab_container `director_tabs` (196,74) 454x550 follows=all, tab_position=left tab_width=80 → **tab bodies 374 wide**. Left rail = cast scroll_list (follows left|top|bottom) + group ctrls. Bottom status_strip. Consider raising min_height 560→~600.

## Per-tab treatment (worst-first order)
1. **CINEMATIC sub-tab (DO FIRST — root-caused overlap):** the ONLY host stacking `panel_cinecam_params`(h420)+`panel_flycam_orbit`(h214) in one content (params top0, orbit top424, scroll h428 over content h642). Brittle: params reserves 420 but visible content ends ~415 with a DEAD ~90px band (y227-317); orbit at 424 = 4px gutter; at UIScaleFactor>1 controls inflate → params crosses 420 → OVERLAPS orbit. (Standalone floater_cinematic_camera.xml puts them on separate tabs → never collides.) **FIX:** replace the two absolute embeds with a vertical `LLLayoutStack` inside the existing scroll_container: layout_panel[params, auto_resize=false h420 min_height420] + layout_panel[orbit, auto_resize=false h222 min_height222]; both panels internals UNTOUCHED (shared files). Now orbit can never ride into params at any UI scale. Optional cleanup (edits shared file → also fixes standalone): move the 50 cards from top317→~237 to kill the dead band, drop params min_height→~340. ~30 min, low risk, big payoff. VERIFY at UI scale 1.0 AND 1.25.
2. **Normalize already-scrolled tabs** (Switcher/Frame/Camera-Shake sub-tabs + Path + Weather): sub-tab panel follows=all → scroll_container follows=all with right=-N/bottom=-N → inner content panel keeps DEFINITE height (follows="left|top", NOT all). Cheap, removes min-height clipping.
3. **Props** → scroll-wrap (mirror Path). **Animate** → scroll-wrap (protect the fixed 180x180 `view_border` `animation_preview` — C++ blits GL dummy by rect; pin min 180x180). **Move** → stack or scroll-wrap (protect the fixed 132x132 `compass_dial`; 4-button rows' min_widths must sum <374).
4. **Takes/Shafts/Time** — small, fit already; optional scroll-wrap for UI-scale safety.
5. **Ghosts** — already responsive (the reference pattern); verify only.
6. **Outer-frame layout_stack (Option B)** — LAST, only if follows-based frame insufficient: vertical [transport fixed]/[middle elastic = horizontal [cast fixed ~188 min_width]+[tabs elastic]]/[status fixed].

## LLLayoutStack cheat-sheet (verified in fork; refs: panel_settings_water.xml, floater_fixedenvironment.xml, panel_people.xml)
`<layout_stack orientation="vertical" follows="all" left4 top4 right-4 bottom-4 animate="false">` with `<layout_panel auto_resize="false" user_resize="false" height=H min_height=H>` for FIXED sections + EXACTLY ONE `auto_resize="true" min_height=0` elastic panel. Vertical honors min_height; horizontal min_width; min_dim both. Always user_resize=false + animate=false (don't fight C++ setVisible). right=-N/bottom=-N = fill w/ margin (needs follows=all).

## scroll_container vs layout_stack rule
If a panel has a min content height below which controls become unusable (spinners/labeled sliders/fixed widgets/the card-stack/the 4x3 button grid) → scroll_container (or fixed layout_panel in a scroll), do NOT reflow internals. Reserve layout_stack for sections that SHOULD trade space. KEEP-FIXED+SCROLL: panel_cinecam_params, panel_cinecam_frame, panel_director_switcher (has fixed 4x3 grid), panel_path_editor, panel_cinecam_operator.

## ADVERSARIAL RISKS (SL layout_stack = flexbox-lite, sharp edges)
1. **No wrapping** — horizontal stack squeezes below min_width → clip/overlap. 4-button rows in 374px (Move Start/Stop, Path Add/Insert/Delete/Clear) must have min_widths summing <374 or you re-create the overlap bug.
2. **≥2 elastic panels trap** — two auto_resize=true w/ min_height=0 both collapse → hidden lists. Props (2 scroll_lists) + Animate (list+preview) most exposed. One elastic per stack; real min_heights.
3. **min_dim vs min_height/min_width** — wrong one → silent 0 collapse.
4. **tab_container child sizing** — a child follows="left|top" (no bottom) does NOT shrink → clips when viewport shrinks (this is WHY Move/Props/Animate clip now); making a tall panel follows=all in a short tab SQUASHES/overlaps it. Safe pattern: tab panel follows=all → scroll_container follows=all → content panel DEFINITE height + follows="left|top" (never all).
5. **layout_stack in scroll_container** — only with DEFINITE total height (all auto_resize=false); an auto_resize=true inside a scroll degenerates. Cinematic fix keeps both false.
6. **Card-stack catastrophe** — never stack panel_cinecam_params' 50 cards (4900px / flicker vs C++ setVisible). Keep fixed. Same for path gaze rows + suspend_banner overlay.
7. **Shared-file blast radius** — all 11 panels embedded by standalone floaters too (floater_actor_mover/cinematic_camera/flycam_orbit/animation_explorer...). Every panel edit = two-host visual check; embed heights in BOTH hosts may need updating together.
8. **Fixed widgets** — anim_preview 180x180 view_border + actor_mover 132x132 compass_dial: pin min=fixed, auto_resize=false, or interaction breaks.
9. **use_ellipses masking** — too-narrow → silent "…"; verify with real strings.
10. **save_rect** — floater + standalone cinematic save_rect=true; returning users have old rects possibly < new min → confirm min clamps sanely.

Most at risk: Props, Animate, Move (button-row min_width sum + fixed widgets). Least: Ghosts (done), Takes/Shafts/Time (fit). Effort: Cinematic ~30min low-risk; normalize+scroll-wrap Props/Animate/Move = few hours low-med; full per-control reflow of dense panels = days, med-high risk, low payoff → RECOMMEND AGAINST except the targeted spots.

## Verify each tab: default (no overlap, dead band gone) → drag to MIN (all reachable via scroll, none clipped) → drag LARGE (proportions kept) → **UIScaleFactor 1.25 repeat** (where absolute fails) → check the STANDALONE floater sharing the panel. Pure-XUI edits need no relink, just relaunch.
