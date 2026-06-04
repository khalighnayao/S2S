# AI tagged-logging reference

Developer reference for the structured AI decision logs. Every AI subsystem writes a
`<Domain>AI.log` of grep-friendly, tagged lines so you can see *what the AI decided and
why* without a debugger. This complements the design plan in
[`ai-logging-rollout.md`](ai-logging-rollout.md) — that one is the rollout plan, this one
documents the system as it actually exists.

The logging is **observation only**: it never changes AI behaviour. It has, however, been
the primary tool for finding AI bugs (e.g. a never-ending-turn hang and a long-dead
property-control subsystem were both first spotted as anomalous log volume).

---

## 1. Enabling it

Logs are gated by a verbosity level. Set it via the in-game **BUG options → Autolog**
("Player BBAI log level"). All four internal scope globals
(`gPlayerLogLevel` / `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel`) are currently
driven from that **single** option (`Autolog__LogLevelPlayerBBAI`), so one knob turns
everything on. `0` = off.

On a `_DEBUG` build all four are forced to `4`.

Logs are written to:

```
Documents/My Games/Beyond The Sword/Logs/<Domain>AI.log
```

### Level mapping

Levels are cumulative (a level-N setting emits levels 1..N). They mean the same thing in
every subsystem:

| Level | Meaning | Typical tags |
|---|---|---|
| **1** | Headline: begin/end, baselines, the final decision/mission committed | `begin`, `best`, `decision`, `mission`, `produced` |
| **2** | Per-decision: scores, per-option choices, dedup/skip summaries | `score`, `order`, `act`, `push`, `danger` |
| **3** | Per-candidate trace: each candidate/flavour/factor considered | `cand`, `tender/cand`, detailed `skip` |
| **4** | Inner-loop trace: per-unit / per-iteration spam (off by default) | `assess` rejects, `postprocess` |

**Volume warning:** level 3 is verbose and level 4 is *very* verbose (ContractBroker can
emit 10k+ lines/turn at level 4). For routine play use 1–2; use 3 when chasing a specific
decision; use 4 only for a targeted dig.

---

## 2. The registry

Each subsystem has a `log<Domain>AI(int level, fmt, ...)` helper in `BetterBTSAI.{h,cpp}`
that writes one file, gated by one scope global. Tag prefixes are short readable mnemonics;
path segments after the `/` are lowercase and may nest (`[XXX/group/detail]`).

| Domain | Tag | Log file | Scope global | Emitted in |
|---|---|---|---|---|
| Worker | `[WAI]` | `BuildEvaluation.log` | player | `CvWorkerAI` |
| Hunter | `[HAI]` | `HunterAI.log` | unit | `CvHunterAI` |
| Decision (flavours) | `[DAI]` | `DecisionAI.log` | player | `CvDecisionAI` |
| Diplomacy / deals | `[DIP]` | `DiploAI.log` | player | `CvPlayerAI`, `CvDeal` |
| Team & war | `[WAR]` | `WarAI.log` | team | `CvTeamAI::AI_doWar` |
| Unit dispatch | `[UNT]` | `UnitAI.log` | unit | `CvUnitAI`, `CvSelectionGroupAI` |
| City production | `[CIT]` | `CityAI.log` | city | `CvCityAI`, `CvCity` |
| Group & army | `[GRP]` | `GroupAI.log` | unit | `CvSelectionGroupAI`, `CvArmy` |
| Espionage | `[ESP]` | `EspionageAI.log` | player | `CvPlayerAI` |
| Founding / settle | `[FND]` | `FoundAI.log` | player | `CvUnitAI::AI_found` |
| Combat | `[COM]` | `CombatAI.log` | unit | `CvUnitAI`, `CvSelectionGroupAI` |
| ContractBroker | `[CTB]` | `ContractBroker.log` | player | `CvContractBroker` |
| Game session header | `[GAME]` | `GameInfo.log` | (ungated) | `CvGame::onFinalInitialized` |

`GameInfo.log` is written once on game start/load (speed, handicap, map, options, players)
so every other log can be read against the active ruleset.

---

## 3. Subsystem tag detail

Only the higher-traffic / more-useful subsystems are expanded here; the rest follow the
same `begin`/`cand`/`score`/`best`/`decision` vocabulary. Authoritative field lists live in
the doc-comment beside each `log<Domain>AI` in `BetterBTSAI.cpp` and the headers
(`CvWorkerAI.h`, `CvDecisionAI.h`).

### `[CIT]` — city production (`CityAI.log`)

Decision side (`CvCityAI::AI_chooseProduction`):
- `[CIT/begin]` (1) — production-choice context (pop, danger, finances).
- `[CIT/order]` (1) — the order the city commits to (TRAIN/CONSTRUCT/CREATE/MAINTAIN) + reason.
- `[CIT/danger]` (2) — inputs to the "minimal attack (danger)" military gate
  (`minAtk`/`need`/`ownedAtk`/`fire`).
- `[CIT/prop]` (2) — inputs to the property-control production gate
  (`val`/`change`/`proj`/`eval`/`check` + `getting`/`good`/`maxed` veto flags + `fire`).

