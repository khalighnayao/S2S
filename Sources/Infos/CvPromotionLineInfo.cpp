//------------------------------------------------------------------------------------------------
//  FILE:    CvPromotionLineInfo.cpp
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
#include "CvPromotionLineInfo.h"


//TB Promotion Line Info
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvPromotionLineInfo
//
//  DESC:   Contains info about Promotion Lines
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CvPromotionLineInfo::CvPromotionLineInfo() :
									m_ePrereqTech(NO_TECH),
									m_eObsoleteTech(NO_TECH),
									m_ePropertyType(NO_PROPERTY),
#ifdef OUTBREAKS_AND_AFFLICTIONS
									m_iToleranceBuildup(0),
									m_iToleranceDecay(0),
									m_iCommunicability(0),
									m_iWorseningProbabilityIncrementModifier(0),
									m_iWorsenedCommunicabilityIncrementModifier(0),
									m_iWorsenedOvercomeIncrementModifier(0),
									m_iOvercomeProbability(0),
									m_iOvercomeAdjperTurn(0),
									m_iOutbreakModifier(100),
									m_iOvercomeModifier(100),
									m_bAffliction(false),
#endif
									m_bEquipment(false),
									m_bCritical(false),
									m_bNoSpreadonBattle(false),
									m_bNoSpreadUnitProximity(false),
									m_bNoSpreadUnittoCity(false),
									m_bNoSpreadCitytoUnit(false),
									m_bBuildUp(false),
									m_bPoison(false)
{
}


CvPromotionLineInfo::~CvPromotionLineInfo()
{
}


bool CvPromotionLineInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvString szTextVal;
	pXML->GetOptionalChildXmlValByName(szTextVal, L"PrereqTech");
	m_ePrereqTech = (TechTypes) pXML->GetInfoClass(szTextVal);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"ObsoleteTech");
	m_eObsoleteTech = (TechTypes) pXML->GetInfoClass(szTextVal);

	pXML->GetOptionalChildXmlValByName(szTextVal, L"PropertyType");
	m_ePropertyType = (PropertyTypes) pXML->GetInfoClass(szTextVal);

#ifdef OUTBREAKS_AND_AFFLICTIONS
	pXML->GetOptionalChildXmlValByName(&m_iToleranceBuildup, L"iToleranceBuildup");
	pXML->GetOptionalChildXmlValByName(&m_iToleranceDecay, L"iToleranceDecay");
	pXML->GetOptionalChildXmlValByName(&m_iCommunicability, L"iCommunicability");
	pXML->GetOptionalChildXmlValByName(&m_iWorseningProbabilityIncrementModifier, L"iWorseningProbabilityIncrementModifier");
	pXML->GetOptionalChildXmlValByName(&m_iWorsenedCommunicabilityIncrementModifier, L"iWorsenedCommunicabilityIncrementModifier");
	pXML->GetOptionalChildXmlValByName(&m_iWorsenedOvercomeIncrementModifier, L"iWorsenedOvercomeIncrementModifier");
	pXML->GetOptionalChildXmlValByName(&m_iOvercomeProbability, L"iOvercomeProbability");
	pXML->GetOptionalChildXmlValByName(&m_iOvercomeAdjperTurn, L"iOvercomeAdjperTurn");
	pXML->GetOptionalChildXmlValByName(&m_iOutbreakModifier, L"iOutbreakModifier", 100);
	pXML->GetOptionalChildXmlValByName(&m_iOvercomeModifier, L"iOvercomeModifier", 100);
	pXML->GetOptionalChildXmlValByName(&m_bAffliction, L"bAffliction");
