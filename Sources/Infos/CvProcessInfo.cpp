//------------------------------------------------------------------------------------------------
//  FILE:    CvProcessInfo.cpp
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
#include "CvProcessInfo.h"


//======================================================================================================
//					CvProcessInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvProcessInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvProcessInfo::CvProcessInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvProcessInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvProcessInfo::~CvProcessInfo()
{
	// The commerce-modifier array is owned by the declarative layer (addCommerce); it frees it.
	CvInfoUtil(this).uninitDataMembers();
}


int CvProcessInfo::getProductionToCommerceModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, i);
	return m_paiProductionToCommerceModifier ? m_paiProductionToCommerceModifier[i] : 0;
}


void CvProcessInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnum(m_iTechPrereq, L"TechPrereq")
		.addCommerce(m_paiProductionToCommerceModifier, L"ProductionToCommerceModifiers")
	;
}


bool CvProcessInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvProcessInfo::copyNonDefaults(const CvProcessInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvProcessInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

