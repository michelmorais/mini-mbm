#pragma once

#include <Windows.h>
#include <vector>
#include <string>

class PARSE_laucher_ARGS
{
public:
	PARSE_laucher_ARGS();
	bool noSplash;
	bool noBorder;
	bool enableResizeWindow;
	bool maximizedWindow;
	bool enableBorder;
	bool allowFullScreen;
	bool full_screen_checked;
	bool disable_select_monitor;
	int window_theme;
	unsigned int positionXWindow;
	unsigned int positionYWindow;
	std::string fileNameInitialLua;
	std::string nameAplication;
	std::vector<unsigned int> width_list;
	std::vector<unsigned int> height_list;
	std::vector<unsigned int> expected_width_list;
	std::vector<unsigned int> expected_height_list;

private:
	void parserArgs(const LPWSTR* szArglist, const int pNumArgs);
};
