# Phase 3b — Route the AI's win-% through the binomial engine

Status: **IMPLEMENTED on `main` (working tree), pending in-game playtest.** Non-air AI
attacks now decide on the engine's binomial `getCombatOdds()/10` — the same number the
UI preview shows and the resolver rolls — instead of the strength-ratio heuristic. The
heuristic still runs (drives the predicted-HP contract + the `[COM/calib]` log); the
per-leader personality bias is preserved; air keeps the heuristic. `AI_finalOddsThreshold`
recentres the bar `*23/20` (+15%, air-exempt). See "Implemented" at the bottom.

> History note: this work was first done on branch `combat-ai-binomial-odds` (PR #23,
> June 2026) but that branch never merged to `main`/`release` — the merge commit is not
> an ancestor of either. It was re-applied directly onto `main`'s working tree. The
> stale "planned / not started" status above was the symptom that surfaced the gap.

This was deferred on purpose. It is not a refactor — it shifts AI aggression, and the
safety is in the rollout (instrument + calibrate + playtest), not in the diff.

Phase 3a (the shared `buildRoundModel` Layer 1) is done — resolution, the odds
functions, the preview, and the AI's *damage inputs* already share one formula.

## What changes, precisely

Today `CvUnitAI::AI_attackOddsAtPlotInternal` (CvUnitAI.cpp ~1167–1353) does NOT
call `getCombatOdds`. It:
1. Gets attacker `currCombatStr`/`currFirepower` (air uses `airCurrCombatStr`).
2. Calls `getDefenderCombatValues` (now a `buildRoundModel` adapter) for per-round
   damage + odds.
3. Estimates first-strike HP loss, sets `AI_setPredictedHitPoints`, re-reads HP.
4. Computes `iNeededRoundsUs`/`iNeededRoundsThem`, **boosts the loser's effective
   strength by the round-count difference**, then returns
   `iOdds = iOurStrength*100 / (iOurStrength + iTheirStrength)` (a strength ratio,
   1–99), and sets predicted HP on both units.
5. Applies the per-leader personality bias:
   `iOdds += (iOdds * 2 * AI_getAttackOddsChange()) / 100`.

The strength-ratio + round-count-boost in step 4 is a *cheap approximation* of the
win probability. 3b replaces ONLY that win-% number with the engine's binomial:

```
iOdds = getCombatOdds(this, pDefender) / 10;   // permille -> percent
// keep the personality bias line unchanged
```

Everything else in the function — the predicted-HP machinery — stays. See below.

## Sub-problems (each must be handled, none are optional)

### 1. Predicted-HP side effects must survive
`AI_setPredictedHitPoints` / `getHP()` reading it (CvUnit.cpp:11326) is load-bearing:
`CvSelectionGroupAI::AI_attackOdds` chains it across the stack sim, and `CvPlot`
checks `== 0` for "going to die". The engine (`getCombatOdds`) is pure — no HP
side effects.

**Approach (minimal, recommended):** keep the existing round-count expected-HP
estimate (`HP - neededRounds*damagePerHit`) purely for the predicted-HP writes,
and use `getCombatOdds` only for the returned odds. The strength-boost-for-odds
becomes dead and is removed; the needed-rounds / expected-remaining-HP code stays.
This isolates the behaviour change to the odds number and leaves the stack sim's
HP bookkeeping byte-identical.

**Alternative (heavier):** expose a lean engine call returning win% *and* expected
attacker/defender remaining HP (the data already exists in `computeCombatPreview`:
`fExpHPAttackerWin/PullOut/Retreat`, `fExpHPDefenderWin`). Only worth it if we
later want the predicted HP to match the engine's distribution too. Not needed
for v1 of 3b.

### 2. Personality bias stays
`AI_getAttackOddsChange()` (set per turn from the leader, CvPlayerAI.cpp ~16902)
multiplicatively biases the odds before every threshold compare. Keep the bias line
exactly, applied to the engine odds. Without it all leaders' aggression collapses.