#endif
	pXML->GetOptionalChildXmlValByName(&m_bEquipment, L"bEquipment");
	pXML->GetOptionalChildXmlValByName(&m_bCritical, L"bCritical");
	pXML->GetOptionalChildXmlValByName(&m_bNoSpreadonBattle, L"bNoSpreadonBattle");
	pXML->GetOptionalChildXmlValByName(&m_bNoSpreadUnitProximity, L"bNoSpreadUnitProximity");
	pXML->GetOptionalChildXmlValByName(&m_bNoSpreadUnittoCity, L"bNoSpreadUnittoCity");
	pXML->GetOptionalChildXmlValByName(&m_bNoSpreadCitytoUnit, L"bNoSpreadCitytoUnit");
	pXML->GetOptionalChildXmlValByName(&m_bBuildUp, L"bBuildUp");
	pXML->GetOptionalChildXmlValByName(&m_bPoison, L"bPoison");

	// bool vector without delayed resolution
	pXML->SetOptionalVector(&m_aiUnitCombatPrereqTypes, L"UnitCombatPrereqTypes");
	pXML->SetOptionalVector(&m_aiNotOnUnitCombatTypes, L"NotOnUnitCombatTypes");
	pXML->SetOptionalVector(&m_aiNotOnDomainTypes, L"NotOnDomainTypes");
	pXML->SetOptionalVector(&m_aiOnGameOptions, L"OnGameOptions");
	pXML->SetOptionalVector(&m_aiNotOnGameOptions, L"NotOnGameOptions");
	pXML->SetOptionalVector(&m_aiCriticalOriginCombatClassTypes, L"CriticalOriginCombatClassTypes");

	// int vector utilizing pairing without delayed resolution
	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aUnitCombatContractChanceChanges, L"UnitCombatContractChanceChanges");

	pXML->SetOptionalPairVector<UnitCombatModifierArray, UnitCombatTypes, int>(&m_aUnitCombatOvercomeChanges, L"UnitCombatOvercomeChanges");

	pXML->SetOptionalPairVector<TechModifierArray, TechTypes, int>(&m_aTechContractChanceChanges, L"TechContractChanceChanges");

	pXML->SetOptionalPairVector<TechModifierArray, TechTypes, int>(&m_aTechOvercomeChanges, L"TechOvercomeChanges");

	pXML->SetOptionalVector(&m_aiCategories, L"Categories");

	return true;
}


