#pragma once

#ifndef CV_ART_INFO_SCALABLE_ASSET_H
#define CV_ART_INFO_SCALABLE_ASSET_H

#include "CvInfoBase.h"
#include "CvArtInfoAsset.h"

//
//////////////////////////////////////////////////////////////////////////
// Another base class
//////////////////////////////////////////////////////////////////////////

class CvArtInfoScalableAsset
	: public CvArtInfoAsset
	, public CvScalableInfo
{
public:
	// Chains CvArtInfoAsset's fields + the CvScalableInfo mixin's. Also resolves the name
	// ambiguity between CvInfoBase::getDataMembers (virtual) and CvScalableInfo::getDataMembers
	// for this and all derived classes — keep this override even if it ever becomes trivial.
	void getDataMembers(CvInfoUtil& util);
	// read/copyNonDefaults forward to CvArtInfoAsset (the family's single declarative delegation
	// point); they must stay declared here to shadow CvScalableInfo's same-named methods.
	bool read(CvXMLLoadUtility* pXML);
	void copyNonDefaults(const CvArtInfoScalableAsset* pClassInfo);
};

#endif // CV_ART_INFO_SCALABLE_ASSET_H
