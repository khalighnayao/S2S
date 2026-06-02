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
- **R6 terrain/feature turn damage** — **DONE** (removed wholesale across ~15 files:
  CvUnit::doTurn application + XP; CvPlot getTotalTurnDamage/getTerrainTurnDamage/
  getFeatureTurnDamage; CvUnit isTerrainProtected/getTerrainProtectedCount/
  changeTerrainProtected + TerrainKeyedInfo.m_iTerrainProtected serialization;
  CvFeatureInfo iTurnDamage; CvPromotionInfo IgnoreTerrainDamage; CvUnitCombatInfo
  TerrainIgnoreDamageChangeTypes + orphan m_iIgnoreTerrainDamage; AI avoidance
  (CvUnitAI guard/explore/retreat/heal/defensive/tile-work, CvPlayerAI promotion
  valuation), pathfinding cost (CvGameCoreUtils x2), CvSelectionGroup heal gates;
  UI help (feature/terrain/plot/promotion/unitcombat); `MODDERGAMEOPTION_TERRAIN_DAMAGE`
  enum + dual Python registration; XML schema + data (FeatureInfos/PromotionInfos +
  2 modules); GameText keys; BUG toggle (RoMSettings.xml + ANewDawnSettings.py +
  RoMOptionsTab.py); Pedia. Build-verified, XML well-formed. Reimplement later as a
  terrain-attrition rule the AI understands — see good-ideas list.).
### `GAMEOPTION_COMBAT_*` triage (user-decided)
Footprint measured as functional C++ refs (excludes the 3 boilerplate refs every option
has: enum decl + 2 Python registrations).
- **KEEP:** `SIZE_MATTERS` (~129, core feature), `STRENGTH_IN_NUMBERS` *(see below — now
  slated remove)*, `VANILLA_ENGINE` (1, the lean-core switch), `NEW_RANDOM_SEED` (~1,
  RNG/reload infra, not combat spaghetti), `WITHOUT_WARNING` *(undecided — keep for now)*.
- **REMOVE — dead enum** (0 functional C++ refs, no XML content): `HEART_OF_WAR` (was
  repel/knockback/unyielding — gutted by R5c; only a commented building line + def + text).
  Trivial: enum + 2 Python + XML option def + GameText.
- **`EQUIPMENT` / `UPRANGE` — DONE** (user: "remove it all"). 0 C++ logic, but they gated the
  whole off-by-default equipment/weapon-loadout system: **88 unitcombat classes** (22
  `WEAPON_STYLE_*` holding styles + 26 `WEAPON_SIDEARM_*` + 40 `WEAPON_BACKUP_*` named
  weapons) via `<OnGameOptions>`. Removed the 88 `<UnitCombatInfo>` defs + **4,906
  `<SubCombatType>` refs across 19 unit/module files** + 88 orphaned GameText keys + both
  options (enum/Python/GameOptionInfos/GameText, incl. EQUIPMENT→UPRANGE enforce). KEPT
  non-gated weapons (primary `WEAPON_H2H_*`/`WEAPON_METHOD_*` and always-on futuristic
  `WEAPON_BACKUP_*` like antimatter/coilgun/plasma). Build green, all XML valid.
- **REMOVE:** `BATTLEWORN` — **DONE**. Was a commented-out `#define BATTLEWORN` (data-wiring
  dead-compiled → already inert). Purged 31 dead `#ifdef BATTLEWORN` blocks (283 lines) +
  the live-but-inert strAdjper Rnd/Att/Def layer (CvUnit accessors/members/serialization,
  currCombatStr application, CvGameTextMgr unit+promotion help) + schema/option/enum/Python/
  GameText (incl. 26 orphaned RAGE/FATIGUE/RAMPAGE/TIRES/DETERMINATION/DEMORALIZATION keys).
  KEPT the round/attack/defense counters (getDefenseCount still used by visibility logic) and
  the separate `iStrAdjperTurn` field. Build green, XML valid.
- **KEEP (reversed from earlier "remove"):** `SURROUND_DESTROY`. Per user it is a *functional*
  game option (AI underutilizes it, but it works) — distinct from the broken/non-functioning
  TB options. It's a LIVE system (real `surroundedDefenseModifier` combat application + city
  dynamic defense, ~250 refs / 27 files). We strip broken cruft and leave working features.
  A turnkey removal map exists at surround-destroy-removal-map.md IF ever revisited; no
  obligation. Concept also in good-ideas list.
