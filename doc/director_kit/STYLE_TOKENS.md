# Director UI style tokens

Readable companion to `style_tokens.json`. Every value comes from `mockup_src/Director.dc.html` unless marked otherwise. Line numbers are `Director.dc.html:N`. Verify with `python -I verify_tokens.py`.

Frame: 1200 x 760 preview. Floater 1160 x 720 by default, 850 x 560 at minimum (`Director.dc.html:2016-2017`).

## 1. Colours

### Surfaces

| Token | Hex | Role | Source |
|---|---|---|---|
| canvas_outside | `#0B0C0E` | page backdrop behind floater | 14, 49 |
| window_bg | `#111316` | floater body | 2021 |
| panel_bg | `#15171B` | rail, header, tab strip, transport, menus, help pane | 56, 2022, 199, 712, 726 |
| rail_border | `#262A31` | panel separators, rail right border, help left border | 2022, 56, 199, 712 |
| hairline | `#22262C` | row separators (page bar, who row, chip strip, minimum help strip top) | 219, 248, 268, 706 |
| chip_strip_bg | `#131518` | section chip strip | 219 |
| inset_bg | `#121418` | control wells: search, seg group, list box, who row | 211, 248, 325, 1990, 2703 |
| card_bg | `#1B1E23` | card, who chip | 2300, 2697 |
| card_border | `#2A2F36` | card border and header rule | 2300, 2301 |
| subrow_bg | `#181B1F` | selector bar, dial cell | 395, 1873 |
| subrow_border | `#23272D` | selector bar bottom, light-map frame | 395, 452 |
| control_bg | `#262A31` | select, Save as, active rail item, toast, active chip | 24, 296, 2073, 289, 2186 |
| control_border | `#343A42` | buttons, selects, search, chips, menus, seg group | 24, 296, 211, 2186 |
| active_fill | `#2F343B` | selected seg / mode fill | 2144, 2275 |
| divider | `#2F343B` | 18 px vertical dividers, master-switch left rule | 759, 804, 315 |
| hover_bg | `#1E2228` | rail item hover, seg hover | 32, 38 |
| row_hover_bg | `#22262D` | control row hover, dial cell hover | 1813, 1873 |
| ghost_hover_bg | `#22262C` | ghost icon button hover | 35 |
| menu_hover_bg | `#262A31` | menu / result row hover (.res) | 41 |
| disabled_dash | `#3A4049` | dashed placeholder border, switch off track | 448, 2008, 1464 |
| scrollbar_thumb | `#343A42` | 8 px scrollbar thumb, radius 4 | 29 |

### Text

| Token | Hex | Role | Source |
|---|---|---|---|
| text_primary | `#E6E8EB` | value text, input text | 24, 530, 541 |
| text_strong | `#F1F2F4` | active rail or tab label, toast text | 2073, 2095, 289 |
| text_label | `#C9CED6` | row label, idle rail label, chip label, help body | 1817, 2073, 2186, 712-715 |
| text_secondary | `#9BA3AE` | inactive tab, crumb, default icon colour, meta labels | 112, 2095, 716; Main 40 |
| text_muted | `#8A929D` | section caps, notes, placeholder, card reset when unchanged | 142, 166, 500 |
| text_disabled | `#5C646F` | reset when default, REC off, idle cast dot | 1470, 2795, 2531 |
| text_disabled_icon | `#4A515B` | disabled icon button (undo or redo empty) | 2389 |

### Accent and state

| Token | Hex | Role | Source |
|---|---|---|---|
| accent_amber | `#E8A33D` | active underline, switch on, changed reset, focus, ACTION, needle | 2071, 2095, 2300, 2587, 1464 |
| accent_hover | `#F3BE6A` | link hover, amber text on tinted button, warning text | 428, 356; Kit 12 |
| accent_text_on | `#141413` | text and knob on amber | 1464, 2587, 307 |
| record_red | `#E5484D` | REC on, record dot, running ACTION, delete confirm | 2795, 822, 2587, 798 |
| record_text_on | `#FFFFFF` | text on red | 2795, 2587, 798 |
| danger_text | `#F2A7A9` | danger menu item, running timecode, status text | 2500, 2751, 822 |
| ok_green | `#46A758` | Walking cast dot (ring `rgba(70,167,88,.18)`) | 895, 2512 |
| ghost_purple | `#8E7CFF` | Ghost cast dot | 898 |
| warn_band_border | `#3A2F1E` | master-off banner bottom rule | 356 |
| warn_band_text | `#F3BE6A` | master-off banner text | 356 |

### Controls

| Token | Hex | Role | Source |
|---|---|---|---|
| switch_off_track | `#3A4049` | switch off | 1464 |
| switch_on_track | `#E8A33D` | switch on | 1464 |
| switch_knob_off | `#C9CED6` | knob off | 1465 |
| switch_knob_on | `#141413` | knob on | 1465 |
| slider_track_empty | `#363C44` | unfilled slider track | 19, 20 |
| slider_track_fill | `#E8A33D` | filled slider track | 19, 20 |
| slider_thumb | `#E6E8EB` | slider thumb | 21, 22 |
| swatch_border | `#4A515B` | colour swatch border, compass bezel | 1965, 524 |
| focus_outline | `#E8A33D` | focus ring 2 px | 25, 36 |
| toast_border | `#E8A33D` | toast border and check | 289, 290 |
| tab_underline | `#E8A33D` | active tab 2 px underline | 2095 |
| chip_hover_border | `#4A515B` | chip hover border | 39 |

### Dial, arc and light map

