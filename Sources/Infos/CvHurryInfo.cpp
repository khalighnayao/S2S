//------------------------------------------------------------------------------------------------
//  FILE:    CvHurryInfo.cpp
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
#include "CvHurryInfo.h"


//======================================================================================================
//					CvHurryInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvHurryInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvHurryInfo::CvHurryInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvHurryInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvHurryInfo::~CvHurryInfo()
{
}


int CvHurryInfo::getGoldPerProduction() const
{
	return m_iGoldPerProduction;
}


int CvHurryInfo::getProductionPerPopulation() const
{
	return m_iProductionPerPopulation;
}


bool CvHurryInfo::isAnger() const
{
	return m_bAnger;
}


void CvHurryInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iGoldPerProduction, L"iGoldPerProduction")
		.add(m_iProductionPerPopulation, L"iProductionPerPopulation")
		.add(m_bAnger, L"bAnger")
	;
}


bool CvHurryInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvHurryInfo::copyNonDefaults(const CvHurryInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvHurryInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

