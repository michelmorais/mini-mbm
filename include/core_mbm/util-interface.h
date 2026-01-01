/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef INTERFACE_UTIL_H_
#define INTERFACE_UTIL_H_

#include "core-exports.h"
#include <vector>
#include <string>
#include <platform/mismatch-platform.h>

typedef void (*OnScriptPrintLine)();
typedef void (*OnAddPathScript)(const char*);

enum TYPE_LOG : char
{
    TYPE_LOG_ERROR,
    TYPE_LOG_INFO,
    TYPE_LOG_WARN,
};

enum COLOR_TERMINAL
{
    COLOR_TERMINAL_WHITE,
    COLOR_TERMINAL_RED,
    COLOR_TERMINAL_YELLOW,
    COLOR_TERMINAL_GREEN,
    COLOR_TERMINAL_BLUE,
    COLOR_TERMINAL_MAGENTA,
    COLOR_TERMINAL_CIAN,
};

namespace util
{
    #if defined   _WIN32
        API_IMPL WCHAR *toWchar(const char *str, WCHAR *outText);
        API_IMPL char *toChar(const WCHAR *wstr, char *outText);
        API_IMPL void getDisplayMetrics(int * width, int * height);
    #endif
    API_IMPL const char* getDirSeparator();
    API_IMPL const char  getCharDirSeparator();
    API_IMPL const char *getPathFromFullPathName(const char *fileNamePath);
    API_IMPL float degreeToRadian(const float degree);
    API_IMPL float radianToDegree(const float radian);
    API_IMPL void setRandomSeed();
    API_IMPL float getHeightMaxWithInitialSpeed(const float gravity, const float speedInitial) noexcept;
    API_IMPL float getHeightWithTime(const float gravity, const float time) noexcept;
    API_IMPL float getTimeWithMaxHeight(const float gravity, const float heigth) noexcept;
    API_IMPL float getSpeedWithTimeFall(const float gravity, const float time) noexcept;
    API_IMPL float getSpeedWithHeight(const float gravity, const float heigth) noexcept;
    API_IMPL int getRandomInt(const int min, const int max) noexcept;
    API_IMPL char getRandomChar(const char min, const char max) noexcept;
    API_IMPL float getRandomFloat(const float min, const float max) noexcept;
    API_IMPL uint32_t FloatToDWORD(float &Float) noexcept;
    API_IMPL float getByteProp(); // 1 / 255
    API_IMPL void getAABB(const float halfDimInOut[2], const float angleRadian, float *widthOut, float *heightOut) noexcept;
    API_IMPL void split(std::vector<std::string> &result, const char *in, const char delim);
    API_IMPL FILE* openFile(const char *fileName, const char *mode);
    API_IMPL const char * getFullPath(const char *fileName, bool *existPath );
    API_IMPL void addPath(const char *newPathSource);
    API_IMPL bool getSizeFile(FILE *fp, size_t *sizeOut);
    API_IMPL const char * existFile(const char *fileName);
    API_IMPL bool saveToFileBinary(const char *fileName,const  void *header,const size_t sizeOfHeader,const void *dataIn,const size_t sizeOfDataIn, FILE **fileOut);
    API_IMPL bool addToFileBinary(const char *fileName,const  void *dataIn,const size_t sizeOfDataIn, FILE **fileOutOpened);
    API_IMPL void getAllPaths(std::vector<std::string> &lsPathOut);
    API_IMPL bool directoy_exists(const char * folder_base_name);
    API_IMPL bool create_tmp_directoy(const char * folder_name,char* folder_name_output,const int size_folder_name_output);
    API_IMPL void remove_directory(const char * folder);
    API_IMPL const char * getDecompressModelFileName();
    API_IMPL const char* getBaseName(const char *fileName);
	API_IMPL void setOnAddPathScript(OnAddPathScript onAddPathScript) noexcept;
    API_IMPL bool isBackendEngineOpenGlEs() noexcept;
    API_IMPL bool isBackendEngineDirectx() noexcept;
    API_IMPL bool isBackendEngineVulkan() noexcept;
    API_IMPL bool isBackendEngineMetal() noexcept;

