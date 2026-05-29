#pragma once

#ifndef CV_WORKER_AI
#define CV_WORKER_AI

#include "CvEnums.h"
#include <map>

class CvUnitAI;
class FDataStreamBase;

// Per-player tunable weights for worker scoring. The defaults preserve the
// behavior the codebase had before these constants were extracted -- changing
// them at runtime (e.g. setting one player's weights aggressive and another's
// conservative) is the seam the cache and the per-player CvWorkerAI design were
// built to enable.
//
// Naming convention: each field documents what term in the score it scales.
// See CvWorkerAI::improveBonus for the actual usage sites; every magic-number
// scalar in that function should come from here.
struct WorkerScoringWeights
{
	// Inner build-pick: time tiebreaker is timeScoreNumerator / (build.getTime() + 1).
	// Larger numerator => more headroom for the comparator; 10000 matches legacy.
	int timeScoreNumerator;

	// Per-plot scoring (after a build is selected):
	int improvementYieldInCityRadius; // multiplies calculateImprovementYieldChange per yield type
	int natureYieldInCityRadius;      // multiplies calculateNatureYield per yield type
	int defenseDivisor;               // divides air-bomb + defense modifiers for fort value
	int zocSourceBonus;               // flat add when fort improvement isZOCSource
	int aiObjectiveScale;             // multiplies bonus.getAIObjective()

	// Whole-plot multipliers:
	int bonusValueMultiplier;         // applied once before path-turns division (legacy 1000)
	int atPlotBonus;                  // multiplied in when unit is already on the plot (legacy 3)
	int cityRadiusBonus;              // multiplied in when plot is in city radius (legacy 2)
	int noTradeableBonusMultiplier;   // multiplied in when player has no other trade copies (legacy 2)

	// Constructor installs the legacy defaults so existing-game balance is preserved.
	WorkerScoringWeights();
};

// One instance per CvPlayer. Holds per-turn, player-specific worker planning state:
//
//   1. improveBonus -- unified planner for sending a worker to improve a resource tile.
//      Combines the legacy CvUnitAI::AI_improveBonus and the CvWorkerService::ImproveBonus
//      experiment into a single entry point; both old implementations have been deleted.
//
//   2. BonusEval cache -- memoizes the per-plot inner sweep so the same plot is not
//      re-scored every time another worker (or the same worker on a later micro-tick)
//      plans its turn.
//
//   3. Target claim ledger -- scaffolding for cross-unit dedup that bypasses MISSIONAI.
//      Not used by improveBonus (which leans on AI_plotTargetMissionAIs + iMaxWorkers
//      for team-build-aware dedup), kept available for future planning paths that
//      lack that machinery.
//
//   4. WorkerScoringWeights -- per-player tunable knobs for the magic-number scalars
//      in the scoring formula. Defaults match legacy behavior.
//
// All maps are turn-scoped; onTurnBegin wipes them. All entries are POD-or-stdlib-of-POD
// so a future read/write pair for save persistence is mechanical (not wired today).
//
// Per-player by construction, so a future world where two players use different scoring
// weights cannot leak entries across players. Cache key also includes UnitTypes because
// CvUnit::canBuild varies by unit type and different worker tiers must not share entries.
//
// ----------------------------------------------------------------------------
// Log taxonomy for improveBonus (use these tags to grep BuildEvaluation.log):
//
//   [WAI/begin]         function entry; identifies the unit + allowed range
//   [WAI/plotset-empty] plotSet had no reachable plots
//   [WAI/plot/skip]     plot rejected by an outer filter (with reason code)
//   [WAI/plot/close]    Super Forts close-enough result for non-owned plot
//   [WAI/plot/access]   working-city/connected/route-capable gate decision
//   [WAI/build/hit]     inner build evaluation read from cache
//   [WAI/build/cand]    per-candidate inner-loop evaluation
//   [WAI/build/winner]  inner-loop final pick for this plot
//   [WAI/score]         per-plot scoring breakdown after inner pick
//   [WAI/dedup]         AI_plotTargetMissionAIs vs iMaxWorkers result
//   [WAI/best]          new best plot during outer iteration
//   [WAI/mission]       final mission decision (MOVE_TO vs ROUTE_TO)
//   [WAI/end]           function exit with success/failure
//
// Levels: 1 -> [begin] [end] [best]; 2 -> [plot/...] [build/winner] [score]
//         [dedup] [mission]; 3 -> [build/cand] (per-candidate detail).
// ----------------------------------------------------------------------------
class CvWorkerAI
{
public:
	explicit CvWorkerAI(PlayerTypes owner = NO_PLAYER);

