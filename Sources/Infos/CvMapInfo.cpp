//------------------------------------------------------------------------------------------------
//  FILE:    CvMapInfo.cpp
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
#include "CvMapInfo.h"


//======================================================================================================
//					CvMapInfo
//======================================================================================================

CvMapInfo::CvMapInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvMapInfo::~CvMapInfo()
{
}


void CvMapInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iGridWidth, L"iGridWidth")
		.add(m_iGridHeight, L"iGridHeight")
		.add(m_iWrapX, L"bWrapX", -1)
		.add(m_iWrapY, L"bWrapY", -1)
		.add(m_bStartRevealed, L"bStartRevealed")
		.add(m_szInitialWBMap, L"InitialWBMap")
		.add(m_szMapScript, L"MapScript")
	;
}


bool CvMapInfo::read(CvXMLLoadUtility* pXML)
{
	CvHotkeyInfo::read(pXML);

	CvInfoUtil(this).readXml(pXML);

	return true;
}

