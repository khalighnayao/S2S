#include "CvGameCoreDLL.h"
#include "CvCombatModel.h"

#include "FProfiler.h"

#include "CvGameAI.h"
#include "CvGame.h"
#include "CvGlobals.h"
#include "CvGameCoreUtils.h" // getBinomialCoefficient, range
#include "CvInfos.h"
#include "CvPlayerAI.h"
#include "CvPlot.h"
#include "CvUnit.h"

#include <cmath> // pow

// ---------------------------------------------------------------------------
// Combat-odds engine. Bodies relocated verbatim from CvGameCoreUtils.cpp (no
// math change) as Phase 1 of the combat-odds unification; later phases route the
// AI and resolution paths through here. See CvCombatModel.h.
// ---------------------------------------------------------------------------

// FUNCTION: getCombatOdds
// Calculates combat odds, given two units
// Returns value from 0-1000
// Written by DeepO
//
// bSuppressFirstStrikes computes the same win probability as if neither unit had
// any first strikes (used to measure the first-strike contribution for the UI).
static int getCombatOddsImpl(const CvUnit* pAttacker, const CvUnit* pDefender, bool bSuppressFirstStrikes)
{
	PROFILE_EXTRA_FUNC();

	// setup battle, calculate strengths and odds

	//TB Combat Mod begin
	//Added ST
	int iAttackerStrength = pAttacker->currCombatStr(NULL, NULL);
	int iAttackerFirepower = pAttacker->currFirepower(NULL, NULL);
	int iDefenderStrength = pDefender->currCombatStr(pDefender->plot(), pAttacker);
	int iDefenderFirepower = pDefender->currFirepower(pDefender->plot(), pAttacker);

	FAssert(iAttackerStrength + iDefenderStrength > 0);
	FAssert(iAttackerFirepower + iDefenderFirepower > 0);

	// Plain strength-ratio odds (precision/dodge hit-modifier removed).
	const int iDefenderOdds = GC.getCOMBAT_DIE_SIDES() * iDefenderStrength / std::max(1, iAttackerStrength + iDefenderStrength);
	const int iAttackerOdds = GC.getCOMBAT_DIE_SIDES() - iDefenderOdds;
	if (iDefenderOdds == 0)
	{
		return 1000;
	}
	if (iAttackerOdds == 0)
	{
		return 0;
	}
	int iOdds = 0;
	const int iStrengthFactor = (iAttackerFirepower + iDefenderFirepower + 1) / 2;

	// calculate damage done in one round (armor/puncture mitigation removed)
	const int iDamageToAttackerBase = GC.getCOMBAT_DAMAGE() * (iDefenderFirepower + iStrengthFactor) / std::max(1, iAttackerFirepower + iStrengthFactor);
	const int iDamageToDefenderBase = GC.getCOMBAT_DAMAGE() * (iAttackerFirepower + iStrengthFactor) / std::max(1, iDefenderFirepower + iStrengthFactor);

	const int iDamageToAttacker = std::max(1, iDamageToAttackerBase * (100 + pDefender->damageModifierTotal()));
	const int iDamageToDefender = std::max(1, iDamageToDefenderBase * (100 + pAttacker->damageModifierTotal()));

	// calculate needed rounds.
	// Needed rounds = round_up(health/damage)
	int iNeededRoundsAttacker = (std::max(0, 100*pDefender->getHP() + 100*pAttacker->combatLimit(pDefender) - 100*pDefender->getMaxHP()) + iDamageToDefender - 100) / iDamageToDefender;
	int iNeededRoundsDefender = (100*pAttacker->getHP() + iDamageToAttacker - 100) / iDamageToAttacker;
	if (iNeededRoundsAttacker > 20 * iNeededRoundsDefender || iNeededRoundsDefender > 20 * iNeededRoundsAttacker || iNeededRoundsAttacker + iNeededRoundsDefender > 100)
	{
		// Solves inaccuracies that arise when combat involves an extremely weak unit like a pigeon
		//	Where a very strong unit would get max XP from combat against the very weak one because the combat odds
		//	here would evaluate to less than 1% chance of winning due to the math for fOddsAfterEvent breaking down.
		// The tiny overall reduction in accuracy by doing this for these edge cases is worth the efficiency boost and math integrity.
		iNeededRoundsAttacker /= 10;
		iNeededRoundsDefender /= 10;
	}
	// Clamp to a minimum of 1: a unit can never need 0 rounds to be defeated.
	// The /=10 scaling above can floor a small round count (e.g. 2) to 0 when the
	// other side's count is huge (an overwhelmingly strong attacker vs a ~0.01
	// strength animal). A 0 here makes the per-round probability accumulation below
	// degenerate and report a near-zero win chance for an attacker that should win
	// ~100% -- which made hunters refuse trivial kills. The previous guard only
	// caught negative values, so a floored 0 slipped through.
	if (iNeededRoundsAttacker < 1) iNeededRoundsAttacker = 1;
	if (iNeededRoundsDefender < 1) iNeededRoundsDefender = 1;
	const int iMaxRounds = iNeededRoundsAttacker + iNeededRoundsDefender - 1;

	// calculate possible first strikes distribution.
	// We can't use the getCombatFirstStrikes() function (only one result, no distribution), so we need to mimic it.
	const int iAttackerLowFS = (bSuppressFirstStrikes || pDefender->immuneToFirstStrikes()) ? 0 : pAttacker->firstStrikes();
	const int iAttackerHighFS = (bSuppressFirstStrikes || pDefender->immuneToFirstStrikes()) ? 0 : pAttacker->firstStrikes() + pAttacker->chanceFirstStrikes();

	const int iDefenderLowFS = (bSuppressFirstStrikes || pAttacker->immuneToFirstStrikes()) ? 0 : pDefender->firstStrikes();
	const int iDefenderHighFS = (bSuppressFirstStrikes || pAttacker->immuneToFirstStrikes()) ? 0 : (pDefender->firstStrikes() + pDefender->chanceFirstStrikes());

	// For every possible first strike event, calculate the odds of combat.
	// Then, add these to the total, weighted to the chance of that first strike event occurring
	for (int i1 = iAttackerLowFS; i1 < iAttackerHighFS + 1; i1++)
	{
		for (int i2 = iDefenderLowFS; i2 < iDefenderHighFS + 1; i2++)
		{
			// for every possible combination of fs results, calculate the chance

			if (i1 >= i2)
			{
				// Attacker gets more or equal first strikes than defender
				const int iFirstStrikes = i1 - i2;

				// For every possible first strike getting hit, calculate both
				// the chance of that event happening, as well as the rest of
				// the chance assuming the event has happened. Multiply these
				// together to get the total chance (Bayes rule).
				// i3 counts the number of successful first strikes
				for (int i3 = 0; i3 < iFirstStrikes + 1; i3++)
				{
					// event: i3 first strikes hit the defender

					// calculate chance of i3 first strikes hitting: fOddsEvent
					// f(k;n,p)=C(n,k)*(p^k)*((1-p)^(n-k))
					// this needs to be in floating point math
					const float fOddsEvent = (
						((float)getBinomialCoefficient(iFirstStrikes, i3))
						* pow((((float)iAttackerOdds) / GC.getCOMBAT_DIE_SIDES()), i3)
						* pow(1.0f - ((float)iAttackerOdds) / GC.getCOMBAT_DIE_SIDES(), iFirstStrikes - i3)
					);
					// calculate chance assuming i3 first strike hits: fOddsAfterEvent
					float fOddsAfterEvent = 1;

					if (i3 < iNeededRoundsAttacker)
					{
						// odds for _at_least_ (iNeededRoundsAttacker - i3) (the remaining hits the attacker needs to make)
						// out of (iMaxRounds - i3) (the left over rounds) is the sum of each _exact_ draw
						fOddsAfterEvent = 0;

						for (int i4 = iNeededRoundsAttacker - i3; i4 < iMaxRounds - i3 + 1; i4++)
						{
							// odds of exactly i4 out of (iMaxRounds - i3) draws.
							// f(k;n,p)=C(n,k)*(p^k)*((1-p)^(n-k))
							// this needs to be in floating point math
							fOddsAfterEvent += (
								((float)getBinomialCoefficient(iMaxRounds - i3, i4))
								* pow(((float)iAttackerOdds) / GC.getCOMBAT_DIE_SIDES(), i4)
								* pow(1.0f - ((float)iAttackerOdds) / GC.getCOMBAT_DIE_SIDES(), iMaxRounds - i3 - i4)
							);
						}
					}
					// Multiply these together, round them properly, and add
					// the result to the total iOdds
					iOdds += (int)(1000 * (fOddsEvent * fOddsAfterEvent + 0.0005));
				}
			}
			else // (i1 < i2)
			{
				// Attacker gets less first strikes than defender
				const int iFirstStrikes = i2 - i1;

				// For every possible first strike getting hit, calculate both
				// the chance of that event happening, as well as the rest of
				// the chance assuming the event has happened. Multiply these
				// together to get the total chance (Bayes rule).
				// i3 counts the number of successful first strikes
				for (int i3 = 0; i3 < (iFirstStrikes + 1); i3++)
				{
					// event: i3 first strikes hit the defender

					// First of all, check if the attacker is still alive.
					// Otherwise, no further calculations need to occur
					if (i3 < iNeededRoundsDefender)
					{
						// Calculate chance of i3 first strikes hitting: fOddsEvent
						// f(k;n,p)=C(n,k)*(p^k)*((1-p)^(n-k))
						// This needs to be in floating point math
						const float fOddsEvent = ((float)getBinomialCoefficient(iFirstStrikes, i3)) * pow((((float)iDefenderOdds) / GC.getCOMBAT_DIE_SIDES()), i3) * pow((1.0f - (((float)iDefenderOdds) / GC.getCOMBAT_DIE_SIDES())), (iFirstStrikes - i3));

						// calculate chance assuming i3 first strike hits: fOddsAfterEvent
						float fOddsAfterEvent = 0;

						// odds for _at_least_ iNeededRoundsAttacker (the remaining hits the attacker needs to make)
						// out of (iMaxRounds - i3) (the left over rounds) is the sum of each _exact_ draw
						for (int i4 = iNeededRoundsAttacker; i4 < iMaxRounds - i3 + 1; i4++)
						{
							// Odds of exactly i4 out of (iMaxRounds - i3) draws.
							// f(k;n,p)=C(n,k)*(p^k)*((1-p)^(n-k))
							// This needs to be in floating point math
							fOddsAfterEvent += (
								((float)getBinomialCoefficient(iMaxRounds - i3, i4))
								* pow((((float)iAttackerOdds) / GC.getCOMBAT_DIE_SIDES()), i4)
								* pow(1.0f - ((float)iAttackerOdds) / GC.getCOMBAT_DIE_SIDES(), iMaxRounds - i3 - i4)
							);
						}
						// Multiply these together, round them properly, and add the result to the total iOdds
						iOdds += (int)(1000 * (fOddsEvent*fOddsAfterEvent + 0.0005));
					}
				}
			}
		}
	}

	// Weigh the total to the number of possible combinations of first strikes events
	// note: the integer math breaks down when #FS > 656 (with a die size of 1000)
	iOdds /= ((bSuppressFirstStrikes || pDefender->immuneToFirstStrikes() ? 0 : pAttacker->chanceFirstStrikes()) + 1) * ((bSuppressFirstStrikes || pAttacker->immuneToFirstStrikes() ? 0 : pDefender->chanceFirstStrikes()) + 1);

	FASSERT_BOUNDS(0, 1001, iOdds);
	return iOdds;
}

