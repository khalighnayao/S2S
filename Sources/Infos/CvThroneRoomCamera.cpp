//------------------------------------------------------------------------------------------------
//  FILE:    CvThroneRoomCamera.cpp
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
#include "CvThroneRoomCamera.h"


//======================================================================================================
//					CvThroneRoomCamera
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvThroneRoomCamera()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomCamera::CvThroneRoomCamera()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvThroneRoomCamera()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomCamera::~CvThroneRoomCamera()
{
}


const char* CvThroneRoomCamera::getFileName()
{
	return m_szFileName;
}


// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvThroneRoomCamera::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szFileName, L"FileName")
	;
}


bool CvThroneRoomCamera::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvThroneRoomCamera::copyNonDefaults(const CvThroneRoomCamera* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