| Token | Hex | Role | Source |
|---|---|---|---|
| dial_bezel_fill | `#15171B` | compass face | 524 |
| dial_tick | `#8A929D` | compass and arc ticks | 524, 527 |
| dial_track | `#2F343B` | arc baseline | 527 |
| dial_range | `#4A515B` | arc range | 527 |
| dial_needle | `#E8A33D` | needle and hub | 524, 527 |
| dial_hub_inner | `#141413` | hub centre | 524, 527 |
| dial_label_dim | `#5C646F` | compass E/S/W, arc min and max | 524, 527 |
| lightmap_bg_centre | `#161D27` | light map radial centre | 452 |
| lightmap_bg_edge | `#111418` | light map radial edge | 452 |
| light_key | `#E8A33D` | Key hue | 1354 |
| light_fill | `#7FA8E0` | Fill hue | 1355 |
| light_back | `#F1F2F4` | Back hue | 1356 |
| light_bg | `#A98BF5` | BG hue | 1357 |
| light_off_grey | `#8A929D` | light switched off | 1598 |

Scene art colours (globe, beams, backdrop) are listed under `colors.lightmap_scene` in the JSON. They are diagram palette, not UI tokens.

### Colour swatches for the Kit page (examples, not tokens)

| Hex | Use | Source |
|---|---|---|
| `#FFF1DE` | 5600 K key colour sample | Kit 43; Main 226 |
| `#FFE0BE` | 4300 K fill colour sample | Main 233 |
| `#E4ECFF` | 7000 K back colour sample | Main 239 |
| `#F2F5FF` | rim tint sample | Main 253 |

### Overlays (literal colour-function strings)

| Value | Use | Source |
|---|---|---|
| `rgba(232,163,61,.12)` | launcher and link button bg | 428, 443 |
| `rgba(232,163,61,.08)` | master-off banner bg | 356 |
| `rgba(232,163,61,.22)` | card flash glow | 2300 |
| `rgba(232,163,61,.25)` | control flash outer ring | 1790 |
| `rgba(232,163,61,.28)` | control row flash bg | 1813 |
| `rgba(232,163,61,.45)` | slider focus halo | 23 |
| `rgba(232,163,61,.7)` | flash inset ring, dial cell flash border | 1816, 1873 |
| `rgba(229,72,77,.22)` | ACTION running glow | 2737 |
| `rgba(70,167,88,.18)` | walking glow | 2512 |
| `rgba(255,255,255,.015)` | placeholder fill | 2008 |
| `rgba(17,19,22,.55)` | loading scrim | 80 |
| `rgba(17,20,24,.88)` | light-map flip button bg | 471 |
| `rgba(20,20,19,.3)` | ACTION arm left rule | 2738 |
| `rgba(0,0,0,.45-.65)` | shadows (see section 4) | 2021, 175, 289, 81 |

## 2. Fonts

Families: `'IBM Plex Sans', sans-serif` and `'IBM Plex Mono', monospace`. Google Fonts link loads Sans 400/500/600 and Mono 400/500 only (`Director.dc.html:12`). See ambiguity A7.

| Role | Family | Size | Weight | Letter-spacing | Colour | Source |
|---|---|---|---|---|---|---|
| Window title | Sans | 13px | 600 | .02em | `#E6E8EB` | 111 |
| Crumb | Sans | 12px | 400 | - | `#9BA3AE` | 112 |
| Rail label | Sans | 13px | 400 idle, 600 current | - | `#C9CED6` / `#F1F2F4` | 2072, 2073 |
| Page tab | Sans | 13px | 400 idle, 600 active | - | `#9BA3AE` / `#F1F2F4` | 2095 |
| Card title | Sans | 13px | 600 | .01em | `#E6E8EB` | 369 |
| Row label | Sans | 12px, line-height 20px | 400 | - | `#C9CED6` | 1817 |
| Value | Mono | 12px | 500 | - | `#E6E8EB` | 530, 541 |
| Note | Sans | 11px | 400 | - | `#8A929D` (Kit: `#9BA3AE`) | 142, 193; Kit 120 |
| Section caps | Sans | 11px | 400 | .08em, uppercase | `#8A929D` | 142, 713 |
| In-card sub-heading | Sans | 10.5px | 500 | .09em, uppercase | `#8A929D` | 500 |
| NEW badge | Sans | 10px | 600 | .08em | `#141413` on `#E8A33D` | 2304 |
| Other badge | Sans | 10px | 400 | .04em | `#C9CED6` | 2305 |
| Chip | Sans | 12px | 400 idle, 600 lit | - | `#C9CED6` / `#F1F2F4` | 2186 |
| Chip count | Mono | 11px | 500 | - | `#8A929D` / `#E8A33D` | 2187 |
| Seg item | Sans | 12px | 400 / 600 selected | - | `#9BA3AE` / `#F1F2F4` | 2144 |
| Cast name | Sans | 12px | 400 | - | `#E6E8EB` | 158, 2513 |
| Cast status | Sans | 10.5px | 400 | - | `#8A929D` | 166 |
| Cast heading | Sans | 11px | 400 | .08em, uppercase | `#8A929D` (count `#C9CED6`) | 142 |
| Select | Sans | 12px | 400 | - | `#E6E8EB` | 24 |
| Card picker select | Sans | 11.5px | 400 | - | `#E6E8EB` | 378 |
| Button (small) | Sans | 12px | 400 | - | `#E6E8EB` | 296 |
| Button (small, transport) | Sans | 11.5px | 400 | - | `#C9CED6` | 770, 771 |
| Primary button | Sans | 12px | 600 | - | `#141413` | 307, 359 |
| ACTION | Sans | 12px | 600 | .08em | `#141413` / `#FFFFFF` running | 2737 |
| Timecode | Mono | 13px | 500 | - | `#E6E8EB` (`#F2A7A9` running) | 2751 |
| REC pill | Mono | 10.5px | 600 | .06em | `#5C646F` / `#FFFFFF` | 2795 |
| UI ms chip | Sans | 11px | 400 | - | `#9BA3AE` | 860 |
| Hotkey ? | Sans | 13px | 600 | - | inherit | 863 |
| Toast | Sans | 12px | 400 | - | `#F1F2F4` | 289 |
| Toast (transport) | Sans | 11.5px | 400 | - | `#F1F2F4` | 826 |
| Help kicker | Sans | 11px | 400 | .08em, uppercase | `#8A929D` | 713 |
| Help title | Sans | 15px, line-height 1.3 | 600 | - | `#E6E8EB` | 714 |
| Help body | Sans | 12px, line-height 1.55 | 400 | - | `#C9CED6` | 715 |
| Help meta | Sans / Mono value | 11px | 400 | - | `#9BA3AE` / `#E6E8EB` | 716-718 |
| Help footer | Sans | 11px, line-height 1.5 | 400 | - | `#8A929D` | 722 |
| Loading title | Sans | 14px | 600 | - | inherit | 85, 92 |
| Loading body | Sans | 12px | 400 | - | `#9BA3AE` | 86 |
| Error text | Mono | 12px | 400 | - | `#E8A33D` | 93 |
| Master label | Sans | 12px | 400 | - | `#C9CED6` (max 110 px) | 316 |
| Master state | Sans | 11px | 600 | - | `#E8A33D` / `#8A929D` | 2664 |
| Banner text | Sans | 12px | 400 | - | `#F3BE6A` | 356 |
| Reset confirm title | Sans | 12px | 600 | - | `#F1F2F4` | 303 |
| Reset confirm body | Sans | 11px | 400 | - | `#9BA3AE` | 304 |
| Legend head | Sans | 11px, line-height 1.45 | 400 | - | `#9BA3AE` | 814 |
| Legend item | Sans | 12px | 400 | - | `#C9CED6` | 816 |
| Legend readout | Mono | 11px | 500 | - | `#C9CED6` | 478 |
| Dial readout | Mono | 12px | 500 | - | `#E6E8EB` | 530 |
| Map hint | Sans | 11px | 400 | - | `#8A929D` | 492 |
| Scene name | Sans | 12px | 400 | - | `#9BA3AE` (value `#E6E8EB`) | Main 192 |