    API_IMPL const uint32_t get_mode_draw_from_string(const char* str_mode_draw);
    API_IMPL const char* get_mode_draw_from_uint(const uint32_t mode_draw, const char* default_mode_draw_ret);
    API_IMPL const uint32_t get_mode_cull_face_from_string(const char* str_mode_cull_face);
    API_IMPL const char* get_mode_cull_face_from_uint(const uint32_t mode_cull_face, const char* default_mode_cull_face_ret);
    API_IMPL const uint32_t get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction);
    API_IMPL const char* get_mode_front_face_direction_from_uint(const uint32_t mode_front_face_direction, const char* default_mode_front_face_direction_ret);

    API_IMPL const bool is_mode_draw_valid(const uint32_t mode_draw)noexcept;
    API_IMPL const bool is_mode_cull_face_valid(const uint32_t mode_cull_face)noexcept;
    API_IMPL const bool is_mode_front_face_direction_valid(const uint32_t mode_front_face_direction)noexcept;
}

namespace log_util
{
    API_IMPL const char* getDirSeparator();
    API_IMPL const char *getDescriptionError(const unsigned int error);
    API_IMPL void replaceString(std::string &source, const std::string &from, const std::string &to);
    API_IMPL void replaceString(std::string &source, const char *from, const char *to);
    API_IMPL void repalceDefaultSeparator(const char *fileNameIn, std::string &fileNameOut);
    API_IMPL const char *basename(const char *fileName);
    API_IMPL void checkGlError(const char *fileName, const int numLine, const char *message);
    API_IMPL void checkGlError(const char *fileName, const int numLine);
    API_IMPL char *formatNewMessage(const size_t length, const char *message, va_list params);
    API_IMPL bool fail(const int lineNum, const char *fileName, const char *format, ...);
    API_IMPL bool onFailed(FILE *fp,const char* fileName, const int numLine, const char *format, ...);
	API_IMPL void log_tag(const TYPE_LOG type_log,const char* tag, const char *format, ...);
	API_IMPL void * log_tag_file_and_line(const int lineNum, const char *fileName,const TYPE_LOG type_log, const char *format, ...);
    API_IMPL void print_colored(const COLOR_TERMINAL color_print_terminal, const char *format, ...);
	API_IMPL void setScriptPrintLine(OnScriptPrintLine onScriptPrintLine) noexcept;
    API_IMPL void callScriptPrintLine() noexcept;
}

#endif

#ifndef ERROR_LOG
    #define ERROR_LOG(...) log_util::log_tag(TYPE_LOG::TYPE_LOG_ERROR, "ERROR", __VA_ARGS__)
    #define INFO_LOG(...)  log_util::log_tag(TYPE_LOG::TYPE_LOG_INFO,  "INFO",  __VA_ARGS__)
    #define WARN_LOG(...)  log_util::log_tag(TYPE_LOG::TYPE_LOG_WARN,  "WARN",  __VA_ARGS__)

	#ifdef _DEBUG
	#    define PRINT_IF_DEBUG(...) log_util::log_tag_file_and_line(__LINE__,__FILE__,TYPE_LOG_ERROR, __VA_ARGS__ );
	#else
	#    define PRINT_IF_DEBUG(...) while(false)
	#endif

	#ifdef _DEBUG
	#    define PRINT_INFO_IF_DEBUG(...) log_util::log_tag_file_and_line(__LINE__,__FILE__,TYPE_LOG_INFO, __VA_ARGS__ );
	#else
	#    define PRINT_INFO_IF_DEBUG(...) while(false)
	#endif

	#define ERROR_AT(line_num,file_name,...)  log_util::log_tag_file_and_line(line_num,file_name,TYPE_LOG_ERROR, __VA_ARGS__ );
	#define INFO_AT(line_num,file_name,...)   log_util::log_tag_file_and_line(line_num,file_name,TYPE_LOG_ERROR, __VA_ARGS__ );
	#define WARN_AT(line_num,file_name,...)   log_util::log_tag_file_and_line(line_num,file_name,TYPE_LOG_ERROR, __VA_ARGS__ );
#endif

API_IMPL const char* getLodePNGVersion();