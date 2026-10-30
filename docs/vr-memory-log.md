# Memory log (temporary, for testing)

> **Temporary.** Added to diagnose `Create2DTexture ... 8007000e = Not enough
> memory resources` crashes on an 8 GB RTX 4060 in the airport mission. Remove it
> once that is understood; see [Removing it](#removing-it).

## What it writes

`minidumps/overlord-memory-log.txt` in the game folder:

- one line when the game starts;
- one `sample` line every 5 seconds;
- one `TEXTURE ... FAILED` line the moment any texture fails to load, with its
  size and error code, written before the game shows its fatal error.

Each line records the map, the game's committed memory and RAM use, Windows'
total commit against its limit (RAM plus page file), free RAM, and the game's
dedicated and shared GPU memory use against the GPU budget.

The file is capped at **1 MB** (roughly 4–5 hours of play). Past that it starts
over instead of growing. Delete it at any time; it is recreated on the next run.

- Turn it off: console `vr_memoryLog 0` (saved). `vr_memoryLog 1` turns it back on.
- Write a line on demand: console `memory_log`.

## Reading it

The last lines before a crash show which limit was hit:

- `windows_commit` near its limit: the system ran out of committed memory.
  Close background apps or enlarge the page file.
- `gpu ... used` near `budget`: the graphics card was full. Lower texture
  quality or VR render resolution.
- `game_committed` rising steadily across `sample` lines in one mission: a leak.

## Removing it

Delete these, then rebuild:

1. `src/client/component/memory_log.cpp`
2. In `src/client/component/d3d11/texture_diagnostics.hpp` and `.cpp`: the
   `failure_observer` type, `set_failure_observer`, the `observer` variable and
   the one line in `record_failure` that calls it.
3. This file.

The saved `vr_memoryLog` value in the player's config is ignored once removed.
