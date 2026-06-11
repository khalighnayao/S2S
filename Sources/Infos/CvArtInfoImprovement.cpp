//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoImprovement.cpp
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
#include "CvArtInfoImprovement.h"


//////////////////////////////////////////////////////////////////////////
// CvArtInfoImprovement
//////////////////////////////////////////////////////////////////////////

CvArtInfoImprovement::CvArtInfoImprovement()
{
	CvInfoUtil(this).initDataMembers();
}


CvArtInfoImprovement::~CvArtInfoImprovement()
{
}


bool CvArtInfoImprovement::isExtraAnimations() const
{
	return m_bExtraAnimations;
}


void CvArtInfoImprovement::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoScalableAsset::getDataMembers(util);
	util
		.add(m_bExtraAnimations, L"bExtraAnimations")
		.add(m_szShaderNIF, L"SHADERNIF")
	;
}


void CvArtInfoImprovement::copyNonDefaults(const CvArtInfoImprovement* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

