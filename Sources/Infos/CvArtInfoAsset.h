#pragma once

#ifndef CV_ART_INFO_ASSET_H
#define CV_ART_INFO_ASSET_H

#include "CvInfoBase.h"
#include "CvAssetInfoBase.h"

class CvArtInfoAsset
	: public CvAssetInfoBase
{
public:

	CvArtInfoAsset() {}
	virtual ~CvArtInfoAsset() {}

	DllExport const char* getNIF() const;
	DllExport const char* getKFM() const;

	void setNIF(const char* szDesc);
	void setKFM(const char* szDesc);

	void getDataMembers(CvInfoUtil& util);

	// The ONE declarative delegation point of the art-info family: getDataMembers dispatches
	// virtually, so the single CvInfoUtil readXml here reads every declared field of the concrete
	// leaf. Leaf read() overrides forward to this (directly or via CvArtInfoScalableAsset::read)
	// and add only their bespoke, non-declarative reads — never a second CvInfoUtil delegation.
	bool read(CvXMLLoadUtility* pXML);

	void copyNonDefaults(const CvArtInfoAsset* pClassInfo);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	CvString m_szKFM;
	CvString m_szNIF;
};

#endif // CV_ART_INFO_ASSET_H
