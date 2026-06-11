//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoScalableAsset.cpp
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
#include "CvArtInfoScalableAsset.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoScalableAsset
/////////////////////////////////////////////////////////////////////////////////////////////

void CvArtInfoScalableAsset::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoAsset::getDataMembers(util);
	CvScalableInfo::getDataMembers(util);
}


bool CvArtInfoScalableAsset::read(CvXMLLoadUtility* pXML)
{
	// The CvInfoUtil delegation in CvArtInfoAsset::read covers the CvScalableInfo mixin fields
	// too (declared above), so the hand-written CvScalableInfo::read is no longer called here.
	return CvArtInfoAsset::read(pXML);
}


void CvArtInfoScalableAsset::copyNonDefaults(const CvArtInfoScalableAsset* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

