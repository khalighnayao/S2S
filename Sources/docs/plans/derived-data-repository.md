# Derived-data repository — architecture plan

**Four drivers, served by one pattern** (they reinforce each other, not compete):

1. **Only (re)calculate what changed** — stop wiping whole caches every turn and rebuilding from
   scratch. (The worst example, the building-value `PreLoop`, was 94% of
   `CalculateAllBuildingValues` — it rebuilt a *flag-independent* set on every call though its result
   never changed. Memoizing it already cut CABV 3.6×.)
2. **One API surface (reusability)** — developers ask a getter (`getBuildingValue`,
   `getConstructibleSet`, `getUnitsEnabledBy`, …) instead of re-deriving; new code reuses tested
   logic instead of reinventing it (and its bugs).
3. **De-duplicate** — the same "scan all building/unit types to re-derive a prereq/constructible
   relationship" idiom is hand-rolled in many places (CABV `PreLoop`, `.NotDeveloping`, the religious
   loop, the O(B²) "needed for other buildings" loop, and across `CvCityAI`/`CvCity`/`CvPlayer`). A
   repository getter computed once replaces them all; the duplicated loops get deleted.
4. **Shrink the monster files** — moving these derivations out of the ~14k-line `CvCityAI.cpp` (and
   the equally huge `CvUnitAI.cpp`/`CvPlayerAI.cpp`) into focused repository classes is a direct
   file-size win.

