# Codebase bug-hunt — standing initiative

A recurring, systematic sweep of the codebase (C++ in `Sources/`, Python in
`Assets/Python/`) for the class of "obvious" latent bugs that this large legacy
Civ4/C2C-derived codebase accumulates: copy-paste slips, operator-precedence
mistakes, shadowed members, inverted conditions, uninitialised variables, and
out-of-bounds access. The goal is to verify each finding against source and file
one GitHub issue per confirmed bug.

## Why this exists

The first pass (CvUnit / CvUnitAI / CvCity / CvCityAI) found ~23 real bugs.
The patterns repeat across the whole engine, so periodic sweeps keep paying off.
Keeping the method and conventions written down means anyone (or any agent) can
pick the work up where it was left.

## Method (proven)

1. **Fan out** parallel reviewers, each over a ~7–8k-line chunk of a single
   file. Ask only for high-confidence findings, with an explicit checklist:
   - `&&` vs `||`, operator precedence (missing parens around `||` inside `&&`)
   - inverted conditions, `=` vs `==`
   - off-by-one / out-of-bounds array or `std::vector` access
   - null derefs after `get*()` / `plot()` / `getPlotCity()` that can return NULL
   - copy-paste: wrong variable / enum / "checks X but uses Y"
   - member-variable shadowing (`int m_xxx = ...` declares a local, member never written)
   - a function passed without `()` (decays to an always-true pointer)
   - comma-operator slip: an argument placed outside the call parens — `f(a), b)`
   - wrong best-value comparator (`<` vs `>`), or a missing `iValue > iBestValue` guard
   - an accumulator not reset between outer-loop iterations, or mutated in the wrong place
   - uninitialised variables; missing `break` / switch fallthrough
2. **Re-read every flagged line against source before filing.** Reviewers
   produce false positives — e.g. an idiom that looks wrong but is intentional
   (the `AI_chooseUnit` cross-counter return value was one such non-bug). Do not
   file anything you have not personally confirmed.
3. **File one GitHub issue per confirmed bug** (repo `Stones2Stars/S2S`).

## GitHub issue convention

- **Title:** `` `<file>: <function>() — <symptom>` ``
- **Body**, in this order: `## Summary`, `## Location`
  (`Sources/file:line — Class::func`), `## Problem` (with a code block),
  `## Suggested fix` (code), `## Impact`.
- **Labels:** always `bug`; add `game mechanics` for core gameplay
  (CvUnit/CvCity/…), `unit automation` for CvUnitAI, `code structure` for
  dead-code/structural issues, `infrastructure` for tooling/logging.
- After landing a fix, reference the issue from the commit/PR (`Fixes #NN`).

## Progress

**Pass 1 — issues #48–#70 (filed):**

| Area | Files | Issues |
|---|---|---|
| Units | `CvUnit.cpp`, `CvUnitAI.cpp` | #48–#56 |
| Cities | `CvCity.cpp`, `CvCityAI.cpp` | #57–#70 |

Fixed so far (PR #71): #48 (`setSMPowerValue` shadow), #57 (`conscript`
negated count), #61 (`unitHasCityOrPlotPropertySources` uninitialised). The
remaining issues are filed but unfixed.

## Next targets

- **C++:** `CvPlayer` / `CvPlayerAI`, `CvTeam` / `CvTeamAI`, `CvGame`,
  `CvPlot`, `CvSelectionGroup` / `CvSelectionGroupAI`, `CvMap`, `CvDeal`,
  `CvGameTextMgr`, the combat / info modules.
- **Python (`Assets/Python/`):** `CvEventManager.py`, `CvGameUtils.py`, the BUG
  options and screens. Python-specific smells: bare `except:`, `==` vs `is`
  misuse, mutable default arguments, wrong indentation under `if`/`for`, missing
  `self.`, calls with the wrong argument count, off-by-one `range()` loops, and
  integer-vs-float division.

## See also

- [`../reference/ai-logging-reference.md`](../reference/ai-logging-reference.md) — the tagged AI logs are the
  complementary, data-driven way to surface AI bugs.
- Build / compile-check: `Tools/_Build.ps1 Assert build` from `Sources/`.
