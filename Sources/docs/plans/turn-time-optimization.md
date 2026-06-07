# Turn-Time Optimization — Investigation & Hot-Path Map

**Status:** Investigation complete; no code changes yet. This is a problem map to drive a
measured optimization pass.

**Question that started this:** late-game turns take *significantly* longer than early
turns. How much of that is just "more cities," and what scales worse than linearly with
game progress (tech count, unit count, revealed-plot count) regardless of city count?

**Headline answer:** city count is only one driver. There are several per-turn passes that
scale with **revealed plots** or **unit count** rather than cities, plus a handful of
per-turn re-invocation ("spin") bugs whose cost grows with unit count. The single most
suspicious item is an every-turn full-map visibility recompute that the source itself
labels a "stickytape" hack.

> **Measure before cutting.** Every claim below is a static read of the code. The repo
> already ships a working sampling profiler — use it to confirm which of these actually
> dominate *before* optimizing. See [Profiling](#profiling-how-to-measure). The big-O
> labels are guides to *where to look*, not evidence that a path is hot.

---

## TL;DR ranked suspects

| # | Hot path | File:line | Scales with | Per-turn frequency |
|---|----------|-----------|-------------|--------------------|
| 1 | **Full-map visibility clear + `updateSight`** ("stickytape") | `CvGame.cpp:5869` | plots × units | once / game turn |
| 2 | **`AI_doEnemyUnitData` visible-plot scan** | `CvPlayerAI.cpp:24686` | visible plots × units-on-plot | once / AI player |
| 3 | **Per-unit pathfinding × group re-decide cascades** | `CvSelectionGroupAI.cpp:149` + `CvPathGenerator.cpp:819` | units × decision-depth × path cost | once / unit (×1–50 cascade) |
| 4 | **Found-value clear** (cheap) + lazy recompute (expensive) | `CvPlayerAI.cpp:1189` | plots (clear); revealed plots (recompute, gated) | once / AI player |
| 5 | **`AI_doDiplo`** tech/bonus/deal trading | `CvPlayerAI.cpp:17394` | players² × max(techs, bonuses, deals) | once / AI player |
| 6 | **Resource-consumption recompute** | `CvPlayer.cpp:27065` | bonuses × cities × buildings/city | once / player |
| 7 | **`clearCanConstructCache(NO_BUILDING)`** | `CvPlayer.cpp:27773` | buildingTypes × cities | once / player |
| 8 | **Property solver** (3-phase, single-pass) | `CvPropertySolver.cpp:447` | cities + units + plots (objects w/ manipulators) | once / map |
| 9 | **`updateTradeRoutes`** | `CvPlayer.cpp:4257` | cities² | once / player |
| 10 | **Plot-group flood-fill** (when dirty) | `CvPlayer.cpp:4095` | plots | dirty-driven |

Items that scale with **plots** (1, 2, 4, 10) or **units** (1, 2, 3) are the ones that grow
"beyond just more cities," because a maturing game reveals most of the map and fields large
armies even when city count plateaus. Items 5–7, 9 scale with tech/bonus/building counts,
which also climb over a game independently of cities.

---

## The per-turn pipeline (where the time goes)

Once-per-game-turn driver: `CvGame::update` (`CvGame.cpp:2270`) → `CvGame::doTurn`
(`CvGame.cpp:5788`). Per living player: `CvPlayer::doTurn` (`CvPlayer.cpp:3676`) and the AI
hooks `CvPlayerAI::AI_doTurnPre/Post` and `AI_doTurnUnitsPre/Post`.

```
CvGame::doTurn (CvGame.cpp:5788)
├─ per team:   CvTeam::doTurn                       (espionage loop O(MAX_TEAMS²))
├─ per map:    CvPropertySolver::doTurn  +  CvMap::doTurn (O(plots) per map)
├─ doGlobalWarming                                  (O(plots), feature-gated)
├─ doHeadquarters / doDiploVote
└─ FULL-MAP visibility clear + updateSight  ← suspect #1  (CvGame.cpp:5869)

CvPlayer::doTurn (CvPlayer.cpp:3676)
├─ doUpdateCacheOnTurn → clearCanConstructCache     ← suspect #7  (O(buildings[×cities]))
├─ AI_doTurnPre                                     (CvPlayerAI.cpp:391)
│   ├─ AI_doEnemyUnitData                           ← suspect #2  (CvPlayerAI.cpp:24686)
│   └─ AI_doDiplo (via AI_doTurnPost)               ← suspect #5  (CvPlayerAI.cpp:17394)
├─ recalculatePopulationgrowthratepercentage        (O(buildings×cities))
├─ recalculateAllResourceConsumption                ← suspect #6  (CvPlayer.cpp:27065)
├─ per city:  CvCity::doTurn                        (O(cities); many sub-passes)
├─ doTurnUnits                                      ← suspect #3
│   ├─ O(plots) owned-plot guardable scan           (CvPlayer.cpp:3980)
│   └─ 4 domain passes × all selection groups → group/unit AI + pathfinding
└─ updateTradeRoutes                                ← suspect #9  (O(cities²))

CvPlayerAI::AI_doTurnUnitsPre (CvPlayerAI.cpp:539)
├─ plotDangerCache.clear()                          (TTL = 1 turn; see notes)
└─ AI_updateFoundValues(true)                       ← suspect #4  (clear only here)
```

---

## Detailed findings

### 1. Full-map visibility recompute ("stickytape") — `CvGame.cpp:5869`
Every game turn, `doTurn` clears visibility counts on **every plot** and then calls
`GC.getMap().updateSight(true, false)`, which re-applies sight for every unit/city.

```cpp
// CvGame.cpp:5865  — comment is verbatim from the source
// Recalculate vision on load (a stickytape - can't find where it's skewing visibility counts)
// Hopefully won't create a noteable delay but it may
for (int iJ = 0; iJ < GC.getMap().numPlots(); iJ++)
    GC.getMap().plotByIndex(iJ)->clearVisibilityCounts();
GC.getMap().updateSight(true, false);
```
- **Cost:** O(plots) clear + O(units × sightRange²) re-apply, **every turn, for the whole game**.
- **Why it's the #1 suspect:** it is an acknowledged hack working around a visibility-count
  drift bug, not a designed per-turn cost. If the underlying drift were fixed (or this were
  gated to load-only, as the comment's "on load" intent suggests), it could potentially be
  removed entirely. **Verify with the profiler first** (look for `updateSight` self-time).

### 2. `AI_doEnemyUnitData` — `CvPlayerAI.cpp:24686`
Scans **every plot visible to the team**, and for each, iterates the units on it to tally
enemy strength used by AI threat assessment.
- **Cost:** O(visible plots × units-per-plot). Grows as the map is revealed and armies grow.

### 3. Per-unit pathfinding × group re-decide cascades
Two multiplied costs:

- **Pathfinder** `CvPathGenerator::generatePath` (`CvPathGenerator.cpp:819`): A* with a hard
  ceiling of 20 000 iterations (`CEILING_ITERATIONS`, `CvPathGenerator.cpp:22`) and a
  distance-scaled soft optimization limit. Per query ranges from cheap to expensive on long
  routes / complex terrain. Path caching (lines ~872–900) reuses routes for units continuing
  along the same path.
- **Group re-decide loop** `CvSelectionGroupAI::AI_update` (`CvSelectionGroupAI.cpp:149`):
  `while (... readyToMove())` re-runs the head unit's `AI_update()` until it stops being
  ready, with a **hard safety cap of 50** iterations (`iTempHack >= 50`, line ~173). When a
  unit "acts" without consuming its move, it re-decides the same thing every pass and spins
  to the cap. Multiply by ~60+ `generatePath` call sites across `CvUnitAI` decision helpers.
- **Net:** late game = many units × (cascade depth) × (1–5 paths each). This is the most
  likely place where "more units" turns into super-linear wall-clock.

Known spin patterns (some fixed, all worth re-checking with the profiler):
- **Heal spin** (`CvUnitAI.cpp:~12721`) — mitigated by a `canHeal()` guard; can still spin if
  a unit claims heal while `canHeal()` is false mid-cascade. See memory
  `hunter-move-reinvocation`.
- **Property-control RESERVE↔PROPERTY_CONTROL flip** — documented fixed
  (`property-control-oscillation` memory); was ~750 flips/turn.
- **Explore/defensive two-tile oscillation** (`CvUnitAI.cpp:~16519`) — prevention filter in place.
- **City-attack retry** `AI_pickTargetCity` (`CvUnitAI.cpp:~17075`) — up to 8 adjacent × 2
  passes × `generatePath` per attack decision.
- **`AI_refreshExploreRange`** (`CvUnitAI.cpp:16441`) — builds a `CvReachablePlotSet`
  (O(range²)) then pathfinds many candidate plots; called per exploring/hunting unit when
  range is exhausted. Linked to the still-open sea-AI spin (memory `sea-ai-rework`).

### 4. Found values — clear is cheap, recompute is lazy — `CvPlayerAI.cpp:1189`
**Correction to first-pass notes:** the per-turn call `AI_updateFoundValues(true)`
(`AI_doTurnUnitsPre:551`) takes the `bClear` branch, which only walks plots calling
`clearFoundValue` and **early-returns at line 1209**. The expensive part — per-plot
`AI_foundValue()` — is the `bClear=false` path (lines 1213–1245) and is **gated by
`bNeedsCalculating`** (only areas lacking a cached best-found value), i.e. lazy/on-demand,
not every plot every turn.
- **Per-turn cost:** O(revealed plots) memory clears per AI player. Real and plot-scaling,
  but far cheaper than "recompute every plot every turn." Don't over-prioritize it.

### 5. `AI_doDiplo` — `CvPlayerAI.cpp:17394`
Tech-source precompute over players × techs, a 2-pass players loop scanning all active deals
and all bonuses for trade offers.
- **Cost:** ~O(players² × max(techs, bonuses, deals)). Player count is fixed, but tech/bonus/
  deal counts climb through the game.

### 6. `recalculateAllResourceConsumption` — `CvPlayer.cpp:27065`
Loops every bonus type and, per bonus, every city and that city's buildings.
- **Cost:** O(bonuses × cities × buildings-per-city), every turn per player. Grows on three
  axes that all increase over a game.

### 7. `clearCanConstructCache(NO_BUILDING)` — `CvPlayer.cpp:27773`
Per-turn cache flush over all building types; with `bIncludeCities` it also flushes per city.
- **Cost:** O(buildingTypes [× cities]). Cheap per element but unconditional; worth checking
  whether the whole cache must be dropped every turn vs. invalidated on actual state change.

### 8. Property solver — single-pass, not iterative — `CvPropertySolver.cpp:447`
Good news: **no convergence loop.** `gatherAndSolve` (`CvPropertySolver.cpp:420`) runs a fixed
3-phase predict→correct→apply pipeline (propagators, interactions, sources). Manipulator
gathering (`gatherActiveManipulators`, line 312) iterates the 7 game-object types; the CITY/
UNIT/PLOT iterators are O(cities)/O(units)/O(plots).
- **Cost:** O(objects-with-manipulators) per map per turn — linear, no hidden iteration blow-up.
  Lower priority unless the profiler says otherwise. Reference: `docs/reference/CvPropertySolver.md`.

### 9. `updateTradeRoutes` — `CvPlayer.cpp:4257`
Clears then re-establishes routes by comparing cities pairwise.
- **Cost:** ~O(cities²) per player.

### 10. Plot-group flood-fill — `CvPlayer.cpp:4095`
When plot groups are dirty, re-colors regions via recursive flood-fill across all plots.
Dirty-driven (not literally every turn) but O(plots) when it fires, and can chain into
`updateTradeRoutes`.

---

## Measuring: `[PERF]` stopwatches (primary) — IMPLEMENTED

The headline phases are now instrumented with wall-clock stopwatches that log to
`Performance.log`, gated by their own knob — **works with any DLL (Assert/Release), no
special Profile build.** This is the primary measurement path; the FProfiler below is the
fallback for drilling deeper.

- **Enable:** BUG options → Autolog → **"Performance / turn-timing log level"**
  (`Autolog__LogLevelPerf` → `gPerfLogLevel`). `0` = off, `1` = on.
- **Mechanism:** `PERF_SCOPE("label", ownerIdOr-1)` (RAII `ScopedPerfTimer` +
  `win32::Stopwatch`, in `BetterBTSAI.h`/`.cpp`); one `[PERF/phase] turn= owner= phase= ms=`
  line per phase on scope exit. Off-cost = one integer compare, so it ships in normal DLLs.
- **Instrumented (headline pass):** `CvGame::doTurn`, **`doTurn.visibilityRebuild`** (the
  stickytape, suspect #1), `CvPlayer::doTurn` (per-player via `owner=`),
  `CvPlayer::doTurnUnits`, `AI_doDiplo`, `CvPropertySolver::doTurn`,
  `recalculateAllResourceConsumption`, `updateTradeRoutes`.
- **Analyze:** see the `[PERF]` grep/awk recipes in
  [`../reference/ai-logging-reference.md`](../reference/ai-logging-reference.md) §4 (total ms
  per phase, stickytape cost/turn, slowest empire). Sum across late-game turns to rank the
  suspects in real ms before optimizing.
- **Next:** add `PERF_SCOPE` around the unit/pathfinding cascade (suspect #3) if the headline
  pass points there.

### Intra-session growth diagnostic (turn times climb the longer you play, reset on reload)

Owner observation: turn time grows turn-over-turn within a session and **resets on game reload**.
That is a sharp signal — reload reconstructs everything *from the save*, so unit/city **counts
are preserved** across a reload. Therefore a phase that grows-and-resets is driven by
**in-memory accumulated state**, NOT by unit/city count (count-driven cost would survive the
reload). Decoder ring for `Performance.log` (`awk -F'phase=| ms=' '/PERF/{print $3"\t"$2}'`
grouped by turn, watch each phase's ms trend):
- **All phases inflate ~uniformly** → allocator/heap fragmentation from per-turn churn
  (CvReachablePlotSet/path objects). Reload defrags. Hard to fix in-code.
- **`doTurn.visibilityRebuild` climbs alone** → the visibility-count **drift** the stickytape
  papers over is accumulating within the session (load rebuilds clean from the save). *Leading
  hypothesis* — it's the most direct "in-memory state that resets on reload" candidate here.
- **`CvPropertySolver::doTurn` climbs** → property manipulators/objects accumulating.
- **`CvPlayer::doTurnUnits` climbs but unit count is flat** → a per-unit cache or re-decide
  spin growing (not raw count).
- **`AI_doDiplo` climbs** → deals/known-tech/bonus lists growing.
- **`CvGame::doTurn` climbs more than the sum of sub-phases** → growth in an *un-instrumented*
  span; add a `PERF_SCOPE` to bisect.

**Accumulator hunt — results (verified).** Searched for in-memory state that grows per turn and
resets on load:
- **FALSE POSITIVES (verified):** the static "current-X" caches do NOT accumulate — they self-
  clear during normal iteration. `resultsCache` (`CvUnitAI.cpp:1078`) clears whenever the owning
  player changes (many times/turn); `g_bestDefenderCache` (`CvPlot.cpp:3840`) clears whenever a
  different plot is queried (constantly); `cachedTargets` is the same player-keyed pattern. Don't
  chase these.
- **GENUINE CANDIDATE:** `CvContractBroker::m_workRequests`. Per-turn `cleanup()`
  (`CvContractBroker.cpp:54`) erases only **fulfilled** requests (`:77-88`); unfulfilled ones
  persist, and the broker is only fully wiped by `reset()` (`:41`) on load. Chronically-unfulfilled
  + re-posted requests ⇒ grows per turn, resets on reload. **Already self-instrumented:**
  `[CTB/turn] workRequestsRemaining=N` (`:91`) — enable `[CTB]`/player logging and watch that
  number across turns; a monotonic climb confirms the leak. Would surface in PERF as
  `CvPlayer::doTurnUnits` rising.
- **FALLBACK HYPOTHESIS:** if no single phase climbs but all inflate ~uniformly → heap
  fragmentation from per-turn allocation churn (CvReachablePlotSet / FAStar nodes / path vectors);
  resets on EXE restart. Different fix class (pooling), not a logic leak.

Two cheap data checks disambiguate: (1) `[PERF]` per-phase trend (one phase vs uniform);
(2) `[CTB] workRequestsRemaining` trend (climbing = the broker leak).

### First measured `[PERF]` data — turn ~1256 save (5 turns)

| Phase | result |
|---|---|
| `CvPlayer::doTurn` (per player) | **DOMINANT** — big AI empires (owners 1/4/9/10) ~10 s each, **max 15.7 s** |
| `CvPlayer::doTurnUnits` | 405 ms max — **unit AI/pathfinding is NOT the bottleneck** |
| `doTurn.visibilityRebuild` (stickytape) | ~35 ms — **not** the cost here (kills the #1 hypothesis for this save) |
| `CvPropertySolver::doTurn` | ~166 ms |
| `recalculateAllResourceConsumption` | ≤326 ms |
| `AI_doDiplo` / `updateTradeRoutes` | ≤234 ms / 0.4 ms |

**Contract broker EXONERATED (measured):** every vector bounded and per-turn-cleared —
`workRequests` max 142, `advertisingUnits` max 807, `advertisingTenders` max 140,
`contractedUnits` max 14; none grow across turns. The 26 MB `ContractBroker.log` is just
level-3 log volume (84k `[CTB/tender/cand]` lines), not a leak. The earlier "genuine candidate"
is wrong.

**Caveat — timings contaminated:** this run had Autolog at level ≥3 (hence the 26 MB of AI
logs), so megabytes of synchronous log I/O happened *inside* the timed turns. The 10–15 s is
partly disk I/O. **Re-measure clean: `gPerfLogLevel`=1, Autolog log level = 0.**

**Conclusion + next step:** the sink is the **economic/per-city** half of `CvPlayer::doTurn`,
NOT units, visibility, property, or the broker — consistent with the CvCityAI building/unit
valuation loops above. Added three bisection scopes inside `CvPlayer::doTurn`
(`doTurn.cities` = the `DoCityTurn` loop, `doTurn.AI_doTurnPre`, `doTurn.AI_doTurnPost`); a clean
re-measure will show whether the 10 s is the per-city production valuation (expected) or the
player-level AI economy.

### Full `[PERF]` scope inventory (comprehensive pass)

Instrumented top-to-bottom through the per-turn hot path (prune later if noisy). Labels:
- **Game/player:** `CvGame::doTurn`, `CvPlayer::doTurn`, `CvPlayer::doTurnUnits`,
  `doTurn.visibilityRebuild`, `CvPropertySolver::doTurn`, `recalculateAllResourceConsumption`,
  `updateTradeRoutes`, `AI_doDiplo`.
- **`CvPlayer::doTurn` split:** `doTurn.cities`, `doTurn.AI_doTurnPre`, `doTurn.AI_doTurnPost`.
- **`AI_doTurnPre` split (`pre.*`):** `pre.AI_updateBonusValue`, `pre.AI_doEnemyUnitData`,
  `pre.AI_doCommerce`, `pre.AI_doMilitary`, `pre.AI_doCivics`, `pre.AI_doReligion`,
  `pre.AI_doMilitaryProductionCity`.
- **Per-city (`city.*`, owner = city owner):** `city.doTurn`, `city.cacheFlush`,
  `city.recalcPopGrowth`, `city.AI_doTurn`, `city.AI_updateBestBuild`,
  `city.AI_updateWorkersNeededHere`, `city.doCheckProduction`, `city.doCulture`,
  `city.doProduction`, `city.AI_chooseProduction`, `city.CalculateAllBuildingValues`,
  `city.AI_bestUnitAI`.

Per-city scopes log once per city (and `AI_bestUnitAI` several times per production choice), so
**aggregate by phase label**. Total ms per phase across the whole log:
```
awk -F'phase=| ms=' '/PERF\/phase/{sum[$2]+=$3; n[$2]++} END{for(p in sum) printf "%12.1f  %6d  %s\n", sum[p], n[p], p}' Performance.log | sort -rn
```
Per phase **per owner** (which empire dominates each phase):
```
awk -F'owner=| phase=| ms=' '/PERF\/phase/{k=$3"|owner="$2; sum[k]+=$4} END{for(k in sum) printf "%12.1f  %s\n", sum[k], k}' Performance.log | sort -rn | head -40
```
Read it as a tree: `CvPlayer::doTurn` ≈ `doTurn.cities` + `doTurn.AI_doTurnPre` +
`doTurn.AI_doTurnPost` + misc; `doTurn.cities` ≈ Σ `city.doTurn`; `city.doTurn` ≈ `city.AI_doTurn`
(+ `AI_updateBestBuild`/`WorkersNeededHere`) + `city.doProduction` (→ `AI_chooseProduction` →
`CalculateAllBuildingValues` + `AI_bestUnitAI`) + the rest. Whichever leaf carries the mass is
the optimization target.

## FProfiler: how to measure (deeper, fallback)

The repo also ships a complete sampling profiler.

- **Macros:** `PROFILE_FUNC()` / `PROFILE(name)` / `PROFILE_BEGIN/END` —
  `Sources/FProfiler.h:94`. ~1100 call sites already exist; `CvCityAI` and `CvPlayerAI` are
  densely instrumented.
- **Build configs:** `Profile` and `ProfileExtra` in `Sources/fbuild.bff:159`. Build via
  `Tools/MakeDLLProfile.bat` (or `MakeDLLProfileExtra.bat` for the denser `PROFILE_EXTRA_FUNC`
  sites). These rebuild + deploy.
- **Enable/dump:** `startProfilingDLL(false)` / `stopProfilingDLL(false)`
  (`CvGameCoreDLL.cpp:477`). Output is tab-delimited `IFP_log.txt` in the BTS `Logs/` dir,
  with columns: total ms, main-thread ms, avg, #calls, child time, **self time**, parent.
- **Suggested method:** wrap `CvGame::doTurn` body in start/stop (or trigger at a known
  late-game autosave), play ~5 turns, sort `IFP_log.txt` by **self time**. Confirm whether
  `updateSight`, `AI_doEnemyUnitData`, `generatePath`, `AI_doDiplo`, and
  `recalculateAllResourceConsumption` actually dominate before touching them.
- **Gap:** there is no per-player or per-turn ms breakdown today — only per-function aggregate.
  A cheap add would be a per-player wall-clock log around `CvPlayer::doTurn` to see which
  players (size/army) cost most.

---

## Suggested next steps (in order)

1. **Measure** with a `ProfileExtra` DLL on a late-game save; rank by self-time. (No code risk.)
2. **Visibility hack (#1):** investigate the underlying visibility-count drift the "stickytape"
   works around; if fixable or gateable to load-only, this may be the largest single win.
3. **Pathfinding/spins (#3):** add cascade-depth + per-unit path-count logging (extend existing
   `[UNT]` taxonomy) to find units that hit the `iTempHack` cap; fix remaining spin sources.
4. **Cheap O(n) wins (#6, #7):** make resource-consumption and can-construct caches
   event-driven (invalidate on actual change) instead of full recompute/flush every turn.
5. Re-measure after each change; keep the profiler DLL around as the regression check.

## City & Unit AI loop hot-paths (structural; confirm with `[PERF]`/FProfiler)

From a focused read of `CvCityAI`/`CvUnitAI` (structural claims; the *magnitudes* are unproven
until measured — earlier agent "savings %" estimates were discarded as fabricated):

**City AI**
- Building-value cache is **flushed every turn unconditionally** — `CvCity::doTurn` calls
  `AI_FlushBuildingValueCache(false)` (`CvCity.cpp:~1253`) and `AI_doTurn` clears
  `m_buildValueCache` (`CvCityAI.cpp:~239`). A lazy/`bRetainValues=true` path exists but isn't
  used here. Candidate: event-driven invalidation (tech/building/civic) instead of per-turn.
- `AI_bestUnitAI` (`CvCityAI.cpp:4023`) loops **all** unit types calling `canTrain` +
  `player.AI_unitValue` with **no pre-filter by the city's actual UNITAI need**; its
  `m_bestUnits` cache is cleared each turn. Candidate: pre-gate candidates by need before the
  value call (this directly intersects the dog/hunter glut in `unit-ai-valuation.md`).
- `CalculateAllBuildingValues` (`CvCityAI.cpp:~12465`) has O(bonuses)/O(unitcombat) inner loops
  per building per focus-flag; some are stable unless tech/bonus changes.

**Unit AI** (aligns with the suspect-#3 cascade work above)
- The attack cascade (`AI_attackMove`, `CvUnitAI.cpp:2401`) can construct **many**
  `CvReachablePlotSet` objects in one call (each `AI_anyAttack`/`AI_cityAttack`/`AI_pillageRange`
  builds its own). Candidate: build once per attack phase and share.
- Expensive work before cheap gates: `AI_anyAttack` (`~17634`) builds the reachable set before
  any `canAttack()` check; `AI_safety` (`~15752`) builds the set twice (two passes) with no
  "already safe?" early-out. Candidate: cheap capability/danger gates first.
- This is exactly the "more granular gating earlier in the UnitAI process" lever — and the
  `CvHunterAI` → hunter+explorer vs dedicated army-module split is the structural home for it.

## ROOT CAUSE FOUND (turn ~1260 data) — `CalculateAllBuildingValues` is ~87% of turn time

Measured tree (comprehensive `[PERF]` split): `CvPlayer::doTurn` 170.8 s → `doTurn.cities` 155 s
→ Σ `city.doProduction` 148 s → `AI_chooseProduction` 145 s → **`CalculateAllBuildingValues`
134.7 s** (n=1353, ~100 ms each). Everything else is noise: `AI_bestUnitAI` called **71,642×**
but only 921 ms total (well cached); `AI_updateBestBuild` 264 ms; visibility 80 ms; property
442 ms; all `pre.*` AI economy < 4 s. **So the earlier suspects (visibility stickytape, broker,
unit AI, AI_bestUnitAI pre-filter) are all dead — the cost is building-value recompute.**

**Two compounding causes:**
1. **Cache destroyed every turn** — `CvCity::doTurn:1256` calls `AI_FlushBuildingValueCache()`
   with default `bRetainValues=false` → `SAFE_DELETE(cachedBuildingValues)` (`CvCityAI.cpp:12450`).
   Only 3 callers total ⇒ **no event-driven invalidation**; the design brute-forces a full
   recompute every turn.
2. **`AI_chooseProduction` runs every city every turn** — `CvCity::doProduction:16479` gate
   `!isProduction() || isProductionProcess() || AI_isChooseProductionDirty()`. Late-game cities
   run a **process** (wealth/research) which never completes ⇒ re-decide every turn ⇒ full
   recompute every turn even when nothing changed.

**Fix (small surface):** the cache already supports retain mode (`bRetainValues=true` keeps
values, marks `m_bIncomplete`).

**Step 1 (retain every turn) — MEASURED: ~40%, not 5×.** `CvPlayer::doTurn` avg 3558→2135 ms;
CABV per player-turn 2806→1532 ms; CABV calls/city 2.86→1.96. Why not more: `flush(true)` sets
`m_bIncomplete=true` **every turn**, and the first per-building cache-miss then forces a full
recompute of all cached flags (`AI_buildingValueThreshold` line 4794-4802). So we still pay ~1
recompute/city/turn. (`GetValue` returns -1 only for an uncached building, line 4616.)

**Step 2 (IN) — staggered periodic refresh.** Replaced the per-turn flush with a full refresh
only every `iRefreshPeriod` turns (=4), staggered by city id `((turn + cityID) % period == 0)`,
so ~1/period of cities recompute each turn and the rest hit the retained cache. Building changes
still flush promptly via `setHasBuilding` (`CvCity.cpp:14354`); newly-teched buildings are picked
up within ≤`iRefreshPeriod` turns. Bounded staleness; the Koshling comment at `doProduction`
already notes the AI deliberately shouldn't churn production on tech. Tune `iRefreshPeriod`, or go
fully event-driven (flush on tech-acquired / civic-change) if AI building adoption lags.

Optional (B): gate `AI_chooseProduction` so process-running cities don't re-decide every turn.

## Making each `CalculateAllBuildingValues` call cheaper (orthogonal to caching)

Why a single call is ~80-100 ms: it re-derives **static prereq relationships by brute force**
every call. Per building it scans all unit/building types:
- `.NotDeveloping` (`CvCityAI.cpp:13061`) — **O(buildings × units)** + `BoolExpr::evaluateChange`
  per match (13090, expensive) — which units the building enables (`isPrereqAndBuilding`).
- `.Sea` (13229) — O(buildings × units) — sea-unit free-XP (`canTrain` + domain).
- religious (13769) — O(buildings × units) — units this building is a prereq for.
- "needed for other buildings" (13812) — **O(buildings²)** — `getBuildingPrereqBuilding`.

~3-4M iterations/call in C2C (~1000 buildings × ~1000 units). The rest (`.Defense`/`.Happy`/…
over bonuses/unit-combats/specialists) has much smaller constants, so these all-types loops are
the bulk.

**Measure-first (IN, Assert build OK):** added a `PERF_ACCUM(double&)` accumulating timer
(`BetterBTSAI.h`) that sums a section's ms across the building loop and logs once per call via
`[PERF/cabv] owner= flags= defense= notdev= sea= commerceYields= commerceVal=`
(`CvCityAI.cpp` CalculateAllBuildingValues, 12467-13929). Wrapped the suspects: `.Defense`,
`.NotDeveloping` (the O(B×U) BoolExpr loop), `.Sea`, `.CommerceYields` (holds the religious
O(B×U) + the O(B²) at 13812), and the `getBuildingCommerceValue` helper (5×/building).
**MEASURED (turns 1264-1266, 925 calls) — HYPOTHESIS WRONG.** The named sub-scopes I bet on are
NEGLIGIBLE: of 80,311 ms total CABV, the instrumented five summed to only **1,374 ms (1.7%)** —
commerceVal 883, sea 233, commerceYields 138, **notdev 106, defense 14**. So the O(B×U) loops
(`.NotDeveloping` 106 ms, `.Sea` 233 ms) are **~0.4%** — the reverse-prereq-index plan would save
almost nothing. **~98% of the cost is elsewhere**: the `PreLoop` (O(buildings²), once/call), the
untagged dimensions (`.Happy`/`.Health`/`.Experience`/`.Maintenance`/`.Specialist`/`.Food`), and
inline per-building code. Lesson: measure before refactoring — the "obvious" big-O loops weren't
the cost.

**Now fully decomposed (IN, 2nd build):** added PERF_ACCUM to PreLoop, the whole `.building` body,
and every remaining named dimension. New line:
`[PERF/cabv] owner= flags= preloop= building= defense= happy= health= exp= notdev= sea= maint=
spec= commerceYields= commerceVal= food=`. `building` = per-building total (overlaps the named
dims inside it); `preloop` = once/call setup. `preloop + building ≈ CABV total`; if the named
dims sum << `building`, the cost is untagged inline per-building code → instrument those chunks
next. Aggregate with the matching awk over all `preloop=|building=|…|food=` fields.

**ROOT CAUSE CONFIRMED (full decomposition, turns 1264-1267, 1428 calls): the `PreLoop` is 94%.**
`preloop = 114,208 ms` vs the whole per-building loop `building = 6,806 ms` (every dimension tiny:
exp 545, commerceVal 1350, sea 360, happy 869, notdev 172, …). The PreLoop (`CvCityAI.cpp:12583-
12630`) builds `buildingsToCalculate` — constructible buildings PLUS buildings that would become
constructible if an "enabler" were built — via an **O(enablers × buildings)** sweep that runs
`BoolExpr::evaluateChange(pObject, queries)` (construct condition + GOM override, 12621) per pair.
~80 ms/call. **Crucially it does NOT depend on `iFocusFlags`, yet reruns in full on every CABV
call** (multiple/city/turn) producing the identical set — pure redundant waste. NOT promotions
(`exp`=545 ms), NOT the O(B×U) scans, NOT per-building. Three "obvious" structural bets were all
wrong; only measurement found it.

**Fix tiers (safest first):**
1. **Memoize `buildingsToCalculate` on the per-city `BuildingValueCache`** — compute once per
   cache-lifetime, not per CABV call. No new staleness (same rebuild schedule as today; flushed by
   `setHasBuilding`), no loop risk (it's an input set, not a control-flow value). ~2-3× off PreLoop.
   **IMPLEMENTED + MEASURED — 3.6× on CABV.** Added `m_buildingsToCalculate`/
   `m_buildingsToCalculateValid` to `BuildingValueCache`; PreLoop runs only when invalid;
   `AI_FlushBuildingValueCache(true)` (retain) also clears the flag (enabler-building changes rebuild
   the set). Result (4-turn runs, before 1264-67 → after 1268-71): `preloop` 114,208 → **24,930 ms
   (4.6×)**; `CalculateAllBuildingValues` ~121k → **33,319 ms (3.6×)**; per-call preloop 80 → **15.6
   ms (5.1×)**; `CvPlayer::doTurn` avg/player ~3558 → **1337 ms**. Confirms the PreLoop was rerun ~5×
   per cache redundantly. `preloop` is still ~75% of (the now-much-smaller) CABV — runs once per
   cache-build (≈ once/city/turn). Further gains = Tier 2 (cross-turn retain, event-driven) or Tier 3
   (prune BoolExpr evals).
2. **Live-update repository (the owner's idea), first datum = the constructible set.** It changes
   only on building-built / tech-acquired / bonus-gained → compute once, live-update on those
   endpoints → PreLoop ~0 across turns. Safe (advisory input set, recomputable).
3. **Prune the BoolExpr evals** via a static index of which buildings' construct conditions
   reference building X / its free bonuses, so only relevant pairs are evaluated.

**(Superseded) earlier idea — precompute static reverse-prereq indices** (kept for reference;
measurement shows it's NOT the win): (load/first-use; same for all
cities/players, whole game): `building → [units it's a prereq for]` (replaces 13061 & 13769),
`building → [buildings it's a prereq for]` (replaces 13812), and a static `[sea unit types]`
list (narrows 13229). Each loop then iterates the *few* related entries instead of all ~1000;
dynamic checks (`canTrain`/`isHasTech`/`AI_totalAreaUnitAIs`) and the `evaluateChange` only run
for relevant entries. O(B×U)/O(B²) → O(B × few). Confirm the split first with a `[PERF]` scope on
those three blocks if desired.

## Three orthogonal optimization tracks (they stack)
- **(a) Call it less** — queue multiple buildings and only re-decide `AI_chooseProduction` on
  events (owner's idea), plus the periodic cache refresh already shipped. Attacks frequency.
- **(b) Make each call cheaper** — the reverse-prereq indices above. Attacks per-call cost.
- **(c) Stop thrashing** — owner reports AI cities flip-flop half-finished buildings (wasted
  hammers). Measurable now via `[CIT/cancel] progressLost` in CityAI.log. Own fix; improves AI
  quality too.

## Cross-references
- Memory: `ai-unit-movement-to-player-level`, `hunter-move-reinvocation`,
  `property-control-oscillation`, `sea-ai-rework`, `worker-escort-stall-mechanism`.
- Reference docs: `CvPathGenerator.md`, `pathfinding.md`, `CvPropertySolver.md`,
  `CvPlayerAI.md`, `CvSelectionGroupAI.md`.
