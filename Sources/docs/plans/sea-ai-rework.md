# Sea / naval AI — rework notes

Naval AI is a known weak area in this codebase and is slated for a broader rework.
This file captures the starting point, the bug that kicked it off, and the logging
now in place to drive a data-driven improvement (see
[`../reference/ai-logging-reference.md`](../reference/ai-logging-reference.md)).

## The trigger bug

Attack-sea **automation never left the player's borders** — automated attack ships
stayed in friendly waters instead of going out to engage the enemy.

Root cause: a human-automated attack ship runs the same `CvUnitAI::AI_attackSeaMove`
cascade as the AI. The only routine in that cascade that *proactively* moves toward
enemies — `CvUnitAI::AI_seaAreaAttack` — filtered candidate plots to:

```cpp
pLoopPlot->getOwner() == getOwner()   // only enemies inside OUR OWN waters
```

i.e. it only responded to enemies that had entered our territory (the code comment
literally calls it an "incursion"). Combined with `AI_blockade` only firing once the
ship is *already standing on* an enemy-owned plot, nothing ever sent a ship out
across neutral water toward a distant enemy.

**Fix (PR #182):** drop the own-territory filter so `AI_seaAreaAttack` pursues any
*visible* enemy ship in the same sea area. This is **peace-safe** — the
`generatePath()` immediately below still refuses to path into territory we'd have to
declare war on, so automation never auto-declares war; a ship only goes where it can
already legally move (at war, or through neutral/own water). Applies to AI and
human-automated units alike, so it warrants a playtest.

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
