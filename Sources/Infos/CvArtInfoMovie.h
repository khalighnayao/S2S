#pragma once

#ifndef CV_ART_INFO_MOVIE_H
#define CV_ART_INFO_MOVIE_H

#include "CvInfoBase.h"
#include "CvArtInfoAsset.h"

class CvArtInfoMovie
	: public CvArtInfoAsset
	, private bst::noncopyable
{
public:

	CvArtInfoMovie() {}
	virtual ~CvArtInfoMovie() {}

	// No fields of its own; the inherited CvArtInfoAsset::read delegation covers everything.

	void copyNonDefaults(const CvArtInfoMovie* pClassInfo);
};

#endif // CV_ART_INFO_MOVIE_H