void CvPromotionLineInfo::copyNonDefaults(const CvPromotionLineInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const bool bDefault = false;
	const int iDefault = 0;
	const CvString cDefault = CvString::format("").GetCString();

	CvInfoBase::copyNonDefaults(pClassInfo);

	if (getPrereqTech() == NO_TECH) m_ePrereqTech = pClassInfo->getPrereqTech();
	if (getObsoleteTech() == NO_TECH) m_eObsoleteTech = pClassInfo->getObsoleteTech();

#ifdef OUTBREAKS_AND_AFFLICTIONS
	if (getToleranceBuildup() == iDefault) m_iToleranceBuildup = pClassInfo->getToleranceBuildup();
	if (getToleranceDecay() == iDefault) m_iToleranceDecay = pClassInfo->getToleranceDecay();
	if (getCommunicability() == iDefault) m_iCommunicability = pClassInfo->getCommunicability();
	if (getWorseningProbabilityIncrementModifier() == iDefault) m_iWorseningProbabilityIncrementModifier = pClassInfo->getWorseningProbabilityIncrementModifier();
	if (getWorsenedCommunicabilityIncrementModifier() == iDefault) m_iWorsenedCommunicabilityIncrementModifier = pClassInfo->getWorsenedCommunicabilityIncrementModifier();
	if (getWorsenedOvercomeIncrementModifier() == iDefault) m_iWorsenedOvercomeIncrementModifier = pClassInfo->getWorsenedOvercomeIncrementModifier();
	if (getOvercomeProbability() == iDefault) m_iOvercomeProbability = pClassInfo->getOvercomeProbability();
	if (getOvercomeAdjperTurn() == iDefault) m_iOvercomeAdjperTurn = pClassInfo->getOvercomeAdjperTurn();
	if (getOutbreakModifier() == iDefault) m_iOutbreakModifier = pClassInfo->getOutbreakModifier();
	if (getOvercomeModifier() == iDefault) m_iOvercomeModifier = pClassInfo->getOvercomeModifier();
	if (isAffliction() == bDefault) m_bAffliction = pClassInfo->isAffliction();
#endif
	if (isEquipment() == bDefault) m_bEquipment = pClassInfo->isEquipment();
	if (isCritical() == bDefault) m_bCritical = pClassInfo->isCritical();
	if (isNoSpreadonBattle() == bDefault) m_bNoSpreadonBattle = pClassInfo->isNoSpreadonBattle();
	if (isNoSpreadUnitProximity() == bDefault) m_bNoSpreadUnitProximity = pClassInfo->isNoSpreadUnitProximity();
	if (isNoSpreadUnittoCity() == bDefault) m_bNoSpreadUnittoCity = pClassInfo->isNoSpreadUnittoCity();
	if (isNoSpreadCitytoUnit() == bDefault) m_bNoSpreadCitytoUnit = pClassInfo->isNoSpreadCitytoUnit();
	if (isBuildUp() == bDefault) m_bBuildUp = pClassInfo->isBuildUp();
	if (isPoison() == bDefault) m_bPoison = pClassInfo->isPoison();

	if (getPropertyType() != NO_PROPERTY) m_ePropertyType = pClassInfo->getPropertyType();

	// bool vectors without delayed resolution
	if (getNumUnitCombatPrereqTypes() == 0)
	{
		m_aiUnitCombatPrereqTypes.clear();
		for ( int i = 0; i < pClassInfo->getNumUnitCombatPrereqTypes(); i++)
		{
			m_aiUnitCombatPrereqTypes.push_back(pClassInfo->getUnitCombatPrereqType(i));
		}
	}

	if (getNumNotOnUnitCombatTypes() == 0)
	{
		m_aiNotOnUnitCombatTypes.clear();
		for ( int i = 0; i < pClassInfo->getNumNotOnUnitCombatTypes(); i++)
		{
			m_aiNotOnUnitCombatTypes.push_back(pClassInfo->getNotOnUnitCombatType(i));
		}
	}

	if (getNumNotOnDomainTypes() == 0)
	{
		m_aiNotOnDomainTypes.clear();
		for ( int i = 0; i < pClassInfo->getNumNotOnDomainTypes(); i++)
		{
			m_aiNotOnDomainTypes.push_back(pClassInfo->getNotOnDomainType(i));
		}
	}

	if (getNumOnGameOptions() == 0)
	{
		m_aiOnGameOptions.clear();
		for ( int i = 0; i < pClassInfo->getNumOnGameOptions(); i++)
		{
			m_aiOnGameOptions.push_back(pClassInfo->getOnGameOption(i));
		}
	}

	if (getNumNotOnGameOptions() == 0)
	{
		m_aiNotOnGameOptions.clear();
		for ( int i = 0; i < pClassInfo->getNumNotOnGameOptions(); i++)
		{
			m_aiNotOnGameOptions.push_back(pClassInfo->getNotOnGameOption(i));
		}
	}

	if (getNumCriticalOriginCombatClassTypes() == 0)
	{
		m_aiCriticalOriginCombatClassTypes.clear();
		for ( int i = 0; i < pClassInfo->getNumCriticalOriginCombatClassTypes(); i++)
		{
			m_aiCriticalOriginCombatClassTypes.push_back(pClassInfo->getCriticalOriginCombatClassType(i));
		}
	}

	// int vector utilizing pairing without delayed resolution
	if (getNumUnitCombatContractChanceChanges()==0)
	{
		for (int i=0; i < pClassInfo->getNumUnitCombatContractChanceChanges(); i++)
		{
			const UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			const int iChange = pClassInfo->getUnitCombatContractChanceChange(i);
			m_aUnitCombatContractChanceChanges.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumUnitCombatOvercomeChanges()==0)
	{
		for (int i=0; i < pClassInfo->getNumUnitCombatOvercomeChanges(); i++)
		{
			const UnitCombatTypes eUnitCombat = ((UnitCombatTypes)i);
			const int iChange = pClassInfo->getUnitCombatOvercomeChange(i);
			m_aUnitCombatOvercomeChanges.push_back(std::make_pair(eUnitCombat, iChange));
		}
	}

	if (getNumTechContractChanceChanges()==0)
	{
		for (int i=0; i < pClassInfo->getNumTechContractChanceChanges(); i++)
		{
			const TechTypes eTech = ((TechTypes)i);
			const int iChange = pClassInfo->getTechContractChanceChange(i);
			m_aTechContractChanceChanges.push_back(std::make_pair(eTech, iChange));
		}
	}

	if (getNumTechOvercomeChanges()==0)
	{
		for (int i=0; i < pClassInfo->getNumTechOvercomeChanges(); i++)
		{
			const TechTypes eTech = ((TechTypes)i);
			const int iChange = pClassInfo->getTechOvercomeChange(i);
			m_aTechOvercomeChanges.push_back(std::make_pair(eTech, iChange));
		}
	}

	CvXMLLoadUtility::CopyNonDefaultsFromVector(m_aiCategories, pClassInfo->m_aiCategories);

}


void CvPromotionLineInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_ePrereqTech);
	CheckSum(iSum, m_eObsoleteTech);
