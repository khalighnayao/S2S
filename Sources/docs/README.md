# Developer documentation

Notes for people working on the **DLL and engine** (C++/Python internals).
Player- and end-user-facing documentation lives separately in the top-level
[`docs/`](../../docs/) folder — keep the two apart.

This folder is split by intent:

- **[`reference/`](reference/)** — *how the code works today.* Durable
  architecture notes tied to specific classes/systems. Update these when the
  behaviour they describe changes.
- **[`plans/`](plans/)** — *forward-looking work.* Refactor scopes, rollouts,
  removal maps, design captures, and standing initiatives. These describe
  intended or in-progress change, not the current state.

> Line numbers in these notes are anchors at time of writing; they drift. Treat
> them as "around line N" and confirm against the named function.

## Reference — how it works

| Doc | Subject |
|---|---|
| [CvGlobals](reference/CvGlobals.md) | Master singleton (`GC`): info arrays, engine interfaces, services |
| [CvMap](reference/CvMap.md) | Map grid, areas, plot groups, viewport, multi-map support |
| [CvPlot](reference/CvPlot.md) | Individual tile: terrain, units, properties, path cache |
| [CvGameAI](reference/CvGameAI.md) | Global AI helpers; normalised combat value; dirty-work propagation |
| [CvPlayerAI](reference/CvPlayerAI.md) | Per-player AI: research, diplomacy, military, danger caching |
| [CvCityAI](reference/CvCityAI.md) | City management: citizen assignment, production, building scoring |
| [CvUnitAI](reference/CvUnitAI.md) | Unit behaviour: missions, combat, contracts, promotion, upgrade |
| [CvTeamAI](reference/CvTeamAI.md) | Team strategy: war planning, diplomacy, area classification |
| [CvSelectionGroupAI](reference/CvSelectionGroupAI.md) | Group coordination: attacks, garrison, mission state |
| [CvWorkerAI](reference/CvWorkerAI.md) | Per-player worker cache + build-claim ledger |
| [CvArmy](reference/CvArmy.md) | Multi-stack coordinated assault groups |
| [CvContractBroker](reference/CvContractBroker.md) | Publish/subscribe unit-need dispatch between cities and units |
| [CvPathGenerator / CvPath](reference/CvPathGenerator.md) | Pluggable A\* pathfinder with per-unit cost/validity callbacks |
| [pathfinding](reference/pathfinding.md) | Engine FAStar finders (step/route/area/plot-group), the callback contract, team-restricted distance |
| [CvOutcome](reference/CvOutcome.md) | Probabilistic mission results defined in XML |
| [CvProperties](reference/CvProperties.md) | Generic extensible property container (crime, pollution, …) |
| [CvPropertySolver](reference/CvPropertySolver.md) | Per-turn solver for the property simulation system |
| [PlotSnapshot](reference/PlotSnapshot.md) | Plot-state snapshot schema + call-site conventions for logging |
| [ai-logging-reference](reference/ai-logging-reference.md) | The structured/tagged AI decision logs as they exist today |
| [UnitAI_Selection](reference/UnitAI_Selection.md) | How the AI picks a unit's `UNITAI` and concrete unit type |
| [UnitSelection_Mechanics](reference/UnitSelection_Mechanics.md) | The selection step itself: role → chosen `UnitTypes` |
| [doProduction](reference/doProduction.md) | `CvCity::doProduction` — the per-turn city production step |
| [constructibility-and-prerequisites](reference/constructibility-and-prerequisites.md) | The unified `ConstructRequirement` model, the static enabler reverse-index (CABV turn-time), and help-text rendering |
| [declarative-info-loading](reference/declarative-info-loading.md) | `CvInfoUtil` / `getDataMembers` — how `Cv*Info` classes load from XML, the wrapper catalog, and the byte-identical migration recipe (#196) |
| [MapScript_Process](reference/MapScript_Process.md) | End-to-end lifecycle of a C2C mapscript (`C2C_World`) |

## Plans — work in flight

| Doc | Subject |
|---|---|
| [codebase-bug-hunt](plans/codebase-bug-hunt.md) | Standing C++/Python bug-sweep initiative + GitHub issue convention |
| [combat-simplification-scope](plans/combat-simplification-scope.md) | Consolidated scope of the combat-system simplification |
| [combat-model-sketch](plans/combat-model-sketch.md) | `CvCombatModel` engine API sketch (for review) |
| [combat-odds-baseline](plans/combat-odds-baseline.md) | Pre-refactor combat-odds baseline (Phase 0 regression reference) |
| [combat-phase3b-plan](plans/combat-phase3b-plan.md) | Route the AI's win-% through the binomial engine |
| [ai-logging-rollout](plans/ai-logging-rollout.md) | Plan to roll the tagged-logging structure across the AI codebase |
| [sea-ai-rework](plans/sea-ai-rework.md) | Naval AI weaknesses, the attack-sea cascade, and the logging driving the rework |
| [dead-code-xml-pass](plans/dead-code-xml-pass.md) | Tiered dead-code / dead-XML removal plan; XML→JSON web-Civilopedia north-star |
| [gamespeed-simplification](plans/gamespeed-simplification.md) | `CvGameSpeedInfo`: Percents→named fields, GameTurnInfos collapse, derived calendar table (#196) |
| [fight-or-flight](plans/fight-or-flight.md) | `FIGHT_OR_FLIGHT` design capture (removal + plugin reimplementation) |
| [surround-destroy-removal-map](plans/surround-destroy-removal-map.md) | `SURROUND_DESTROY` removal map (deferred) |

## Where docs go

- A note about **how existing code behaves** → `reference/`.
- A note about **a change you intend to make** (plan, scope, rollout, removal) → `plans/`.
- **Player-facing** rules, manuals, FAQs, key bindings → top-level [`docs/`](../../docs/).

## The repo is the single source of truth — mirror knowledge here

**Durable project knowledge must live in this repository, not in any one
developer's (or AI assistant's) private/local notes.** If a finding, decision,
plan, taxonomy, or "hard-won fact" is worth remembering, it belongs in a committed
file so everyone — every contributor and every agent — sees the same thing.

- Learned how something works, or why? → add/update a `reference/` doc.
- Started or scoped an initiative, or captured a design decision? → a `plans/` doc.
- Cross-cutting, must-not-rediscover facts → the relevant `AGENTS.md`
  ("Key Subsystem Knowledge").
- Player-facing rules → top-level [`docs/`](../../docs/).

Local assistant memory (e.g. a tool's per-developer memory store) is a personal
*index/cache* only — it is **not** a substitute for the in-repo copy, and the
in-repo copy is authoritative. When you record something locally, mirror the
shareable part here in the same change. Keep these docs current as the code moves.
