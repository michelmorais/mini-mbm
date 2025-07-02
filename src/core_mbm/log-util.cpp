/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2016 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <util-interface.h>
#include <platform/mismatch-platform.h>
#include <map>
#include <cstdarg>
#include <cr-static-local.h>
#include <GLES2/gl2.h>

#if defined ANDROID
    #include <android/asset_manager.h>
    #include <android/log.h>
    #include <jni.h>
    #include <unistd.h>
#endif

OnScriptPrintLine onScriptPrintLine = nullptr;

namespace log_util
{

void setScriptPrintLine(OnScriptPrintLine onNewScriptPrintLine) noexcept
{
	onScriptPrintLine = onNewScriptPrintLine;
}

const char *getDescriptionError(const unsigned int error)
{
	switch (error)
    {
        case 0x0500: // GL_INVALID_ENUM:
        {
            return ("\nAn unacceptable value is specified for an enumerated argument.\n"
                    "The offending command is ignored\n"
                    "and has no other side effect than to set the error flag.\n");
        }
        case 0x0501: // GL_INVALID_VALUE:
        {
            return ("\nA numeric argument is out of range.\n"
                    "The offending command is ignored\n"
                    "and has no other side effect than to set the error flag.\n");
        }
        case 0x0502: // GL_INVALID_OPERATION:
        {
            return ("\nThe specified operation is not allowed in the current state.\n"
                    "The offending command is ignored\n"
                    "and has no other side effect than to set the error flag.\n");
        }
        case 0x0506: // GL_INVALID_FRAMEBUFFER_OPERATION:
        {
            return ("\nThe framebuffer object is not complete. The offending command\n"
                    "is ignored and has no other side effect than to set the error flag.\n");
        }
        case 0x0505: // GL_OUT_OF_MEMORY:
        {
            return ("\nThere is not enough memory left to execute the command.\n"
                    "The state of the GL is undefined,\n"
                    "except for the state of the error flags,\n"
                    "after this error is recorded.\n");
        }
        default:
        {
            static char errStr[255];
            sprintf(errStr, "Unknown error gl: decimal:[%d] hexadecimal [0x%x] ", (int)error, (int)error);
            return errStr;
        }
    }
}

void replaceString(std::string &source, const std::string &from, const std::string &to)
{
    std::string newString;
    newString.reserve(source.length()); // avoids a few memory allocations

    std::string::size_type lastPos = 0;
    std::string::size_type findPos;

    while (std::string::npos != (findPos = source.find(from, lastPos)))
    {
        newString.append(source, lastPos, findPos - lastPos);
        newString += to;
        lastPos = findPos + from.length();
    }
    // Care for the rest after last occurrence
    newString += source.substr(lastPos);
    source.swap(newString);
}

void replaceString(std::string &source, const char *from, const char *to)
{
    const std::string _from(from);
    const std::string _to(to);
    replaceString(source, _from, _to);
}

void repalceDefaultSeparator(const char *fileNameIn, std::string &fileNameOut)
{
    if (fileNameIn)
    {
        std::string source(fileNameIn);
#ifdef _WIN32
        const std::string from("/");
        const std::string to("\\");
#else
        const std::string from("\\");
        const std::string to("/");
#endif
        replaceString(source, from, to);
        fileNameOut = source;
    }
}

const char *basename(const char *fileName)
{
    if (fileName)
    {
        std::string f;
        repalceDefaultSeparator(fileName, f);
        const size_t t2 = f.find_last_of(util::getCharDirSeparator());
        if (t2 == std::string::npos)
        {
            return fileName;
        }
        else
        {
            if (fileName[t2 + 1])
                return basename(&fileName[t2 + 1]);
            return &fileName[t2];
        }
    }
    return "nullptr";
}

void checkGlError(const char *fileName, const int numLine, const char *message)
{
    for (GLenum error = glGetError(); error; error = glGetError())
    {
        CR_DEFINE_STATIC_LOCAL(std::vector<GLenum>, lsErrors);
        bool mustContinue = false;
        for (uint32_t lsError : lsErrors)
        {
            if (lsError == error)
            {
                mustContinue = true;
                break;
            }
        }
        if (mustContinue)
            continue;
        lsErrors.push_back(error);
        const char *errorAsString = getDescriptionError(error);
		if(onScriptPrintLine)
			onScriptPrintLine();
        INFO_LOG("File [%s] Line[%d] %s()\n%s", basename(fileName), numLine, message ? message : "[message]",errorAsString);
    }
}

void checkGlError(const char *fileName, const int numLine)
{
    for (GLenum error = glGetError(); error; error = glGetError())
    {
        CR_DEFINE_STATIC_LOCAL(std::vector<GLenum>, lsErrors);
        bool mustContinue = false;
        for (uint32_t lsError : lsErrors)
        {
            if (lsError == error)
            {
                mustContinue = true;
                break;
            }
        }
        if (mustContinue)
            continue;
        lsErrors.push_back(error);
        const char *errorAsString = getDescriptionError(error);
		if(onScriptPrintLine)
			onScriptPrintLine();
        INFO_LOG("\nFile [%s] Line[%d] \n%s", basename(fileName), numLine, errorAsString);
    }
}

char *formatNewMessage(const size_t length, const char *message, va_list params)
{
    auto ret = new char[((length + 1) * sizeof(char))];
    vsnprintf(ret, length + 1, message, params);
    ret[length] = 0;
    return ret;
}

bool fail(const int lineNum, const char *fileName, const char *format, ...)
{
    va_list va_args;
    va_start(va_args, format);
    const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
    va_end(va_args);
    va_start(va_args, format);
    char *_buffer = formatNewMessage(length, format, va_args);
    va_end(va_args);
#ifdef _WIN32
    HWND hConsole = GetConsoleWindow();
    ShowWindow(hConsole, SW_SHOWNOACTIVATE);
#endif
	if(onScriptPrintLine)
		onScriptPrintLine();
    ERROR_LOG("File[%s] line[%d]\n%s\n", basename(fileName), lineNum, _buffer);
    delete[] _buffer;
    return false;
}

bool onFailed(FILE *fp, const char *fileName, const int numLine, const char *format, ...)
{
    va_list va_args;
    va_start(va_args, format);
    const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
    va_end(va_args);
    va_start(va_args, format);
    char *_buffer = formatNewMessage(length, format, va_args);
    va_end(va_args);
	if(onScriptPrintLine)
		onScriptPrintLine();
    log_util::log_tag_file_and_line(numLine, fileName,TYPE_LOG_ERROR, _buffer);
    delete[] _buffer;
    if (fp)
        fclose(fp);
    return false;
}

void log_tag(const TYPE_LOG type_log,const char* tag, const char *format, ...)
{
    va_list va_args;
    va_start(va_args, format);
    const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
    va_end(va_args);
    va_start(va_args, format);
    char *_buffer = formatNewMessage(length, format, va_args);
    va_end(va_args);

    #if defined ANDROID
        switch(type_log)
        {
            case TYPE_LOG_ERROR:
            {
                typedef std::map<std::string, bool> mapError;
                CR_DEFINE_STATIC_LOCAL(mapError, errorList);
                if (errorList[_buffer] == false)
                    __android_log_print(ANDROID_LOG_ERROR, tag, "%s", _buffer);
                errorList[_buffer] = true;
            }
            break;
            case TYPE_LOG_INFO:
            {
                __android_log_print(ANDROID_LOG_INFO, tag, "%s", _buffer);
            }
            break;
            case TYPE_LOG_WARN:
            {
                __android_log_print(ANDROID_LOG_DEBUG, tag, "%s", _buffer);
            }
            break;
        }
    #elif defined(__linux__) && !defined(ANDROID)

        switch(type_log)
        {
            case TYPE_LOG_ERROR:
            {
				typedef std::map<std::string, bool> mapError;
                CR_DEFINE_STATIC_LOCAL(mapError, errorList);
                if (errorList[_buffer] == false)
					fprintf(stdout, "\033[1;31mERR\033[0m %s\n", _buffer);
				errorList[_buffer] = true;
            }
            break;
            case TYPE_LOG_INFO:
            {
                fprintf(stdout, "\033[1;32mINFO\033[0m %s\n", _buffer);
            }
            break;
            case TYPE_LOG_WARN:
            {
                fprintf(stdout, "\033[1;33mWARN\033[0m %s\n", _buffer);
            }
            break;
        }
    #elif defined _WIN32
		HWND hConsole = GetConsoleWindow();
		HANDLE hConsoleSTD = GetStdHandle(STD_OUTPUT_HANDLE);
        ShowWindow(hConsole, SW_SHOWNOACTIVATE);
        switch(type_log)
        {
            case TYPE_LOG_ERROR:
            {
				typedef std::map<std::string, bool> mapError;
                CR_DEFINE_STATIC_LOCAL(mapError, errorList);
                if (errorList[_buffer] == false)
				{
					SetConsoleTextAttribute(hConsoleSTD, 12);
					fprintf(stdout, "ERR");
					SetConsoleTextAttribute(hConsoleSTD, 15);
					fprintf(stdout, " %s\n", _buffer);
				}
				errorList[_buffer] = true;
            }
            break;
            case TYPE_LOG_INFO:
            {
				SetConsoleTextAttribute(hConsoleSTD, 10);
				fprintf(stdout, "INFO");
				SetConsoleTextAttribute(hConsoleSTD, 15);
                fprintf(stdout, " %s\n", _buffer);
            }
            break;
            case TYPE_LOG_WARN:
            {
				SetConsoleTextAttribute(hConsoleSTD, 14);
				fprintf(stdout, "WARN");
				SetConsoleTextAttribute(hConsoleSTD, 15);
                fprintf(stdout, " %s\n", _buffer);
            }
            break;
        }
	#else
		#error "Platform not defined for LOG"
    #endif
    delete[] _buffer;
}

void * log_tag_file_and_line(const int lineNum, const char *fileName,const TYPE_LOG type_log, const char *format, ...)
{
	va_list va_args;
    va_start(va_args, format);
    const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
    va_end(va_args);
    va_start(va_args, format);
    char * buffer = formatNewMessage(length, format, va_args);
    va_end(va_args);
	switch(type_log)
    {
        case TYPE_LOG_ERROR:
        {
			ERROR_LOG("File[%s] line[%d]\n%s\n", basename(fileName), lineNum, buffer);
        }
        break;
        case TYPE_LOG_INFO:
        {
            INFO_LOG("File[%s] line[%d]\n%s\n", basename(fileName), lineNum, buffer);
        }
        break;
        case TYPE_LOG_WARN:
        {
            WARN_LOG("File[%s] line[%d]\n%s\n", basename(fileName), lineNum, buffer);
        }
        break;
    }
	
	delete[] buffer;
	return nullptr;
}

void print_colored(const COLOR_TERMINAL color_print_terminal, const char *format, ...)
{
    va_list va_args;
    va_start(va_args, format);
    const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
    va_end(va_args);
    va_start(va_args, format);
    char * buffer = formatNewMessage(length, format, va_args);
    va_end(va_args);

    #if defined ANDROID
        if(color_print_terminal == COLOR_TERMINAL_RED)
            ERROR_LOG("%s",buffer);
        else if(color_print_terminal == COLOR_TERMINAL_YELLOW)
            WARN_LOG("%s",buffer);
        else
            INFO_LOG("%s",buffer);
    #else
        #if defined _WIN32

            if(color_print_terminal == COLOR_TERMINAL_WHITE)
            {
                fprintf(stdout, " %s\n", buffer);
            }
            else
            {
                static const std::map<COLOR_TERMINAL,int> map_color =
                {
                    {COLOR_TERMINAL_WHITE,   15},
                    {COLOR_TERMINAL_RED,     12},
                    {COLOR_TERMINAL_YELLOW,  14},
                    {COLOR_TERMINAL_GREEN,   10},
                    {COLOR_TERMINAL_BLUE,    9},
                    {COLOR_TERMINAL_MAGENTA, 13},
                    {COLOR_TERMINAL_CIAN,    11},
                };

                const int color = map_color.at(color_print_terminal);
                HANDLE hConsoleSTD = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(hConsoleSTD, color);
                fprintf(stdout, " %s\n", buffer);
                SetConsoleTextAttribute(hConsoleSTD, 15);
            }
        #elif defined(__linux__) && !defined(ANDROID)
            static const std::map<COLOR_TERMINAL,std::string> map_color =
            {
                {COLOR_TERMINAL_WHITE,   ""},
                {COLOR_TERMINAL_RED,     "\033[31m"},
                {COLOR_TERMINAL_YELLOW,  "\033[33m"},
                {COLOR_TERMINAL_GREEN,   "\033[32m"},
                {COLOR_TERMINAL_BLUE,    "\033[34m"},
                {COLOR_TERMINAL_MAGENTA, "\033[35m"},
                {COLOR_TERMINAL_CIAN,    "\033[36m"},
            };

            const std::string & color = map_color.at(color_print_terminal);
            fprintf(stdout, "%s%s%s",color.c_str(), buffer, "\033[0m");
        #else
            #error "Unknown platform"
        #endif

    #endif

    delete [] buffer;
}

}