- **REMOVE + document:** `STRENGTH_IN_NUMBERS` — **DONE**. Confirmed commented-out
  `#define STRENGTH_IN_NUMBERS` (dead-compiled) — the entire stack-support mechanic
  (getDefenderSupportValue/getAttackerSupportValue/ClearSupports + front/short/medium/long/
  flank support percents + support-formation unit refs) was inert. Purged ~2,574 lines of
  dead `#ifdef` across 15 files (incl. CvUnit.cpp -1,908) + the serialization-skip cruft +
  `bSINView` Alt-key preview mode + option enum/Python/GameOptionInfos/GameText + schema
  (Unit + Buildings) + `TB_PRESSALT_STR`. No XML data used these fields (confirms "never
  implemented"). Build green, XML valid. Concept in good-ideas list.
- **UNDECIDED — keep for now, revisit:** `REALISTIC_SIEGE` (~4, breakdown/bombard siege),
  `AMNESTY` (~3, Hidden-Nationality RoP attack block), `HIDE_SEEK` (~31, multi-layer
  invisibility), `WITHOUT_WARNING` (~23, ambush/assassination/stealth — a real gameplay
  feature with assassination missions + UI).
- **DEFER:** `OUTBREAKS_AND_AFFLICTIONS` — handled by the R4 afflictions+critical task.
- NOTE: GAMEOPTION enum order must stay aligned with `CIV4GameOptionInfos.xml`; removing a
  value shifts later options' save bits (save-compat downprioritized) and requires deleting
  the matching XML `<GameOptionInfo>` + GameText.

## KEEP — vanilla BTS combat (the boundary)
First strikes, vanilla withdrawal probability, collateral, combat limit, strength/
firepower, XP/promotion-on-combat. (If any of these are actually TB reimplementations
rather than vanilla, revisit.)

## Weapon types — RESOLVED
This is the `UNITCOMBAT_WEAPON_STYLE_*` taxonomy gated by `GAMEOPTION_COMBAT_EQUIPMENT`
(and `_UPRANGE`): ~75 weapon-style unitcombat classes in CIV4UnitCombatInfos.xml
(single-hand / two-handed / primary+secondary / shield / H2H-vs-distance, …) toggled via
`<OnGameOptions>`. No C++ logic reads the EQUIPMENT/UPRANGE options, but the content exists.
Decision pending: remove the whole weapon-style system (and its options) or keep it. Until
decided, EQUIPMENT/UPRANGE stay.

## Good ideas to reimplement properly later (do NOT lose the concepts)
Captured so the spirit survives the removal:
- **Terrain/feature turn damage** — environmental attrition the AI actually understands.
- **Accuracy (precision/dodge)** — hit-chance modifiers as a clean, optional rule.
- **Mitigation (armor/puncture)** — damage-reduction vs penetration.
- **Escape/control (withdraw/pursuit/repel/knockback)** — disengage & push mechanics.
- **Afflictions/disease, cold/environmental damage, stun, power-shot, critical** — status
  & burst effects, if wanted, as discrete well-scoped systems.
- **Surround & destroy** (`SURROUND_DESTROY`) — positional bonuses for surrounding enemy
  units/cities (land-ambush simulation). Good concept; the original needed AI that
  actually exploits it. Reimplement as a rule with AI that values encirclement.
- **Strength in numbers** (`STRENGTH_IN_NUMBERS`) — stacking-based combat scaling. Concept
  worth keeping; the TB version was never properly implemented (no observable in-game
  effect). Reimplement cleanly as an optional rule.
These become future "rules" plugged into the unified engine, not inline spaghetti.

## Sequencing
Removals first (each builds), then unify the lean core into the engine with pluggable
rules. The modifier-family removal (R5) sharply simplifies `getCombatOdds` /
`getDefenderCombatValues` / `resolveCombat` (precision/dodge feed hit chance; armor/
puncture feed damage; withdraw/repel/knockback are per-round branches), so it should
land before the resolution unification (Phase 3).