### 3. Air combat has no engine path
`getCombatOdds` uses `currCombatStr(NULL,NULL)`; it does not branch on air or use
`airCurrCombatStr`/interception. For DOMAIN_AIR attackers, **keep the existing
heuristic** (gate: `if (air) <old path> else iOdds = getCombatOdds(...)/10`).
Unifying air odds is a separate task (see combat-model-sketch GAP #1).

### 4. Threshold recalibration — THE risk, and the real work
~40 AI thresholds (AI_anyAttack/AI_cityAttack/… 60/70/80/etc.) are tuned to the
*current* strength-ratio distribution and then inflated by `AI_finalOddsThreshold`
(CvUnitAI.cpp ~25406: ×6/5 base, ×6/7 city, ×6/9 attack, ×2/3 single-defender,
×3/2 cargo). The binomial win% has a **different shape** than the strength ratio —
near 50/50 they roughly agree, but for lopsided matchups they diverge sharply
(binomial pushes toward 0/100 faster). Swapping the source without recalibration
will silently change how aggressive the AI is at every threshold.

**Methodology (do NOT skip):**
- a. **Instrument first.** Before changing the return value, log BOTH numbers
  (`iOdds_heuristic`, `getCombatOdds/10`) for every AI attack evaluation, with the
  matchup, to a dedicated log. Run several AI-vs-AI autoplay games. This gives the
  empirical heuristic→binomial mapping across the real distribution of matchups the
  AI actually faces (not synthetic).
- b. **Decide the calibration target.** Either (i) remap each threshold through the
  measured mapping so decisions are unchanged at first, then loosen toward the more
  accurate binomial deliberately; or (ii) adjust the `AI_finalOddsThreshold`
  multipliers globally so the *aggregate* aggression matches a baseline autoplay.
  (i) is safer (per-threshold), (ii) is less code but blunter.
- c. **Hunter raw-odds path** (`CvHunterAI` + `AI_huntRange` `bRawOdds`,
  CvUnitAI.cpp ~27141) bypasses `AI_finalOddsThreshold` — recheck its thresholds
  separately; the binomial fixed the very-weak-defender underflow that originally
  motivated the raw-odds path, so it may simplify.

### 5. Stack goodness inherits the change
`CvSelectionGroupAI::AI_attackOdds` (the loss-ratio "goodness" score, NOT a win%)
consumes per-unit odds + predicted HP. It needs no structural change, but its
output shifts because its inputs shift. Include it in the autoplay comparison.

### 6. Performance
`getCombatOdds` (nested binomial + `pow`) is heavier than a strength ratio, and the
AI evaluates many candidate attacks per turn. The per-player `AI_attackOddsAtPlot`
cache (keyed on plot+attacker+defender) already absorbs repeats, but
`modifyPredictedResults=true` calls bypass the cache. Profile an autoplay turn
before/after; if hot, memoise `getCombatOdds` per (attacker,defender,HP) within a
turn, or fall back to the heuristic for clearly-decided matchups (odds the
strength ratio already puts >95% or <5%).

## Step sequence

1. **Instrument** (no behaviour change): log heuristic vs binomial odds side by side
   in `AI_attackOddsAtPlotInternal`. Run autoplay, collect the mapping. *(buildable,
   shippable, reversible.)*
2. **Swap behind a flag**: replace step-4 odds with `getCombatOdds/10` (air gated to
   the old path), keep predicted-HP code, keep the bias. Put it behind a temporary
   `MODDERGAMEOPTION_*` / define so it can be A/B'd against the heuristic.
3. **Calibrate thresholds** per §4b using the §1 data; verify hunter + stack paths.
4. **Profile** (§6); add memoisation only if the autoplay turn time regresses.
5. **Bake in**: once autoplay aggression + win-rate are acceptable, remove the flag
   and the dead strength-ratio-for-odds code. Update combat-model-sketch status.

## Acceptance criteria
- AI-vs-AI autoplay over N turns shows aggression and unit-loss rates within an
  agreed band of the pre-3b baseline (or deliberately, justifiably different).
- No measurable per-turn time regression (or mitigated by §6).
- The number the AI decided on == `getCombatOdds` == the UI preview == what the
  resolver rolls (the whole point: one combat number, end to end).

## Open questions — as resolved
- Calibration target: **re-tuned by feel** with a single global recenter (§4b option ii),
  not a per-threshold remap. Validated on ~5.7k autoplay evals: heur 60% == binom 80%,
  heur 70% == binom 96%; post-swap odds are healthily bimodal (~56% clear loss, ~31%
  clear win, ~7% judgment calls — the recenter only bites on that 7%).
- A/B option: **not kept.** No temporary `MODDERGAMEOPTION` flag — the swap went straight
  in. The gated `[COM/calib]` log is the calibration harness instead.
- Air: **keeps the heuristic** (and is exempt from the recenter). `getCombatOdds` has no
  air path yet (no `airCurrCombatStr`/interception). Air-odds engine path is a separate
  task (combat-model-sketch GAP #1).

## Implemented (what actually shipped to the working tree)
Two edits in `Sources/CvUnitAI.cpp`, plus this doc + `combat-model-sketch.md` +
`ai-logging-reference.md`:

1. **`AI_attackOddsAtPlotInternal`** — for non-air, capture `iBinomialOdds =
   getCombatOdds(this, pDefender) / 10` on entry, and after the strength-ratio `iOdds`
   is computed (saved as `iHeuristicBase`), swap: `if (iBinomialOdds >= 0) iOdds =
   iBinomialOdds;`. The personality bias line is applied to the result unchanged. The
   round-count / strength-boost / predicted-HP machinery is **left intact** — the boosts
   are now dead for the returned odds but still drive `AI_setPredictedHitPoints` (the
   stack sim in `CvSelectionGroupAI::AI_attackOdds` chains on it) and the `[COM/calib]` log.
2. **`AI_finalOddsThreshold`** — non-air only, `iFinalOddsThreshold = iFinalOddsThreshold
   * 23 / 20` at the tail (after the existing ×6/divisor block, before the `[COM/threshold]`
   log). The recenter knob.

Calibration log: `[COM/calib]` (level 3, `CombatAI.log`, via `logCombatAI`) emits
`heurBase` vs `binom` pre-bias odds + matchup per non-air eval. Kept (gated) as the
harness for re-tuning `23/20` and the per-player follow-up — folded into the existing
`[COM]` domain rather than a separate `CombatOddsCalibration.log`.

### Remaining before "done"
- **Playtest (FinalRelease):** confirm AI aggression / unit-loss feel acceptable. The
  Assert build compiles clean; behaviour is unverified until played.
- **Profile (§6):** check autoplay turn time — `getCombatOdds` is heavier than a strength
  ratio. The `AI_attackOddsAtPlot` cache absorbs repeats, but `modifyPredictedResults`
  calls bypass it. Memoise only if it regresses.
- **Hunter raw-odds path (§4c):** recheck `AI_huntRange` `bRawOdds` thresholds — it bypasses
  `AI_finalOddsThreshold`, so the recenter does not reach it.

## Follow-up: per-player / per-trait configurable aggression (FUTURE)
Replace the hardcoded `23/20` with a data-driven, per-leader knob so aggression varies by
civ/leader (barbarians reckless → lower bar; pacifists cautious → higher bar). The single
chokepoint is `AI_finalOddsThreshold`'s recenter line. Smallest-first:
1. Move `23/20` into a GlobalDefine (e.g. `AI_BINOMIAL_ODDS_THRESHOLD_PERCENT = 115`), read
   via `GC.getDefineINT(...)`; cache it if `AI_finalOddsThreshold` (PROFILE_FUNC) shows hot.
2. Add a per-leader/trait aggression term combined multiplicatively; barbarians via
   `GET_PLAYER(getOwner()).isNPC()`. Prior art: `AI_getAttackOddsChange()` (the *odds* bias,
   distinct from this *threshold* bar).
3. Keep air exempt and the hunter raw-odds path bypassing this until they get engine odds.
