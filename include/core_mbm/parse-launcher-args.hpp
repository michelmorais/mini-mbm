#pragma once

#include <core_mbm/core-exports.h>
#include <vector>
#include <string>

struct STORE_DATA_ARG;

class API_IMPL PARSE_laucher_ARGS
{
public:
	PARSE_laucher_ARGS();
	PARSE_laucher_ARGS(const char** argv, const int pNumArgs);
#if defined _WIN32
	PARSE_laucher_ARGS(int argc, wchar_t** argv);
#endif
	~PARSE_laucher_ARGS();
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
	const char* getFileNameInitialLua() const;
	const char* getNameApplication() const;
	bool getWidthHeight(unsigned int& width, unsigned int& height) const;
	bool getExpectedWidthHeight(unsigned int& width, unsigned int& height) const;

private:
	STORE_DATA_ARG* data_arg;

private:
	void parserArgs(const char** argv, const int pNumArgs);
};