#ifdef OUTBREAKS_AND_AFFLICTIONS
	//integers
	CheckSum(iSum, m_iToleranceBuildup);
	CheckSum(iSum, m_iToleranceDecay);
	CheckSum(iSum, m_iCommunicability);
	CheckSum(iSum, m_iWorseningProbabilityIncrementModifier);
	CheckSum(iSum, m_iWorsenedCommunicabilityIncrementModifier);
	CheckSum(iSum, m_iWorsenedOvercomeIncrementModifier);
	CheckSum(iSum, m_iOvercomeProbability);
	CheckSum(iSum, m_iOvercomeAdjperTurn);
	CheckSum(iSum, m_iOutbreakModifier);
	CheckSum(iSum, m_iOvercomeModifier);
	//booleans
	CheckSum(iSum, m_bAffliction);
#endif // OUTBREAKS_AND_AFFLICTIONS
	CheckSum(iSum, m_bEquipment);
	CheckSum(iSum, m_bCritical);
	CheckSum(iSum, m_bNoSpreadonBattle);
	CheckSum(iSum, m_bNoSpreadUnitProximity);
	CheckSum(iSum, m_bNoSpreadUnittoCity);
	CheckSum(iSum, m_bNoSpreadCitytoUnit);
	CheckSum(iSum, m_bBuildUp);
	CheckSum(iSum, m_bPoison);

	CheckSum(iSum, (int)m_ePropertyType);
	// bool vectors without delayed resolution
	CheckSumC(iSum, m_aiUnitCombatPrereqTypes);
	CheckSumC(iSum, m_aiNotOnUnitCombatTypes);
	CheckSumC(iSum, m_aiNotOnDomainTypes);
	CheckSumC(iSum, m_aiOnGameOptions);
	CheckSumC(iSum, m_aiNotOnGameOptions);
	CheckSumC(iSum, m_aiCriticalOriginCombatClassTypes);
	// int vector utilizing pairing without delayed resolution
	CheckSumC(iSum, m_aUnitCombatContractChanceChanges);
	CheckSumC(iSum, m_aUnitCombatOvercomeChanges);
	CheckSumC(iSum, m_aTechContractChanceChanges);
	CheckSumC(iSum, m_aTechOvercomeChanges);
}


void CvPromotionLineInfo::doPostLoadCaching(uint32_t iThis)
{
	PROFILE_EXTRA_FUNC();
	//Establish speedy promotion & Building reference by line
	m_aiPromotions.clear();
	m_aiBuildings.clear();

	for ( int i = 0; i < GC.getNumPromotionInfos(); i++)
	{
		if (GC.getPromotionInfo((PromotionTypes)i).getPromotionLine() == iThis)
		{
			m_aiPromotions.push_back(i);
		}
	}
	for ( int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		if (GC.getBuildingInfo((BuildingTypes)i).getPromotionLineType() == iThis)
		{
			m_aiBuildings.push_back(i);
		}
	}
}


TechTypes CvPromotionLineInfo::getObsoleteTech() const
{
	return m_eObsoleteTech;
}

