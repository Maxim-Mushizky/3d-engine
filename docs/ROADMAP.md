# Forge Roadmap — toward a competent beginner 3D tool

Source of truth for what's next after M8. Every item is a GitHub issue on
[Maxim-Mushizky/forge-3d-engine](https://github.com/Maxim-Mushizky/forge-3d-engine/issues)
— full specs and acceptance criteria live there. Workflow: one branch per issue
(`feat/<n>-short-name`), PR references the issue.

## Philosophy (the filter for every feature)

1. **Easy 3D for beginners** — a first-time user makes something they're proud of
   in their first session, without reading docs.
2. **Deep customizability** — preferences, themes, keys, materials, skies, and
   primitives are all user-editable; config lives in plain JSON files the user
   can hand-edit.

## P0 — core competence (a tool, not a toy)

| # | Issue | Notes |
|---|-------|-------|
| [#1](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/1) | Scene save/load (.forge) | **Do first.** Unblocks #7, #11, #12 |
| [#2](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/2) | Drag-and-drop import | Small; do early |
| [#3](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/3) | Render image to PNG | Reuses turntable job pattern |
| [#4](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/4) | glTF (.glb) export | tinygltf writer |
| [#5](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/5) | Transform snapping | ImGuizmo snap param |
| [#6](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/6) | Toast notifications | Small; #2/#3 reference it |

## P1 — beginner experience

| # | Issue | Notes |
|---|-------|-------|
| [#7](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/7) | Welcome screen + starter templates | Better after #1 |
| [#8](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/8) | First-run contextual hints | |
| [#9](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/9) | Material preset library | `forge_materials.json` |
| [#10](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/10) | F1 shortcut cheat-sheet | Builds the action table #15 needs |
| [#11](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/11) | Parametric primitives (recipes) | Recipe also serializes in #1 |
| [#12](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/12) | Auto-save + crash recovery | Depends on #1 |

## P1 — customizability

| # | Issue | Notes |
|---|-------|-------|
| [#13](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/13) | Settings dialog + forge_settings.json | **Backbone** — #5/#7/#8/#12/#14/#15 persist into it |
| [#14](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/14) | Theme customization | Theme.h → data-driven palette |
| [#15](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/15) | Customizable keybindings | Needs #10's action table + #13 |
| [#16](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/16) | Full texture maps in raster | normal/rough/metal + tiling |
| [#17](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/17) | Path tracer texture sampling | Atlas; albedo first |
| [#18](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/18) | Glass / dielectric materials | transmission + IOR |
| [#19](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/19) | Spot + area lights | Area mostly falls out of emissive path |

## P2 — polish & infra

| # | Issue | Notes |
|---|-------|-------|
| [#20](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/20) | Turntable: PNG sequence + presets | |
| [#21](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/21) | Sky library panel + procedural sky controls | |
| [#22](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/22) | Viewport MSAA / grid / outline | |
| [#23](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/23) | Undo history panel | |
| [#24](https://github.com/Maxim-Mushizky/forge-3d-engine/issues/24) | CI: Windows MinGW build | Do early-ish once PRs start |

## Suggested order

1. **#1 save/load** → 2. **#6 toasts** (tiny) → 3. **#2 drag-drop** → 4. **#13 settings**
→ 5. **#5 snapping** → 6. **#3 render PNG** → 7. **#24 CI** → 8. **#4 glb export**
→ then P1 beginner-UX and customizability tracks in parallel, P2 as filler.

## Local repo note

This working directory is **not yet a git checkout** — the GitHub repo was
bootstrapped separately. Before the branch-per-issue workflow starts:
`git init` + `git remote add origin https://github.com/Maxim-Mushizky/forge-3d-engine.git`
+ fetch and reconcile with the pushed `main` (or re-clone and overlay this tree).
