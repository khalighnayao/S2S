//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoBuilding.cpp
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
#include "CvArtInfoBuilding.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoBuilding
/////////////////////////////////////////////////////////////////////////////////////////////

CvArtInfoBuilding::CvArtInfoBuilding()
{
	CvInfoUtil(this).initDataMembers();
}


CvArtInfoBuilding::~CvArtInfoBuilding()
{
}


bool CvArtInfoBuilding::isAnimated() const
{
	return m_bAnimated;
}


const char* CvArtInfoBuilding::getLSystemName() const
{
	return m_szLSystemName;
}


void CvArtInfoBuilding::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoScalableAsset::getDataMembers(util);
	util
		.add(m_szLSystemName, L"LSystem")
		.add(m_bAnimated, L"bAnimated")
	;
}

void CvArtInfoBuilding::copyNonDefaults(const CvArtInfoBuilding* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

