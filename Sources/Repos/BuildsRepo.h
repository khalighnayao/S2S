#pragma once

#ifndef BUILDS_REPO_H
#define BUILDS_REPO_H

// Named lookups over the global CvBuildInfo array.
//
// Mirrors BuildingsRepo: rebuilt after XML load and after every
// cvInternalGlobals::updateReplacements() (replacement pointer swaps can
// change CvBuildInfo fields for a given BuildTypes id).

class BuildsRepo
	: private bst::noncopyable
{
public:
	static BuildsRepo& get();

	// Rebuild every index from GC.m_paBuildInfo. Idempotent.
	void rebuild();

	// Builds whose CvBuildInfo::getImprovement() != NO_IMPROVEMENT.
	// Sorted ascending by BuildTypes id.
	const std::vector<BuildTypes>& improvementBuilds() const;

private:
	BuildsRepo();
	~BuildsRepo();

	std::vector<BuildTypes> m_improvementBuilds;
};

#endif // BUILDS_REPO_H
