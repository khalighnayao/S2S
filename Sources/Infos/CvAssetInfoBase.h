#pragma once

#ifndef CV_ASSET_INFO_BASE_H
#define CV_ASSET_INFO_BASE_H

#include "CvInfoBase.h"

// The below classes are for the ArtFile Management
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  classes : CvArtInfos
//
// This is also an abstract BASE class
//
//  DESC:  Used to store data from Art\Civ4ArtDefines.xml
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvAssetInfoBase
	: public CvInfoBase
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvAssetInfoBase()  {}
	virtual ~CvAssetInfoBase() {}

	const char* getTag() const;
	void setTag(const char* szDesc);

	DllExport const char* getPath() const;
	void setPath(const char* szDesc);

	// Declarative field registry (chained from CvArtInfoAsset::getDataMembers). This class has no
	// read() of its own: the whole art-info family is read by the single CvInfoUtil delegation in
	// CvArtInfoAsset::read, whose virtual getDataMembers dispatch covers this chain link.
	void getDataMembers(CvInfoUtil& util);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	CvString m_szPath;
};

#endif // CV_ASSET_INFO_BASE_H