Light-map and dial SVG text uses SVG units (not px): axis labels `font-size="6.5"`, arc limits `font-size="6"`, OFF label `font-size="7.5"`, light names `font-size="10"`, camera labels `font-size="9"` (`Director.dc.html:454-470, 524, 527`).

## 3. Radii and borders

| Token | Value | Use | Source |
|---|---|---|---|
| window | 8px | floater | 2021 |
| card | 6px | card | 2300 |
| rail_item | 6px | group button | 2073 |
| popover_menu | 6px | menus, popovers, dialogs | 175, 258, 812, 832, 302 |
| control | 4px | buttons, selects, search, rows, icon buttons | 24, 296, 748, 2389 |
| seg_group | 5px | seg container | 211 |
| seg_item | 3px | seg item | 2144 |
| tab | 0 | page tab (underline indicates) | 2095 |
| chip | 12px | section chip, who chip | 2186, 2697 |
| cast_chip | 13px | transport Cast chip | 2730 |
| switch | 9px | switch track | 1464 |
| switch_knob | 7px | knob | 1465 |
| dial | 24px | 48 px dial | 522, 545 |
| swatch | 3px | colour swatch | 1965 |
| slider_thumb | 6px | thumb | 21, 22 |
| rec_pill | 10px | REC | 2795 |
| toast | 12px (transport 11px) | toast | 289, 826 |
| badge | 3px | badges, tags | 2304, 169 |
| placeholder | 4px | dashed placeholder | 2008 |
| dial_cell | 6px | dial cell | 1873 |
| light_map | 6px | light map | 452 |
| legend_chip | 4px | light legend chip | 1664 |
| menu_item | 4px | menu item | 2500 |
| scrollbar | 4px | scrollbar thumb | 29 |

| Border | Value | Use | Source |
|---|---|---|---|
| hairline | 1px `#262A31` | panel separators | 56, 199, 712, 726 |
| hairline_faint | 1px `#22262C` | row separators | 219, 248, 268 |
| card | 1px `#2A2F36` | card | 2300 |
| control | 1px `#343A42` | controls | 24, 211, 2186 |
| divider | 1px `#2F343B` | 18 px vertical divider | 759 |
| dashed | 1px dashed `#3A4049` | placeholder | 448, 2008 |
| amber_focus | 1px `#E8A33D` | lit chip, open who chip, toast, reset confirm, flashing card | 2186, 2697, 289, 302, 2300 |
| rec_on | 1px `#E5484D` | REC when recording | 2795 |
| swatch | 1px `#4A515B` | colour swatch | 1965 |
| dot_ring | 1.5px | legend dot, tab off-pip | 1662, 205 |
| tab_underline | 2px `#E8A33D` | active tab | 2095 |
| focus | 2px outline `#E8A33D`, offset 1px (dial 2px) | keyboard focus | 25, 36 |
| slider_focus | box-shadow 0 0 0 3px rgba(232,163,61,.45) | slider thumb focus | 23 |
| action_arm | 1px `rgba(20,20,19,.3)` left | ACTION split rule | 2738 |

## 4. Shadows

