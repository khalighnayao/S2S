# Sea / naval AI — rework notes

Naval AI is a known weak area in this codebase and is slated for a broader rework.
This file captures the starting point, the bug that kicked it off, and the logging
now in place to drive a data-driven improvement (see
[`../reference/ai-logging-reference.md`](../reference/ai-logging-reference.md)).

## The trigger bug

Attack-sea **automation never left the player's borders** — automated attack ships
stayed in friendly waters instead of going out to engage the enemy.

**Two distinct dispatch paths — get this right before touching naval movement.**
A unit's per-turn move is chosen *differently* for AI vs. human automation
(`CvUnitAI::doUnitAIMove`, ~`263`):

- **AI players (and un-automated units):** dispatched by **UNITAI role** →
  `AI_attackSeaMove` for `UNITAI_ATTACK_SEA`. Its only proactive "engage" routine,
  `AI_seaAreaAttack`, used to filter candidate plots to
  `pLoopPlot->getOwner() == getOwner()` (only enemies that entered *our own* waters —
  the comment calls it an "incursion"). **Fix (PR #182):** dropped that filter so it
  pursues any *visible* enemy ship in the sea area. Peace-safe — the `generatePath()`
  below still refuses to path into would-be-war territory, so no auto-DoW.
- **Human automation (the actual bug report):** dispatched by **`getAutomateType()`**
  (`switch` at `doUnitAIMove` ~`265`), *not* by UNITAI. "Attack" naval automation is
  `AUTOMATE_HUNT` → `CvUnitAI::AI_SearchAndDestroyMove` → **`CvHunterAI::autoHuntMove`**
  (generic combat units a player toggled to Hunt; `UNITAI_HUNTER` units go to
  `hunterMove` instead). **So PR #182 did not touch this path at all** — which is why
  ships were still stuck after it.

Why `autoHuntMove` kept ships home: it engages only within `AI_huntRange(1/3/6)` (~6
tiles), and with no target in range it fell straight to `AI_moveToBorders()`, which
*by design* only moves the unit **within its own territory** (bails if not on an owned
plot; only steps to plots `getOwner() == getOwner()`), then `AI_patrol()`.

**Fix (this PR):** in `autoHuntMove`, for `DOMAIN_SEA` units, *before* the
`AI_moveToBorders` fallback, take the fight out: `AI_seaAreaAttack()` (chase visible
enemy ships, now relaxed by #182) → `AI_blockade()` (sail to an enemy coast) →
`AI_explore()` (ships have the movement to go *find* targets, so roam the map instead
of retreating to the border). All reachable from `CvHunterAI` via
`friend class CvHunterAI`. New `[HAI/engage]` / `[HAI/explore]` log reasons mark them.

## The cascade to understand for the rework

`AI_attackSeaMove` (CvUnitAI.cpp, ~`6983`) is **defensive / escort-first**. Rough
priority order:

```
anyAttack(1/2) → seaBombardRange → heal → break-blockade (only if our city blockaded)
→ group(carrier/attack) → [in own/neutral water] shadow assault/carrier + group assault
→ blockade (ONLY if already on an enemy plot) → pillageRange(4) → protect(3,8)
→ travelToUpgradeCity → seaAreaAttack → patrol → retreatToCity → safety → SKIP
```

The offensive options (`seaAreaAttack`, `blockade`, `pillageRange`) sit **late** and
several are range- or territory-bounded, while escort/protect/group sit early — so
even after the `AI_seaAreaAttack` relaxation, a ship with nearby friendly assets may
still prefer staying home over sailing out. Re-ordering / re-weighting that cascade
is the substance of the rework.

## Logging in place (use this before changing behaviour)

Per [`codebase-bug-hunt.md`](codebase-bug-hunt.md) and the project's
"log-then-audit" rule, the attack-sea cascade is now fully instrumented. Each turn a
unit emits one `[UNT/act]` line naming the helper that won, via
`CvUnitAI::AI_logAct(decision, reason, target)` → `logUnitAI(2, "[UNT/act] …")`
(`BetterBTSAI.cpp`). The sea-specific helpers added in PR #182:

| helper | `decision` | `reason`s |
|---|---|---|
| `AI_seaAreaAttack` | `seaAreaAttack` | `pursueEnemyInArea`, `ambush` |
| `AI_blockade` | `blockade` | `blockadeHere`, `moveToBlockade` |
| `AI_seaBombardRange` | `seaBombard` | `bombardOrPlunder`, `moveToBombard` |
| `AI_shadow` | `shadow` | `shadowHere`, `moveToShadow` |

The generic helpers in the cascade (`anyAttack`, `patrol`, `safety`, `pillageRange`,
`protect`, `group`, `heal`) were already instrumented. To study naval behaviour,
filter `UnitAI.log` for `[UNT/act]` lines on `type=` of the sea UNITAI roles and read
them alongside `[UNT/move]`.

## Plan

1. Gather real `[UNT/act]` traces from games (especially at war, large navies).
2. Identify where automated/AI attack ships stall (escort/protect/group winning over
   offence; range caps; pathing refusals).
3. Re-order / re-weight `AI_attackSeaMove` (and siblings: `AI_reserveSeaMove`,
   assault/escort) with data behind each change. Keep each step Assert-built and
   playtested — naval AI changes affect every player.
