#pragma once

#include <core_mbm/core-exports.h>
#include <vector>
#include <string>

class API_IMPL PARSE_laucher_ARGS
{
public:
	explicit PARSE_laucher_ARGS(const char** argv, const int pNumArgs);
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
	void parserArgs(const char** argv, const int pNumArgs);
};
