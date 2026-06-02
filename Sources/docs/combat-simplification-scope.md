# Combat Simplification Scope (consolidated)

The combat rework has grown from "unify combat odds" into stripping the TB Combat Mod
suite down to a clean, maintainable core, then unifying that core into one engine with
pluggable game-option rules. This doc is the single source of what stays vs goes.
Priorities: OOS-safe > maintainable/sane > save-compat (save-compat downprioritized).

## KEEP — the lean combat core
- Strength/firepower: base/curr/max combat strength, firepower (incl. `SIZE_MATTERS`
  scaling and `STRENGTH_IN_NUMBERS` support).
- Combat rounds: `COMBAT_DAMAGE`, HP, needed-rounds, win odds (the unified engine).
- Combat limit (capture / can't-fully-kill).
- Collateral damage.
- Barb free-wins (standardized via one rule).
- `VANILLA_ENGINE` toggle (combat-mode rule).
- Endurance (explicitly kept).

## REMOVE — confirmed
- R1 Cold damage — **DONE** (code cold-free across 12 files incl. terrain-cold trigger,
  CvUnit fields/serialization, Info flags+XML schema, UI, AI; Assert build OK). Leftover
  unused TXT_KEY_*COLD* GameText entries are harmless; trim later if desired.
- R2 Stuns — **DONE** (CvUnit fields/serialization incl. keyed-info member + temp-array
  save/load; resolveCombat gates rewritten to keep first-strike/hit logic; checkForStun
  + accessors; Info vectors+XML+schema; UI combat-odds preview + help; AI valuation;
  Assert build OK; zero functional stun refs, only 2 harmless comments left for R4).
- R3 Power-shots — **DONE** (5 scalar modifier sub-types; build OK).
- R4 Afflictions + critical + `OUTBREAKS_AND_AFFLICTIONS` option — **PARTIAL**:
  - DONE: `#define OUTBREAKS_AND_AFFLICTIONS` confirmed commented-out → purged ~400 dead
    `#ifdef` blocks across 32 files via a Python unifdef-equivalent (keeps `#else`
    live branches). Build-verified, behavior-neutral (never compiled).
  - REMAINING (deferred as ONE coupled task — see findings): live afflict (216) +
    critical (189) + `GAMEOPTION_COMBAT_OUTBREAKS_AND_AFFLICTIONS` enum/Python/XML +
    runtime isOption checks.
  - **FINDING 1:** live afflictions is a disease/property SUBSYSTEM (CvCity 39,
    CvBuildingInfo 11, CvStructs 14, property manipulators), not just combat.
  - **FINDING 2:** critical is NOT cleanly separable from afflictions — the combat
    stat (criticalVSOpponentProbTotal/criticalModifier/criticalVSUnitCombat) is
    removable, but isCritical() (promotion-line flag), canInflictCritical(),
    assignCritical are affliction-trigger concepts wired into the promotion-line/
    disease system. And the GAMEOPTION enum can't be removed while deferred
    afflictions still uses isOption(). => Do afflictions + critical + option as ONE
    dedicated task, not a partial critical removal.
- R5 modifier family (dodge/precision/armor/puncture/withdraw/repel/...).  R6 terrain damage.

## REMOVE — TB Combat Mods come out wholesale (user-confirmed)
TB Combat Mods are overcomplicated, unmaintainable, poorly implemented. Remove the suite
now; note the good ideas for proper reimplementation later (below). Boundary = "keep
vanilla BTS combat, remove TB additions."
- **R5 modifier family** (~250 refs) decomposes into THREE removals:
  - **R5a accuracy:** dodge + precision (hit-chance modifiers; in getDefenderCombatValues
    + resolveCombat hit-modifier). Always-on, no option, cleanly separable.
  - **R5b mitigation:** armor + puncture (damage reduction in getDefenderCombatValues).
    Always-on, cleanly separable.
  - **R5c escape/control = `GAMEOPTION_COMBAT_FIGHT_OR_FLIGHT`** (live option): withdraw/
    pursuit/repel/overrun/unyielding/knockback TB layer + the option itself. **Badly
    implemented per user — document for reimplementation** (good-ideas list). NUANCE:
    keep **vanilla** withdrawalProbability(); note getWithdrawalProbability() currently
    halves it unless FIGHT_OR_FLIGHT is on (CvUnitInfo.cpp:668) — drop that conditional
    so vanilla withdrawal works unconditionally. Its own dedicated task like OUTBREAKS.
- **R6 terrain/feature turn damage** (AI can't handle it).
- Keep checking the other `GAMEOPTION_COMBAT_*` (BATTLEWORN, FIGHT_OR_FLIGHT,
  HEART_OF_WAR, EQUIPMENT, REALISTIC_SIEGE, SURROUND_DESTROY, UPRANGE, ...) as we go —
  most are TB additions and likely also go.

## KEEP — vanilla BTS combat (the boundary)
First strikes, vanilla withdrawal probability, collateral, combat limit, strength/
firepower, XP/promotion-on-combat. (If any of these are actually TB reimplementations
rather than vanilla, revisit.)

## Weapon types
No `WeaponType`/`getWeapon` symbol (0 refs) — ambiguous. Park it; user to point at what
this refers to (likely UNITCOMBAT subcombat classes or the `EQUIPMENT` option) before
acting.

## Good ideas to reimplement properly later (do NOT lose the concepts)
Captured so the spirit survives the removal:
- **Terrain/feature turn damage** — environmental attrition the AI actually understands.
- **Accuracy (precision/dodge)** — hit-chance modifiers as a clean, optional rule.
- **Mitigation (armor/puncture)** — damage-reduction vs penetration.
- **Escape/control (withdraw/pursuit/repel/knockback)** — disengage & push mechanics.
- **Afflictions/disease, cold/environmental damage, stun, power-shot, critical** — status
  & burst effects, if wanted, as discrete well-scoped systems.
These become future "rules" plugged into the unified engine, not inline spaghetti.

## Sequencing
Removals first (each builds), then unify the lean core into the engine with pluggable
rules. The modifier-family removal (R5) sharply simplifies `getCombatOdds` /
`getDefenderCombatValues` / `resolveCombat` (precision/dodge feed hit chance; armor/
puncture feed damage; withdraw/repel/knockback are per-round branches), so it should
land before the resolution unification (Phase 3).
