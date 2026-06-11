//------------------------------------------------------------------------------------------------
//  FILE:    CvThroneRoomInfo.cpp
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
#include "CvThroneRoomInfo.h"


//======================================================================================================
//					CvThroneRoomInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvThroneRoomInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomInfo::CvThroneRoomInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvThroneRoomInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomInfo::~CvThroneRoomInfo()
{
}


const char* CvThroneRoomInfo::getEvent()
{
	return m_szEvent;
}


const char* CvThroneRoomInfo::getNodeName()
{
	return m_szNodeName;
}


int CvThroneRoomInfo::getFromState()
{
	return m_iFromState;
}


int CvThroneRoomInfo::getToState()
{
	return m_iToState;
}


int CvThroneRoomInfo::getAnimation()
{
	return m_iAnimation;
}


// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvThroneRoomInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szEvent, L"Event")
		.add(m_iFromState, L"iFromState")
		.add(m_iToState, L"iToState")
		.add(m_szNodeName, L"NodeName")
		.add(m_iAnimation, L"iAnimation")
	;
}


bool CvThroneRoomInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvThroneRoomInfo::copyNonDefaults(CvThroneRoomInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