The same Game-level reverse-index that makes the PreLoop fast (#1) is the shared API (#2), lets us
delete the duplicated scans (#3), and pulls logic out of the giant files (#4).

**Scope of the idea:** a *uniform, simple-to-use* pattern that ALL these derived data sources adopt
— building values, the constructible-building set, the prereq/enabler indices, tech/civic/
promotion/bonus values, needed counts, area unit-AI counts, etc. Incremental "live update" of a
value (apply a delta rather than recompute) is a per-datum *bonus*; the requirement is
**change-driven recompute behind a shared getter**.

> **Sequencing caveat (file-size vs risk).** There are two kinds of file-size work and they carry
> very different risk: **(a) pure mechanical splitting** — moving functions out of the monster .cpp
> into more translation units with *zero behaviour change* (safe, fast, no repository needed); and
> **(b) repository extraction** — the de-dup/reusability program here, which *touches AI behaviour*
> and is correctness-sensitive. Given the recent stale-cache hang, **attack file size first with (a)
> the mechanical split**, and land the repository extraction (b) deliberately afterward with the
> debug-verify backstop (§5). The constructible-set memoization already banked the big safe perf
> win, so there is no urgency to rush the behaviour-sensitive parts.

---

## 0. Status

- **Skeleton IMPLEMENTED** (`Sources/CvDerivedData.h`): the `TLazy<T>` holder, the templated
  `CvDataRepository<TOwner>` base (typed owner back-pointer + `reset()`/`invalidateAll()` hooks),
  and the four empty level repositories — `CvGameDataRepository`, `CvTeamDataRepository`,
  `CvPlayerDataRepository`, `CvCityDataRepository`. Each AI level (`CvGameAI`/`CvTeamAI`/
  `CvPlayerAI`/`CvCityAI`) owns one (`m_dataRepository`), exposes `AI_dataRepository()`, and wires
  its back-pointer via `init(this)` in its constructor. Header-only; Assert build clean.
  **No data yet** — added case by case per §6.
- **Related perf fix already shipped** (independent of the repository): the constructible-set
  memoization on the per-city `BuildingValueCache` (§6 step 1's trivial first step) — CABV 3.6×. See
  [`turn-time-optimization.md`](turn-time-optimization.md).

---

## 1. Why now — the evidence

- **Measured:** `CalculateAllBuildingValues` is the dominant late-game turn cost, and its `PreLoop`
  (`CvCityAI.cpp:12583-12630`) is 94% of it — recomputing a *flag-independent* set on every call.
  See [`turn-time-optimization.md`](turn-time-optimization.md).
- **The pattern is everywhere, done the brute-force way:** `CvPlayerAI::AI_doTurnPre` wipes
  `m_cachedTechValues`, `m_aiCivicValueCache`, `m_numBuildingsNeeded`, `m_missionTargetCache`,
  `plotDangerCache`, and re-runs `AI_updateBonusValue()` — **every turn, unconditionally**. That is a
  degenerate repository with the dumbest possible invalidation ("invalidate all, always").
- So this isn't a new system — it's **formalizing the existing per-player caches into one pattern
  and replacing "clear everything every turn" with "invalidate only what an event touched."**

---

## 2. Core abstraction (C++03, fits the codebase)

A derived datum = **value + dirty flag + a recompute method + the events that dirty it.** Keep it
lightweight: a member on its owner, accessed through a getter that lazily recomputes.

```cpp
// Lightweight lazy/dirty holder. No registry required to start.
template <typename T>
class TLazy
{
public:
    TLazy() : m_bDirty(true) {}
    bool  dirty()  const { return m_bDirty; }
    void  invalidate()   { m_bDirty = true; }
    const T& value() const { return m_value; }          // assumes !dirty()
    void  set(const T& v) { m_value = v; m_bDirty = false; }
private:
    T    m_value;
    bool m_bDirty;
};
```

Owner exposes a typed getter that hides the dirty-check + recompute. This is the **simple-to-use**
surface — replicating it for a new datum is three lines:

```cpp
// in CvCityAI
const std::set<BuildingTypes>& AI_constructibleSet() const
{
    if (m_constructibleSet.dirty())
        m_constructibleSet.set(computeConstructibleSet());   // the old PreLoop body
    return m_constructibleSet.value();
}
```

For data that benefits from *incremental* update (counts/sums), the same holder can be updated in
place by an event endpoint instead of marked dirty — the **bonus** path:

```cpp
void CvPlayerAI::onBuildingBuilt(BuildingTypes b) { m_buildingCount[b] += 1; /* no recompute */ }
```

> No virtual-registry/function-pointer machinery is required to start. A central registry
> (`enum DatumId` + subscription map) is a later convenience once many data sources exist; the
> `TLazy<T>` member + getter + event-method idiom already delivers "only recompute what changed."

---

## 3. Scopes & ownership — four levels: Game, Team, Player, City

The repository exists at **four levels**, each datum living at the **highest level where its inputs
are invariant**, so shared work is done once for everything below it. A lower level's getter reads
the level above (City → Player → Team → Game).

| Level | Owner | Holds | Invalidated by |
|---|---|---|---|
| **Game** | `CvGame` / `CvGlobals` | Static XML-derived data: prereq reverse-indices (building→units/buildings), the `EnablesOtherBuildings` graph, unit/building classifications. Built **once at load** and never invalidated. | never |
| **Team** | `CvTeamAI` | Team-shared facts: known/obsolete tech sets, tech-derived unit/building availability, war state, shared vision-derived data | tech acquired/obsoleted, war/peace |
| **Player** | `CvPlayerAI` | tech value, civic value, promotion value, bonus value, building counts, area unit-AI counts, needed-building counts | civic/trait/strategy change, unit/building count change |
| **City** | `CvCityAI` | **constructible-building set (PreLoop output)**, building values, best-build, **the city's declared needs (plots/yields it wants)**, workers-needed | building built/lost, tech, bonus, pop/plot change |

Each datum is placed at the level matching what actually moves it: a tech fact at Team (one
recompute serves every city on the team), a civic value at Player, the constructible set at City.

---

## 4. Invalidation model — "only what changed"

Two pieces: **event endpoints** (what happened) and **subscriptions** (which data each event dirties).

### Event endpoints (called from the existing mutation sites)
A small, explicit set — the "live-update endpoints":

| Endpoint | Called from (mutation site) | Dirties (examples) |
|---|---|---|
| `onTechAcquired(team, tech)` | `CvTeam::setHasTech` | team tech sets; player tech/unit/building values; every city's constructible set |
| `onBuildingChanged(city, b, added)` | `CvCity::setHasBuilding` | that city's constructible set + building values; player building counts |
| `onCivicChanged(player)` | `CvPlayer::setCivics` | player civic value; building values (modifiers) |
| `onBonusChanged(city, bonus)` | bonus accrual/loss | that city's constructible set + building/yield values |
| `onUnitChanged(player, unit)` | unit create/kill/promote/AI-reassign | area unit-AI counts; unit/promotion values |
| `onCityFoundedRazed / onPopulationChanged` | found/raze/grow | counts; city-scoped values |

Each endpoint calls `invalidate()` on the specific `TLazy<>` members it affects (explicit and
readable), or applies an incremental delta for the bonus path.

### Strategy per datum
- **Default = dirty + lazy full recompute.** Simplest, covers everything, "only recompute what
  changed" at whole-datum granularity. Use unless profiling says otherwise.
- **Incremental delta = opt-in.** Only for cheap aggregates (counts/sums) where the delta is
  obviously correct. Higher bug risk; reserve for proven-hot aggregates.

---

## 5. Safety & correctness (the hard-won rules)

These are non-negotiable given the production-cache hang we already hit:

1. **Advisory data only.** The repository holds *derived AI heuristics*, never synced game state. A
   stale advisory number → a slightly worse decision, never a desync.
2. **Never feed a repository value into control flow that can loop.** The building-*value* cache
   hung the game because stale values looped `AI_chooseProduction`. The first target here (the
   constructible *set*) is an input set, not a loop-driving value — safe. Vet each datum for this.
3. **Determinism / MP-OOS.** Derived-only, rebuilt identically on every machine. Must never become
   an input to synced state.
4. **Save/load.** Don't serialize as truth — mark everything dirty on load and rebuild lazily.
5. **Bounded-staleness backstop.** Keep a coarse safety net (e.g. a once-per-N-turns full
   invalidate, or invalidate-on-load) so a *missed* hook can only cause bounded staleness, never
   permanent wrong data. Because the data is advisory (rule #1), a bounded backstop is acceptable —
   unlike the value-cache, this can never loop.
6. **Optional debug-verify mode.** Behind a define, recompute a datum and assert it equals the
   cached value, to catch missing invalidation hooks early (mirrors the visibility debug-count
   technique). Cheap insurance during migration.

---

## 6. Migration plan (incremental, measured)

1. **Prove the pattern on the measured #1 cost — the constructible set (PreLoop).** Move the
   PreLoop body into `CvCityAI::computeConstructibleSet()` behind a `TLazy<std::set<BuildingTypes>>`
   on the city. Invalidate from `onBuildingChanged`/`onTechAcquired`/`onBonusChanged`. Backstop:
   invalidate when the existing per-turn `BuildingValueCache` is (re)built. Measure `preloop` drop.
   - *Trivial first step — DONE (shipped, 3.6× CABV):* memoized the set on the existing per-city
     `BuildingValueCache` so it's computed once per cache-lifetime instead of once per CABV call —
     redundant within-turn reruns gone, zero new staleness. The cross-turn event-driven version
     (this step proper, behind the repository) is the remaining work.
2. **Absorb the `AI_doTurnPre` per-turn clears one at a time.** Convert each blanket `.clear()`
   (tech value, civic value, needed-building counts, bonus value) into event-driven invalidation.
   Acceptance: `AI_doTurnPre` no longer unconditionally wipes that datum.
3. **Add the global/static tier** (prereq indices, `EnablesOtherBuildings` graph) computed once at
   load — these never change and several hot loops re-derive them.
4. **Introduce the central registry** (`enum DatumId` + event→ids map) only once enough data sources
   exist that the per-member idiom is repetitive — a convenience layer, not a prerequisite.

Each step is independently measurable and revertible; nothing is migrated until profiling shows it
matters (and is recompute-bound rather than wasteful work better *removed* than cached).

---

## 7. Data-source inventory (candidates to absorb)

Per-city: constructible set (PreLoop), building values, best-build, workers-needed, yield/commerce
base rates. Per-player: tech value (`m_cachedTechValues`), civic value (`m_aiCivicValueCache`),
needed buildings (`m_numBuildingsNeeded`), bonus value (`AI_updateBonusValue`), promotion value
(`AI_promotionValue`), area unit-AI counts, mission targets (`m_missionTargetCache`), plot danger
(`plotDangerCache`). Per-team: tech/obsolete sets. Global: prereq reverse-indices, enabler graph.

---

## 8. What this unlocks beyond perf — city-declared needs (worker AI)

The City-level repository is the natural home for **a city's declared needs** — the plots/yields it
wants (food when starving, production, a specific bonus, etc.). That is itself a derived datum:
computed from the city's state, change-driven (re-derive when pop/plots/buildings change), and read
by others rather than recomputed by each consumer.

This moves us toward the existing **city-driven worker valuation** strategy (memory
`city-driven-worker-valuation`): instead of a worker being *linked to* a city and pulling its
to-do list from that one city, **cities publish their needs into the City-level repository, and the
per-player worker AI (`CvWorkerAI`) reads across all cities' published needs** to decide what to
improve and where. The repository decouples "what a city wants" (city level, computed once, cached)
from "which worker does it" (player level, the existing claim ledger), and removes the per-worker,
per-city recomputation of need.

So the same pattern that fixes the building-value `PreLoop` cost also provides the substrate for
city-needs-driven worker/improvement valuation — one mechanism, two payoffs (speed + better AI
coordination). It also aligns with the north-star of lifting unit decisions from per-`CvUnit` to
per-`CvPlayer` orchestration (`ai-unit-movement-to-player-level`).

## 9. Open decisions

- Per-member `TLazy<T>` idiom vs a central registry from day one — recommend idiom first, registry
  later.
- How precise to make subscriptions (per-datum events vs coarse scope invalidation) — start
  per-datum where cheap, coarse where the dependency is broad.
- Whether to keep the per-turn `BuildingValueCache` delete as the backstop or move fully
  event-driven (the delete is what makes it safe today; keep until event hooks are proven complete).

## Cross-references
- [`turn-time-optimization.md`](turn-time-optimization.md) — the measurements that motivate this.
- Memory: `turn-time-optimization`, `city-driven-worker-valuation`, `ai-unit-movement-to-player-level`.
