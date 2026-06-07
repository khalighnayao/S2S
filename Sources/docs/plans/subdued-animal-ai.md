# Subdued / tamed animal AI — plan (early)

**Status:** planning seed, from the project owner's domain knowledge. Goal: make the AI treat
subdued animals as a **managed strategic resource** (spread / build / butcher by value), not
just idle units that pile up. Distinct from the (separate, now-mild) historical *subdued-animal
spam* and from the War-Dog-spam question.

Legend: **[V]** verified in code; **[O]** owner-reported design knowledge, not yet code-verified.

## The distinction that matters [O]
- **Subdued animal** = the captured mobile *unit* (`UNITCOMBAT_SUBDUED`, `UNITAI_SUBDUED_ANIMAL`;
  cannot merge — `bCannotMergeSplit`). It can be walked into a city and consumed.
- **Tamed animal** = the in-city *building/resource* produced from a subdued animal (e.g. a
  building that grants `BONUS_HORSE` / spreads a resource), vs. **butchering** it for a one-off
  yield. The AI currently doesn't model "unit → which in-city outcome" as a strategic choice.

## What the AI does today [V]
- Keep-value self-limits: in the unit keep/disband valuation, `UNITAI_SUBDUED_ANIMAL` value is
  scaled **down** the more you already have — `iValue *= max(0, cities*2 - numSubdued)/max(1,
  cities)` (`CvPlayerAI.cpp:20916`). So ~2/city is the soft target → the old glut is mostly gone
  (matches owner's observation).
- Use-value is **"primarily on what they can construct"**: the subdued-animal branch of
  `AI_unitValue` (`CvPlayerAI.cpp:~26153`) loops the unit's constructible buildings and scores by
  `AI_getNumBuildingsNeeded(eBuilding, ...) > 0` for the best coastal/capital city. So it only
  values a subdued animal if it can build a *currently-needed* building somewhere.

## Gaps to fix [O]
1. **No butcher evaluation.** The AI never weighs *butchering* an animal in a city — which is
   often worth it when there is **no building left to create** with that animal (one-off
   food/yield). Today an animal with nothing to construct just lingers. → add a butcher-vs-build
   (-vs-keep) value comparison.
2. **No resource spreading.** When a subdued animal can build a resource-granting building (e.g.
   horses), the AI doesn't think to **spread that resource to all cities** — it should distribute
   the bonus across the empire, not stop at one. `AI_getNumBuildingsNeeded` likely undercounts
   this empire-wide intent.
3. **Subdued-vs-tamed not modeled as a decision.** The construct-only valuation sees "buildings I
   can make right now," not the strategic fork: spread the resource, butcher for yield, or keep
   the unit. Each subdued animal should be routed to its best in-city outcome.

## To verify before designing
- [ ] The **butcher** command/outcome (mission or `CvOutcome`?) and what yield it grants — find
      it in XML/`CvOutcome*` and whether any AI hook exists.
- [ ] The **tamed-animal / resource-spread building line** (which buildings grant `BONUS_HORSE`
      etc. from which subdued units) and whether they're one-per-empire or one-per-city.
- [ ] Whether subdued animals actually pile up unused mid/late game (instrument a count) — sets
      the priority.

## North star
Cities (or the player AI) declare what they need (a resource they lack, food when starving), and
each subdued animal is valued for its **best** in-city action: build-to-spread-resource, butcher,
or hold. Ties into [[city-driven-worker-valuation]] (cities declaring needs) and the
[[hunter-ai-module-split]] / hunter economy that produces these animals.

## Cross-references
- Related report: `unit-ai-valuation.md` (hunter glut; dog/War-Dog spam still open).
- Memory: [[unit-ai-valuation]], [[city-driven-worker-valuation]], [[building-improvement-yields]].
