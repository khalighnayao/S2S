//------------------------------------------------------------------------------------------------
//  FILE:    BuildsRepo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "BuildsRepo.h"
#include "CvGlobals.h"
#include "CvInfos.h"

BuildsRepo& BuildsRepo::get()
{
	static BuildsRepo s_instance;
	return s_instance;
}

BuildsRepo::BuildsRepo()
{
}

BuildsRepo::~BuildsRepo()
{
}

void BuildsRepo::rebuild()
{
	PROFILE_EXTRA_FUNC();

	m_improvementBuilds.clear();

	const int iNumBuilds = GC.getNumBuildInfos();
	for (int iI = 0; iI < iNumBuilds; ++iI)
	{
		if (GC.getBuildInfo((BuildTypes)iI).getImprovement() != NO_IMPROVEMENT)
		{
			m_improvementBuilds.push_back((BuildTypes)iI);
		}
	}
}

const std::vector<BuildTypes>& BuildsRepo::improvementBuilds() const
{
	return m_improvementBuilds;
}
