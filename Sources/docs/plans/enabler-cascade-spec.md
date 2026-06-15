# Enabler / Disabler — formal spec (DRAFT v0.3, 2026-06-15)

**Status: BASELINE — owner-accepted 2026-06-15.** This is the **uniform surface** we build #430 on: settled enough
to implement against, with future hole-poking expected but the foundation fixed. Formalizes the *availability* half
of the #428 cascade. Supersedes v0.1 (which mis-modelled the enabler as a modifier-style push-cascade) and v0.2
(which over-loaded `requires`).

- **v0.2's correction (owner, 2026-06-14):** enablers are a TWO-STEP process; modifiers are one-step. We had
  been treating them the same — that was the bug.
- **v0.3's correction (owner, 2026-06-15) — ONE `requires` gate + the BUILD/OPERATE (dormancy) insight.** Two
  refinements landed this session, in order:
  1. *(explored, then folded)* A sticky/fluid analysis suggested splitting the gate into a structural part and a
     fluid part. **Withdrawn:** there is **ONE `requires` object**, a complex BoolExpr (`all`/`any`/`none`) over
     **every** prereq kind (tech, civic, religion, resource, bonus, building). The fragmentation we were modelling
     (`enabledWhen` vs `requires`, the sticky/fluid divide) is just an artifact of the legacy prereq zoo —
     PrereqTech / PrereqAndBonus / PrereqVicinityBonus / PrereqCivic / Obsolete* / the disabled-building chain,
     decades of ad-hoc authoring. Collapsing it into one uniform gate is the point.
  2. *(kept — the real contribution)* **`requires` is what it takes to BUILD *and* to keep OPERATING.** It is
     re-evaluated at build time (greying) **and continuously on already-built things** (dormancy): lose a required
     civic / resource / religion and the building goes **dormant — disabled, not demolished** — and wakes again
     when the requirement returns. Civic and religion are *fluid* (revolution swaps a civic; inquisition removes a
     religion), so this dormancy is general, not resource-only.
  - **Why it matters:** the replacement for the ~42 scattered `canBuild`/`canConstruct`/`canTrain` checks is, BY
    DESIGN, applied to a far smaller list — the generated candidate frontier — never "every building/unit in the
    game" at any moment. The small-list property is the whole point, not an optimization bolted on (§2, §4, §14).
  - **Sticky vs fluid is NOT model structure** — it survives only as an optional recompute-*cadence* hint (tech
    rarely changes; resources/civics flip often), which §2 treats as a separate optimization layer.

Synthesis of the owner design sessions 2026-06-14/15 + the enabler rulings in `building-cascade-conversion.md` +
`store.PREREQ_FIELDS`/`OBSOLETE_FIELDS` + the `formalize-enabler-vocabulary` workflow (verb/scope/composition,
with the verify-pass data-shape corrections). Formalize this BEFORE the heavy entities; then retrofit the ~21
already-converted infos.

---

## 0. Scope — model the ACTION, not the OUTCOME

The cascade answers one question: **"can I take this action now?"** — build a unit/building/wonder, lay an
improvement (via a build), research a tech, adopt a civic, found a religion/corporation. It decides what is
**OFFERED**.

It does NOT model what the action *produces*. A build yields either an **independent OUTCOME** — a **unit** (off
to the map) or an **improvement** (on a tile), each with its own lifecycle that **exits this model** — or
persistent **CITY STATE**, a **building** (becomes part of the city; stays in the model). Everything about a
produced instance's fate (a unit surviving, a building demolished, a wonder keeping tourism after its effect
obsoletes, `IMPROVEMENT_COTTAGE → HAMLET` upgrading) is the **outcome/instance-lifecycle system — a deferred
neighbor, explicitly out of scope.**

So the cascade's working state is the **STATE** entities — buildings + techs + civics + religions + bonuses…
(the things you *have* that gate further actions). **Units & improvements are leaf actions**; the cascade only
decides whether to offer them.

---

## 1. THE headline — enablers are 2-step (GENERATE then GATE); modifiers are 1-step

- **A modifier is a LOCAL deposit.** Each source drops its effect on the target; the target sums. Order-free,
  commutative, **one pass**. You never need the whole picture — every deposit is independent.
- **Availability is GENERATE-then-GATE — TWO passes.** It cannot fold-as-you-go: the candidate set must be
  *generated* before any per-candidate gate can run.

Trying to run the enabler like the modifier (one-pass deposit) is what produced all the push-cascade machinery
(deposit-on-event, "disable before enable", per-turn reconciliation, chain-walking). None of it is needed.

**`enables` and `requires` COEXIST — two aspects that both apply, not two homes for one edge (owner, 2026-06-15).**
A target (a building, say) is positioned in the permanent possibility space by its `enables` relationships AND
reversibly gated by its `requires` conditions — *both at once*. **The classifier is the NATURE of the action, not
the atom kind:**
- **`enables` — PERMANENT / irreversible actions** (forward, on the source): **hard-constructive** (research a
  tech, build a prereq → permanently makes things possible) and **destructive** (obsoletion / replacement →
  permanently makes things impossible). This is the GENERATOR: the union of `enables` over HAS, **minus** the
  destroyed, is the candidate frontier CAN GET — bounded by HAS, read **FORWARD** (no reverse lookup). **Tech is
  authored HERE, never in `requires`** — putting it on the target would force a tech→targets reverse index (the
  reverse-lookup clusterfuck).
