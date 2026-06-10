#pragma once

#ifndef CV_GAME_SPEED_INFO_H
#define CV_GAME_SPEED_INFO_H

#include "CvInfoBase.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvGameSpeedInfo
//
//  DESC:
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvGameSpeedInfo
	: public CvInfoBase
	, private bst::noncopyable
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvGameSpeedInfo();
	virtual ~CvGameSpeedInfo();

	int getSpeedPercent() const;
	int getHammerCostPercent() const;
	int getUnitYieldScalePercent() const;
	int getNumTurnIncrements() const;

	const GameTurnInfo& getGameTurnInfo(int iIndex) const;
	const CvDateIncrement& getDateIncrement(int iIndex) const;
	std::vector<CvDateIncrement>& getIncrements();
	bool getEndDatesCalculated() const;
	void setEndDatesCalculated(bool bCalculated);

	void allocateGameTurnInfos(int iSize);

	bool read(CvXMLLoadUtility* pXML);
	void copyNonDefaults(const CvGameSpeedInfo* pClassInfo);
	void getCheckSum(uint32_t& iSum) const;

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	int m_iSpeedPercent;
	// Scale for unit-produced yields (e.g. subdued-animal food/production), the
	// ADAPT_UNIT_YIELD channel of <AdaptUnitYield> expressions. Grows slower than
	// iSpeedPercent (~sqrt) so yields don't outpace the longer research/build times.
	int m_iUnitYieldScalePercent;
	int m_iNumTurnIncrements;

	GameTurnInfo* m_pGameTurnInfo;
	std::vector<CvDateIncrement> m_aIncrements;
	bool m_bEndDatesCalculated;
};

#endif // CV_GAME_SPEED_INFO_H