| Token | Value | Source |
|---|---|---|
| window | `0 20px 60px rgba(0,0,0,.55)` | 2021 |
| popover | `0 14px 36px rgba(0,0,0,.55)` | 327, 735, 812, 832 |
| menu | `0 12px 30px rgba(0,0,0,.6)` | 175, 258, 302 |
| toast | `0 8px 22px rgba(0,0,0,.5)` | 289 |
| loading dialog | `0 16px 40px rgba(0,0,0,.5)` | 81 |
| ACTION running | `0 0 0 3px rgba(229,72,77,.22)` | 2737 |
| card flash | `0 0 0 3px rgba(232,163,61,.22)` | 2300 |
| control flash | `0 0 0 2px #E8A33D, 0 0 0 5px rgba(232,163,61,.25)` | 1790 |

## 5. Spacing

| Token | Value | Source |
|---|---|---|
| Page padding | `14px 16px 16px` | 362 |
| Card grid | `repeat(2, minmax(0,1fr))`, gap 12px, align start | 365 |
| Card body padding | `6px 8px 8px 12px`, gap 2px | 2329 |
| Card header padding | `0 8px 0 12px`, height 36px | 2301 |
| Control row grid | `100px minmax(0,1fr) 52px 20px`, column-gap 6px | 1816 |
| Control row | min-height 28px, margin `0 -4px`, padding `0 4px` (list rows padding 4px) | 1816 |
| Header | height 32px, padding `0 10px 0 14px`, gap 10px | 110 |
| Page tab strip | height 40px, padding `0 16px`, gap 2px | 199-200 |
| Mode seg margin | `0 12px 0 8px` | 211 |
| Section chip strip | height 38px, padding `0 16px`, gap 6px | 219 |
| Who row | height 34px, padding `0 16px`, gap 10px | 248 |
| Page bar | height 44px, padding `0 16px`, gap 6px | 268 |
| Transport | height 40px, padding `0 10px`, gap 8px | 726 |
| Help pane | width 224px, padding 16px, gap 10px | 712 |

## 6. Components

### 6.1 Floater window
- Size 1160 x 720 default (left 20, top 20). Minimum 850 x 560 (left 175, top 60).
- Style: `display:flex; column; bg #111316; border 1px #2A2F36; radius 8px; overflow hidden; box-shadow 0 20px 60px rgba(0,0,0,.55); outline none` (2021).
- Header 32px, bg `#15171B`, bottom border `#262A31`. Title 13/600. Crumb `› Group › Page` 12px `#9BA3AE`. Minimise and close buttons 24x24, transparent, `#9BA3AE`, icons 12px stroke 1.5 (`icons/window_minimize.svg`, `icons/window_close.svg`).

### 6.2 Group rail
- Width 176px; 56px at minimum (labels hidden, icon centred). Padding `10px 8px`, gap 2px, bg `#15171B`, right border `#262A31`, scrolls when short (118, 2022).
- Item: height 40px, radius 6px, padding `0 12px` (centred when folded), gap 10px. Icon 20x20 box with 18px glyph (`icons/rail_*.svg`). Label 13px.
- States: default transparent, colour `#C9CED6`. Hover (non-current) bg `#1E2228`, colour `#E6E8EB`. Current bg `#262A31`, colour `#F1F2F4`, weight 600, icon `#E8A33D`. Active dot 6px `#E8A33D`, margin-left auto (135).
- Cast section below the groups: top border `#262A31`, margin-top 10px, padding-top 10px. Heading 11px caps `#8A929D`. Four 22x22 ghost tool buttons with 14px icons (`icons/cast_tool_*.svg`), hover bg `#22262C`.
- Cast row: min-height 36px, padding `3px 8px`, gap 8px, radius 4px. Selected bg `#262A31` with inset 2px left bar `#E8A33D`. Dot 8px (`#46A758` Walking, `#E8A33D` On mark, `#5C646F` Idle, `#8E7CFF` Ghost). Name 12px. Status 10.5px `#8A929D`. Tag pill: padding `0 4px`, line-height 14px, radius 3px, border `#343A42`, colour `#C9CED6`. Focus badge `icons/cast_row_framed.svg` (amber). Looks-at badge `icons/cast_row_looks_at.svg` (`#7FA8E0`).
- Cast context menu: 156px wide, `top 38px left 6px`, padding 4px, bg `#15171B`, border `#343A42`, radius 6px, menu shadow. Items 26px, padding `0 8px`, radius 4px, 12px `#E6E8EB`; danger `#F2A7A9`. Group submenu item 24px, padding `0 8px 0 18px`, selected `#E8A33D`.

### 6.3 Page tabs
- Strip height 40px, bg `#15171B`, bottom border `#262A31`. List padding `0 16px`, gap 2px, `overflow-x auto`, no wrap.
- Tab: height 32px, padding `0 12px`, gap 6px, 13px, transparent. Idle `#9BA3AE` 400; hover `#C9CED6`. Active `#F1F2F4` 600, `border-bottom 2px solid #E8A33D`.
- Off pip: 6px ring, border 1.5px `#8A929D`, shown when all cards on that tab are switched off.

### 6.4 Easy / Advanced segmented control
- Container: padding 2px, gap 2px, bg `#121418`, border 1px `#343A42`, radius 5px, margin `0 12px 0 8px`.
- Item: height 22px, padding `0 10px`, radius 3px, 12px. Selected bg `#2F343B`, colour `#F1F2F4` 600, `box-shadow inset 0 -2px 0 #E8A33D`. Idle transparent `#9BA3AE`; hover bg `#1E2228`, colour `#E6E8EB`.

