//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialUnitInfo.cpp
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
#include "CvSpecialUnitInfo.h"



//======================================================================================================
//					CvSpecialUnitInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvSpecialUnitInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialUnitInfo::CvSpecialUnitInfo() :
	m_bValid(false),
	m_bCityLoad(false),
	m_bSMLoadSame(false),
	m_iCombatPercent(0),
	m_iWithdrawalChange(0)
{ }


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvSpecialUnitInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvSpecialUnitInfo::~CvSpecialUnitInfo()
{
}


bool CvSpecialUnitInfo::isValid() const
{
	return m_bValid;
}


bool CvSpecialUnitInfo::isCityLoad() const
{
	return m_bCityLoad;
}


bool CvSpecialUnitInfo::isSMLoadSame() const
{
	return m_bSMLoadSame;
}


int CvSpecialUnitInfo::getCombatPercent() const
{
	return m_iCombatPercent;
}


int CvSpecialUnitInfo::getWithdrawalChange() const
{
	return m_iWithdrawalChange;
}


// Arrays


bool CvSpecialUnitInfo::read(CvXMLLoadUtility* pXML)
{

	if (!CvInfoBase::read(pXML))
	{
		return false;
	}
	pXML->GetOptionalChildXmlValByName(&m_bValid, L"bValid");
	pXML->GetOptionalChildXmlValByName(&m_bCityLoad, L"bCityLoad");
	pXML->GetOptionalChildXmlValByName(&m_bSMLoadSame, L"bSMLoadSame");
	pXML->GetOptionalChildXmlValByName(&m_iCombatPercent, L"iCombatPercent");
	pXML->GetOptionalChildXmlValByName(&m_iWithdrawalChange, L"iWithdrawalChange");


	return true;
}


void CvSpecialUnitInfo::copyNonDefaults(const CvSpecialUnitInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const bool bDefault = false;
	const int iDefault = 0;

	CvInfoBase::copyNonDefaults(pClassInfo);

	if (isValid() == bDefault) m_bValid = pClassInfo->isValid();
	if (isCityLoad() == bDefault) m_bCityLoad = pClassInfo->isCityLoad();
	if (isSMLoadSame() == bDefault) m_bSMLoadSame = pClassInfo->isSMLoadSame();
	if (getCombatPercent() == iDefault) m_iCombatPercent = pClassInfo->getCombatPercent();
	if (getWithdrawalChange() == iDefault) m_iWithdrawalChange = pClassInfo->getWithdrawalChange();

}


void CvSpecialUnitInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_bValid);
	CheckSum(iSum, m_bSMLoadSame);
	CheckSum(iSum, m_iCombatPercent);
	CheckSum(iSum, m_iWithdrawalChange);

}

