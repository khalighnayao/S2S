# CvUnitAI

**File:** `CvUnitAI.h` / `CvUnitAI.cpp`  
**Inherits:** [`CvUnit`](CvUnit.md)

## Overview
Adds automated behaviour to every unit in the game. The class manages the full unit
lifecycle from AI perspective: mission selection, movement, combat decisions, contract
dispatch, healing, promotion, upgrade, and serialisation.

## Contract State Machine
Units participate in the `CvContractBroker` workflow through a five-state machine:

| State | Description |
|---|---|
| `CONTRACTUAL_STATE_NONE` | Not currently seeking work. |
| `CONTRACTUAL_STATE_AWAITING_ANSWER` | Work request sent; waiting for broker response. |
| `CONTRACTUAL_STATE_AWAITING_WORK` | Matched to a request; en route to work site. |
| `CONTRACTUAL_STATE_FOUND_WORK` | Arrived at site and executing mission. |
| `CONTRACTUAL_STATE_NO_WORK_FOUND` | Broker search failed; falls back to default behaviour. |

## Lifecycle
| Method | Description |
|---|---|
| `CvUnitAI(bool bIsDummy)` | Constructor; initialises all AI state. Pass `true` for placeholder/dummy units. |
| `AI_init(UnitAITypes, int iBirthmark)` | Sets AI type and unique birthmark; resets state. |
| `AI_uninit()` | Releases resources; resets variables. |
| `AI_reset(UnitAITypes, bool bConstructorCall)` | Full state reset; used both at construction and on reassignment. |
| `operator=(const CvUnitAI&)` | Deep copy of all AI members. |
| `read(FDataStreamBase*)` | Loads all AI state from a save stream. |
| `write(FDataStreamBase*)` | Saves all AI state to a save stream. |

## Core Behaviour
| Method | Description |
|---|---|
| `AI_update()` | Main per-turn entry. Checks if the unit can move, handles automation/contracts, executes the current mission, and finalises state. Returns `true` if the unit should wait until next turn. |
| `AI_follow()` | Pursues a target: attacks, pillages, or advances toward an objective. Returns `true` if an action was taken. |
| `AI_load(UnitAITypes, ...)` | Finds and boards the best available transport matching the given criteria. |
| `AI_upgrade()` | Walks the upgrade chain and performs an upgrade if conditions are met. |
| `AI_promote()` | Evaluates the available promotion tree and selects the best option. |

## Combat
| Method | Description |
|---|---|
| `AI_attackOdds(pPlot, bPotentialEnemy, ...)` | Calculates attack odds against the best defender on a plot (0–100). |
| `AI_attackOddsAtPlot(pPlot, pDefender, ...)` | Calculates odds against a specific defender on a plot. |
| `AI_sacrificeValue(pPlot)` | Estimates how expendable the unit is in a combat on the given plot. |
| `scoreCityHealerNeed(eUnitCombat, eDomain, pCity)` | Scores a city's need for a healer of a given combat type and domain. |

## Build & City Support
| Method | Description |
|---|---|
| `AI_bestCityBuild(pCity, ...)` | Finds the best tile improvement to build in or around a city. Returns the target plot and build type. |
| `AI_isCityAIType()` | Returns `true` if the unit's AI type is a city-related role (garrison, defender, etc.). |
| `AI_isCityGarrison(pCity)` | Returns `true` if this unit is tasked as a garrison for the given city. |
| `AI_setAsGarrison(pCity)` | Assigns this unit as garrison for a city. |

## Group & Mission
| Method | Description |
|---|---|
| `AI_groupFirstVal()` | Leader-priority value for group formation (range: `LEADER_PRIORITY_MIN`–`LEADER_PRIORITY_MAX`). |
| `AI_groupSecondVal()` | Secondary sort value (combat strength) for group formation. |
| `AI_searchRange(int iRange)` | Calculates the effective search radius for mission targeting. |
| `AI_isAwaitingContract()` | Returns `true` if the unit is in `CONTRACTUAL_STATE_AWAITING_ANSWER`. |

## Border Patrol (AUTOMATE_BORDER_PATROL)
`AI_borderPatrol()` is the human "Border Patrol" automation. Cascade (2026-06-11, #24):
return-to-borders (when >2 tiles out) → heal → **intercept** visible enemies nearest-first out
to range 12 (odds bar = the `AUTO_PATROL_MIN_COMBAT_ODDS` modder option, +5/+10/+15 at range
5/7/12; targets confined to own territory except range 1-2 with `AUTO_PATROL_CAN_LEAVE_BORDERS`)
→ **walk the border** (`AI_patrolBorders`: randomized circuit biased onto border tiles,
continues the unit's heading; patrollers split into two rotational streams by unit-ID parity so
they don't all march one way) → roam outside (option) → retreat/safety. City capturing during
intercepts is gated by `AUTO_PATROL_NO_CITY_CAPTURING`. Historical bug: the range-7/12
intercepts used to sit AFTER the walk and were unreachable — patrollers circled past interior
intruders.

## State / Identity
| Method | Description |
|---|---|
| `AI_getUnitAIType()` | Returns the current `UnitAITypes` role. |
| `AI_setUnitAIType(UnitAITypes)` | Overrides the unit's AI role. |
| `AI_getBirthmark()` | Returns the unit's unique AI identifier (set at creation). |
| `AI_setBirthmark(int)` | Sets the unit's birthmark (normally only called during `AI_init`). |
| `AI_flushValueCache()` | Clears the cached generic value score so it is recalculated next time. |
| `getIntendedConstructBuilding()` | Returns the building this unit intends to construct (if any). |
| `getIntendedHeritage()` | Returns the heritage this unit intends to found (if any). |

## Logging
| Method | Description |
|---|---|
| `SendLog(function, message)` | Emits a structured log entry via the `FLB` logger for AI debugging. |

## Related
- [`CvUnit`](CvUnit.md) — base class (non-AI state)  
- [`CvSelectionGroupAI`](CvSelectionGroupAI.md) — groups of units driven together  
- [`CvContractBroker`](CvContractBroker.md) — work-request dispatch system  
- [`CvPlayerAI`](CvPlayerAI.md) — player AI that drives unit updates  
- [`BetterBTSAI.h`](../BetterBTSAI.h) — logging macros (`LOG_BBAI_UNIT`, `LOG_EVALAI_UNIT`)  