### 6.5 Section chips
- Strip height 38px, padding `0 16px`, gap 6px, bg `#131518`, bottom border `#22262C`, nowrap scroll.
- Chip: height 24px, padding `0 10px`, radius 12px, gap 6px, 12px, 1px border. Idle border `#343A42` colour `#C9CED6` 400. Lit (has matches or selected) bg `#262A31`, border `#E8A33D`, colour `#F1F2F4` 600. Hover (not selected) border `#4A515B`, colour `#E6E8EB`.
- Match dot 6px `#E8A33D`. Count mono 11px 500, `#8A929D` (lit: `#E8A33D`).

### 6.6 Who row
- Height 34px, padding `0 16px`, gap 10px, bg `#121418`, bottom border `#22262C`.
- Caption 10.5px caps `#8A929D`. Chip: height 24px, padding `0 8px`, radius 12px, bg `#1B1E23`, border `#343A42` (open `#E8A33D`), colour `#E6E8EB` (none: `#9BA3AE`). Icon `icons/who_person.svg` 12px, caret `icons/who_chevron_10.svg` 9px.
- Badge: 10px, letter-spacing .04em, padding `0 5px`, line-height 15px, radius 3px, border `#343A42` (pinned `#E8A33D`).
- Listbox: absolute, top 28px, width 220px, padding 4px, menu styling.

### 6.7 Page bar
- Height 44px, padding `0 16px`, gap 6px, bottom border `#22262C`.
- Preset select 160px. Save as: height 24px, padding `0 10px`, radius 4px, border `#343A42`, bg `#262A31`, 12px.
- Icon buttons: 26x24, radius 4px, border `#343A42`, transparent. Enabled colour `#C9CED6`, disabled `#4A515B`. Used for delete preset, reset page, undo, redo (`icons/trash.svg`, `icons/reset_arc.svg`, `icons/undo.svg`, `icons/redo.svg`).
- Reset page confirm: width 230px, padding `10px 12px`, border `#E8A33D`, radius 6px, menu shadow. Cancel: 24px, border `#343A42`, colour `#C9CED6`. Reset page: 24px, bg `#E8A33D`, colour `#141413`, 600.
- Master switch: margin-left 6px, padding-left 10px, border-left `#2F343B`. Label 12px `#C9CED6` max 110px. Switch (34x18). State text 11px 600.
- Find field: width 170px, height 24px, padding `0 8px 0 26px`, bg `#121418`, border `#343A42` (amber while a query is typed), radius 4px, 12px, placeholder `#8A929D`. Magnifier `icons/search.svg` at left 8px, top 6px.
- Results popover: absolute, right 0, top 30px, width 384px, max-height 420px, bg `#15171B`, border `#343A42`, radius 6px, popover shadow. Header 32px, rows 28px, footer 26px.

### 6.8 Master-off banner
- Height 32px, padding `0 16px`, gap 8px, border-bottom `#3A2F1E`, bg `rgba(232,163,61,.08)`, text `#F3BE6A` 12px. Icon `icons/master_off_power.svg` 13px. Turn on button: height 22px, padding `0 10px`, radius 4px, bg `#E8A33D`, colour `#141413`, 12/600.

### 6.9 Card
- Section: bg `#1B1E23`, border 1px `#2A2F36`, radius 6px, flex column. Flashing: border `#E8A33D`, glow `0 0 0 3px rgba(232,163,61,.22)`.
- Header: height 36px, padding `0 8px 0 12px`, gap 8px, border-bottom `#2A2F36`. Title 13/600, `letter-spacing .01em`, ellipsis.
- Badges: NEW (amber fill, `#141413` text) or plain (`#262A31` bg, `#343A42` border, `#C9CED6` text). Both 10px, radius 3px, padding `0 5px`, line-height 16px.
- Collapse button: height 22px, padding `0 8px`, border `#343A42`, radius 4px, 11px, `Show` / `Hide`.
- Card switch: 34x18 (see 6.12). Card reset: 24x24, no border, icon 13px `icons/reset_arc.svg`, colour `#E8A33D` if changed else `#8A929D`.
- Body: padding `6px 8px 8px 12px`, gap 2px, opacity 1 (on) or 0.45 (switched off), transition opacity .15s.
- Selector bar (combined cards): padding `6px 8px 6px 12px`, gap 8px, bg `#181B1F`, bottom border `#23272D`, label 11px `#8A929D`.
- Grid: two columns, gap 12px, align-items start.
- More options link: `height 22px; padding 0 6px; margin-top 4px; radius 4px; transparent; colour #9BA3AE; 12px; gap 4px`. Caret svg 9px rotates 90deg (`transition .12s`). Label `More options (n)` / `Fewer options` (680, 2343-2344).

### 6.10 Control row (the shared row)
- Grid `100px minmax(0,1fr) 52px 20px`, column-gap 6px, align-items centre, min-height 28px, margin `0 -4px`, padding `0 4px`, radius 4px.
- Background: transparent; hover `#22262D`; flash `rgba(232,163,61,.28)` with inset `rgba(232,163,61,.7)` ring.
- Label: 12px, line-height 20px, `#C9CED6` (matched by search: `#E8A33D`), nowrap, ellipsis, title tooltip.
- Value: 52px, mono 12/500 `#E6E8EB`, right-aligned.
- Reset: 20px column, 20x20 button, icon 11px `icons/reset_row.svg`. Colour `#E8A33D` when value differs from default, else `#5C646F`.
- Sub-heading inside a card: 10.5px/500 caps `#8A929D`, letter-spacing .09em, min-height 22px, separated by a `#262A31` top rule (margin-top 6px, padding `8px 0 3px`).

### 6.11 Slider
- Input: width 100%, height 20px, transparent, no margin. Track 4px, radius 2px, `linear-gradient(to right, #E8A33D var(--p), #363C44 var(--p))`.
- Thumb 12x12, radius 6px, `#E6E8EB`, margin-top -4px. Focus-visible: `box-shadow 0 0 0 3px rgba(232,163,61,.45)`.
- Value text: mono 12/500 `#E6E8EB`.
- Minimum track length: 120px (Kit 108).

