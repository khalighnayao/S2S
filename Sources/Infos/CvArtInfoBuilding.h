#pragma once

#ifndef CV_ART_INFO_BUILDING_H
#define CV_ART_INFO_BUILDING_H

#include "CvInfoBase.h"
#include "CvArtInfoScalableAsset.h"

class CvArtInfoBuilding
	: public CvArtInfoScalableAsset
	, private bst::noncopyable
{
public:

	CvArtInfoBuilding();
	virtual ~CvArtInfoBuilding();

	bool isAnimated() const;
	DllExport const char* getLSystemName() const;

	// No read() override: all fields are declarative, so the inherited
	// CvArtInfoScalableAsset::read -> CvArtInfoAsset::read delegation reads them.
	void getDataMembers(CvInfoUtil& util);

	void copyNonDefaults(const CvArtInfoBuilding* pClassInfo);

protected:

	bool m_bAnimated;
	CvString m_szLSystemName;
};

#endif // CV_ART_INFO_BUILDING_H