Pipeline side (`CvCity::pushOrder`/`popOrder`/`doProduction`):
- `[CIT/push]` (2) — an order enters the queue (catches contract-driven units).
- `[CIT/push/reject]` (2) — the queue anti-spam guard blocked another copy of a unit/building.
- `[CIT/produced]` (1) — a unit/building/project actually completes. UNIT lines carry
  `ownerHas` (count of that unit type) + `aiRoleHas` (count of that UNITAI) → surfaces
  unit-spam outliers.
- `[CIT/cancel]` (1) — an order popped without finishing (switch / abandon / obsolete) +
  `progressLost`; repeated cancels = production thrashing.
- `[CIT/waste]` (1) — production overflowed the cap and was burned to gold.
- `[CIT/proplevel]` (1) — per-city snapshot of every active property (`val` + per-turn
  `change`) at turn start, for tracking crime/disease/etc. **trends** over time.

### `[UNT]` — unit AI (`UnitAI.log`)

- `[UNT/move]` (2) — which `AI_*Move` routine (UNITAI role) the unit ran this turn.
- `[UNT/act]` (2) — **the "why":** the decision helper that won the unit's prioritised
  cascade (`decision=retreatToCity reason=danger`), emitted at each helper's commit point so
  only the winning action logs. Read with `[UNT/move]` for the full picture.
- `[UNT/role]` (1) — a UNITAI role reassignment (`UNITAI 4 -> 10`).
- `[UNT/mission]` (2) — the committed MissionAI + target (central, in
  `CvSelectionGroupAI::AI_setMissionAI`).

### `[COM]` — combat (`CombatAI.log`)

- `[COM/odds]` (3) — attack odds the AI computed for a target plot.
- `[COM/threshold]` (3) — the go/no-go odds bar required to attack.
- `[COM/decision]` (2) — the attack target a unit commits to (`AI_cityAttack` / `AI_anyAttack`
  / `AI_leaveAttack`), closing the odds → outcome loop.

### `[WAR]` — team war (`WarAI.log`, **team** scope)

- `[WAR/begin]` (1) — once/turn per-team baseline (enemy-power %, funding) so the log
  populates even with no transition.
- `[WAR/area]` (1) — per-area military posture change.
- `[WAR/warplan]` (1) — warplan transition vs another team.

### `[CTB]` — ContractBroker (`ContractBroker.log`)

The unit work/contract market. High volume by nature. Headline events
(`[CTB/contract]` unit→work, `[CTB/tender/win]`) are level 1; per-candidate tender
evaluation is level 3; the per-unit assess/reject/suitability spam is level 4.

---

## 4. Reading the logs (grep recipes)

```bash
cd "Documents/My Games/Beyond The Sword/Logs"

# What did one unit decide this turn (routine + winning action)?
grep 'unit=876682' UnitAI.log

# Production-output stream for a player; spot unit spam (watch aiRoleHas climb)
grep '\[CIT/produced\].*owner=1 UNIT' CityAI.log

# Why is the property gate (not) firing? show the veto flags
grep '\[CIT/prop\].*PROPERTY_CRIME' CityAI.log

# Crime trend per city over turns
grep '\[CIT/proplevel\].*PROPERTY_CRIME' CityAI.log

# Role-reassignment thrashing (oscillation detector)
grep -oE 'UNITAI [0-9]+ -> [0-9]+' UnitAI.log | sort | uniq -c | sort -rn

# Per-unit re-evaluation spin (a unit appearing hundreds of times = a bug)
grep -oE 'UNT/move\] owner=[0-9]+ unit=[0-9]+' UnitAI.log | sort | uniq -c | sort -rn | head
```

The fields are `key=value`, space-separated, by design — `grep`/`awk`/`sort | uniq -c` is
the intended analysis path.

---

## 5. Adding logging to a new subsystem

1. Add `log<Domain>AI(int level, const char* fmt, ...)` to `BetterBTSAI.{h,cpp}` — copy an
   existing one and swap the **log file name** and the **scope global** (`gPlayerLogLevel` /
   `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel` — pick by the decision's natural owner).
2. Reserve a readable ~3-char tag prefix in the registry above (avoid collisions).
3. Instrument the decision functions:
   - `begin`/`end` lines carry the actor id + `turn=` context.
   - Pick the level by the mapping in §1 (headline=1 … inner-loop=4).
   - Keep fields `key=value` and space-separated.
4. Document the tag table + level mapping in a doc-comment beside the `log<Domain>AI`
   definition (and update §2/§3 here).
5. For an expensive log line, guard it: `if (g<Scope>LogLevel >= N) { ... }` so the lookups
   aren't paid when logging is off.

### Conventions

- C++03 only (no `auto`, lambdas, range-for, `nullptr` in new code paths that must match the
  surrounding style; use `foreach_`).
- Adding a `.cpp` to a new `Sources/<Dir>/` requires wiring `Sources/fbuild.bff`, not just the
  `.vcxproj` (else LNK2001 in FastBuild only).
- Log the *winning* decision at its commit point (one line per decision), not every candidate,
  unless it's a level-3+ trace. The repeated-line count is itself a signal — don't dedup it
  away (a unit logged 196× was how the turn-hang bug was found).
