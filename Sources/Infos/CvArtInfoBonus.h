#pragma once

#ifndef CV_ART_INFO_BONUS_H
#define CV_ART_INFO_BONUS_H

#include "CvInfoBase.h"
#include "CvArtInfoScalableAsset.h"

class CvArtInfoBonus
	: public CvArtInfoScalableAsset
	, private bst::noncopyable
{
public:
	CvArtInfoBonus();
	virtual ~CvArtInfoBonus() {}

	int getFontButtonIndex() const;

	DllExport const char* getShaderNIF() const;
	void setShaderNIF(const char* szDesc);

	// No read() override: all fields are declarative, so the inherited
	// CvArtInfoScalableAsset::read -> CvArtInfoAsset::read delegation reads them.
	void getDataMembers(CvInfoUtil& util);
	void copyNonDefaults(const CvArtInfoBonus* pClassInfo);

protected:
	CvString m_szShaderNIF;		//!< The NIF used if the graphics card supports shaders
	int m_iFontButtonIndex;
};

#endif // CV_ART_INFO_BONUS_H