### 6.12 Toggle switch
- Track 34x18, radius 9px, no border. On `#E8A33D`, off `#3A4049`.
- Knob 14px, radius 7px, top 2px, left 18px (on) or 2px (off). Knob on `#141413`, off `#C9CED6`. `transition left .12s`.
- Value label: `On` / `Off` 12px `#9BA3AE` right-aligned (Main.dc.html:122).

### 6.13 Select / combo
- `.sel`: height 24px (22px in card header; font 11.5px), bg `#262A31`, colour `#E6E8EB`, border 1px `#343A42`, radius 4px, padding `0 24px 0 8px`, 12px Sans, ellipsis.
- Caret: 10x10 `icons/chevron_down_10.svg`, absolute right 8px, top 7px, `#9BA3AE`.
- Focus: outline 2px `#E8A33D`, offset 1px.
- Card-header presets: wrap width 120px.

### 6.14 Number input
- 64x22, padding `0 6px`, bg `#121418`, border 1px `#343A42`, radius 4px, mono 12/500 right-aligned. Focus border `#E8A33D`.

### 6.15 Colour swatch
- 40x18, radius 3px, border 1px `#4A515B`, background = colour (fallback `#808080`). Value text mono 12/500 (e.g. `5600 K`).

### 6.16 List box
- Container: grid span 2, padding 4px, bg `#121418`, border 1px `#343A42`, radius 4px, height `rows*20 + 10` px, `overflow-y auto`.
- Item: 20px high, padding `0 8px`, radius 3px, mono 12px, ellipsis. Selected: bg `#E8A33D`, colour `#141413`. Idle: transparent, `#C9CED6`.

### 6.17 Placeholder row
- Grid span 2, min-height 24px (72px for lists), 1px dashed `#3A4049`, radius 4px, padding `0 8px` (`6px 8px` for lists), mono 11px `#8A929D`, bg `rgba(255,255,255,.015)`.

### 6.18 Compass dial
- Cell: padding `4px 6px 6px 8px`, radius 6px, border 1px `#262A31`, bg `#181B1F`, hover `#22262D`, flash border `rgba(232,163,61,.7)` and bg `rgba(232,163,61,.28)`.
- Header row: height 20px. Label 12px `#C9CED6`. Reset 20x20.
- Dial 48x48, radius 24px, cursor crosshair, focus outline 2px `#E8A33D` offset 2px.
- Face (`icons/dial_compass.svg`): bezel circle r 20, fill `#15171B`, stroke `#4A515B` 1.5. Ticks `#8A929D` stroke 1.4. N/E/S/W labels 6.5 SVG units. Needle 2px `#E8A33D`. Hub r 3 `#E8A33D`, centre r 1.2 `#141413`.
- Readout: mono 12/500 `#E6E8EB`.

### 6.19 Arc dial
- Face (`icons/dial_arc.svg`, 48x48): baseline dashed `#2F343B`, arc stroke `#2F343B` 2, range stroke `#4A515B` 2.5 round, ticks `#8A929D` 1.3 round. Min and max labels `font-size 6`, `#5C646F`. Needle 2px `#E8A33D` from pivot (24,34). Pivot r 3 amber, centre r 1.2 `#141413`.

### 6.20 Light map card
- Map: aspect-ratio 320/205, radius 6px, `radial-gradient(ellipse 80% 90% at 50% 45%, #161D27 0%, #111418 70%)`, border 1px `#23272D`, overflow hidden, no selection.
- Scene: full-bleed SVG (viewBox 0 0 320 205). Beams stroke 1.4. Light glyphs: glow + ring + body. Names font-size 10, weight 600, in light hue. OFF label font-size 7.5, weight 600.
- Flip button (`icons/light_map_flip_side.svg`): absolute left 6px, top 6px, height 26px, padding `0 7px 0 3px`, gap 4px, border `#343A42`, radius 4px, bg `rgba(17,20,24,.88)`, colour `#C9CED6`, 11px/500.
- Legend grid: 4 columns, gap 4px. Chip: padding `5px 6px`, radius 4px, border `#2A2F36` (selected: light hue), bg `#262A31` when selected. Dot 8px, radius 4px, border 1.5px (on: hue fill; off: hatched `#8A929D`). Name 12px ellipsis, readout mono 11px `#C9CED6`.
- Legend key: near `icons/legend_near.svg` (solid), far `icons/legend_far.svg` (ring), off `icons/legend_off.svg` (slashed disc). Label 11px `#9BA3AE`.
- View row: `View` 11px `#8A929D`, then the seg group (Top, Side, 2.5D).
- Hint: 11px `#8A929D`, right-aligned, margin-top -2px.

### 6.21 Help pane (right, 224px)
- Aside: width 224px, padding 16px, gap 10px, border-left 1px `#262A31`, bg `#15171B`.
- Kicker 11px caps `#8A929D`. Title 15/600 line-height 1.3. Body 12px `#C9CED6` line-height 1.55, max-height 260px. Meta: top border `#262A31`, padding-top 10px, 11px; values mono `#E6E8EB`. Footer 11px `#8A929D`.
- Minimum-size strip (replaces the aside): height 30px, padding `0 16px`, gap 8px, border-top `#22262C`, bg `#15171B`, 12px `#9BA3AE`.

