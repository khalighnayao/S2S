#pragma once

#ifndef CV_SPECIAL_UNIT_INFO_H
#define CV_SPECIAL_UNIT_INFO_H

#include "CvInfoBase.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvSpecialUnitInfo
//
//  DESC:
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvSpecialUnitInfo
	: public CvInfoBase
	, private bst::noncopyable
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvSpecialUnitInfo();
	virtual ~CvSpecialUnitInfo();

	bool isValid() const;
	bool isCityLoad() const;
	bool isSMLoadSame() const;

	// Arrays

	int getCombatPercent() const;
	int getWithdrawalChange() const;

	void getDataMembers(CvInfoUtil& util);
	bool read(CvXMLLoadUtility* pXML);

	void copyNonDefaults(const CvSpecialUnitInfo* pClassInfo);

	void getCheckSum(uint32_t& iSum) const;

	//----------------------PROTECTED MEMBER VARIABLES----------------------------

protected:

	bool m_bValid;
	bool m_bCityLoad;
	bool m_bSMLoadSame;

	int m_iCombatPercent;
	int m_iWithdrawalChange;
};

#endif // CV_SPECIAL_UNIT_INFO_H