int getCombatOdds(const CvUnit* pAttacker, const CvUnit* pDefender)
{
	return getCombatOddsImpl(pAttacker, pDefender, false);
}

/*************************************************************************************************/
/** ADVANCED COMBAT ODDS                      11/7/09                           PieceOfMind      */
/** BEGIN                                                                       v1.1             */
/*************************************************************************************************/

//Calculates the probability of a particular combat outcome
//Returns a float value (between 0 and 1)
//Written by PieceOfMind
//n_A = hits taken by attacker, n_D = hits taken by defender.
float getCombatOddsSpecific(const CvUnit* pAttacker, const CvUnit* pDefender, int n_A, int n_D)
{
	PROFILE_EXTRA_FUNC();

	int iAttackerStrength = pAttacker->currCombatStr(NULL, NULL);
	int iAttackerFirepower = pAttacker->currFirepower(NULL, NULL);
	int iDefenderStrength = pDefender->currCombatStr(pDefender->plot(), pAttacker);
	int iDefenderFirepower = pDefender->currFirepower(pDefender->plot(), pAttacker);

	//TB Combat Mods End

	const int iStrengthFactor = ((iAttackerFirepower + iDefenderFirepower + 1) / 2);

	const int iDamageToAttackerBase = GC.getCOMBAT_DAMAGE() * (iDefenderFirepower + iStrengthFactor) / std::max(1, iAttackerFirepower + iStrengthFactor);
	const int iDamageToDefenderBase = GC.getCOMBAT_DAMAGE() * (iAttackerFirepower + iStrengthFactor) / std::max(1, iDefenderFirepower + iStrengthFactor);

	const int iDamageToAttacker = std::max(1, iDamageToAttackerBase + iDamageToAttackerBase * pDefender->damageModifierTotal() / 100);
	const int iDamageToDefender = std::max(1, iDamageToDefenderBase + iDamageToDefenderBase * pAttacker->damageModifierTotal() / 100);

	// Plain strength-ratio odds (precision/dodge hit-modifier removed).
	int iDefenderOdds = GC.getCOMBAT_DIE_SIDES() * iDefenderStrength / (iAttackerStrength + iDefenderStrength);
	int iAttackerOdds = GC.getCOMBAT_DIE_SIDES() - iDefenderOdds;

	if (GC.getDefineINT("ACO_IgnoreBarbFreeWins")==0)
	{
		if (pDefender->isHominid())
		{
			//defender is barbarian
			if (!GET_PLAYER(pAttacker->getOwner()).isHominid() && GET_PLAYER(pAttacker->getOwner()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(pAttacker->getOwner()).getHandicapType()).getFreeWinsVsBarbs())
			{
				//attacker is not barb and attacker player has free wins left
				//I have assumed in the following code only one of the units (attacker and defender) can be a barbarian

				iDefenderOdds = std::min((10 * GC.getCOMBAT_DIE_SIDES()) / 100, iDefenderOdds);
				iAttackerOdds = std::max((90 * GC.getCOMBAT_DIE_SIDES()) / 100, iAttackerOdds);
			}
		}
		else if (pAttacker->isHominid())
		{
			//attacker is barbarian
			if (!GET_PLAYER(pDefender->getOwner()).isHominid() && GET_PLAYER(pDefender->getOwner()).getWinsVsBarbs() < GC.getHandicapInfo(GET_PLAYER(pDefender->getOwner()).getHandicapType()).getFreeWinsVsBarbs())
			{
				//defender is not barbarian and defender has free wins left and attacker is barbarian
				iAttackerOdds = std::min((10 * GC.getCOMBAT_DIE_SIDES()) / 100, iAttackerOdds);
				iDefenderOdds = std::max((90 * GC.getCOMBAT_DIE_SIDES()) / 100, iDefenderOdds);
			}
		}
	}

	const int iNeededRoundsAttacker = (pDefender->getHP() - pDefender->getMaxHP() + pAttacker->combatLimit(pDefender) - (((pAttacker->combatLimit(pDefender))==pDefender->getMaxHP())?1:0))/iDamageToDefender + 1;
	const int iNeededRoundsDefender = (pAttacker->getHP() + iDamageToAttacker - 1 ) / iDamageToAttacker;

	const int N_D = (
		(
			std::max(0, pDefender->getHP() + pAttacker->combatLimit(pDefender) - pDefender->getMaxHP())
			+ iDamageToDefender
			- (pAttacker->combatLimit(pDefender) == pDefender->getMaxHP())
		)
		/ iDamageToDefender
	);
	const int N_A = 1 + (pAttacker->getHP() - 1) / iDamageToAttacker;

	// Vanilla attacker withdrawal only (TB pursuit/knockback/repel/defender-withdrawal removed).
	const float RetreatOdds = range(pAttacker->withdrawalProbability(), 0, 100) / 100.0f;

	const int AttFSnet = (pDefender->immuneToFirstStrikes() ? 0 : pAttacker->firstStrikes()) - (pAttacker->immuneToFirstStrikes() ? 0 : pDefender->firstStrikes());
	const int AttFSC = pDefender->immuneToFirstStrikes() ? 0 : pAttacker->chanceFirstStrikes();
	const int DefFSC = (pAttacker->immuneToFirstStrikes()) ? 0 : (pDefender->chanceFirstStrikes());

	const float P_A = ((float)iAttackerOdds) / GC.getDefineINT("COMBAT_DIE_SIDES");
	const float P_D = ((float)iDefenderOdds) / GC.getDefineINT("COMBAT_DIE_SIDES");
	float answer = 0.0f;
	// (1) Defender dies or is taken to combat limit
	if (n_A < N_A && n_D == iNeededRoundsAttacker)
	{
		float sum1 = 0.0f;
		for (int i = (-AttFSnet-AttFSC < 1 ? 1 : -AttFSnet - AttFSC); i <= DefFSC - AttFSnet; i++)
		{
			for (int j = 0; j <= i; j++)
			{
				if (n_A >= j)
				{
					sum1 += (float)getBinomialCoefficient(i,j) * pow(P_A,(float)(i-j)) * getBinomialCoefficient(iNeededRoundsAttacker-1+n_A-j,iNeededRoundsAttacker-1);
				}
			}
		}
		sum1 *= pow(P_D,(float)n_A)*pow(P_A,(float)iNeededRoundsAttacker);
		answer += sum1;

		float sum2 = 0.0f;

		for (int i = (0 < AttFSnet - DefFSC ? AttFSnet - DefFSC : 0); i <= AttFSnet + AttFSC; i++)
		{
			for (int j = 0; j <= i; j++)
			{
				if (N_D > j)
				{
					sum2 = sum2 + getBinomialCoefficient(n_A+iNeededRoundsAttacker-j-1,n_A) * (float)getBinomialCoefficient(i,j) * pow(P_A,(float)iNeededRoundsAttacker) * pow(P_D,(float)(n_A+i-j));
				}
				else if (n_A == 0)
				{
					sum2 = sum2 + (float)getBinomialCoefficient(i,j) * pow(P_A,(float)j) * pow(P_D,(float)(i-j));
				}
				else sum2 = sum2 + 0.0f;
			}
		}
		answer += sum2;
	}
	// (2) Attacker dies!
	else if (n_D < N_D && n_A == N_A)
	{
		float sum1 = 0.0f;
		for (int i = (-AttFSnet - AttFSC < 1 ? 1 : -AttFSnet - AttFSC); i <= DefFSC - AttFSnet; i++)
		{
			for (int j = 0; j <= i; j++)
			{
				if (N_A > j)
				{
					sum1 += getBinomialCoefficient(n_D+N_A-j-1,n_D) * (float)getBinomialCoefficient(i,j) * pow(P_D,(float)(N_A)) * pow(P_A,(float)(n_D+i-j));
				}
				else if (n_D == 0)
				{
					sum1 += (float)getBinomialCoefficient(i,j) * pow(P_D,(float)(j)) * pow(P_A,(float)(i-j));
				}
			}
		}
		answer += sum1;

		float sum2 = 0.0f;

		for (int i = (0 < AttFSnet-DefFSC ? AttFSnet - DefFSC : 0); i <= AttFSnet + AttFSC; i++)
		{
			for (int j = 0; j <= i; j++)
			{
				if (n_D >= j)
				{
					sum2 += (float)getBinomialCoefficient(i,j) * pow(P_D,(float)(i-j)) * getBinomialCoefficient(N_A-1+n_D-j,N_A-1);
				}
			}
		}
		sum2 *= pow(P_A,(float)(n_D))*pow(P_D,(float)(N_A));
		answer += sum2;
		answer = answer * (1.0f - RetreatOdds);
	}
	// (3) Attacker retreats (vanilla withdrawal)
	else if (n_A == (N_A-1) && n_D < N_D)
	{
		float sum1 = 0.0f;
		for (int i = (AttFSnet+AttFSC>-1?1:-AttFSnet-AttFSC); i <= DefFSC - AttFSnet; i++)
		{
			for (int j = 0; j <= i; j++)
			{
				if (N_A > j)
				{
					sum1 += getBinomialCoefficient(n_D+N_A-j-1,n_D) * (float)getBinomialCoefficient(i,j) * pow(P_D,(float)(N_A)) * pow(P_A,(float)(n_D+i-j));
				}
				else if (n_D == 0)
				{
					sum1 += (float)getBinomialCoefficient(i,j) * pow(P_D,(float)(j)) * pow(P_A,(float)(i-j));
				}
			}
		}
		answer += sum1;

		float sum2 = 0.0f;
		for (int i = (0 < AttFSnet-DefFSC ? AttFSnet-DefFSC : 0); i <= AttFSnet + AttFSC; i++)
		{
			for (int j = 0; j <= i; j++)
			{
				if (n_D >= j)
				{
					sum2 += (float)getBinomialCoefficient(i,j) * pow(P_D,(float)(i-j)) * getBinomialCoefficient(N_A-1+n_D-j,N_A-1);
				}
			}
		}
		sum2 *= pow(P_A,(float)(n_D))*pow(P_D,(float)(N_A));
		answer += sum2;
		answer = answer * RetreatOdds;
	}
	else FErrorMsg("Unexpected value.  Process should not reach here.");

	// dividing by (t+w+1) as is necessary
	return answer / ((float)(AttFSC + DefFSC + 1));
}
// !getCombatOddsSpecific