### 6.22 Transport bar (bottom, 40px)
- Container: height 40px, padding `0 10px`, gap 8px, bg `#15171B`, top border `#262A31`.
- ACTION split button: main half height 26px, min-width 92px, padding `0 12px 0 10px`, radius `4px 0 0 4px`, bg `#E8A33D` idle or `#E5484D` running, colour `#141413` or `#FFFFFF`, 12/600 letter-spacing .08em, gap 7px. Glyph 10px (`icons/transport_action_play.svg` or `_stop.svg`). Running adds `0 0 0 3px rgba(229,72,77,.22)`.
- Arm half: 22x26, radius `0 4px 4px 0`, border-left 1px `rgba(20,20,19,.3)`, caret `icons/armed_chevron_up.svg`. Armed panel above: width 260px, bottom 34px, padding `8px 10px 10px 12px`, rows grid `minmax(0,1fr) 88px`.
- Transport icon buttons (tbtn): 28x26, radius 4px, border 1px `#343A42` (on: `#E8A33D`), bg transparent (on `#262A31`), colour `#C9CED6` (on `#E8A33D`). Glyphs: play, stop (10px), marks set, marks reset (13px), paths (14px, dashed), legend (13px).
- Timecode: mono 13/500 `#E6E8EB`, padding `0 4px` (running `#F2A7A9`).
- Dividers: 1px x 18px `#2F343B`.
- Scene select: container 130px, `.sel`. Load and Save: height 24px, padding `0 8px`, border `#343A42`, radius 4px, transparent, `#C9CED6`, 11.5px.
- Scene menu: width 230px, bottom 32px, padding 6px, menu styling. Delete confirm button: bg `#E5484D`, colour `#FFFFFF`, 12/600, height 24px.
- REC pill: height 20px, padding `0 7px`, radius 10px, gap 5px, mono 600 10.5px letter-spacing .06em. Off: border `#343A42`, colour `#5C646F`, dot `#5C646F`. On: border and bg `#E5484D`, colour `#FFFFFF`, dot `#FFFFFF`. Dot 6px.
- Status pill (when shown): 7px dot `#E5484D`, text 12px `#F2A7A9`.
- Transport toast: height 22px, max-width 220px, radius 11px, padding `0 10px`, bg `#262A31`, border 1px `#E8A33D`, 11.5px `#F1F2F4`.
- Cast chip: height 26px, padding `0 8px 0 10px`, radius 13px, border `#343A42` (open `#E8A33D`), bg transparent (open `#262A31`), 12px `#9BA3AE`, gap 6px. Count `#E6E8EB` 600. Caret `icons/cast_chip_chevron_up.svg`. Popover: width 240px, bottom 32px, right 0.
- UI ms chip: 11px `#9BA3AE`, border 1px `#2F343B`, radius 3px, padding `2px 6px`.
- Hotkey button: tbtn with `?` 13/600.

### 6.23 Loading overlay
- Skeleton: `linear-gradient(90deg, #1B1E23 0%, #23272E 50%, #1B1E23 100%)`, size 840px, shimmer 1.4s. Disabled under reduced motion.
- Scrim: `rgba(17,19,22,.55)`. Dialog: min-width 300px, max-width 460px, padding `22px 26px`, gap 16px, bg `#15171B`, border `#2A2F36`, radius 8px, loading-dialog shadow.
- Spinner: 26px, border 3px `#2F343B`, top `#E8A33D`, spin .9s linear. Error: `icons/error_circle.svg` 24px in `#E5484D`.

### 6.24 Menus and popovers
- Container: bg `#15171B`, border 1px `#343A42`, radius 6px, popover or menu shadow.
- Item (`.res`): transparent; hover and focus `#262A31`. Hint text 10.5px `#8A929D`.

### 6.25 Scrollbars
- 8px wide and tall. Thumb `#343A42`, radius 4px. Track transparent.

## 7. Icons

All in `icons/`, copied verbatim from the mockup. Attributes are as written in source. Icons with `{{...}}` placeholders are runtime-computed.

