//------------------------------------------------------------------------------------------------
//  FILE:    BuildingsRepo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "BuildingsRepo.h"
#include "CvBuildingInfo.h"
#include "CvGlobals.h"

BuildingsRepo& BuildingsRepo::get()
{
	static BuildingsRepo s_instance;
	return s_instance;
}

BuildingsRepo::BuildingsRepo()
{
}

BuildingsRepo::~BuildingsRepo()
{
}

void BuildingsRepo::rebuild()
{
	PROFILE_EXTRA_FUNC();

	const int iNumBuildings = GC.getNumBuildingInfos();
	const int iNumReligions = GC.getNumReligionInfos();

	m_byReligion.clear();
	m_byReligion.resize(iNumReligions);

	for (int iI = 0; iI < iNumBuildings; ++iI)
	{
		const int iReligion = GC.getBuildingInfo((BuildingTypes)iI).getReligionType();
		if (iReligion >= 0 && iReligion < iNumReligions)
		{
			m_byReligion[iReligion].push_back((BuildingTypes)iI);
		}
	}
	// Iteration over [0, iNumBuildings) guarantees each bucket is ascending,
	// so no explicit sort is required.
}

const std::vector<BuildingTypes>& BuildingsRepo::byReligion(ReligionTypes eReligion) const
{
	if (eReligion < 0 || (int)eReligion >= (int)m_byReligion.size())
	{
		return m_emptyBucket;
	}
	return m_byReligion[eReligion];
}
