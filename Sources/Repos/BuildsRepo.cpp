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
	m_routeBuilds.clear();

	const int iNumBuilds = GC.getNumBuildInfos();
	for (int iI = 0; iI < iNumBuilds; ++iI)
	{
		const CvBuildInfo& kBuild = GC.getBuildInfo((BuildTypes)iI);
		if (kBuild.getImprovement() != NO_IMPROVEMENT)
		{
			m_improvementBuilds.push_back((BuildTypes)iI);
		}
		if (kBuild.getRoute() != NO_ROUTE)
		{
			m_routeBuilds.push_back((BuildTypes)iI);
		}
	}
}

const std::vector<BuildTypes>& BuildsRepo::improvementBuilds() const
{
	return m_improvementBuilds;
}

const std::vector<BuildTypes>& BuildsRepo::routeBuilds() const
{
	return m_routeBuilds;
}
