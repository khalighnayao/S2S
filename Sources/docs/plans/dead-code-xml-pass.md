# Dead Code & Dead XML Removal Pass — Plan

Status: **planned / not started.** Companion to the standing bug-hunt
(`Sources/docs/plans/codebase-bug-hunt.md`). Same discipline: tooling/grep
*generates candidates*, but every removal is **personally verified against source
before deletion**, in small reviewable PRs, Assert-build before each C++ PR.

## Why now

The mod is C2C-derived and has accumulated zombie code: disabled `#if 0` blocks,
leftovers from already-removed subsystems (ACO histogram, BBAI logging), orphan
data, and dead-branch logic. Removing it shrinks the surface area before any
larger rework.

**North-star (deferred, not this pass):** move data storage from **XML → JSON** to
simplify a future **web-based Civilopedia** port. The XML structure is also
acknowledged to be haphazard in places and would benefit from a structural rework
— but neither the rework nor the migration is happening yet. What *is* worth doing
now is the dead-data pass, because:

1. You don't want to port garbage — migrate a clean, minimal dataset.
2. The detection tooling below (enumerate every tag the DLL actually reads, every
   `<Type>`, every `TXT_KEY_*`, every define) **is** the data-model extractor a
   JSON migration needs later. Build it reusably with that second use in mind.

## Method

- One subsystem/category per PR. `Fixes`/`Refs` the relevant issue when one exists.
- C++: Assert-build before PR (`Tools/_Build.ps1 Assert build`).
- XML/Python: `Tools/XmlValidator.exe -a` and
  `Tools/XMLTools/verify-python-callbacks.py` after changes.
- Tooling **emits candidate lists (CSV), never auto-deletes.** Human-verify +
  test-load a save before removing data.

## Tier 1 — high-confidence, immediate (low risk)

Quick wins; verify zero references personally, then remove (code + header decls).

- **Disabled blocks** — ~19 `#if 0` in `Sources/`, clustered in
  `CvGameCoreDLL.cpp` (profiler instrumentation) and `CvGameCoreUtils.cpp`
  (pathfinding short-circuits/profiling). Plus commented-out `logBBAI()` calls in
  `CvUnitAI.cpp` (~12966–13146) left from the deleted BBAI logging.
- **Orphan Python file** — `Assets/Python/MapScriptToolsOld.py` (~179 KB, zero
  `import` references anywhere). Delete.
- **Marked-unused functions** — `CvCity::calculateTotalTradeYield` and
  `CvPlayer::calculateTotalTradeYield` (both tagged `/* UNUSED */`, no callers) and
  their header declarations.
- **billw-disabled stubs** — `CvPlayer.cpp:~3805` (`AI_doCentralizedProduction`
  "no point calling it"), `CvPlayerAI.cpp:~1547` (wonder-city block disabled "to
  stop static analysis"). Confirm truly dead, then remove or finish.

## Tier 2 — audit required (medium risk)

- **ACO BUG-options surface — REMOVED (2026-06-09).** The old Advanced Combat Odds
  options screen was fully dead once the preview became a pure `computeCombatPreview`
  renderer (only `ACO__SwapViews` was still read, and it merely inverted the Shift-gated
  "needed rounds" line). Removed: `BugACOOptionsTab.py` (+ its `Python.pyproj` entry),
  `Assets/Config/Advanced Combat Odds.xml`, the two `init.xml` registration lines, ~21
  `TXT_KEY_BUG_OPT_ACO__*`/`TXT_KEY_BUG_OPTTAB_ACO` GameText keys, the whole `ACO_*`
  GlobalDefine block in `GlobalDefinesAlt.xml`, the `CONCEPT_ADVANCED_COMBAT_ODDS`
  Civilopedia concept (NewConceptInfo + 2 GameText keys), and a stray ACO sentence
  mis-glued onto `TXT_KEY_CONCEPT_ACTIVE_DEFENSE_PEDIA`. The `SwapViews` C++ read was
  dropped (`iView` now comes straight from `shiftKey()`).
  **KEEP (live, not ACO cruft):** the `Art/ACO/{green,yellow,red}_bar_*.dds` bars (the
  `computeCombatPreview` renderer draws them, `CvGameTextMgr.cpp` ~3158) and the
  `TXT_ACO_*` renderer text keys (e.g. `TXT_ACO_VICTORY`). These are the new lean
  preview's assets — the `ACO` here is just retained naming.
- **Dead-branch zombies (handle via the bug backlog, not bulk deletion):**
  - `#105` `m_bGameStart` / `checkGameStart()` — the flag is never set true so the
    branch is dead. Already `needs-signoff` (semantics decision: needs `>` not `>=`
    plus init-true). Resolve there.
  - `#139` (**Python**, `Revolution.py spawnRevolutionaries` — `isNPC() and not
    isNPC()` always-false guard) and `#64` (`CvCity::removeWorstCitizenActualEffects`
    inverted-while) — re-verify; these are logic-dead, fix-or-remove via the backlog.
- **Removed-game-option code paths** — code gated on options that no longer exist.

## Tier 3 — data orphans (needs tooling)

Build small Python audit tools (extend the `verify-python-callbacks.py` pattern).
Each emits a candidate CSV; nothing is auto-removed.

1. **Dead schema tags** — enumerate every tag the DLL reads (grep
   `GetChildXmlValByName` / `SetVariableListTagPair` string-literal tag names in
   `Sources/`), diff against tags present in the schema/XML. **Caveat:** generic
   loaders and `BoolExpr`/`IntExpr` expression reads are not literal-greppable —
   maintain an allowlist of tags consumed generically.
2. **Orphan `<Type>` entries** — collect every `<Type>` per category, cross-ref
   against all XML references + Python literals. **Caveats:** savegame
   compatibility (a removed Type can crash old saves — test-load); modular
   `z*`-prefixed XML appended to base lists; Type strings built by concatenation in
   Python.
3. **Dead `TXT_KEY_*`** — defined (`GameText/*`) vs referenced (XML
   `<Description>/<Help>`, Python `getText`/`TRNSLTR`). **Caveat:** dynamic key
   construction (`"TXT_KEY_" + type`) — not statically visible.
4. **Dead GlobalDefines** — `<DefineName>` in `GlobalDefines*.xml` vs `getDefine*`
   uses (C++ + Python). **Caveat:** `FVariableSystem` string lookups aren't
   statically visible.

## Hard caveats (a detection script MUST account for these)

- **Boost.Python bindings:** a C++ function with no C++ caller may be live via a
  `Cy*Interface.cpp` `.def()` + Python. Check both layers before removing.
- **Generic tag loaders** (`SetVariableListTagPair`) and **expression reads**
  (`BoolExpr`/`IntExpr`) consume tags not visible as literals.
- **Modular `z*` XML** is appended to base info lists.
- **Savegame compatibility:** test-load existing saves after any data removal.
- **Dynamic strings** in Python (Type and TXT_KEY concatenation).

## Suggested order

Tier 1 (fast, low-risk, builds momentum) → Tier 3 tooling (generate candidate
lists, reusable for the eventual JSON extraction) → Tier 2 audits. Keep each PR
small and per-category.
