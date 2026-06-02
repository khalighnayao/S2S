# FIGHT_OR_FLIGHT — design capture (for removal + plugin reimplementation)

Captured before removing the TB "escape/control" combat layer so the *spirit* survives.
Gated by the live option **`GAMEOPTION_COMBAT_FIGHT_OR_FLIGHT`**. The current
implementation is badly structured (per-round branches woven into `CvUnit::resolveCombat`
+ a wide `*VSOpponentProbTotal`/`*Total` accessor family + Info/promotion/unit-combat
data + UI + AI). It will be **removed in R5c** and **reimplemented as the first pluggable
combat-mod rule** on the unified `CvCombatModel` engine (see
combat-simplification-scope.md, combat-model-sketch.md).

## Concept
Combat isn't only "fight to the death." Units can disengage or be displaced:
- **Withdrawal / flight** — a losing unit flees instead of dying (attacker withdrawal
  when it drops below an HP threshold; defender withdrawal).
- **Pursuit** — the opponent chases, reducing the fleer's withdrawal chance.
- **Repel** — a defender shoves the attacker back, ending the fight without either dying
  (fortification boosts it).
- **Knockback** — an attacker shoves the defender back/off the plot.
- **Unyielding** — resists being knocked back / repelled.
- **Overrun** — pushes through a defender's repel/fortification.
- **Early withdraw** — flee sooner (higher HP threshold).
- **Per-opponent / per-terrain / per-unit-combat** situational modifiers on all of these.

## Sub-mechanics & formulas (from CvUnit::resolveCombat + accessors)
All chances are percentages, clamped [0,100], resolved with `getSorenRandNum(100, ...)`
rolls per round: "Withdrawal", "DefenderWithdrawal", "Repel", "Pursuit", "Knockback".

- **Defender repel:** `AdjustedRepel = clamp(defender.repelVSOpponentProbTotal(attacker) - attacker.unyieldingTotal(), 0,100)`.
  `repelVSOpponentProbTotal` = `repelTotal()` + `fortRepelTotal()` (fortify-based; vs
  `overrunTotal()`) + per-unit-combat repel. Retries: `repelRetriesTotal()`.
- **Defender withdrawal:** `AdjustedDefWithdraw = clamp(defender.withdrawVSOpponentProbTotal(attacker,plot) - attacker.pursuitVSOpponentProbTotal(defender), 0,100)`.
  `withdrawVSOpponentProbTotal` = vanilla `withdrawalProbability()` + `withdrawVSUnitCombatTotal()`
  + `withdrawOnTerrainTotal()` (terrain/feature/hill/peak). `earlyWithdrawTotal()` raises
  the trigger HP threshold (`withdrawalHP(maxHP, early)`).
- **Attacker withdrawal:** symmetric — attacker flees when `getDamage()+dmg >= withdrawalHP(maxHP, earlyWithdraw)` and its adjusted withdrawal (attacker.withdraw - defender.pursuit) succeeds.
- **Knockback:** `AdjustedKnockback = clamp(attacker.knockbackVSOpponentProbTotal(defender) - defender.unyieldingTotal(), 0,100)`. Retries: `knockbackRetriesTotal()`.
- **Multi-round/retry accumulation** (probability of triggering within N tries):
  `z += adj*y/100; y = adj*(100-z)/100` looped over the retry/round count — captures
  "chance it happens at least once."
- **withdrawAdjperAtt / currentWithdrawAdjperAtt:** withdrawal odds degrade per attack
  taken (so being focus-fired lowers escape chance).
- These feed `getCombatOdds`/ACO preview as RetreatOdds/RepelOdds/KnockbackOdds/
  DefRetreatOdds multipliers on the kill chance.

## Option behaviour (`GAMEOPTION_COMBAT_FIGHT_OR_FLIGHT`)
- **ON:** full feature — `getWithdrawalProbability()` returns full value; the VS/terrain/
  pursuit/repel/knockback Info data is loaded and applied.
- **OFF (default):** `CvUnitInfo::getWithdrawalProbability()` returns `m_iWithdrawalProbability/2`
  (CvUnitInfo.cpp:668); most Info read paths skip the FIGHT_OR_FLIGHT data
  (CvUnitInfo/CvPromotionInfo/CvUnitCombatInfo `read`/`copyNonDefaults` guard on the
  option). So today, with the option off, vanilla half-withdrawal still runs but the TB
  layer is inert.

## Data model (XML + Info classes) — to recreate
Per-unit base + promotion/unit-combat "Change" variants, plus per-unit-combat arrays:
- withdrawal: `iWithdrawalProbability` (vanilla, KEEP), `WithdrawVSUnitCombatTypes`,
  `WithdrawOnTerrainTypes`, `withdrawAdjperAtt`, `earlyWithdraw`.
- pursuit: `pursuit`, `PursuitVSUnitCombatTypes`.
- repel: `repel`, `RepelVSUnitCombatTypes`, `fortRepel`, `repelRetries`.
- knockback: `knockback`, `KnockbackVSUnitCombatTypes`, `knockbackRetries`.
- resist: `unyielding`, `overrun`.
Defined across CvUnitInfo / CvPromotionInfo / CvUnitCombatInfo (+ schema) with the usual
get/getNum/is/VSUnitCombat accessors and serialization.

## Integration points (where it currently lives)
- `CvUnit::resolveCombat` — per-round setup (Adjusted* values) + in-loop rolls and the
  withdraw/repel/knockback branches that end combat early and displace units.
- `CvUnit` accessors: `withdrawVSOpponentProbTotal`/`pursuitVSOpponentProbTotal`/
  `repelVSOpponentProbTotal`/`knockbackVSOpponentProbTotal` (CvUnit.cpp ~31507-31630),
  `earlyWithdrawTotal`/`overrunTotal`/`repelTotal`/`fortRepelTotal`/`repelRetriesTotal`/
  `unyieldingTotal`/`knockbackTotal`/`knockbackRetriesTotal` (~14123-14231),
  `withdrawAdjperAttTotal`/`currentWithdrawAdjperAttTotal`, `withdrawVSUnitCombatTotal`.
- UI: `CvGameTextMgr` combat-odds preview (withdraw/repel/knockback breakdown).
- AI: `CvPlayerAI` promotion/unit-combat valuation of these stats.
- `getWithdrawalProbability()` /2-when-off conditional (CvUnitInfo.cpp:668).

## Reimplementation as the first combat-mod plugin
Target the unified engine's RuleSet (combat-model-sketch.md). A `FightOrFlightRule`
registered when the option is on, exposing **resolution hooks** (e.g.
`onBeforeLethalDamage` → withdrawal/repel checks; `onAttackerHit` → knockback) that the
clean `resolveCombat` calls in order. Keep the math above but:
- isolate it from the core hit/damage model (the engine handles strength→odds→damage;
  the rule only adds escape/displacement outcomes),
- keep vanilla `withdrawalProbability()` as the engine baseline (NOT halved); the rule
  layers VS/terrain/pursuit/etc. on top,
- keep all RNG via `getSorenRandNum` for OOS-safety.
This validates the pluggable-rule architecture end-to-end.
