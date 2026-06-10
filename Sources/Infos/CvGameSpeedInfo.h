#pragma once

#ifndef CV_GAME_SPEED_INFO_H
#define CV_GAME_SPEED_INFO_H

#include "CvInfoBase.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvGameSpeedInfo
//
//  DESC:   A game speed scales costs/durations by iSpeedPercent and stretches the
//          game over proportionally more turns. Turn counts and calendar pacing
//          are derived per era from CvEraInfo's historical year span and
//          Normal-speed turn count (see CvDate) — nothing calendar-related is
//          stored here.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvGameSpeedInfo
	: public CvInfoBase
	, private bst::noncopyable
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvGameSpeedInfo();

	int getSpeedPercent() const;
	int getHammerCostPercent() const;
	int getUnitYieldScalePercent() const;

	// Era pacing at this speed, derived from CvEraInfo (not XML-backed).
	int getTurnsInEra(int iEra) const;
	int getEraStartTurn(int iEra) const;
	int getTotalTurns() const;
	int getTicksPerTurnInEra(int iEra) const;

	void getDataMembers(CvInfoUtil& util);
	bool read(CvXMLLoadUtility* pXML);
	void copyNonDefaults(const CvGameSpeedInfo* pClassInfo);
	void getCheckSum(uint32_t& iSum) const;

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	int m_iSpeedPercent;
	// Scale for unit-produced yields (e.g. subdued-animal food/production), the
	// <AdaptUnitYield> expression channel. Grows slower than iSpeedPercent
	// (~sqrt) so yields don't outpace the longer research/build times.
	int m_iUnitYieldScalePercent;
};

#endif // CV_GAME_SPEED_INFO_H
