//------------------------------------------------------------------------------------------------
//  FILE:    CvPlayerColorInfo.cpp
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
#include "CvPlayerColorInfo.h"


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvPlayerColorInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvPlayerColorInfo::CvPlayerColorInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvPlayerColorInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvPlayerColorInfo::~CvPlayerColorInfo()
{
}


int CvPlayerColorInfo::getColorTypePrimary() const
{
	return m_iColorTypePrimary;
}


int CvPlayerColorInfo::getColorTypeSecondary() const
{
	return m_iColorTypeSecondary;
}


int CvPlayerColorInfo::getTextColorType() const
{
	return m_iTextColorType;
}


void CvPlayerColorInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnumAsInt(m_iColorTypePrimary, L"ColorTypePrimary")
		.addEnumAsInt(m_iColorTypeSecondary, L"ColorTypeSecondary")
		.addEnumAsInt(m_iTextColorType, L"TextColorType")
	;
}


bool CvPlayerColorInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvPlayerColorInfo::copyNonDefaults(const CvPlayerColorInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

