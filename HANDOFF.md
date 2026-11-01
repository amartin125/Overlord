# Handoff: Overlord VR fixes ("Beta 8 test")

Written at the end of a cloud Claude Code session so a local session can take
over. Read this first, then `CONTRIBUTING.md` and `docs/development.md`.

## Who and what

- The player is fixing the **Overlord** VR mod for *Call of Duty: Modern Warfare
  2 Campaign Remastered* (upstream: [Aeka0/Overlord](https://github.com/Aeka0/Overlord)).
- **Hardware:** Meta Quest 2 (Touch controllers), **RTX 4060 (8 GB VRAM)**,
  42 GB RAM, Windows. Was on the **official Beta 7** before these changes.
- **Game folder:** `C:\Program Files (x86)\Call of Duty Modern Warfare 2 Campaign Remastered`
- **Desktop shortcut "MW2 VR"** targets `MW2 VR Launcher.exe` in the game folder,
  a third-party helper (not part of Overlord, not touched by the installer) that
  checks VR setup and starts Overlord from that folder. "MW2 VR Settings" also exists.
- The player is not a developer: give click-by-click steps, keep messages
  short, and explain errors in plain words.

## Player preferences (keep following these)

- **No translation work.** Only English (and Chinese where a string already
  existed) is fine; they explicitly said to skip other languages.
- Builds must be **identifiable**: `version.json` name is `Beta 8 test`.
- Diagnostics must stay **small** and be clearly marked temporary.
- Wants game fixes and upgrades; prefers being told what to test.

## Repository state

- Fork: `amartin125/Overlord`, branch **`claude/eager-cori-47ptia`**.
- Based on upstream `5e69c23` (Beta 7 tag + 5 small upstream commits:
  tracer styles, M203 polish, pump shell ejection, legacy module removal,
  language detection). Upstream `main` has since added `2b3f225 Add arms resize`
  (not merged here).
- Latest commit `283de90`; CI build
  [run 41355810884](https://github.com/amartin125/Overlord/actions/runs/41355810884)
  passed all three configurations and produced `Overlord-Installer-283de90…`.
  The player is installing that build now; **nothing below has been tested in a
  headset yet** except where noted.

## How builds were made

- The cloud session could not compile Windows code. It pushed to the branch and
  ran `.github/workflows/build.yml` via `workflow_dispatch` (forks need Actions
  enabled; it is). Artifacts: `overlord-<Config>-<sha>` zips and
  `Overlord-Installer-<sha>` (RelWithDebInfo only). Artifacts expire after 14 days.
- Locally on Windows you can build directly (VS2022, see `docs/development.md`):
  `generate.bat --with-vr-tests` then
  `msbuild build\overlord.sln /t:client /m /p:Configuration=RelWithDebInfo /p:Platform=x64`.
  Release treats warnings as errors.
- Pure-logic test headers were compiled and run on Linux with g++ (C++23):
  `tests/vr/controller_input_tests.cpp`, `tests/vr/underbarrel_tests.cpp`,
  `tests/vr/melee_tests.cpp` (all 0 failures). `head_pose_bridge.cpp` was also
  exercised with a shim for leash and walking behaviour.
- Publishing a public GitHub Release was blocked in the cloud session; the
  installer is only an Actions artifact.

## One-click installer (`tools/installer/`)

- `Installer.cs` (C# 5, compiled in CI by .NET Framework `csc.exe` with the
  client overlay zip embedded as resource `package.zip`), `app.manifest`
  (requireAdministrator). CI step "Build one-click installer" in `build.yml`
  (paths must use backslashes for csc).
- Finds the game folder from desktop shortcuts (target `overlord*.exe` or name
  containing "MW2 VR"), then the default path, then asks. Shows installed and
  packaged `version.json` names. Menu: 1 install, 2 undo last update, 3 exit.
  Backs up replaced files to `overlord-backups\<timestamp>\` with
  `added-files.txt`; undo restores and renames the folder `-restored`.
- Note: the "MW2 VR" shortcut points at `MW2 VR Launcher.exe`, which the
  shortcut matcher accepts by name, so the game folder resolved correctly.

## Changes on this branch (all saved dvars unless noted)

| Problem | Change | Files | Toggle |
|---|---|---|---|
| View drifted from body; "stuck on a table that isn't there" (upstream #122, #45) | Horizontal camera offset from native eye bounded by a dragged world shift, applied to camera/hands/body | `vr/roomscale_leash.hpp`, `head_pose_bridge.cpp` | `vr_roomscaleLeash` 0.25 m (0 off) |
| Physical crouch only lowered camera | Head below recentered height requests native crouch; view lifted to avoid double lowering; 1 s lift hold on rise | `gameplay/controller_stance.hpp`, `controller_component.cpp`, `camera.cpp` | `vr_physicalCrouchDepth` 0.35 m |
| Stick crouch hard; diagonal turns caused crouch/prone in fights | Threshold 0.90→0.75 (continue 0.65) and push must be mostly vertical (`|y| >= 1.5|x|`) | `controller_stance.hpp` | — |
| Grenade launcher / Masterkey unusable | Grip-held closed M203 support or shotgun support/pump fires with that hand's Trigger | `underbarrel_grip.hpp`, `underbarrel_runtime.cpp` | `vr_underbarrelSupportFire` 1 |
| Support grip drops on launcher rifles (#120) | `vr_supportGripLock` now applies to module-owned support | same | existing |
| Sprint (#95, #125) and Cliffhanger gap jump (#80, #10) | Gamepad-style latch: click while pushing forward keeps sprint until forward released; any held Trigger cancels | `controller_buttons.hpp` | — |
| NPC body-blocking (#118) | Player traces pass through friendly/neutral actors; enemies (from `getaiarray("bad_guys")`, refreshed 250 ms on server pipeline) keep reduced-core collision | `npc_collision.cpp`, `npc_collision_policy.hpp` | `vr_npcPassThrough` 1 |
| ADS slows to a crawl (#123) | Clears native ADS slow-walk flag for local VR player | `ads_sprint.cpp`, `ads_sprint_bridge.hpp` | `vr_adsFullSpeed` 1 |
| Stuck after checkpoint-save stall (blue/purple flash) — also in original | Locomotion `interrupt()` keeps held sticks working after stalls/focus blips < 2 s; pause/menus/recenter/load still need neutral | `controller_locomotion.hpp`, `controller_component.cpp` | — |
| VRAM | Custom-knife pack `h2_vr_melee.ff` (28 MB) loads only when `vr_meleeWeapon` is baton/karambit/ending/bloody | `fastfiles.cpp` | — |
| VRAM | Render scale applied to runtime-recommended eye size (OpenXR + OpenVR) | `engine_scene_extent.hpp`, `openxr_runtime.cpp`, `openvr_runtime.cpp`, `vr_component.cpp` | `vr_renderScale` 1.0 (0.5–1, needs `vr_reinit`) |
| Guns too far forward | "Meta Quest 2" calibration preset: `back = 0.04 m` | `vr/settings.hpp` | launcher preset |
| Roomscale walking | Optional: idle stick + offset > 0.12 m walks toward head (≤ 60% speed); bridge absorbs body catch-up only toward head, only while a walk request is fresh | `vr/roomscale_walk.hpp`, leash `compensate`, bridge `roomscale_walk_target` | `vr_roomscaleWalk` 0 (off) |
| Comfort | Optional radial edge darkening while stick-moving/turning, in OpenXR eye blit shader | `vr/comfort_vignette.hpp`, `texture_blit.cpp`, `openxr_runtime.cpp` | `vr_comfortVignette` 0 (off) |
| Diagnostics | **Temporary** memory log (see below) | `memory_log.cpp`, `d3d11/texture_diagnostics.*` | `vr_memoryLog` 1 |

Also: `version.json` → `Beta 8 test` / `0.1.0.8`; English and Chinese help text
for stance and underbarrel; docs updated (`vr-stance-controls.md`,
`vr-camera-architecture.md`, `vr-underbarrel-runtime.md`,
`vr-underbarrel-support.md`, `vr-native-ads.md`, `vr-controller-interaction.md`,
`vr-melee.md`, `vr-melee-customization.md`, `vr-presentation-options.md`).
`vr_input_status` prints crouch/leash/walk state; `vr_npc_collision_status`
prints pass-through and hostile counters.

## Open issues

1. **`Create2DTexture … 8007000e` (E_OUTOFMEMORY) in the airport mission
   ("No Russian").** Player says it did not happen on official Beta 7. Task
   Manager showed dedicated GPU memory around 60% (unconfirmed it was the
   memory figure, not usage). Code review found nothing on this branch that
   creates textures; the knife pack is now conditional and `vr_renderScale`
   exists. The player had **not yet** applied: Shader Preloading off, Texture
   Resolution Normal, lower runtime resolution, page file 16–32 GB, closing
   local AI tools (ComfyUI, Qwen, OpenJarvis seen on their desktop). Next step:
   get `minidumps\overlord-memory-log.txt` (last ~20 lines show which of
   Windows commit / GPU budget / leak is the limit) and the newest
   `minidumps\overlord-crash-*` zip, and try an A/B against Beta 7 via
   installer option 2.
2. **Stuck in fights** — two fixes landed (diagonal-turn prone, stall resume);
   needs player confirmation.
3. **Gun distance** — confirm the Quest 2 preset value or tune
   `vr_handOffsetBack` / `vr_handOffsetUp` / `vr_handAnglePitch`.
4. **Untested in headset:** everything in the table, especially physical
   walking, comfort vignette and render scale.
5. **Performance:** upstream measurements show ~22 ms CPU per owner
   transaction in heavy scenes (`docs/vr-render-performance-review.md`). Not
   attempted; needs on-device profiling (`vr_debugWaitTiming`).
6. Not handled: crashes needing dumps (#127, #116, #104, #111), WMR/Pico axis
   inversions (#86, #109), Stinger/Javelin aiming (#110, #87).

## Temporary items to remove later

- **Memory log**: follow "Removing it" in `docs/vr-memory-log.md`
  (delete `src/client/component/memory_log.cpp`, the `failure_observer` hook in
  `d3d11/texture_diagnostics.*`, the doc and its `docs/README.md` link).
- This `HANDOFF.md` once the local session has absorbed it.
