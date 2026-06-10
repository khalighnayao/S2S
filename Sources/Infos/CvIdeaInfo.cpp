//------------------------------------------------------------------------------------------------
//  FILE:    CvIdeaInfo.cpp
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
#include "CvIdeaInfo.h"


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvIdeaInfo
//
//  DESC:   Contains info about Ideas
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvIdeaInfo::CvIdeaInfo()
{
	CvInfoUtil(this).initDataMembers();
}


CvIdeaInfo::~CvIdeaInfo()
{
}


IdeaClassTypes CvIdeaInfo::getIdeaClass() const
{
	return m_eIdeaClass;
}


void CvIdeaInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnum(m_eIdeaClass, L"IdeaClassType")
	;
}


bool CvIdeaInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvIdeaInfo::copyNonDefaults(const CvIdeaInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvIdeaInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}