- **`requires` — REVERSIBLE conditions** (forward, on the target): **enabling / disabling** — civic adopted /
  resource connected / religion present → buildable & operating; revolted / cut / inquisited → greyed & dormant.
  A complex BoolExpr (`all`/`any`/`none`) over the reversible kinds (civic, religion, resource/bonus, reversible
  building states). Gates the generated candidates AND re-checks built things for dormancy (§3). Checked forward
  (set-membership against HAS). `enabledWhen` and the sticky/fluid divide were legacy artifacts — collapsed here.

So: **destructive or hard-constructive → `enables`; reversible enable/disable → `requires`** — and the two coexist
on the same entity.

The two passes: **(1) GENERATE candidates via `enables` minus destroyed (bounded by HAS); (2) GATE each by its
reversible `requires`.** `available(X) = X ∈ generated ∧ X.requires ⊆ HAS`. Step 1 bounds the set to what you could
reach, so step 2's per-candidate gate runs over a **small list**, never the whole database — the whole point
(§4, §14). Both reads are FORWARD — `enables` forward from the source, `requires` forward from the target — so the
hot path never inverts; reverse indices are cold-path (pedia) only.

---

## 2. The three terms — HAS, CAN GET, HAS THE MEANS TO (owner framing, 2026-06-15)

The whole model reduces to three sets, computed in order each recompute:

1. **HAS** — what you currently possess: tech, civics, religion, connected resources/bonuses, built buildings.
   Gathered in pass 1.
2. **CAN GET** — the candidate frontier: everything HAS unlocks via `enables`. "Everything we can have." Generated
   in pass 2a — cheap, bounded by HAS (the union over-produces; the gate confirms).
3. **HAS THE MEANS TO** — of the CAN-GET candidates, the ones whose `requires` is satisfied right now. Verified in
   pass 2b against the one `requires` gate.

The build list falls straight out: **CAN GET but `requires` unmet → greyed/hidden** (per the failing clause's
disposition, §4); **CAN GET ∧ `requires` met → buildable**; not in CAN GET → hidden. And the same `requires`
check, re-run on things you already HAVE, is the **operational/dormancy ledger** — a built Forge that loses its
iron, or whose required civic is revolted away, fails `requires` and goes dormant (§3).

### The two passes that produce them

**PASS 1 — gather HAS.** Traverse **right-then-down** (§9) and collect everything you currently possess, at each
scope (§8). Output: the scoped HAS sets.