| File | Source | Size | viewBox | Stroke | Purpose |
|---|---|---|---|---|---|
| window_minimize.svg | 114 | 12 | 0 0 12 12 | currentColor 1.5 | minimise |
| window_close.svg | 115 | 12 | 0 0 12 12 | currentColor 1.5 | close |
| rail_cast.svg | 122 | 18 | 0 0 24 24 | currentColor 1.6 round | Cast |
| rail_performance.svg | 123 | 18 | 0 0 24 24 | currentColor 1.6 round | Performance |
| rail_camera.svg | 124 | 18 | 0 0 24 24 | currentColor 1.6 | Camera |
| rail_lens.svg | 125 | 18 | 0 0 24 24 | currentColor 1.6 | Lens |
| rail_light.svg | 126 | 18 | 0 0 24 24 | currentColor 1.6 round | Light |
| rail_look.svg | 127 | 18 | 0 0 24 24 | currentColor 1.6 round | Look |
| rail_world.svg | 128 | 18 | 0 0 24 24 | currentColor 1.6 | World |
| rail_capture.svg | 129 | 18 | 0 0 24 24 | currentColor 1.6, dot fill currentColor | Capture |
| cast_tool_you.svg | 145 | 14 | 0 0 16 16 | currentColor 1.4 round | mark me |
| cast_tool_animesh.svg | 146 | 14 | 0 0 16 16 | currentColor 1.4 round | add animesh |
| cast_tool_focus.svg | 147 | 14 | 0 0 16 16 | currentColor 1.4 round | focus |
| cast_tool_remove.svg | 148 | 14 | 0 0 16 16 | currentColor 1.4 round | remove |
| cast_row_framed.svg | 160 | 11 | 0 0 16 16 | #E8A33D 1.6 round | framed badge |
| cast_row_looks_at.svg | 163 | 12 | 0 0 16 16 | #7FA8E0 1.5 | looks-at badge |
| search.svg | 324 | 12 | 0 0 12 12 | #8A929D 1.3 | find magnifier |
| error_circle.svg | 90 | 24 | 0 0 24 24 | currentColor 1.7 round | failed state |
| chevron_down_10.svg | 287 | 10 | 0 0 10 10 | #9BA3AE 1.4 | select caret |
| reset_arc.svg | 390 | 13 | 0 0 14 14 | currentColor 1.4 round | card and page reset |
| reset_row.svg | 673 | 11 | 0 0 14 14 | currentColor 1.5 round | row reset |
| trash.svg | 297 | 12 | 0 0 14 14 | currentColor 1.3 round | delete preset |
| undo.svg | 312 | 13 | 0 0 14 14 | currentColor 1.4 round | undo |
| redo.svg | 313 | 13 | 0 0 14 14 | currentColor 1.4 round | redo |
| toast_check.svg | 290 | 10 | 0 0 12 12 | #E8A33D 1.6 round | toast check |
| master_off_power.svg | 357 | 13 | 0 0 16 16 | currentColor 1.5 round | master-off banner |
| who_person.svg | 252 | 12 | 0 0 16 16 | currentColor 1.4 round | who chip |
| who_chevron_10.svg | 255 | 9 | 0 0 10 10 | currentColor 1.5 | who caret |
| cast_chip_chevron_up.svg | 830 | 9 | 0 0 10 10 | currentColor 1.5 | Cast chip caret |
| armed_chevron_up.svg | 733 | 10 | 0 0 10 10 | currentColor 1.5 | ACTION arm caret |
| transport_action_play.svg | 729 | 10 | 0 0 10 10 | fill currentColor | ACTION idle |
| transport_action_stop.svg | 730 | 10 | 0 0 10 10 | fill currentColor | ACTION running |
| transport_play.svg | 756 | 10 | 0 0 10 10 | fill currentColor | play take |
| transport_stop.svg | 757 | 10 | 0 0 10 10 | fill currentColor | stop take |
| transport_marks_set.svg | 805 | 13 | 0 0 16 16 | currentColor 1.4 round | set marks |
| transport_marks_reset.svg | 806 | 13 | 0 0 16 16 | currentColor 1.4 round | reset to marks |
| transport_paths.svg | 808 | 14 | 0 0 16 16 | currentColor 1.4 round, dash 2.2 2 | show paths |
| transport_legend.svg | 810 | 13 | 0 0 16 16 | currentColor 1.4 round | legend |
| legend_near.svg | 490 (1st) | 8 | 0 0 8 8 | fill #C9CED6 | near key |
| legend_far.svg | 490 (2nd) | 8 | 0 0 8 8 | stroke #C9CED6 1 | far key |
| legend_off.svg | 490 (3rd) | 9 | 0 0 9 9 | stroke #8A929D 1, #C9CED6 1.2; fill #2A2F36 | off key |
| light_map_flip_side.svg | 471 | 22 | 0 0 22 22 | mixed | side-view flip (placeholders) |
| dial_compass.svg | 524 | 48 | 0 0 48 48 | mixed | compass face (placeholders) |
| dial_arc.svg | 527 | 48 | 0 0 48 48 | mixed | arc face (placeholders) |

## 8. Ambiguities and gaps

- **A1 Rail fold threshold.** Kit says folds below 980 px window width (`Kit.dc.html:82`). Kit's table says 850 to 1099 px (`Kit.dc.html:103`). Director folds at minimum only (`isMin`). Confirm 980.
- **A2 Card columns.** Kit: drop to 1 column below 340 px (`Kit.dc.html:107-108`). Director: always 2 columns (`Director.dc.html:365`).
- **A3 Section caps.** Kit and Director cast heading use 11px .08em. Director in-card sub-headings use 10.5px 500 .09em (`Director.dc.html:500`). Both recorded.
- **A4 Note colour.** Kit notes `#9BA3AE`; Director notes `#8A929D`. Both recorded.
- **A5 Row negative margin.** Kit rows have no negative margin; Director rows use `margin 0 -4px`. Director values used.
- **A6 Scroll bar on rail.** Director rail has `overflow-y auto` at minimum sizes; Kit does not show it.
- **A7 Font weights.** The Google Fonts link loads Plex Mono 400 and 500 only (`Director.dc.html:12`). The REC pill uses mono 600 (`:2795`). Add Plex Mono 600 or accept synthetic bold.
- **A8 Shadow.** Main has no window shadow. Director uses `0 20px 60px rgba(0,0,0,.55)`. Director used.
- **A9 Light-map scene art.** Only its palette is tokenised. The scene geometry is runtime-computed.
- **A10 VCam page.** `VCamPage.dc.html` has about 100 extra scene colours and its own layout. It is not tokenised component by component. Its chrome reuses the amber, card and text tokens above.
- **A11 Runtime values.** Several values are generated by script and not fixed in source: dial angles, light-map positions, slider `--p`, row flash timings (`Director.dc.html:1813`, `1847`, `1863`). Only the static styles are tokenised.

## 9. Verification

`python -I verify_tokens.py` (stdlib, run from this folder):

- Every hex colour in `style_tokens.json` appears literally in one of `mockup_src/*.dc.html` (case-insensitive).
- Every `rgba(...)` literal appears literally in the sources.
- Every `NNpx` token appears literally in the sources. Note: this is a literal-presence check, so a common token such as `1px` passes wherever it occurs. It does not prove the value applies to that component.
- Every icon file in `icons/` is listed in the JSON, and its full text appears verbatim in `Director.dc.html`.

Last run: PASS. 56 distinct hex colours, 17 distinct `rgba()` literals, 62 distinct px tokens, 44 icons.