	void setOwner(PlayerTypes owner) { m_owner = owner; }
	PlayerTypes getOwner() const { return m_owner; }

	// ---- lifecycle ----
	void onTurnBegin(int gameTurn);

	// ---- worker planning ----
	bool improveBonus(CvUnitAI* unit, int allowedMovementTurns = -1);

	// ---- scoring weights ----
	const WorkerScoringWeights& weights() const { return m_weights; }
	WorkerScoringWeights&       weights()       { return m_weights; }

	// ---- per-plot bonus-improvement cache (turn-scoped) ----
	struct BonusEval
	{
		int        turnComputed;
		BonusTypes bonus;        // entry is a miss if plot's bonus no longer matches
		BuildTypes bestBuild;    // NO_BUILD when qualified == 0
		int        score;        // semantics: yieldSum of bestBuild's improvement
		int        qualified;    // count of builds that survived all filters (0 = none)
	};

	const BonusEval* lookup(int plotIdx, UnitTypes unitType,
	                        int gameTurn, BonusTypes currentBonus) const;
	void record(int plotIdx, UnitTypes unitType, const BonusEval& eval);

	// ---- outer-filter rejection cache (turn-scoped) ----
	// Records plots that improveBonus rejected at the outer filter for a STABLE
	// reason. Subsequent calls in the same turn (e.g. the chained allowedTurns=2,4,
	// unbounded cascade) short-circuit these plots without re-running the filters
	// or the (expensive) generatePath inside the close-enough check.
	//
	// Only reasons that cannot change within a turn for a given unit-type are
	// cached -- ownership, plotInvalid, areaMismatch, noBonus, notCloseEnough.
	// Dynamic reasons (inaccessible-via-connectivity, visibleEnemy) are NOT cached
	// because the underlying state can change as other units move / cities are
	// founded / plot groups update.
	enum OuterRejectReason
	{
		REJECT_NONE             = 0,
		REJECT_OWNERSHIP        = 1,
		REJECT_PLOT_INVALID     = 2,
		REJECT_AREA_MISMATCH    = 3,
		REJECT_NO_BONUS         = 4,
		REJECT_NOT_CLOSE_ENOUGH = 5,
	};

	OuterRejectReason lookupOuterReject(int plotIdx, UnitTypes unitType) const;
	void markOuterRejected(int plotIdx, UnitTypes unitType, OuterRejectReason reason);
	static const char* outerRejectReasonName(OuterRejectReason reason);

	// ---- target claim ledger (cross-unit dedup; reserved for future paths) ----
	bool tryClaim(int plotIdx, int unitId);
	bool isClaimedByOther(int plotIdx, int unitId) const;
	void releaseClaim(int plotIdx, int unitId);
	void releaseAllClaimsBy(int unitId);

private:
	typedef std::pair<int, int> EvalKey; // (plotIdx, unitType)

	PlayerTypes m_owner;
	int         m_lastTurn;

	WorkerScoringWeights m_weights;

	std::map<EvalKey, BonusEval>          m_bonusEvalCache;
	std::map<EvalKey, OuterRejectReason>  m_outerRejected;
	std::map<int, int>                    m_claims; // plotIdx -> unitId
};

#endif // CV_WORKER_AI