**PASS 2 — generate (CAN GET), then verify (HAS THE MEANS TO):**
- **GENERATE — CAN GET (cheap, from HAS):** for each thing you HAVE, look up what it unlocks via `enables` → union
  = the candidate set. This is the **derived `enables`/reverse index**'s hot-path job. **Bounded by HAS** — you
  never scan all ~3869 buildings, only what HAS could reach. The union *over-produces* (it proposes X from one of
  X's prereqs even if X needs others) — a candidate net, not a gate.
- **VERIFY — HAS THE MEANS TO (per candidate, the ONE gate):** each candidate checks its own **`requires ⊆ HAS`** —
  the full BoolExpr: AND (`all`) across every prereq kind, OR (`any`) groups, and NOT (`none`) for obsoletion /
  replacement. This is where the AND/OR/NOT and the over-production filter live, authoritatively, on the target.
  Candidates that pass are buildable; the rest grey or hide (§4).

Why both, why neither alone: **generation** keeps the work bounded by HAS (cheap union); **`requires`** is the
authoritative gate (the AND/OR/NOT and the negatives the union can't express). Neither alone suffices — the union
over-produces, and the gate alone would have to run over the whole DB.

**Cost model — this IS the optimization, not overhead.** Both steps are bounded by HAS, never the ~5000-entity
database. Generation considers only candidates reachable from HAS; the `requires` check runs only on that frontier.
So total cost is proportional to *what you could plausibly get*, not *everything that exists* — strictly cheaper
than re-checking every building/unit/tech each recompute (the old O(all-entities) PreLoop). This is *why* the
`canBuild`/`canConstruct`/`canTrain` replacement works on a far smaller list by design (§14).

**Sticky vs fluid → an optional CADENCE hint only (not model structure).** Atoms differ in how often they change:
tech is effectively monotonic (rarely recompute CAN GET), while resources/civics/religion flip often (re-run
`requires`). If ever measured worthwhile, a cache could regenerate CAN GET only on tech/building change and re-gate
on resource/civic change — but that is a wrapper layer (cache-agnostic, below), **never** a split in the data model.
The model has one `requires`.

**On-the-fly recompute is affordable — no caching needed (owner).** Today's engine already recomputes
availability over the *full* entity set constantly and has tolerated it since time immemorial; the bounded
two-pass is strictly *less* work. So recompute `have`/`canget` on demand — no aggressive caching, no incremental
deposits, no clever invalidation. Even a slightly-too-eager recompute beats the status quo. (This is why the §2
cadence can be "recompute on `have`-change" without engineering a cache around it.)

**The model is CACHE-AGNOSTIC (owner) — keep the decoupling.** The have/frontier computation is a **pure function
`have → frontier`**. If a cache + delta/incremental updates ever prove warranted, that is a **separate
optimization LAYER wrapped around** the pure function — it must never leak into the model. The model defines
*what* the frontier is; caching is only an orthogonal *how-fast*, added if measured and removed without touching
the model (the same stance the doc takes on the derived-data repository: "an optional, measured optimization added
after the engine, not foundational"). So the spec models *only* the pure two-pass; deltaing is out of its scope.

`have` (pass 1) and the frontier (pass 2) are the two lists. **Most of `have` flips turn-over-turn** (resources/
bonuses, civics on revolution, religion on spread/inquisition, buildings) — only **tech** is effectively
monotonic. But `have` is a cheap list to recompile, so this is a cadence note, not a model split (§2).

**Recompute cadence — the frontier is `f(have)`; recompute on `have`-CHANGE (commonly once/turn).** `have` usually
changes only at the turn boundary, so the DOMINANT cadence is once per turn, held for the turn — a single-item or
queued city reads that frontier, and a queue is a plan *over* it. **BUT same-turn `have`-changes do occur and MUST
trigger a mid-turn recompute** (owner — hole in the earlier "frozen per turn" claim): the **AI** processes
production mid-turn, so it can finish building A and then, the SAME turn, build B that depended on A (A's
completion lands in `have` immediately for the AI's next decision); a **human** can hit the same via the
**tamed-animal mission builds** edge case. So the rule is **recompute when `have` changes**, not "once per turn".
This stays cheap: it's a RECOMPUTE of the bounded two-pass over the *affected scope* (one city's `have`), not an
incremental deposit, and it fires only when `have` *actually* changes (rare mid-turn). Per-turn is the common
case, never an invariant. (The exact set of mid-turn `have`-change triggers should be enumerated — see §13.)

---

## 3. `requires` — "do I have the MEANS"; distinguishes one-time BUILD vs continuous OPERATE

**`requires` checks one thing — do I have the MEANS — and it must DISTINGUISH two timings (owner, 2026-06-15),
grounded in today's two building checkpoints:**
- **`requires.build`** — the **one-time** construction requirement (today's `canBuild` resources). Must hold to
  build; if missing the candidate is **GREYED** (with the resource named, §4). **NOT re-checked after** — once
  built, losing it does nothing.
- **`requires.operate`** — the **continuous** prerequisite (today's `PrereqBonuses`; `CvCity::isActiveBuilding`,
  CvCity.cpp:14364). Must hold to build AND to keep running; re-checked every recompute on a built thing — failing
  → **DORMANT** (disabled, not demolished; wakes when it returns, §11).

**Applicability — `requires.operate` is for PERSISTENT STATE only; units use `requires.build` alone (owner,
2026-06-15).** Per §0, units are *leaf actions* that exit the model once built — there is no ongoing operation to
gate — so a unit only ever carries `requires.build`. `requires.operate` (dormancy) applies to persistent city
state, i.e. buildings. (Future caveat: there has been lobbying to disable units on a missing input — e.g. tanks
with no fuel. The structure already supports it: if units ever gain operational state, that is just a
`requires.operate` on the unit — no model change. Not modelled now.)

Each part is a positive BoolExpr (`all` + `any` OR-groups) over the means kinds — **civic, religion,
resource/bonus**. Authored on the target, scope-tagged per clause:
```jsonc
"requires": {
  "build":   { "all": [ {"bonus":"BONUS_STONE","scope":"city","connection":"trade|vicinity"} ] },   // canBuild: greyed if missing, not re-checked
  "operate": { "all": [ {"civic":"CIVIC_GUILDS","scope":"empire"},
                        {"bonus":"BONUS_IRON","scope":"city","connection":"trade|vicinity"} ],       // PrereqBonuses: greyed at build + dormancy
               "any": [ [ {"bonus":"BONUS_COAL"}, {"bonus":"BONUS_OIL"} ] ] }
}
```
- **Build-time gate (greying) = `build` ∧ `operate`** — you need both present to construct; either missing → greyed
  with that resource named. **Operate-time gate (dormancy) = `operate` only** — a built thing re-checks just the
  continuous part. That is the whole distinction the object carries.
- **⛔ TECH IS NOT IN `requires` (either part).** Tech is a permanent CONSTRUCTIVE action → `enables` (the
  generator), read forward. A tech prereq on the target would force a tech→targets **reverse index** — the
  reverse-lookup clusterfuck. So `canBuild`'s STRUCTURAL part (tech, prereq buildings, obsolete/replace) feeds the
  **`enables` side** (generation; unmet → HIDDEN); only `canBuild`'s RESOURCE part goes in `requires.build`.
- **⛔ NO DESTRUCTION IN `requires`.** Obsoletion/replacement are DESTRUCTIVE → `enables`-side `disables`/`replaces`
  (§6), removed in generation. `requires` is positive-only; no `none`.
- **A clean per-case LEVER: grey (`requires`) or hide (`enables`) — and it is a UI question, not operational
  (owner, 2026-06-15).** Both placements work identically in the engine; the only difference is what the player
  sees, so authoring the resource on one side or the other is a **clean, per-case display choice**:
  - **`requires`** → candidate **GREYED** with the resource named (player can craft/trade for it — an active
    decision).
  - **`enables`** → candidate **HIDDEN** until the resource is present.
  The uniform structure gives this lever for free — go either way depending on the case. **General lean: grey on
  resources (→ `requires`)**, because surfacing "what to acquire" is usually what we want; flip a specific resource
  to `enables` when hiding is the better UX for that case. (§4.)
- **Positive only, checked FORWARD** — does HAS contain this atom? (set membership; no reverse lookup). Atoms carry
  `connection` (trade/vicinity) and a scope (§8).
- **Scope resolution → `requires` reads STATISTICS (owner, 2026-06-15; folds in §7/§8).** A **city**-scope means
  checks the local city HAS; an **empire/team/world**-scope means — presence (`∈ HAS` = `count ≥ 1`) AND count
  thresholds (`≥ N of X`, e.g. the old `PrereqNumOfBuildings`) — resolves against the **statistics module**, the
  additive roll-up of per-city "X is had" reports. So verify never crosses city isolation: it reads the local HAS
  for city clauses and statistics for higher scopes, uniformly. This is the same module wanted anyway for
  demographics/AI/score (§8).
- **Leaf atom** = `{<kind>: "TYPE", scope, connection?, disposition}` where `<kind>` ∈
  `civic | religion | bonus | …` (never `tech`). The `all`/`any` nodes nest to form the BoolExpr.

---

## 4. Greying — falls out of the gate (per-clause disposition), for free

The build-list tri-state is a byproduct of the same generate-then-gate pass — no separate "why greyed" computation.
The gate is per-clause, not pass/fail; each clause carries a **disposition** (`greyable` or `hiding`):
- **not in CAN GET** (generation didn't reach it) → **HIDDEN**.
- **in CAN GET ∧ all `requires` clauses met → LISTED** (buildable).
- **in CAN GET ∧ only `greyable` clauses unmet** (a missing connectable resource, an unadopted civic) → **GREYED**
  ("needs `BONUS_COPPER`" / "needs `CIVIC_GUILDS`").
- **in CAN GET ∧ a `hiding` clause unmet** (wrong era, obsoleted via a `none` clause) → **HIDDEN**.

Disposition is a property of the clause kind, set once: resource/bonus/civic that the player can go acquire →
`greyable` ("you could get this"); era/obsolete/structural-impossible → `hiding`. So the same `requires` evaluation
that gates buildability also yields *why* it's greyed/hidden — the tri-state the migration doc wanted, emitted by
the gate itself.

**The frontier is a SHARED choice set — UI *and* AI.** Computed once per recompute, read by both: the UI greys
from it (above), and the **AI production decision** (`doProduction` / `AI_bestCityBuild`) iterates **only the known
frontier** — what the city can actually build now — instead of filtering and scoring the whole ~5000-entity
database every decision. The cascade has already answered "can I build this"; the AI only does the "should I"
scoring, over a small set. That makes `doProduction` (a documented turn-time hotspot) dramatically cheaper —
arguably the biggest systemic payoff of the two-pass model, beyond UI greying.

**ONE trigger, not 42 (owner).** Today availability is recomputed ad-hoc at ~42 discrete call sites
(`canConstruct`/`canTrain`/prereq checks scattered across CvCity/CvPlayer/CvUnit/CvGameTextMgr/AI), each with its
own slightly-divergent logic — a correctness *and* maintenance hazard. The cascade triggers the recompute in **one
place**; everyone **reads the shared frontier**. So: one source of truth (the 42 can no longer silently disagree),
one place to change the rule, compute-once-read-many instead of 42 redundant scans. This is the consolidation
win — and it's why "recompute on the fly" is fine: the *single* recompute replaces dozens of scattered ones.

---

## 5. Base relationship objects — the `enables` family (PERMANENT) + `requires` (REVERSIBLE)

**Authored schema (owner): any item can carry these base relationship sections — `enables`, `disables`,
`replaces`, `requires` (and more to come).** They sort by the **nature of the action** (§1), into two cascades that
COEXIST on the same entity:

- **The `enables` family — PERMANENT actions, forward on the source, drives GENERATION:**
  - **`enables`** — a **constructive** action: what this entity permanently unlocks (`enables.{buildings,units,
    builds,techs,…}`). Tech lives here. Read forward over HAS → the candidate union.
  - **`disables`** (≈ `obsoletes`) and **`replaces`** — **destructive** actions: what this entity permanently
    removes (`disables`: obsoletion; `replaces`: a successor destroying its predecessor). Read forward → SUBTRACTED
    from the candidate union. (§6.)
  - So **CAN GET = union(`enables` over HAS) − (things `disables`/`replaces` over HAS)** — generation is the whole
    permanent family, read forward, bounded by HAS. No reverse lookup.
- **`requires` — REVERSIBLE conditions, forward on the target, drives the GATE:** "do I have the means" (§3).
  Positive civic/religion/resource conditions; checked forward per candidate against HAS.

**Each EDGE is authored ONCE, on its natural end** — `enables`/`disables`/`replaces` on the source (the actor),
`requires` on the target (the thing needing means). **The reverse view of each is derived at load and KEPT, but is
COLD-PATH only** — it powers the pedia / web-Civilopedia ("Unlocks", "Obsoletes", "Required by"), and it is **never
read on the hot path**: generation reads `enables`/`disables`/`replaces` forward; the gate reads `requires` forward.
This is the answer to the reverse-lookup concern (owner, 2026-06-15) — inversion happens once at load for the pedia,
never per-recompute.

**No single "`requires` ⇄ `enables`" mirror.** They are *different relationships* — `enables` = a permanent unlock
(tech→building), `requires` = a reversible means (building→resource) — not two ends of one edge. (An interim draft
that paired them, and an earlier one that put tech/obsoletion in `requires`, are both withdrawn; §1.) An item
commonly carries several of these objects at once: a building `enables` the units it unlocks, may `disables`/
`replaces` a predecessor, and `requires` the resources/civics it needs to operate.

---

## 6. Source-side base objects (`disables`, `replaces`) + auxiliary sections

`disables` and `replaces` are the two **DESTRUCTIVE members of the `enables` family** (§5) — permanent actions,
forward on the source, **processed during generation** (they SUBTRACT from CAN GET). They are **not** `requires`
clauses (`requires` is positive means only, §3). The rest below are **auxiliary** (provisioning / config /
identity — not availability edges). Shapes verified on disk:
- **`disables` (base object; ≈ `obsoletes`)** — the ANTI-ENABLE, per-type sections mirroring `enables`:
  `disables:{buildings:[],units:[],builds:[],bonuses:[],techs:[],…}`. **Obsoletion is the prime scenario**
  (`tech.disables.buildings` = the old `ObsoleteTech`); the same shape carries others — per-civ research ban
  (`civilization.disables.techs`, an OVERRIDE), and more. Source-authored; read forward in generation to remove the
  named targets from CAN GET.
- **`replaces` (base object; self-framing)** — per-type successor sections mirroring `enables`/`disables`:
  `replaces:{buildings:[BUILDING_X],…}` ("I take X's slot"; conditional form carries `onGameOption`, load-stable) —
  destructive (it destroys the predecessor), same reason as `disables`. Source-authored; the **transitive successor
  chain** (`A→B→C` ⇒ A removed once any successor is present) is precomputed once at load and applied in generation.
  **Unit `upgradesTo` is NOT `replaces`** — it is constructive/MANUAL (an *available action*; the old unit isn't
  auto-removed), so it lives under `succession` (below).
- **`grants`** *(auxiliary)* — one-shot event provision (game start, wonder completion); does not sum/propagate.
  `grants:{buildings:[],civics:[],techs:[],units:[],specialists:[],freePromotions:{PROMOTION_X:[UNITCOMBAT_Y]}}`
  (`freePromotions` keyed by PROMOTION → list of UnitCombats). Sources: Civilization, Trait, Project, Religion,
  SpecialBuilding, Heritage.
- **`loadPrune`** *(auxiliary)* — load-stable game-option gate (§10). `loadPrune:{onGameOptions:[],notOnGameOptions:[]}`
  (241 files). If false at load, the entity/channel is **not materialized** — never enters `have` or the candidate
  set. Sibling of `disables` (load-time data removal vs live query).
- **`policies`** *(auxiliary)* — civ meta-capability (non-cascade structural).
  `policies:{playable,aiPlayable,stronglyRestricted}` (128 civ files). Who may play / AI build-lockdown.
- **`succession` / `excludes`** *(auxiliary, structural)* — `succession:{upgradesTo:["UNIT_X"],promotionLine:"…"}`
  (manual unit upgrade + promotion-line link); `excludes:["TRAIT_X"]` (bare-array same-tier mutual exclusion,
  Trait `DisallowedTraitTypes`).
- **EXPLORATORY (not in data):** civ-capability `allows`/`unique` — `allows` is a forward enabler edge growing the
  same generator. Grow into; not data now.

---

## 7. Under-modeled: COUNT / THRESHOLD / WAIVE (flagged)
`specialBuildingsWaived` (lift a SpecialBuilding per-player group CAP), `PrereqNumOfBuildings`/`PrereqAmountBuildings`
(≥N of X), `ProjectInfo PrereqProjects/iNeeded` (N of Y). These are **count thresholds**, not presence atoms — a
clause like `{"countOf":"BUILDING_X", "min":N}` or `{"waivesCap":"SPECIALBUILDING_Y"}`. A set-based `have` can't
answer counts directly. The `have` COUNT of buildings lives in the isolated CITY, so an empire/team-wide "≥N of X
across my cities" can't be answered from a single city's `have`. **RESOLVED (owner): a separate STATISTICS
module.** It lives OUTSIDE the have-builder. As the have-builder gathers each city's `have`, it **reports to
statistics that "X is had, somewhere"**; statistics aggregates the empire/team-wide counts. A count-threshold
clause (`{countOf:"BUILDING_X","min":N,"scope":"empire"}`) reads the **statistics** count, not a city's `have`.
The have-builder stays per-city and isolated — it only emits a report side-channel; the cross-city aggregate lives
in the separate module, which **we want anyway** (demographics, AI strategy, score). Same decoupling as caching
(§2): a sibling consumer of the have-builder's output, never a complication of the model.

---

## 8. Scopes — `have` is SCOPED sets, matched per clause
`world → team → empire → area → city`. `have` is not one global set; each clause names the scope it consults.
**The scope also tells you which step the lookup feeds** — the `enables`-family lookups (tech presence/
obsolescence, building progression) feed **generation** (what's in CAN GET); the `requires` lookups (civic,
religion, connectable bonuses) feed the **gate**.
- **team** — tech presence + tech-obsolescence → generation; **trade-connected bonus presence** → `requires`.
- **empire** — civic / religion / corporation → `requires` (reversible means); per-civ `disables` → generation.
- **area** — VICINITY bonus presence (`hasVicinityBonus`) → `requires`.
- **city** — in-city buildings → generation/`requires` as applicable; the city is where the per-candidate
  `requires` gate is finally evaluated.
- **world** — game-wide presence + game-option gates (mostly load-prune, §10).

**Trade-connected bonus — RESOLVED (owner): TEAM scope, gathered EARLY.** A trade-connected bonus comes in via the
**team** and is added to `have` *earlier* than your local/vicinity bonuses (it's high on the spine; pass 1 gathers
it before the area/city bonuses, §9). The per-city `CvCity::hasBonus = getNumBonuses()>0` is just the **local
materialization** — each city's `have` = the team's trade-connected bonuses (added early) + its own vicinity
bonuses (added later). So there is no empire-vs-city confusion: trade-connected = a **team** clause; vicinity = an
**area/city** clause. Two scopes, gathered at two points in the right-then-down pass.

**Statistics IS the higher-scope `have` (owner).** The scoped `have` splits cleanly: **city**-scope `have` is the
per-city isolated gather; the **empire / team / world** scopes' `have` is the **statistics module** (§7),
aggregated from the per-city "X is had, somewhere" reports the have-builder emits. So a `requires` clause at
empire/team scope consults **statistics**, and that uniformly covers BOTH presence (`∈ have` = `count ≥ 1`) and
count-thresholds (`count ≥ N`) — no special case. The have-builder feeds statistics; verify reads the right scope
per clause (local gather for city, statistics for empire/team/world). Statistics thus *becomes* the empire state
`requires` wants — counting and presence in one place, and a module we want anyway (demographics / AI / score).

**Statistics REUSES the modifier machinery — multi-level + ADDITIVE (owner).** It is not a new mechanism: it
lives at every scope level and **aggregates UP the spine additively, exactly as modifiers sum down it.** A "had X"
report at a city is an additive deposit that rolls up to empire/team/world counts. So **one additive-scope machine
serves both cascades** — modifiers sum *effect-magnitudes*, statistics sum *presence-counts*. Consequently
**`requires` only ever reads the team/empire statistics** (plus the local city `have`): the verify gate consults
the aggregated count/presence at the clause's scope, and never has to itself cross city isolation.

**Statistics bottom out at the CITY (like modifiers) — a clean fun-fact boundary (owner).** Because the leaf is
the city, the **city-level statistics are a well-defined place to record and query per-city facts** — not only the
current-state counts `requires` reads, but **lifetime/historical** ones ("how many knights has this city produced
in its lifetime?", buildings ever built, …). So the one structure serves THREE readers: the **enabler** (current
counts/presence), **demographics / UI / AI** (lifetime + aggregated fun-facts), and the additive roll-up yields
empire/team/world totals for free. One module, many readers — and it's wanted regardless of the cascade.

---

## 9. `have` gather order — right-then-down (sideways then down)
`have` isn't pure enumeration: the lower part is **derived** (you "have" a bonus only if connected/vicinity; a
building may reveal/produce one). So pass 1 gathers in dependency order — each tier left-to-right (the tech tree,
build chain, …), then **down** to the next tier when it can't go further right. Sticky top (techs/civics) first,
volatile bottom (resources/bonuses/buildings) after, so derived `have`-entries resolve against what's already
gathered. This is the enabler's *only* use of the two-axis topology — as a **gather order**, not a propagation.

---

## 10. Gates — prune-at-load vs match-at-runtime
- **LOAD-STABLE → PRUNE** (`loadPrune`, the `CvInfoReplacements` conditional swap, WorldBuilder/BUG toggles,
  per-civ `disables.techs`): rare, deliberate-setup changes → resolve at load, don't materialize if false →
  smaller `have`/candidate sets, zero runtime checks. Rebuild-on-change amortizes to ~never.
- **DYNAMIC → MATCH** (tech/civic/bonus presence; handicap under flexible difficulty): flips during play → the
  two-pass match each recompute.

---

## 11. Out of scope / deferred neighbors
Outcome/instance lifecycle (unit survival, building demolition, wonder residual, `ImprovementUpgrade`); the
**modifier cascade** (`building-cascade-conversion.md`); the **CREST** conditioner case (#430); the
**resource/bonus tier order** pressure-test; the **culture-bonus intermediary** `building→bonus→building`
(migrate faithfully now, collapse in the purge).

---

## 12. Retrofit (what changes in the ~21 converted infos)
**NOTHING IS FLIPPED (owner, 2026-06-15).** The converted `enables.{buildings,units,…}` on the sources (techs etc.)
is **correct as authored** — it is the constructive generator (§5), read forward. v0.2's "flip `enables`→`requires`"
is withdrawn. The retrofit is formalization + adding the `requires` (means) side where it's missing:
- **Keep `enables` on sources** (the constructive generator), and keep `disables`/`replaces` on sources (the
  destructive members, §6). Their reverse views are derived at load for the pedia (cold path) — no hand
  re-authoring, no hot-path inversion.
- **Author `requires` (the MEANS) on targets** — positive civic/religion/resource conditions only. The ~21
  converted infos are mostly sources (tech/civic/religion/era/…), so few carry a `requires`; the bulk lands at the
  **Building / Unit** pass (old `PrereqAndBonus`/`PrereqVicinityBonus` → `requires`). Confirm the means gaps for
  `hurries`, `specialists`, `specialBuildingsWaived` at their passes.
- **Obsoletion/replacement is NOT a `requires` clause** — it is `disables`/`replaces` on the source (destructive,
  §6), removing the candidate from CAN GET in generation. (Withdraws v0.2's "fold into the target's `none`".)
- **Name `loadPrune` + `policies`** as their own sections (already on disk, unnamed).
- Tag each `requires` clause with scope + greyable/hiding disposition (§4).

---

## 13. Open decisions (for hole-poking)
1. **Where each edge is authored — RESOLVED (owner, 2026-06-15), by the NATURE of the action.** Permanent actions
   (constructive `enables`, destructive `disables`/`replaces`, incl. all tech) are authored **forward on the source**
   and drive generation; reversible **means** (`requires`: civic/religion/resource) are authored **on the target**
   and drive the gate. They COEXIST on an entity. Hot path is forward both ways — reverse indices are cold-path
   (pedia) only, so there is **no reverse-lookup clusterfuck** and `enabledWhen` is not needed (the interim
   sticky/fluid two-object draft is withdrawn). *(Perf — RESOLVED: both steps bounded by HAS, never all ~5000
   entities; the frontier is also the AI `doProduction` choice set, strictly cheaper than the old PreLoop.)*
   - **Build-vs-operate — RESOLVED (owner, 2026-06-15), grounded in today's data.** Today's two checkpoints —
     `canBuild` (one-time) and `PrereqBonuses` (continuous, `isActiveBuilding`) — map as: `canBuild`'s STRUCTURAL
     part → `enables` (generation, hides if unmet); the RESOURCE/means part → ONE `requires` set that greys at build
     and dorms at operate (PrereqBonuses are a presence check, never consumed, so no separate build-only set). No
     `persist` flag needed (§3).
2. **Count/threshold/waive — RESOLVED (owner, §7/§8):** a separate **statistics** module (wanted anyway for
   demographics/AI/score) *is* the empire/team-scope `have`. The have-builder reports "X is had, somewhere" as it
   gathers; statistics aggregates; empire/team `requires` clauses (presence AND count) read statistics. The
   have-builder stays per-city isolated. *(GROUNDED — sweep 2026-06-14: SIX count-threshold types, ALL cross-city,
   ZERO per-city — `PrereqNumOfBuildings`/`getNumCitiesPrereq`/`getUnitLevelPrereq`/`getNumTeamsPrereq`/
   `PrereqProjects iNeeded`/the CIVIC city-limit (CvPlayer.cpp:6707/6754/6763/6685/6872/8466) + the SpecialBuilding
   group-cap waive — empirically VALIDATING the separate statistics module: a city's set-`have` answers none.)*
3. **Trade-connected bonus scope — RESOLVED (owner):** TEAM scope, gathered early into `have`; the per-city
   `hasBonus` count is the local materialization, not an empire-vs-city confusion (§8).
4. **Runtime `have`-change triggers + state-retraction — GROUNDED (sweep 2026-06-14).** Cadence CONFIRMED: many
   mid-turn triggers — AI building completion (`doProduction`→`changeHasBuilding`, CvCity.cpp:16519/14380),
   tamed-animal/heritage missions (CvUnit.cpp:8852/8909), `doAutobuild` add/remove, bonus connect/disconnect via
   worker missions (`updatePlotGroupBonus`), city conquest, religion spread, inquisition, nuke. **"Buildings-only"
   state-retraction is FALSE:** **inquisition** retracts a RELIGION (CvUnit.cpp:24036/24083) and cascades
   prereqReligion building removal (24021); bonuses are lost via connectivity-cut/depletion (civics are slot-swap,
   techs add-only). **No model change** — each retraction is just a `have`-change that recomputes the frontier, and
   a `requires` clause on the retracted religion/bonus correctly goes false; religion & bonus clauses are
   first-class, not building-only. (The instance demolition itself stays the deferred OUTCOME system.)
5. **`policies` — RESOLVED (sweep):** AUXILIARY, non-cascade. `isPlayable` (human civ selection,
   CvInitCore.cpp:1570), `isAIPlayable` (AI assignment, CvPlayer.cpp:23171), `isStronglyRestricted` (NPC
   build-whitelist meta-gate, CvCity.cpp:2205/2547) — ZERO participation in the `requires` BoolExpr. Keep §6's
   dedicated `policies` section.
6. **OR-group authoring shape — RESOLVED (owner):** handled by the `requires` full BoolExpr — OR-groups are `any`
   nodes inside it (§3); no separate shaping. *(Pedia rendering of the BoolExpr is a deferred UI detail.)*

---

## 14. DEMOLITION LIST — what the cascade REPLACES and we delete (grounded 2026-06-14)

The grounding sweep's key output (owner: most of the cascade is new code; the value is knowing what gets ripped
out). The cascade (gather HAS → generate CAN GET via the `enables` family → gate by `requires` means, ONE trigger)
subsumes all of this — and because each step is bounded by HAS, the replacement runs on a far smaller list than the
gates below, which re-scan the whole database:

**A. The ~7 scattered gate functions + internals (~2100 lines)** — each → HAS (pass 1) + `enables`-family
generation + per-candidate `requires` gate (pass 2):
- `CvCity::canConstruct`/`canConstructInternal` (CvCity.cpp:2470-2528+) + `CvPlayer::canConstruct`/internal
  (CvPlayer.cpp:6509-6566+, incl. the count loops 6685/6707/6754/6763) — building gate.
- `CvCity::canTrain` (2333) + `CvPlayer::canTrain` (6370-6506) — unit gate.
- `CvPlayer::canEverResearch` (8258-8286) + `CvGame::canEverResearch` — tech gate.
- `CvPlayer::canDoCivics` (8447-8478, incl. city-limit count 8466) — civic gate.
- `CvPlayer::canFoundReligion` (10103); `CvCity::canCreate` (3008) + `CvPlayer::canCreate` (6800-6882, PrereqProjects
  loop) — project gate; `canMaintain` (3024/6885) — process; `CvPlayer::canFound` (6195) — settle.

**B. The ~37 scattered CALL SITES** across 12 files (CvCity/CvCityAI/CvDLLButtonPopup/CvDLLWidgetData/CvGame/
CvGameTextMgr/CvPlayer/CvPlayerAI/CvTeam/CvUnit/CvUnitAI/CyGame), each invoking the gates with divergent
`bContinue`/`bTestVisible`/`bIgnoreCost`/`bExtra` flags → collapse to ONE shared frontier read (the "42 → 1" win).

**C. The 5 caching structures — DELETED** (the frontier is the single recompute-on-change source; §2
cache-agnostic): `CvPlayer` FOUR arrays (`m_bCanConstruct`/`Cached` + the `DefaultParam` pair, 6520-6558);
`CvCity` lazy `std::map<int,bool>*` + the `VALIDATE_CAN_CONSTRUCT_CACHE` shadow-check (2483-2522).

**D. On-demand candidate regeneration — REPLACED by the persistent frontier:** `CvCityAI::CalculateAllBuildingValues`
PreLoop (CvCityAI.cpp:12811-12835) + the `AI_bestCityBuild`-era usages — the per-city-per-decision regeneration the
frontier kills (= the AI `doProduction` choice-set win).

**E. Precursor models — REPLACED by `requires`:** the `ConstructRequirement` GOM struct
(`Sources/ConstructRequirement.h` + `getConstructRequirements`) + its fidelity audit
(`logConstructRequirementFidelity`, CvGlobals.cpp:3465-3542) — the simpler precursor the richer `requires` BoolExpr
supersedes. The **#195 enabler reverse-index** (`buildConstructibilityEnablerIndex`, `getBuildingsEnabledBy`/
`getUnitsEnabledBy`, CvGlobals.cpp:3294/3235/3241) — **PARTLY KEPT** (it *is* the candidate-generation `enables`
index) / partly replaced.

**F. Imperative replace/disable machinery — REPLACED by DERIVATION:** `CvCity::setHasBuilding` removal block —
`getExtendsBuilding` extension cascade (14432-14445) + `setDisabledBuilding` replace/re-enable chain-walk
(14448-14477) — become two derived behaviours, NOT imperative walks: the **destructive** `replaces`/`disables` chain
drops the candidate from CAN GET in generation (§6), and the **operational/dormancy** check ("disabled ≠ absent" —
a built thing whose `requires` means fail goes dormant but persists, §3) is just the per-candidate gate re-run on
things you HAVE.

**G. UI prereq rendering — MIGRATED (re-point, not delete):** `CvGameTextMgr` `buildBuildingRequiresString` /
`appendRequirementHelp` / `appendVicinityRequirementHelp` / `appendCivicRequirementHelp` / `buildRequirementItemLink`
→ re-point from `ConstructRequirement` to the cascade's `requires` + reverse index.

---

*Status: model GROUNDED and design-complete (v0.3 — HAS → CAN GET → HAS THE MEANS TO; `enables` family =
PERMANENT actions [constructive `enables` + destructive `disables`/`replaces`, incl. tech] forward on the source
→ generation; `requires` = REVERSIBLE means [civic/religion/resource] on the target → gate + dormancy; the two
COEXIST; hot path forward-only, reverse indices cold-path/pedia only). Next: (a) retrofit per §12 — NO flip; keep
the `enables` family on sources, author `requires` means on targets, name `loadPrune`/`policies`; (b) prove
HAS → CAN GET → `requires` round-trips on one converted entity; (c) hand the engine (#430) the §2 generate-then-gate
model + §3 `requires` means + §5/§6 `enables` family + §4 greying + this §14 demolition list.*

*Consequence (owner, 2026-06-15): forward-only tech generation RE-ENABLES purging redundant tech-side
prerequisites (e.g. obsidian-weapon / waterproof-concrete / lead-glass) — a tech's predecessors carry the edge
forward via `enables`, so the prereq fields on the tech itself are redundant and can be shed (the model no longer
needs them for any reverse lookup). This is safe **because the whole thing — constructive `enables`, destructive
`disables`/`replaces`, and the `requires` means gate — lives in ONE UNIFORM CHAIN** (the single cascade, one
trigger, §4): there is no scattered per-prereq mechanism left that a shed field could silently break. Capture in
the tech retrofit / dead-structure pass.*
