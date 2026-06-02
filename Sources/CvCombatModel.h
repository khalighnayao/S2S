#pragma once

#ifndef CV_COMBAT_MODEL_H
#define CV_COMBAT_MODEL_H

#include <string>
#include <vector>

class CvUnit;

// ---------------------------------------------------------------------------
// CvCombatModel -- the single home for combat-probability math.
//
// Extracted from CvGameCoreUtils so combat odds live in one trackable,
// modifiable place. This is the foundation of the combat-odds unification:
//   - getCombatOdds          : attacker win probability, out of 1000.
//   - getCombatOddsSpecific  : probability of one specific outcome (n_A hits
//                              taken by attacker, n_D by defender); the ACO
//                              ("Advanced Combat Odds") distribution. win% is a
//                              sum over this.
//
// Subsequent phases fold the AI per-unit odds (CvUnitAI::AI_attackOddsAtPlot*)
// and the per-round resolution model (CvUnit::getDefenderCombatValues /
// resolveCombat) onto this same engine so prediction and resolution can never
// diverge again. See Sources/docs/combat-odds-baseline.md for the regression
// reference.
// ---------------------------------------------------------------------------

int getCombatOdds(const CvUnit* pAttacker, const CvUnit* pDefender);
float getCombatOddsSpecific(const CvUnit* pAttacker, const CvUnit* pDefender, int n_A, int n_D);

// ---------------------------------------------------------------------------
// Combat preview -- the single data source for the in-game combat-odds tooltip.
//
// computeCombatPreview() runs the full ACO distribution once and returns every
// number the UI needs: the four outcome probabilities, expected end-HP and XP
// per outcome, needed rounds, strengths, and the measured first-strike effect
// (win odds with vs. without first strikes). CvGameTextMgr is then a pure
// renderer of this struct -- it does no combat math of its own.
//
// detailLines is the extension seam: when a combat-mod rule (e.g. a future
// FightOrFlight plugin) is active it appends its own labelled rows here, and
// the renderer prints them generically. No per-rule UI edits required.
// ---------------------------------------------------------------------------

enum CombatPreviewCategory
{
	COMBAT_PREVIEW_NEUTRAL,
	COMBAT_PREVIEW_POSITIVE,
	COMBAT_PREVIEW_NEGATIVE,
};

struct CombatPreviewLine
{
	std::wstring szLabel;
	float fValue;                       // percentage (0..100) or HP, per category
	CombatPreviewCategory eCategory;

	CombatPreviewLine(const std::wstring& label, float value, CombatPreviewCategory cat)
		: szLabel(label), fValue(value), eCategory(cat) {}
};

struct CombatPreview
{
	bool bValid;                        // false if inputs are degenerate

	int iAttackerStrength;              // currCombatStr, x100 (display divides)
	int iDefenderStrength;

	int iNeededRoundsAttacker;          // hits attacker must land to win
	int iNeededRoundsDefender;          // hits attacker can take before dying
	int iDamageToAttacker;              // HP lost per landed defender hit
	int iDamageToDefender;

	// Primary outcome probabilities (0..1). Exactly one of kill/pull-out is > 0.
	float fAttackerKillOdds;            // defender destroyed
	float fPullOutOdds;                 // attacker stops at combat limit (cannot kill)
	float fRetreatOdds;                 // attacker withdraws (vanilla withdrawal)
	float fDefenderKillOdds;            // attacker destroyed

	// Expected end-HP per outcome, already normalised (renderer-ready).
	float fExpHPAttackerWin;            // attacker HP if it kills the defender
	float fExpHPAttackerPullOut;        // attacker HP if it stops at combat limit
	int   iExpHPAttackerRetreat;        // attacker HP if it withdraws (deterministic)
	float fExpHPDefenderWin;            // defender HP if it defeats/outlasts attacker
	int   iDefenderHitLimitHP;          // defender's HP floor when combat-limited

	// Base XP rewards (before any experience-rate modifier).
	int iVictoryXP;                     // attacker XP on kill / pull-out
	int iRetreatXP;                     // attacker XP on withdrawal
	int iDefenderKillXP;                // defender XP for killing the attacker

	// First strikes and their measured effect on the win chance.
	int iAttackerFirstStrikes, iAttackerFirstStrikeChances;
	int iDefenderFirstStrikes, iDefenderFirstStrikeChances;
	int iWinOddsWithFS;                 // attacker win prob out of 1000 (first strikes active)
	int iWinOddsNoFS;                   // attacker win prob out of 1000 (first strikes suppressed)

	// Extension seam for combat-mod rules; empty in the vanilla engine.
	std::vector<CombatPreviewLine> detailLines;
};

CombatPreview computeCombatPreview(const CvUnit* pAttacker, const CvUnit* pDefender);

#endif // CV_COMBAT_MODEL_H
