#pragma once

#ifndef CV_MAIN_MENU_INFO_H
#define CV_MAIN_MENU_INFO_H

#include "CvInfoBase.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvMainMenuInfo
//
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvMainMenuInfo
	: public CvInfoBase
	, private bst::noncopyable
{
public:

	CvMainMenuInfo();
	virtual ~CvMainMenuInfo();

	DllExport std::string getScene() const;
	DllExport std::string getSceneNoShader() const;
	DllExport std::string getSoundtrack() const;
	DllExport std::string getLoading() const;
	DllExport std::string getLoadingSlideshow() const;

	void getDataMembers(CvInfoUtil& util);
	bool read(CvXMLLoadUtility* pXML);
	void copyNonDefaults(const CvMainMenuInfo* pClassInfo);

protected:
	// CvString (is-a std::string, no added data) so CvInfoUtil's StringWrapper can bind;
	// the getters still return std::string by value.
	CvString m_szScene;
	CvString m_szSceneNoShader;
	CvString m_szSoundtrack;
	CvString m_szLoading;
	CvString m_szLoadingSlideshow;
};

#endif // CV_MAIN_MENU_INFO_H
