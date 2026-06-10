//------------------------------------------------------------------------------------------------
//  FILE:    CvEmphasizeInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvHeritageInfo.h"
#include "CvGameAI.h"
#include "CvGameTextMgr.h"
#include "CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "CvPlayerAI.h"
#include "CvPython.h"
#include "CvXMLLoadUtility.h"
#include "CvXMLLoadUtilityModTools.h"
#include "CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvEmphasizeInfo.h"


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvEmphasizeInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvEmphasizeInfo::CvEmphasizeInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvEmphasizeInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvEmphasizeInfo::~CvEmphasizeInfo()
{
	// The yield/commerce arrays are owned by the declarative layer (addYields/addCommerce); it frees them.
	CvInfoUtil(this).uninitDataMembers();
}


bool CvEmphasizeInfo::isAvoidGrowth() const
{
	return m_bAvoidGrowth;
}


bool CvEmphasizeInfo::isGreatPeople() const
{
	return m_bGreatPeople;
}


bool CvEmphasizeInfo::isAvoidAngryCitizens() const
{
	return m_bAvoidAngryCitizens;
}


bool CvEmphasizeInfo::isAvoidUnhealthyCitizens() const
{
	return m_bAvoidUnhealthyCitizens;
}



// Arrays

int CvEmphasizeInfo::getYieldChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, i);
	return m_piYieldModifiers ? m_piYieldModifiers[i] : 0;
}


int CvEmphasizeInfo::getCommerceChange(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_piCommerceModifiers ? m_piCommerceModifiers[i] : 0;
}


//
// read from XML
//
void CvEmphasizeInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_bAvoidGrowth, L"bAvoidGrowth")
		.add(m_bGreatPeople, L"bGreatPeople")
		.addYields(m_piYieldModifiers, L"YieldModifiers")
		.addCommerce(m_piCommerceModifiers, L"CommerceModifiers")
		.add(m_bAvoidAngryCitizens, L"bAvoidAngryCitizens")
		.add(m_bAvoidUnhealthyCitizens, L"bAvoidUnhealthyCitizens")
	;
}

//
// read from XML
//
bool CvEmphasizeInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}

void CvEmphasizeInfo::copyNonDefaults(const CvEmphasizeInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