#ifdef OUTBREAKS_AND_AFFLICTIONS
int CvPromotionLineInfo::getToleranceBuildup() const
{
	return m_iToleranceBuildup;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getToleranceDecay() const
{
	return m_iToleranceDecay;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getCommunicability() const
{
	return m_iCommunicability;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getWorseningProbabilityIncrementModifier() const
{
	return m_iWorseningProbabilityIncrementModifier;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getWorsenedCommunicabilityIncrementModifier() const
{
	return m_iWorsenedCommunicabilityIncrementModifier;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getWorsenedOvercomeIncrementModifier() const
{
	return m_iWorsenedOvercomeIncrementModifier;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getOvercomeProbability() const
{
	return m_iOvercomeProbability;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getOvercomeAdjperTurn() const
{
	return m_iOvercomeAdjperTurn;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getOutbreakModifier() const
{
	return m_iOutbreakModifier;
}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef OUTBREAKS_AND_AFFLICTIONS

int CvPromotionLineInfo::getOvercomeModifier() const
{
	return m_iOvercomeModifier;
}
#endif // OUTBREAKS_AND_AFFLICTIONS


bool CvPromotionLineInfo::isEquipment() const
{
	return m_bEquipment;
}


bool CvPromotionLineInfo::isCritical() const
{
	return m_bCritical;
}


bool CvPromotionLineInfo::isNoSpreadonBattle() const
{
	return m_bNoSpreadonBattle;
}


bool CvPromotionLineInfo::isNoSpreadUnitProximity() const
{
	return m_bNoSpreadUnitProximity;
}


bool CvPromotionLineInfo::isNoSpreadUnittoCity() const
{
	return m_bNoSpreadUnittoCity;
}


bool CvPromotionLineInfo::isNoSpreadCitytoUnit() const
{
	return m_bNoSpreadCitytoUnit;
}


bool CvPromotionLineInfo::isBuildUp() const
{
	return m_bBuildUp;
}


bool CvPromotionLineInfo::isPoison() const
{
	return m_bPoison;
}


PropertyTypes CvPromotionLineInfo::getPropertyType() const
{
	return m_ePropertyType;
}


int CvPromotionLineInfo::getUnitCombatPrereqType(int i) const
{
	return m_aiUnitCombatPrereqTypes[i];
}


int CvPromotionLineInfo::getNumUnitCombatPrereqTypes() const
{
	return (int)m_aiUnitCombatPrereqTypes.size();
}


int CvPromotionLineInfo::getNotOnUnitCombatType(int i) const
{
	return m_aiNotOnUnitCombatTypes[i];
}


int CvPromotionLineInfo::getNumNotOnUnitCombatTypes() const
{
	return (int)m_aiNotOnUnitCombatTypes.size();
}


bool CvPromotionLineInfo::isNotOnUnitCombatType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiNotOnUnitCombatTypes, i);
}


int CvPromotionLineInfo::getNotOnDomainType(int i) const
{
	return m_aiNotOnDomainTypes[i];
}


int CvPromotionLineInfo::getNumNotOnDomainTypes() const
{
	return (int)m_aiNotOnDomainTypes.size();
}


bool CvPromotionLineInfo::isNotOnDomainType(int i) const
{
	FASSERT_BOUNDS(0, NUM_DOMAIN_TYPES, i);
	return algo::any_of_equal(m_aiNotOnDomainTypes, i);
}


int CvPromotionLineInfo::getOnGameOption(int i) const
{
	return m_aiOnGameOptions[i];
}


int CvPromotionLineInfo::getNumOnGameOptions() const
{
	return (int)m_aiOnGameOptions.size();
}


bool CvPromotionLineInfo::isOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiOnGameOptions, i);
}


int CvPromotionLineInfo::getNotOnGameOption(int i) const
{
	return m_aiNotOnGameOptions[i];
}


int CvPromotionLineInfo::getNumNotOnGameOptions() const
{
	return (int)m_aiNotOnGameOptions.size();
}


bool CvPromotionLineInfo::isNotOnGameOption(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumGameOptionInfos(), i);
	return algo::any_of_equal(m_aiNotOnGameOptions, i);
}


int CvPromotionLineInfo::getCategory(int i) const
{
	return m_aiCategories[i];
}


int CvPromotionLineInfo::getNumCategories() const
{
	return (int)m_aiCategories.size();
}


bool CvPromotionLineInfo::isCategory(int i) const
{
	return algo::any_of_equal(m_aiCategories, i);
}


int CvPromotionLineInfo::getCriticalOriginCombatClassType(int i) const
{
	return m_aiCriticalOriginCombatClassTypes[i];
}


int CvPromotionLineInfo::getNumCriticalOriginCombatClassTypes() const
{
	return (int)m_aiCriticalOriginCombatClassTypes.size();
}


bool CvPromotionLineInfo::isCriticalOriginCombatClassType(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumUnitCombatInfos(), i);
	return algo::any_of_equal(m_aiCriticalOriginCombatClassTypes, i);
}


// int vector utilizing pairing without delayed resolution
int CvPromotionLineInfo::getNumUnitCombatContractChanceChanges() const
{
	return m_aUnitCombatContractChanceChanges.size();
}


int CvPromotionLineInfo::getUnitCombatContractChanceChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatContractChanceChanges.begin(); it != m_aUnitCombatContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionLineInfo::isUnitCombatContractChanceChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatContractChanceChanges.begin(); it != m_aUnitCombatContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionLineInfo::getNumUnitCombatOvercomeChanges() const
{
	return m_aUnitCombatOvercomeChanges.size();
}


int CvPromotionLineInfo::getUnitCombatOvercomeChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatOvercomeChanges.begin(); it != m_aUnitCombatOvercomeChanges.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionLineInfo::isUnitCombatOvercomeChange(int iUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	for (UnitCombatModifierArray::const_iterator it = m_aUnitCombatOvercomeChanges.begin(); it != m_aUnitCombatOvercomeChanges.end(); ++it)
	{
		if ((*it).first == (UnitCombatTypes)iUnitCombat)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionLineInfo::getNumTechContractChanceChanges() const
{
	return m_aTechContractChanceChanges.size();
}


int CvPromotionLineInfo::getTechContractChanceChange(int iTech) const
{
	PROFILE_EXTRA_FUNC();
	for (TechModifierArray::const_iterator it = m_aTechContractChanceChanges.begin(); it != m_aTechContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (TechTypes)iTech)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionLineInfo::isTechContractChanceChange(int iTech) const
{
	PROFILE_EXTRA_FUNC();
	for (TechModifierArray::const_iterator it = m_aTechContractChanceChanges.begin(); it != m_aTechContractChanceChanges.end(); ++it)
	{
		if ((*it).first == (TechTypes)iTech)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionLineInfo::getNumTechOvercomeChanges() const
{
	return m_aTechOvercomeChanges.size();
}


int CvPromotionLineInfo::getTechOvercomeChange(int iTech) const
{
	PROFILE_EXTRA_FUNC();
	for (TechModifierArray::const_iterator it = m_aTechOvercomeChanges.begin(); it != m_aTechOvercomeChanges.end(); ++it)
	{
		if ((*it).first == (TechTypes)iTech)
		{
			return (*it).second;
		}
	}
	return 0;
}


bool CvPromotionLineInfo::isTechOvercomeChange(int iTech) const
{
	PROFILE_EXTRA_FUNC();
	for (TechModifierArray::const_iterator it = m_aTechOvercomeChanges.begin(); it != m_aTechOvercomeChanges.end(); ++it)
	{
		if ((*it).first == (TechTypes)iTech)
		{
			return true;
		}
	}
	return false;
}


int CvPromotionLineInfo::getPromotion(int i) const
{
	return m_aiPromotions[i];
}


int CvPromotionLineInfo::getNumPromotions() const
{
	return (int)m_aiPromotions.size();
}


bool CvPromotionLineInfo::isPromotion(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumPromotionInfos(), i);
	return algo::any_of_equal(m_aiPromotions, i);
}


int CvPromotionLineInfo::getBuilding(int i) const
{
	return m_aiBuildings[i];
}


int CvPromotionLineInfo::getNumBuildings() const
{
	return (int)m_aiBuildings.size();
}


bool CvPromotionLineInfo::isBuilding(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumBuildingInfos(), i);
	return algo::any_of_equal(m_aiBuildings, i);
}