// I had to add this function to the header file CvGameCoreUtils.h
/*************************************************************************************************/
/** ADVANCED COMBAT ODDS                      11/7/09                           PieceOfMind      */
/** END                                                                                          */
/*************************************************************************************************/

// Runs the full ACO distribution once and packages every figure the combat
// tooltip needs. All math here is lifted verbatim from the old inline body of
// CvGameTextMgr::setCombatPlotHelp -- the renderer is now a pure consumer.
CombatPreview computeCombatPreview(const CvUnit* pAttacker, const CvUnit* pDefender)
{
	PROFILE_EXTRA_FUNC();

	CombatPreview kP;
	kP.bValid = false;
	kP.iAttackerStrength = kP.iDefenderStrength = 0;
	kP.iNeededRoundsAttacker = kP.iNeededRoundsDefender = 0;
	kP.iDamageToAttacker = kP.iDamageToDefender = 0;
	kP.fAttackerKillOdds = kP.fPullOutOdds = kP.fRetreatOdds = kP.fDefenderKillOdds = 0.0f;
	kP.fExpHPAttackerWin = kP.fExpHPAttackerPullOut = kP.fExpHPDefenderWin = 0.0f;
	kP.iExpHPAttackerRetreat = 0;
	kP.iDefenderHitLimitHP = 0;
	kP.iVictoryXP = kP.iRetreatXP = kP.iDefenderKillXP = 0;
	kP.iAttackerFirstStrikes = kP.iAttackerFirstStrikeChances = 0;
	kP.iDefenderFirstStrikes = kP.iDefenderFirstStrikeChances = 0;
	kP.iWinOddsWithFS = kP.iWinOddsNoFS = 0;

	if (pAttacker == NULL || pDefender == NULL)
	{
		return kP;
	}

	const CvPlot* pPlot = pDefender->plot();

	const int iAttackerStrength = pAttacker->currCombatStr(NULL, NULL);
	const int iAttackerFirepower = pAttacker->currFirepower(NULL, NULL);
	const int iDefenderStrength = std::max(1, pDefender->currCombatStr(pPlot, pAttacker));
	const int iDefenderFirepower = std::max(1, pDefender->currFirepower(pPlot, pAttacker));

	if (iAttackerStrength <= 0)
	{
		return kP;
	}

	// --- per-round damage and needed rounds (matches setCombatPlotHelp) ---
	const int iStrengthFactor = (iAttackerFirepower + iDefenderFirepower + 1) / 2;
	const int iDamageToAttackerBase = (GC.getCOMBAT_DAMAGE() * (iDefenderFirepower + iStrengthFactor)) / std::max(1, iAttackerFirepower + iStrengthFactor);
	const int iDamageToDefenderBase = (GC.getCOMBAT_DAMAGE() * (iAttackerFirepower + iStrengthFactor)) / std::max(1, iDefenderFirepower + iStrengthFactor);
	const int iDamageToAttacker = std::max(1, iDamageToAttackerBase + (iDamageToAttackerBase * pDefender->damageModifierTotal()) / 100);
	const int iDamageToDefender = std::max(1, iDamageToDefenderBase + (iDamageToDefenderBase * pAttacker->damageModifierTotal()) / 100);

	const int iCombatLimit = pAttacker->combatLimit(pDefender);
	const bool bCanKill = (iCombatLimit >= pDefender->getMaxHP());
	const int iNeededRoundsAttacker = (pDefender->getHP() - pDefender->getMaxHP() + iCombatLimit - (bCanKill ? 1 : 0)) / iDamageToDefender + 1;
	const int iNeededRoundsDefender = (pAttacker->getHP() + iDamageToAttacker - 1) / iDamageToAttacker;
	const int iDefenderHitLimit = pDefender->getMaxHP() - iCombatLimit;

	// --- outcome probabilities ---
	float AttackerKillOdds = 0.0f, PullOutOdds = 0.0f, RetreatOdds = 0.0f, DefenderKillOdds = 0.0f;
	if (bCanKill)
	{
		for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
		{
			AttackerKillOdds += getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
		}
	}
	else
	{
		for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
		{
			PullOutOdds += getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
		}
	}
	if (pAttacker->withdrawalProbability() > 0)
	{
		for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
		{
			RetreatOdds += getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D);
		}
	}
	for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
	{
		DefenderKillOdds += getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D);
	}

	const int iDefenderOdds = (GC.getCOMBAT_DIE_SIDES() * iDefenderStrength) / std::max(1, iAttackerStrength + iDefenderStrength);
	if (iDefenderOdds == 0)
	{
		DefenderKillOdds = 0.0f; // guaranteed no defender hit
	}

	// --- expected end-HP per outcome ---
	// Attacker HP if it survives to a decisive result (victory or pull-out share
	// this numerator; exactly one denominator is non-zero).
	float E_HP_Att = 0.0f;
	for (int n_A = 0; n_A < iNeededRoundsDefender; n_A++)
	{
		E_HP_Att += (pAttacker->getHP() - n_A * iDamageToAttacker) * getCombatOddsSpecific(pAttacker, pDefender, n_A, iNeededRoundsAttacker);
	}
	const int iExpHPAttackerRetreat = pAttacker->getHP() - (iNeededRoundsDefender - 1) * iDamageToAttacker;

	// Defender HP given the attacker dies or retreats (i.e. the defender wins out).
	float E_HP_Def_Defeat = 0.0f;
	for (int n_D = 0; n_D < iNeededRoundsAttacker; n_D++)
	{
		E_HP_Def_Defeat += (pDefender->getHP() - n_D * iDamageToDefender)
			* (getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender, n_D)
			 + getCombatOddsSpecific(pAttacker, pDefender, iNeededRoundsDefender - 1, n_D));
	}

	kP.fAttackerKillOdds = AttackerKillOdds;
	kP.fPullOutOdds = PullOutOdds;
	kP.fRetreatOdds = RetreatOdds;
	kP.fDefenderKillOdds = DefenderKillOdds;
	kP.fExpHPAttackerWin = (AttackerKillOdds > 0.0f) ? E_HP_Att / AttackerKillOdds : 0.0f;
	kP.fExpHPAttackerPullOut = (PullOutOdds > 0.0f) ? E_HP_Att / PullOutOdds : 0.0f;
	kP.iExpHPAttackerRetreat = iExpHPAttackerRetreat;
	kP.fExpHPDefenderWin = (iDefenderOdds != 0 && (RetreatOdds + DefenderKillOdds) > 0.0f)
		? E_HP_Def_Defeat / (RetreatOdds + DefenderKillOdds) : 0.0f;
	kP.iDefenderHitLimitHP = iDefenderHitLimit;

	// --- XP rewards (base, pre experience-rate modifier) ---
	int iVictoryXP;
	if (pAttacker->combatLimit() < 100)
	{
		iVictoryXP = pAttacker->getExperiencefromWithdrawal(100) / 100;
	}
	else
	{
		iVictoryXP = range((pAttacker->attackXPValue() * iDefenderStrength) / iAttackerStrength,
			GC.getMIN_EXPERIENCE_PER_COMBAT(), GC.getMAX_EXPERIENCE_PER_COMBAT());
	}
	int iRetreatXP = pAttacker->getExperiencefromWithdrawal(pAttacker->withdrawalProbability()) / 100;
	const int iDefenderKillXP = range((pDefender->defenseXPValue() * iAttackerStrength) / iDefenderStrength,
		GC.getMIN_EXPERIENCE_PER_COMBAT(), GC.getMAX_EXPERIENCE_PER_COMBAT());

	if (pDefender->isNPC())
	{
		const int iPotential = (pDefender->isAnimal() ? GC.getANIMAL_MAX_XP_VALUE() : GC.getBARBARIAN_MAX_XP_VALUE()) - pAttacker->getExperience();
		if (iPotential > 0)
		{
			iVictoryXP = range(iVictoryXP, 0, iPotential);
			iRetreatXP = range(iRetreatXP, 0, iPotential);
		}
		else
		{
			iVictoryXP = 0;
			iRetreatXP = 0;
		}
	}
	kP.iVictoryXP = iVictoryXP;
	kP.iRetreatXP = iRetreatXP;
	kP.iDefenderKillXP = iDefenderKillXP;

	// --- strengths, rounds, first strikes and their measured win-swing ---
	kP.iAttackerStrength = iAttackerStrength;
	kP.iDefenderStrength = iDefenderStrength;
	kP.iNeededRoundsAttacker = iNeededRoundsAttacker;
	kP.iNeededRoundsDefender = iNeededRoundsDefender;
	kP.iDamageToAttacker = iDamageToAttacker;
	kP.iDamageToDefender = iDamageToDefender;

	kP.iAttackerFirstStrikes = pDefender->immuneToFirstStrikes() ? 0 : pAttacker->firstStrikes();
	kP.iAttackerFirstStrikeChances = pDefender->immuneToFirstStrikes() ? 0 : pAttacker->chanceFirstStrikes();
	kP.iDefenderFirstStrikes = pAttacker->immuneToFirstStrikes() ? 0 : pDefender->firstStrikes();
	kP.iDefenderFirstStrikeChances = pAttacker->immuneToFirstStrikes() ? 0 : pDefender->chanceFirstStrikes();

	kP.iWinOddsWithFS = getCombatOddsImpl(pAttacker, pDefender, false);
	kP.iWinOddsNoFS = getCombatOddsImpl(pAttacker, pDefender, true);

	kP.bValid = true;
	return kP;
}
