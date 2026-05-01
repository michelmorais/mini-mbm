/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
#ifndef PLUS_WINDOWS_H
#define PLUS_WINDOWS_H

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifdef UNICODE
#undef UNICODE
#define _MBCS 
#endif		  

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef ____PLUS_WIN_MY_DIRSEPARATOR_
#define ____PLUS_WIN_MY_DIRSEPARATOR_ 1

#endif

#ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif

#ifdef __MINGW32__

#ifndef _WIN32_IE
    #define _WIN32_IE 0x0600
#else
    #undef _WIN32_IE
    #define _WIN32_IE 0x0600
#endif

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x600
#else
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x600
#endif

/*
 Compilar com NPP - MingGW - dev - C

 NPP_SAVE
 g++ -o "$(CURRENT_DIRECTORY)\$(NAME_PART)" "$(FULL_CURRENT_PATH)" -static -std=c++0x
 "C:\path_lib_migw\libgdi32.a"  "C:\path_lib_migw\libcomctl32.a" "C:\path_lib_migw\libcomdlg32.a"
 "$(CURRENT_DIRECTORY)\$(NAME_PART)"
 */

/*
 Exemplo: arquivo "main.cpp"

 #include "pluswindows.mbm.h"
 int main()
 {
 mbm::WINDOW w;
 w.init(600,400);
 w.enterLoop(nullptr);
 return 0;
 }
 */
#else

#endif

#define WM_SYSTRAY (WM_APP + 6)
#define WM_SYSTRAY2 (WM_APP + 7)

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (P)
#endif
#ifndef DBG_UNREFERENCED_PARAMETER
#define DBG_UNREFERENCED_PARAMETER(P) (P)
#endif
#ifndef DBG_UNREFERENCED_LOCAL_VARIABLE
#define DBG_UNREFERENCED_LOCAL_VARIABLE(V) (V)
#endif

#pragma once
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <Windowsx.h>
#include <WinUser.h>
#include <shellapi.h>
#include <winreg.h>
#include <Shlobj.h>
#include <list>
#include <vector>
#include <iostream>
#include <Commctrl.h>
#include <math.h>
#include <float.h>  /* FLT_EPSILON on MinGW */
#include <map>
#include <assert.h>
#include <set>
#include <Richedit.h>
#include <windef.h>
#include <winbase.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <thread>

#if defined NO_LIBRARY_WINPLUS
// define NO_LIBRARY_WINPLUS to use directlly the implementation of the plusWindows
// Just include plusWindows.cpp and defaultThemePlusWindows.cpp in your project
#define API_IMPL
#else
#include "core-exports.h"
#endif

#ifdef __MINGW32__
#define TOOLINFO TTTOOLINFO
#ifndef BS_PUSHBOX
#define BS_PUSHBOX 0x0000000AL
#endif
// tagNMTTCUSTOMDRAW is provided by MinGW's <Commctrl.h> (included above)
#else
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Msimg32.lib") // AlphaBlend
#pragma warning(disable : 4996)
#pragma warning(disable : 4512)
#endif

//#ifdef _DEBUG
//  #ifdef _WIN32
//      #ifndef __SELF__LEAK_D
//          #define __SELF__LEAK_D 1
//          // http://msdn.microsoft.com/en-us/library/e5ewb1h3(v=vs.71).aspx
//          #define _CRTDBG_MAP_ALLOC
//          #include <stdlib.h>
//          #include <crtdbg.h>
//          //#ifndef DBG_NEW
//          //#define DBG_NEW_OLD new
//          //#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
//          //#define new DBG_NEW
//          //#endif
//          //class __SELF__LEAK
//          //{
//          //  public:
//          //    __SELF__LEAK()
//          //    {
//          //        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//          //    }
//          //  __SELF__LEAK _SELF_LEAK_START;
//          //}
//      #endif
//  #endif
//#endif
#define PACKVERSION(major, minor) MAKELONG(minor, major)

DWORD GetVersionDll(const char *lpszDllName);
#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap);
int c99_snprintf(char *outBuf, size_t size, const char *format, ...);

#endif

void __destroyMenu(void *extraParams);
API_IMPL const int __getTabStopPixelSize(HWND hwnd);

namespace mbm
{
class __NC_BORDERS
{
  public:
    struct __NC_BUTTONS
    {
        RECT rectClose;
        RECT rectMaximize;
        RECT rectMinimize;
        int  distCloseRight;
        int  distMaxRight;
        int  distMinRight;
        bool isHoverClose;
        bool isHoverMax;
        bool isHoverMin;
        bool hasCloseButton;
        bool hasMaximizeButton;
        bool hasMinimizeButton;
        __NC_BUTTONS();
        virtual ~__NC_BUTTONS();
    };
    HRGN hrgnLeft;
    HRGN hrgnRight;
    HRGN hrgnTop;
    HRGN hrgnBottom;

    HRGN hrgnAll;

    RECT          rectLeft;
    RECT          rectRight;
    RECT          rectTop;
    RECT          rectBottom;
    RECT          rcClient, rcWind;
    __NC_BUTTONS *buttons;
    bool          status;
    int           sides;
    __NC_BORDERS(HWND hwnd, const bool invalidateRegion, __NC_BUTTONS *buttons_);
    virtual ~__NC_BORDERS();
    void Defaultresult(int ret);
};

API_IMPL WCHAR *toWchar(const char *str, WCHAR *outText);
API_IMPL char *toChar(const WCHAR *wstr, char *outText);
API_IMPL void destroyListComBetweenWindows(HWND hwnd);

class STATIC_IMAGE_RESOURCE
{
  public:
      API_IMPL STATIC_IMAGE_RESOURCE(const uint32_t w, const uint32_t h, const uint32_t s, const char *nickName_,const uint32_t *d, const uint32_t c);
    const uint32_t  width;
    const uint32_t  height;
    const uint32_t  size;
    const char *        nickName;
    const uint32_t *data;
    const uint32_t  colorKeying;
};


API_IMPL void split(std::vector<std::string> &result, const char *in, const char delim);
API_IMPL const char *getLastErrWindows(const char *where, char *outMessage);
API_IMPL const char* getHresultErr(HRESULT hr, const char* where, char* outMessage);
API_IMPL bool startUpWindows64(const char *name);
API_IMPL bool startUpWindows(const char *name);

class REGEDIT // regedit do windows. set "Project>Configuration Properties>Linker>Manifest File>UAC Execution Level" to
              // requireAdministrator
{
  public:
    API_IMPL REGEDIT();
    API_IMPL virtual ~REGEDIT();
    
    API_IMPL bool openKey(HKEY hRootKey, const wchar_t *strKey, const DWORD acess = KEY_ALL_ACCESS);
    API_IMPL bool openKey(HKEY hRootKey, const char *strKey, const DWORD acess = KEY_ALL_ACCESS);
    API_IMPL void setVal(LPCTSTR key, DWORD value);
    API_IMPL void setString(LPCTSTR key, const std::string &value);
    API_IMPL DWORD getVal(LPCTSTR key, DWORD valueNotFound);
    API_IMPL std::string getString(LPCTSTR key, const char * stringNotFound);
    API_IMPL void closeKey();
    /*
     //sample:
     DWORD v1, v2;
     mbm::REGEDIT regedit;
     regedit.openKey(HKEY_LOCAL_MACHINE,L"SOFTWARE\\activiesKid");
     v1 = regedit.getVal(L"Value1");
     v2 = regedit.getVal(L"Value2");
     v1 += 5;
     v2 += 2;
     regedit.setVal( L"Value1", v1);
     regedit.setVal( L"Value2", v2);
     regedit.closeKey();
     */
  private:
    HKEY hKey;
    bool printLastErrWindows(const char *where = nullptr);
};

API_IMPL int getRandomInt(const int min, const int max);
API_IMPL char getRandomChar(const char min, const char max);
API_IMPL float getRandomFloat(const float min, const float max);

}
namespace mbm
{
    void __destroyOnExitAllListComBetweenWindows();
};

API_IMPL void __initRandomSeed();

#ifndef PURE
    #define PURE = 0;
#endif

#if UNICODE
API_IMPL WCHAR *getNextClassNameWindow();
#else
API_IMPL char *getNextClassNameWindow();
#endif

struct __TAB_GROUP_DESC
{
    //--------------------------------------------------------------------------------------------
    const int index;
    const int idDest;
    bool      enableVisibleGroups;
    int       idGroupTabBox;
    int       idTabControlByGroup;
    int       displacementX;
    int       displacementY;

    long x;
    long y;
    long width;
    long height;
    long widthButton;
    long heightButton;

    __TAB_GROUP_DESC *              tabFather;
    __TAB_GROUP_DESC *              tabSelected;
    std::vector<__TAB_GROUP_DESC *> lsTabChilds;
    std::vector<HWND *>             lsHwndComponents;
    //--------------------------------------------------------------------------------------------
    API_IMPL __TAB_GROUP_DESC(const int _index, const int _idDest);
};

struct TRACK_BAR_INFO
{
    float minPosition;
    float maxPosition;
    float position;
    float positionInverted;
    float defaultPosition;
    float tickLarge;
    float tickSmall;
    float increment;
    bool  isVertical;
    bool  invertMinMaxText;
    API_IMPL TRACK_BAR_INFO();
};

struct PROGRESS_BAR_INFO
{
    const bool vertical;
    float      minRange;
    float      maxRange;
    float      position;
    API_IMPL PROGRESS_BAR_INFO(const bool vertical_);
};

struct __HEADER_BMP
{
    uint8_t identy[2]; //'BM' - Windows 3.1x, 95, NT, ...
                             //'BA' - OS/2 Bitmap Array (matriz Bitmap_True_Color_24_Bits)
                             //'CI' - OS/2 Color Icon (ícone colorido)
                             //'CP' - OS/2 Color Pointer (Ponteiro colorido)
                             //'IC' - OS/2 Icone
                             //'PT' - OS/2 Ponteiro
    uint8_t length[4];
    uint8_t reserved[4];
    uint8_t offSet[4];
    uint8_t sizeHeader[4]; // 28h - Windows 3.1x, 95, NT, 0Ch - OS/2 1.x, F0h - OS/2 2.x
    uint8_t width[4];
    uint8_t height[4];
    uint8_t plane[2];
    uint8_t bitsPerPixels[2];
    // 1 - Bitmap monocromático (preto e COR_BRANCO)
    // 4 - Bitmap De 16 cores
    // 8 - Bitmap De 256 cores
    // 16 - Bitmap De 16bits (high color)
    // 24 - Bitmap De 24bits (true color)
    // 32 - Bitmap De 32bits (true color)
    uint8_t compressed[4];
    // 0 - nenhuma (Também identificada Por BI_RGB)
    // 1 - RLE 8 bits/Pixel (Também identificada Por BI_RLE4)
    // 2 - RLE 4 bits/Pixel (Também identificada Por BI_RLE8)
    // 3 - Bitfields (Também identificada Por BI_BITFIELDS)
    uint8_t sizeDataArea[4];
    uint8_t resH[4];
    uint8_t resV[4];
    uint8_t colors[4];
    uint8_t importantsColors[4];
    //------------------------------------------------------------------------------------------------------------------
    API_IMPL uint32_t getAsUintFromCharPointer(uint8_t *adress);
};

namespace mbm
{
class WINDOW;
API_IMPL WINDOW *getWindow(HWND hwnd);
API_IMPL WINDOW *getLastWindow();
API_IMPL WINDOW *getFirstWindow();
API_IMPL void    closeAllWindows();

class BMP
{
  public:
    API_IMPL BMP();
    API_IMPL virtual ~BMP();
    API_IMPL void release();
    API_IMPL bool load(HWND hwnd, const int ID_RESOURCE);
    API_IMPL bool loadTrueColor(const char *fileName);
    API_IMPL bool load(const char *fileNameBitmap);
    API_IMPL bool load(mbm::STATIC_IMAGE_RESOURCE &imageResource);
    API_IMPL const int isLoaded()const;
    API_IMPL const int getWidth() const;
    API_IMPL const int getHeight() const;
    API_IMPL void draw(HDC hdc);
    API_IMPL void draw(HDC hdc, const int x, const int y);
    API_IMPL void draw(HDC hdc, const RECT &rect);
    API_IMPL void draw(HDC hdc, const int xPosition, const int yPosition, const int xSource, const int ySource,
                     const int width, const int height);
    API_IMPL bool createBitmap(int width, int heigth);
    API_IMPL bool createBitmap(int width, int heigth, const uint8_t *dataImage);
    API_IMPL bool updateData();
    API_IMPL HBITMAP getHBitmap() const;
    API_IMPL BITMAP *getBitmapInfo();
    API_IMPL uint8_t *getData() const;//RGB
    
  private:
    
    uint32_t getAsUintFromCharPointer(uint8_t *adress);
    
    HBITMAP        data;
    BITMAP         bitmapInfo;
    BITMAPINFO     bInfo;
    uint8_t *dataRGB;
    //--------------------------------------------------------------------------------------------
};

class EVENTS_WIN32
{
public:
    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onTouchDown(HWND w, int key, float x, float y) PURE;

    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onTouchUp(HWND w, int key, float x, float y) PURE;

    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onTouchMove(HWND w, float x, float y) PURE;

    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onTouchZoom(HWND w, float zoom) PURE;

    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onKeyDown(HWND w, int key) PURE;

    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onKeyUp(HWND w, int key) PURE;

    //--------------------------------------------------------------------------------------------
    API_IMPL virtual void onDoubleClick(HWND w, float x, float y, int key) PURE;
    //--------------------------------------------------------------------------------------------

    API_IMPL virtual void onResizeWindow(HWND w, int width, int height) PURE;
};

struct TIMER;
struct RADIO_GROUP;
struct SPIN_PARAMSf;
struct SPIN_PARAMSi;

typedef enum TYPE_WINDOWS_INTO_WINPLUS_ {
    WINPLUS_TYPE_NONE               = -1,
    WINPLUS_TYPE_WINDOW             = 0,
    WINPLUS_TYPE_LABEL              = 1,
    WINPLUS_TYPE_BUTTON             = 2,
    WINPLUS_TYPE_BUTTON_TAB         = 3,
    WINPLUS_TYPE_CHECK_BOX          = 4,
    WINPLUS_TYPE_RADIO_BOX          = 5,
    WINPLUS_TYPE_COMBO_BOX          = 6,
    WINPLUS_TYPE_LIST_BOX           = 7,
    WINPLUS_TYPE_TEXT_BOX           = 8,
    WINPLUS_TYPE_SCROLL             = 9,
    WINPLUS_TYPE_SPIN_INT           = 10,
    WINPLUS_TYPE_SPIN_FLOAT         = 11,
    WINPLUS_TYPE_RICH_TEXT          = 12,
    WINPLUS_TYPE_CHILD_WINDOW       = 14,
    WINPLUS_TYPE_GROUP_BOX          = 15,
    WINPLUS_TYPE_PROGRESS_BAR       = 16,
    WINPLUS_TYPE_TIMER              = 17,
    WINPLUS_TYPE_TRACK_BAR          = 18,
    WINPLUS_TYPE_STATUS_BAR         = 19,
    WINPLUS_TYPE_MENU               = 20,
    WINPLUS_TYPE_SUB_MENU           = 21,
    WINPLUS_TYPE_GROUP_BOX_TAB      = 22,
    WINPLUS_TYPE_TRY_ICON_MENU      = 23,
    WINPLUS_TYPE_TRY_ICON_SUB_MENU  = 24,
    WINPLUS_TYPE_TOOL_TIP           = 25,
    WINPLUS_TYPE_WINDOWNC           = 26,
    WINPLUS_TYPE_WINDOW_MESSAGE_BOX = 27,
    WINPLUS_TYPE_IMAGE              = 28,
} TYPE_WINDOWS_WINPLUS;

typedef enum WINPLUS_TYPE_CURSOR_ {
    WINPLUS_CURSOR_ARROW       = 0,
    WINPLUS_CURSOR_IBEAM       = 1,
    WINPLUS_CURSOR_WAIT        = 2,
    WINPLUS_CURSOR_CROSS       = 3,
    WINPLUS_CURSOR_UPARROW     = 4,
    WINPLUS_CURSOR_SIZENWSE    = 5,
    WINPLUS_CURSOR_SIZENESW    = 6,
    WINPLUS_CURSOR_SIZEWE      = 7,
    WINPLUS_CURSOR_SIZENS      = 8,
    WINPLUS_CURSOR_SIZEALL     = 9,
    WINPLUS_CURSOR_NO          = 10,
    WINPLUS_CURSOR_HAND        = 11,
    WINPLUS_CURSOR_APPSTARTING = 12,
    WINPLUS_CURSOR_WITHOUT     = 13,
    WINPLUS_CURSOR_HELP        = 14
} WINPLUS_TYPE_CURSOR;

struct USER_DRAWER;

class DATA_EVENT
{
  public:
    const TYPE_WINDOWS_WINPLUS type;
    int                        idComponent;
    USER_DRAWER *              userDrawer;
    API_IMPL DATA_EVENT();
    API_IMPL DATA_EVENT(int idComponent_, void *Data, USER_DRAWER *UserDrawer, const TYPE_WINDOWS_WINPLUS type_, const char *_myString);
    API_IMPL const int getAsInt();
    API_IMPL const float getAsFloat();
    API_IMPL const bool getAsBool();
    API_IMPL const char *getAsString();
    API_IMPL TIMER *getAsTimer();
    API_IMPL TRACK_BAR_INFO *getAsTrackBar();
    API_IMPL RADIO_GROUP *getAsRadio();
    API_IMPL SPIN_PARAMSi *getAsSpin();
    API_IMPL SPIN_PARAMSf *getAsSpinf();

  private:
    const int   getInt();
    const float getFloat();
    const bool  getBool();
    const char *getString();
    const char *format(bool v);
    const char *format(int v);
    const char *format(float v);
    const char *format();
    void *             data;
    const char *       myString;
    char               _ret[1024];
};

typedef void(__cdecl *OnDoModal)(WINDOW *window);
typedef void(__cdecl *OnEventWinPlus)(WINDOW *window, DATA_EVENT &dataEvent);
typedef void(__cdecl *OnKeyboardEvent)(WINDOW *window, int VK);
typedef void(__cdecl *OnMouseEvent)(WINDOW *window, int x, int y);
typedef void(__cdecl *OnMouseEventScroll)(WINDOW *window, bool increment);
typedef int(__cdecl *OnParseRawInput)(WINDOW *window, HRAWINPUT phRawInput);

API_IMPL const bool isNumeric(const char letter);
API_IMPL bool isNum(const char *numberAsString);
API_IMPL bool isNum(const WCHAR *numberAsString);
API_IMPL char *trimRight(char *stringSource);
API_IMPL char *trimLeft(char *stringSource);
API_IMPL char *trim(char *stringSource);

struct MONITOR
{
    //---------------------------------------------------------------------------------------------------------------
    long  width;
    long  height;
    POINT position;
    DWORD frequency;
    bool  isPrimary;
    DWORD index;
    //---------------------------------------------------------------------------------------------------------------
    API_IMPL MONITOR();
};

class MONITOR_MANAGER
{
  private:
    std::vector<MONITOR> lsMonitors;

  public:
    //---------------------------------------------------------------------------------------------------------------
    API_IMPL MONITOR_MANAGER();
    API_IMPL virtual ~MONITOR_MANAGER();
    API_IMPL void updateMonitors();
    API_IMPL long getWidthWindow(const DWORD indexMonitor = 0);
    API_IMPL long getHeightWindow(const DWORD indexMonitor = 0);
    API_IMPL POINT getPositionWindow(const DWORD indexMonitor = 0);
    API_IMPL DWORD getIndexMainMonitor();
    API_IMPL bool getMonitor(const DWORD indexMonitor, mbm::MONITOR *monitorOut);
    API_IMPL bool isMainMonitor(const DWORD indexMonitor = 0);
    API_IMPL DWORD getTotalMonitor();
};

struct SPIN_PARAMSi
{
    int min;
    int max;
    int increment;
    int currentPosition;
    SPIN_PARAMSi(int minValue, int maxValue, int increment_, int currentPosition_);
};

struct SPIN_PARAMSf
{
    float minf;
    float maxf;
    float increment;
    float currentPosition;
    int   precision;
    API_IMPL SPIN_PARAMSf(float minValue, float maxValue, float increment_, float currentPosition_, int precision_);
};

struct RADIO_GROUP
{
    const int     idRadio;
    const int     idParent;
    std::set<int> lsRadioGroup;
    bool          checked;
    API_IMPL RADIO_GROUP(const int id, const int _idParent);
    API_IMPL ~RADIO_GROUP();
};

struct TIMER
{
    int            timInMilisecond;
    int            times;
    int            idTimer;
    OnEventWinPlus onEventTimer;
    API_IMPL TIMER(int timeElapsed_inMiliSeconds, int idTimer_, OnEventWinPlus onEventTimer_);
};

struct EDIT_TEXT_DATA
{
    mbm::SPIN_PARAMSi *spin;
    mbm::SPIN_PARAMSf *spinf;
    const int          id;
    char *             text;
    uint32_t       len;
    API_IMPL EDIT_TEXT_DATA(const int _id);
    API_IMPL EDIT_TEXT_DATA(mbm::SPIN_PARAMSi *_spin, mbm::SPIN_PARAMSf *_spinf, const int _id);
    API_IMPL ~EDIT_TEXT_DATA();
};

class DRAW; // forward declaration — defined later in this file

class COM_BETWEEN_WINP
{
    friend class WINDOW;
    friend class COMPONENT_INFO;
    friend class KEY_BOARD_STATE;
    friend class DRAW;
    friend void        __destroyOnExitAllListComBetweenWindows();
    friend API_IMPL void destroyListComBetweenWindows(HWND hwnd);
    friend API_IMPL void destroyAlTimers(HWND hwnd);
    friend API_IMPL void destroyTimer(HWND hwnd, const int idTimer);
    friend API_IMPL WINDOW *getWindow(HWND hwnd);
    friend API_IMPL WINDOW *getLastWindow();
    friend API_IMPL WINDOW *getFirstWindow();
    friend API_IMPL void    closeAllWindows();
    friend API_IMPL RECT getMenuRect(int idWindow, int myId);
    friend API_IMPL COM_BETWEEN_WINP *getComBetweenWinp(const int id);
    friend API_IMPL COM_BETWEEN_WINP *getComBetweenWinp(const HWND hwnd);
    friend API_IMPL COM_BETWEEN_WINP *getComBetweenWinp(const HWND owerHwnd, const int id);
    friend API_IMPL COM_BETWEEN_WINP *getComBetweenWinpTryIcon(const HWND owerHwnd);
    friend API_IMPL COM_BETWEEN_WINP *getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *me,
                                                     TYPE_WINDOWS_WINPLUS typeMe, void *extraParams_, const int idDest,
                                                     USER_DRAWER *UserDrawer);

  public:
    API_IMPL int getId() const;
    API_IMPL mbm::WINDOW *getWindow();
    API_IMPL TYPE_WINDOWS_WINPLUS getType();
    API_IMPL HWND getHwnd();
    USER_DRAWER *userDrawer;

  private:

    OnEventWinPlus                         onEventWinPlus;
    HWND                                   hwnd;
    HWND                                   owerHwnd;
    WINDOW *                               ptrWindow;
    TYPE_WINDOWS_WINPLUS                   typeWindowWinPlus;
    const int                              id;
    int                                    idOwner;
    int                                    idNextFocus;
    void *                                 extraParams;
    static std::vector<COM_BETWEEN_WINP *> lsComBetweenWinp;
    DRAW *                                 graphWin;
    WNDPROC                                _oldProc;
    std::set<COM_BETWEEN_WINP *>           myChilds;
  
    API_IMPL COM_BETWEEN_WINP(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *win, TYPE_WINDOWS_WINPLUS typeMe,
                     void *extraParams_, const int idOwner_, USER_DRAWER *UserDrawer);
    API_IMPL COM_BETWEEN_WINP(COM_BETWEEN_WINP *ncCopy);
    API_IMPL virtual ~COM_BETWEEN_WINP();
};

API_IMPL COM_BETWEEN_WINP *getComBetweenWinp(const int id);
API_IMPL COM_BETWEEN_WINP *getComBetweenWinp(const HWND owerHwnd, const int id);
API_IMPL COM_BETWEEN_WINP *getComBetweenWinp(const HWND hwnd);
API_IMPL COM_BETWEEN_WINP *getComBetweenWinpTryIcon(const HWND owerHwnd);
API_IMPL void destroyListComBetweenWindows(HWND hwnd);
API_IMPL void destroyAlTimers(HWND hwnd);
API_IMPL void destroyTimer(HWND hwnd, const int idTimer);

class COMPONENT_INFO;
typedef void(__cdecl *OnRenderComponent)(mbm::COMPONENT_INFO &component);
//-----------------------------------------------------------------------------------------------------------------
struct USER_DRAWER
{
    bool              enableHover;
    bool              enablePressed;
    API_IMPL virtual bool      render(COMPONENT_INFO & component) = 0;
    void *            that;
    DRAW *            draw;
    API_IMPL USER_DRAWER(void *That = nullptr, DRAW *Draw = nullptr);
};

struct USER_DATA :public USER_DRAWER
{
    API_IMPL USER_DATA();
    API_IMPL USER_DATA(void* newData);
    API_IMPL USER_DATA(void* newData,DRAW *Draw);
    API_IMPL virtual bool      render(COMPONENT_INFO & component);
    API_IMPL void setData(void* newData);
    API_IMPL void* getData();
};
//-----------------------------------------------------------------------------------------------------------------
class COMPONENT_INFO
{
    friend class WINDOW;
    friend class DRAW;

  protected:
    COMPONENT_INFO(COM_BETWEEN_WINP *ptr, const LPDRAWITEMSTRUCT _lpdis, const HDC validHDC, const bool _isHover,
                   const bool _isPressed, USER_DRAWER *UserDrawer);

    COMPONENT_INFO(COM_BETWEEN_WINP *ptr, const bool _isHover, const bool _isPressed, USER_DRAWER *UserDrawer);
    COMPONENT_INFO(COM_BETWEEN_WINP *ptr, PAINTSTRUCT *ps, const HDC validHDC, const bool _isHover, const bool _isPressed,
                   USER_DRAWER *UserDrawer);
    COMPONENT_INFO(COM_BETWEEN_WINP *ptr, RECT *rect, const HDC validHDC, const bool _isHover, const bool _isPressed,
                   USER_DRAWER *UserDrawer);
  public:
      API_IMPL virtual ~COMPONENT_INFO();
    const OnEventWinPlus       onEventWinPlus;
    const HWND                 hwnd;
    const HWND                 owerHwnd;
    const TYPE_WINDOWS_WINPLUS typeWindowWinPlus;
    const int                  id;
    const int                  idOwner;
    const int                  idNextFocus;
    const LPDRAWITEMSTRUCT     lpdis;
    const bool                 isHover;
    const HDC                  hdc;
    const bool                 isPressed;
    RECT                       rect;
    WINDOW *                   ptrWindow;
    USER_DRAWER *              userDrawer;
    POINT                      mouse;
    void *                     extraParams;

  private:
    static void setCursorPos(COMPONENT_INFO *);
};

class DRAW : public USER_DRAWER
{
    friend class mbm::WINDOW;

  public:
    //-----------------------------------------------------------------------------------------------------------
    struct COLOR
    {
        uint8_t red, green, blue;
        API_IMPL COLOR();
        API_IMPL COLOR(const COLORREF &c);
        API_IMPL COLOR(const uint8_t r, const uint8_t g, const uint8_t b);
        COLORREF operator=(const COLOR &) noexcept
        {
            return COLORREF(RGB(red, green, blue));
        }
        operator COLORREF() noexcept
        {
            return COLORREF(RGB(red, green, blue));
        }
        COLOR &operator=(const COLORREF &c) noexcept
        {
            this->red   = GetRValue(c);
            this->green = GetGValue(c);
            this->blue  = GetBValue(c);
            return *this;
        }
    };
    int dwRop;
    API_IMPL DRAW();
    API_IMPL DRAW(mbm::COMPONENT_INFO *component);
    API_IMPL virtual ~DRAW();
    API_IMPL HBRUSH createBrush(uint8_t r, uint8_t g, uint8_t b);
    API_IMPL void release(HBRUSH &hbrush);
    API_IMPL HPEN createPen(uint8_t r, uint8_t g, uint8_t b, int _stylePen = PS_SOLID, int width = 0);
    API_IMPL HPEN createPen(COLORREF color);
    API_IMPL void release(HPEN &hpen);
    API_IMPL HBRUSH createGradientBrush(COLORREF fromColor, COLORREF toColor, const RECT &rc, const bool horizontal = true,
                               const bool power2 = true, const bool reflected = true);
    API_IMPL void drawLine(const int initialX, const int initialY, const int finalX, const int finalY);
    API_IMPL void drawLine(const POINT &initialPoint, const POINT &finalPoint);
    API_IMPL void drawRectangle(const RECT &REct);
    API_IMPL void drawRectangle(const int x, const int y, const int w, const int h);
    API_IMPL void drawCircle(const POINT &point, const int ray);
    API_IMPL void drawCircle(const int initialX, const int initialY, const int ray);
    API_IMPL void drawElipse(const RECT &rect);
    API_IMPL HDC setHDC(HDC newHdc);
    API_IMPL void setFont(HFONT _hfont);
    API_IMPL static HFONT createFont(const char *pszFaceName = "Times New Roman", const int cHeight = 20, const int cWidth = 5,
                            const int cEscapement = 0, const int cOrientation = 0, const int cWeight = FW_NORMAL,
                            const DWORD bItalic = 0, const DWORD bUnderline = 0, const DWORD bStrikeOut = 0);
    API_IMPL void drawText(RECT *rect, const char *text, const bool bakgroundTransparente = true);
    API_IMPL void drawText(const int x, const int y, const char *text, const bool bakgroundTransparente = true);
    API_IMPL void drawTextRotated(const int x, const int y, HWND hwnd, const char *text, DWORD color_text, int angle,
                         const bool bakgroundTransparente = true);
    API_IMPL void drawText(const int x, const int y, const DWORD color, const char *text,
                         const bool bakgroundTransparente = true);
    API_IMPL void drawText(const RECT &rect, const DWORD color, const char *text, const bool bakgroundTransparente = true);
    API_IMPL void drawText(const int x, const int y, const uint8_t red, const uint8_t green,
                         const uint8_t blue, const char *text, const bool bakgroundTransparente = true);
    API_IMPL void drawText(const int x, const int y, const uint8_t red, const uint8_t green,
                         const uint8_t blue, const uint8_t redBack, const uint8_t greenBack,
                         const uint8_t blueBack, const char *text);
    API_IMPL void drawPoygon(const POINT *lpPoints, const int nCount);
    API_IMPL void drawRoundRect(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                              const int nWidth, const int nHeight);
    API_IMPL void drawRoundRect(const RECT &rect, const int nWidth, const int nHeight);
    API_IMPL void drawPie(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                        const int nXRadial1, const int nYRadial1, const int nXRadial2, const int nYRadial2);
    API_IMPL void selectRect(const RECT &rect);
    API_IMPL void drawFrameRect(const RECT &rect, HBRUSH brushColor);
    API_IMPL void drawChord(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                          const int nXRadial1, const int nYRadial1, const int nXRadial2, const int nYRadial2);
    API_IMPL void drawArc(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                        const int nXStartArc, const int nYStartArc, const int nXEndArc, const int nYEndArc);
    API_IMPL void drawArcTo(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                          const int nXStartArc, const int nYStartArc, const int nXEndArc, const int nYEndArc);
    API_IMPL void drawEdge(RECT rect, uint32_t edge = (BDR_RAISEDOUTER | BDR_SUNKENINNER), uint32_t flags = BF_RECT);
    API_IMPL void setArcDirection(const bool CLOCKWISE);
    API_IMPL void drawAngleArc(const int x, const int y, DWORD dwRadius, const float eStartAngle, const float eSweepAngle);
    API_IMPL void drawPolyBezier(const POINT *lppt, const DWORD cCount);
    API_IMPL void drawPolyBezierTo(const POINT *lppt, DWORD cCount);
    API_IMPL void setPenStyle(int style = PS_SOLID);
    API_IMPL void drawBmp(mbm::BMP &bmp, const int xPosition, const int yPosition);
    API_IMPL void drawBmp(mbm::BMP &bmp, const int xPosition, const int yPosition, const int xSource, const int ySource,
                        const int _width, const int _height);
    API_IMPL static SIZE getSizeText(const char *text, HWND hwnd);
  private:
    long _drawSingleLine(std::string &parcialText, int cx, const int cy);
    void _drawEndLineText(int x, int y, const char *text);
    void release();

  public:
    API_IMPL HGDIOBJ selectFontColor(const uint8_t red, const uint8_t green, const uint8_t blue);
    API_IMPL HGDIOBJ selectFontColor(const DWORD color);
    API_IMPL HGDIOBJ setDefaultColor(const uint8_t red = 255, const uint8_t green = 255,
                                   const uint8_t blue = 255);
    API_IMPL HGDIOBJ selectPenColor(const uint8_t red, const uint8_t green, const uint8_t blue);
    API_IMPL HPEN setPen(HPEN _myPen);
    API_IMPL HGDIOBJ setBrush(HGDIOBJ oldBrush);
    API_IMPL HGDIOBJ setBrush(HBRUSH _myBrush);
    API_IMPL HGDIOBJ selectBrushColor(const uint8_t red, const uint8_t green, const uint8_t blue);
    API_IMPL virtual bool render(COMPONENT_INFO &component) PURE;
    API_IMPL virtual bool eraseBackGround(COMPONENT_INFO *);// if true draw background (calls twice, 1° check component is null and  you must to return true, 2° check the component is not null, draw and return true.
    API_IMPL virtual int measureItem(COM_BETWEEN_WINP *, MEASUREITEMSTRUCT *);
    API_IMPL virtual void setCtlColor(HDC hdcStatic);
    API_IMPL void redrawWindow(HWND hwnd, BOOL eraseBck = 0);
    API_IMPL COMPONENT_INFO *getCurrentComponent();
    API_IMPL void present(HDC hdcDest, const int width, const int height);
    API_IMPL void present(HDC hdcDest, const int x, const int y, const int width, const int height);
  private:
    //-----------------------------------------------------------------------------------------------------------
    HBRUSH brush;
    HPEN   penColor;
    HFONT  font;
    HDC    hdcBack;
    int    stylePen;
    
    HBRUSH
    myPtrBrush;      // Personalize seu brush crie seu própio e indique nesta variavel. Quando nullptr é utilizado o default.
    HPEN   myPtrPen; // Personalize sua caneta crie e indique nesta variavel. Quando nullptr é utilizado o default.
    HFONT  myPtrFont; // Personalize sua fonte crie e indique nesta variavel. Quando nullptr é utilizado o default.
    HBRUSH hBrushBackGround;

  protected:
    bool            useTranparency;
    COLORREF        colorKeying;
    COMPONENT_INFO *infoActualComponent;
    
    void doRenderBackBuffer(mbm::COM_BETWEEN_WINP *ptr, LPDRAWITEMSTRUCT lpdis, const bool isHover, const bool _isPressed);
    void InvalidateChilds(COM_BETWEEN_WINP *ptr);
};

extern mbm::DRAW *_winplusDefaultThemeDraw;

class WINDOW
{
    friend class DEVICE;
    friend void ::__destroyMenu(void *extraParams);
    friend API_IMPL WCHAR *saveFileBoxW(WCHAR *extension, WCHAR *title, bool enableReturnExtencion, bool enableAllFileType,
                                      HWND hwnd, const WCHAR *defaultNameInDialog);
    friend API_IMPL WCHAR *openFileBoxW(const WCHAR *extension, const WCHAR *title, bool enableReturnExtencion,
                                      bool enableAllFileType, HWND hwnd, const WCHAR *defaultNameInDialog);
    friend API_IMPL void openFileBoxMult(std::vector<std::string>& result,const char *extension_, const char *title,
                                                            bool enableReturnExtencion, bool enableAllFileType, HWND hwnd,
                                                            const char *defaultNameInDialog);
    friend API_IMPL COM_BETWEEN_WINP *getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *me,
                                                     TYPE_WINDOWS_WINPLUS typeMe, void *extraParams_, const int idDest,
                                                     USER_DRAWER *UserDrawer);

  public:
      API_IMPL WINDOW();
      API_IMPL virtual ~WINDOW();
    
    volatile bool run;
    bool          neverClose;
    bool          askOnExit;
    bool          hideOnExit;
    

    API_IMPL bool init(mbm::MONITOR &monitor, const char *nameApp, const bool enableResize = false,
                     const bool enableMaximizeButton = false, const bool enableMinimizeButton = false,
                     const bool maximized = false, OnEventWinPlus onEventWinPlus = nullptr, const bool withoutBorder = false,
                     DWORD ID_RESOURCE_ICON_APP = 0,
                    const bool doubleBuffer = true);

    API_IMPL bool init(const char *nameApp = nullptr, const int width = 0, const int height = 0, const long positionX = 0xffffff,
                     const long positionY = 0xffffff, const bool enableResize = false, const bool enableMaximizeButton = false,
                     const bool enableMinimizeButton = false, const bool maximized = false,
                     OnEventWinPlus onEventWinPlus = nullptr, const bool withoutBorder = false,
                     DWORD ID_RESOURCE_ICON_APP = 0,const bool doubleBuffer = true);
    API_IMPL void setNameAplication(const char *nameApp);
    API_IMPL const char *getNameAplication() const;
    API_IMPL static bool isEnableRender(HWND hwndIgnore);
    API_IMPL static void disableRender(HWND hwndIgnore);
    API_IMPL DRAW *getGrafics(const int idComponent) const;
    API_IMPL void setCallEventsManager(EVENTS_WIN32 *ptrCallEventsManager);
    API_IMPL uint32_t setObjectContext(void *YOUR_PTR_OBJECT, const uint32_t index);
    API_IMPL uint32_t addObjectContext(void* YOUR_PTR_OBJECT);
    API_IMPL void *getObjectContext(const uint32_t index);
    API_IMPL void setCursor(WINPLUS_TYPE_CURSOR TYPE);
    API_IMPL WINPLUS_TYPE_CURSOR getCursor();
    API_IMPL void setNextCursor();
    API_IMPL void startTimerHover();

    API_IMPL virtual int enterLoop(OnEventWinPlus ptrLogic);
    API_IMPL virtual void doEvents();
    API_IMPL void refresh(const uint32_t idComponent, const int eraseBK);
    API_IMPL void refresh(const int eraseBK);

  private:
    static HHOOK hookMsgProc;
    static BOOL CALLBACK MessageBoxEnumProc(HWND hWnd, LPARAM lParam);
    static LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI ThreadModal(LPVOID OBJECT);
  public:
      API_IMPL virtual void doModal(mbm::WINDOW *parent, OnDoModal onDoModal = nullptr, const bool threadModal = true,
                         const bool disabelParentWindow = true);
    API_IMPL HWND getHwnd(const int id = -1) const;
    API_IMPL bool setDrawer(mbm::DRAW *draw, const int idComponent);
    API_IMPL bool setDrawer(mbm::DRAW *draw, const mbm::TYPE_WINDOWS_WINPLUS typeWindowWinPlus);
    API_IMPL bool setDrawer(mbm::DRAW *draw);
    API_IMPL void setTheme(mbm::DRAW *theme);
    API_IMPL mbm::DRAW *getDrawer(const int id);
    API_IMPL int addWindowChild(const char *title, long x, long y, long width, long height,
                              OnEventWinPlus onEventWinPlus = nullptr, const bool enableResize = true,
                              const bool enableMaximizeButton = true, const int idDest = -1, USER_DRAWER *UserDrawer = nullptr);
    API_IMPL int addLabel(const char *title, long x, long y, long width, long height, const int idDest = -1,OnEventWinPlus onGotClickeOrFocus = nullptr, USER_DRAWER *userDrawer = nullptr);
    API_IMPL bool isLoaded();
    struct __MENU_DRAW
    {
        OnEventWinPlus           onSelectedSubMenu;
        const int                idDest;
        int                      idMenu;
        int                      idSubMenu;
        HWND                     parentHwnd;
        HWND                     hwnd;
        HWND                     hwndSubMenu;
        std::string              title;
        __MENU_DRAW *            child;
        std::vector<std::string> lsSubMenusTitles;
        std::vector<int>         lsSubMenusHeight;
        bool                     isSubMenuVisible;
        int                      diffX, diffY;
        int                      minSize[2];
        uint32_t             sizeSubMenuDrawed;
        int                      indexClickedMenu;

        API_IMPL __MENU_DRAW(const int idDest_);
        API_IMPL virtual ~__MENU_DRAW();

        API_IMPL void hideSubMenu();
        API_IMPL bool showSubMenu();
        API_IMPL bool show(HWND parentHwnd_, const int myId, const int width, const int height, const int diff_x, const int diff_y);
    };
    
    API_IMPL const __MENU_DRAW *getMenuInfo(const int idMenu);
    API_IMPL static void refreshMenu();
    API_IMPL static const bool isAnyMenuVisible();
    API_IMPL int addMenu(const char *title, OnEventWinPlus onSelectedSubMenu, const int idDest = -1, USER_DRAWER *UserDrawer = nullptr);
    API_IMPL int addSubMenu(const char *title, const int idMenu);
    API_IMPL int addStatusBar(const char *textStatusBar0, const uint32_t numberPartsIntoStatusBar,
                            const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addSpinInt(long x, long y, long width, long height, const int idDest = -1, long widthSpin = 0,
                          long heightSpin = 0, OnEventWinPlus onChangeValue = nullptr, int minValue = 0, int maxValue = 10,
                          int increment = 1, int currentPosition = 0, bool vertical = true, const bool enableWrite = true,
                          USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addSpinFloat(long x, long y, long width, long height, const int idDest, long widthSpin = 0,
                            long heightSpin = 0, float minValue = 0.0f, float maxValue = 10.0f, float increment = 0.5f,
                            float currentPosition = 1.0f, int precision = 2, bool vertical = true,
                            const bool enableWrite = true, OnEventWinPlus onChangedValue = nullptr, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addScroll(long x, long y, long width, long height, int scrollSize = 10,
                         OnEventWinPlus onEventWindow = nullptr, const int idDest = -1, USER_DRAWER * UserDrawer = nullptr); // doesnt work
    API_IMPL int addTrayIcon(const int ID_RESOURCE_ICON, OnEventWinPlus onEventWindowByIdMenu, const char *tip,USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addTrayIcon(const char *fileNameIcon, OnEventWinPlus onEventWindowByIdMenu, const char *tip,
                           USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addTrayIcon(OnEventWinPlus onEventWindowByIdMenu, const char *tip, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addMenuTrayIcon(const char *str, const int idMenuTryIcon = -1, const bool hasSubMenu = false,
                               const int position = 0, const bool doubleClicked = false, const bool breakMenu = false,
                               const bool checked = false, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addSubMenuTrayIcon(const char *str, const int position = 0, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL bool showBallonTrayIcon(const char *title, const char *message, int uTimeout, DWORD dwIcon = NIIF_INFO);
    API_IMPL bool setTextTrayIcon(const char *text);
    API_IMPL bool printLastErrWindows(const char *where = nullptr);
    API_IMPL int addToolTip(const char *tip, const int idDest = -1, USER_DRAWER *dataToolTip = nullptr);
    API_IMPL int addButton(const char *title, long x, long y, long width, long height, const int idDest = -1,
                         OnEventWinPlus onPressedByType = nullptr, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addRadioButton(const char *title, long x, long y, long width, long height, const int idDest = -1,
                              OnEventWinPlus onPressedByType = nullptr, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addGroupBox(const char *title, long x, long y, long width, long height, const int idDest = -1,
                           OnEventWinPlus onGotClickeOrFocus = nullptr,USER_DRAWER * UserDrawer = nullptr);
    API_IMPL void killTimer(const int idTimer);
    API_IMPL int addTimer(uint32_t timeMilliseconds, OnEventWinPlus onElapseTimer, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL void setPositionProgressBar(const int IdComponent, int position);
    API_IMPL int getPositionProgressBar(const int IdComponent);
    API_IMPL int addProgressBar(long x, long y, long width, long height, const int idDest = -1, const bool vertical = false,
                              USER_DRAWER * UserDrawer = nullptr);
    API_IMPL void setDefaultPositionTrackBar(const int idTrackBar, const short defaultPosition);
    API_IMPL void setTrackBar(const int idTrackBar, const float position);
    API_IMPL void setMaxPositionTrackBar(const int idTrackBar, const short maxPosition);
    API_IMPL TRACK_BAR_INFO *getInfoTrack(const int idTrackBar);
    API_IMPL float getPositionTrackBar(const int idTrackBar);
    API_IMPL int addCombobox(long x, long y, long width, long height, OnEventWinPlus onPressedByType = nullptr,
                           const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addCheckBox(const char *title, long x, long y, long width, long height,
                           OnEventWinPlus onPressedByType = nullptr, const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addRichText(const char *textIntoRichText, long x, long y, long width, long height, const int idDest = -1,
                           OnEventWinPlus onPressedByType = nullptr, const bool vScroll = true, const bool hScroll = false,
                           USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addTextBox(const char *textIntoTextBox, long x, long y, long width, long height, const int idDest = -1,
                          OnEventWinPlus onPressedByType = nullptr, const bool isPassword = false, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addListBox(long x, long y, long width, long height, OnEventWinPlus onPressedByType = nullptr,
                          const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addTrackBar(long x, long y, long width, long height, const int idDest = -1,
                           OnEventWinPlus onChangeValue = nullptr, float minPosition = 0.0f, float maxPosition = 100.0f,
                           float defaultPosition = 50.0f, float tickSmall = 10.0f, float tickLarge = 25.0f,
                           const bool invertValueText = false, const bool trackBarVertical = false, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL void SetWindowTrans(int percent);
    API_IMPL void RemoveWindowTrans();
  private:
    void hideDestinyNotVisible(const int id, HWND myHwnd);

    struct __DO_MODAL_OBJ
    {
        mbm::WINDOW *w;
        mbm::WINDOW *parent;
        OnDoModal    onDoModal;
        const bool   disabelParentWindow;
        API_IMPL __DO_MODAL_OBJ(mbm::WINDOW *me, mbm::WINDOW *myParent, OnDoModal onDoModalParent, const bool disabelParentWindow_);
    };
    static LRESULT __stdcall TrackProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall StatusProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall MessageBoxProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall ScrollProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam); // doesnt work
    static LRESULT __stdcall WinNCProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall ComboProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall UDProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall ToolTipProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall ProgressBarWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall RichTextProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall EditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static void _onTimeHover(mbm::WINDOW *w, DATA_EVENT &);
  public:
    API_IMPL void setUserDrawer(int idComponent, USER_DRAWER *userDrawer); // user drawer para o componente
    API_IMPL USER_DRAWER *getUserDrawer(int idComponent); // recupera user drawer para do componente
    API_IMPL void setIndexTabByGroup(const int idTabControlByGroup, const int index,
                                   const bool callOnEventWindowByType = true);
    API_IMPL int addTabControlByGroup(long x, long y, long width, long height, long widthButton, long heightButton,
                                    OnEventWinPlus onEventWindowByIndexTab = nullptr, const int idDest = -1,
                                    const bool enableVisibleGroups = true, USER_DRAWER * UserDrawer = nullptr);
    API_IMPL int addTabByGroup(const char *title, const int idTabControlByGroup, const bool newCloumn = false,
                             const long newWidth = 0, USER_DRAWER *UserDataButton = nullptr, USER_DRAWER *UserDrawerGroup = nullptr);
    API_IMPL bool clear(const int idComponent);
    API_IMPL bool getRadioButtonState(const int idRadioButton);
    API_IMPL bool setNextFocus(const int idComponent, const int idComponentNextFocus);
    API_IMPL bool setRadioButtonState(const int idRadioButton);
    API_IMPL bool setCheckBox(const bool checked, const int idCheckBox);
    API_IMPL bool getCheckBoxState(const int idCheckBox);
    API_IMPL bool addText(const int idComponent, const char *text);
    API_IMPL bool removeText(const int idComponent, const int indexString);
    API_IMPL bool setSelectedIndex(const int idComponent, const int indexString);
    API_IMPL int getSelectedIndex(const int idComponent);
    API_IMPL int getTextCount(const int idComponent);
    API_IMPL mbm::SPIN_PARAMSf *getSpinf(const int idSpinf);
    API_IMPL mbm::SPIN_PARAMSi *getSpin(const int idSpin);
    API_IMPL bool updateSpin(const int idSpin);
    API_IMPL bool setFocus(const int idComponent = -1);
    API_IMPL void forceFocus();
    API_IMPL bool setText(const int IdComponent, const char *stringSource, int index = -1);
    API_IMPL bool getText(const int IdComponent, char *stringOut, const WORD sizeStringOut, int index = -1);
    API_IMPL int getTextLength(const int IdComponent, int index = -1);
    API_IMPL std::vector<std::string> *getStatusBar(const int idComponent);
    API_IMPL void setOnKeyboardDown(OnKeyboardEvent function);
    API_IMPL void setOnKeyboardUp(OnKeyboardEvent function);
    API_IMPL bool setOnParserRawInput(OnParseRawInput function);
    API_IMPL void setOnMoveMouseEvent(OnMouseEvent function);
    API_IMPL void setOnClickLeftMouse(OnMouseEvent function);
    API_IMPL void setOnReleaseLeftMouse(OnMouseEvent function);
    API_IMPL void setOnClickRightMouse(OnMouseEvent function);
    API_IMPL void setOnReleaseRightMouse(OnMouseEvent function);
    API_IMPL void setOnClickMiddleMouse(OnMouseEvent function);
    API_IMPL void setOnReleaseMiddleMouse(OnMouseEvent function);
    API_IMPL void setOnScrollMouseEvent(OnMouseEventScroll function);
    API_IMPL bool setMaxLength(const int idComponent, const uint32_t maxLength);
    API_IMPL bool setReadOnlyToRichText(const int idRichText, const bool value);
    API_IMPL void setAlwaysOnTop(const bool value, const bool hideMe = false);
    API_IMPL void setAlwaysOnTop(mbm::WINDOW *hwndParent);
    API_IMPL void setColorKeying(const uint8_t red, const uint8_t green, const uint8_t blue);
    API_IMPL void setColorKeying(const uint8_t red, const uint8_t green, const uint8_t blue,const int idComponent);
    API_IMPL void setPosition(const int x, const int y, const int id = -1);
    API_IMPL long getWidth(const int id = -1);
    API_IMPL WINDOW *getWindow(const HWND hwnd_);
    API_IMPL long getHeight(const int id = -1);
    API_IMPL RECT getRect(const int id = -1);
    API_IMPL RECT getRectAbsolute(const int id = -1);
    API_IMPL RECT getRectRelativeWindow(const int id = -1);
    API_IMPL void setSize(RECT &source, const bool inner = true);
    API_IMPL void resize(const int idComponent, const int x, const int y, const int new_width, const int new_height);
    API_IMPL void resize(const int idComponent, const int new_width, const int new_height);
    API_IMPL static void resize(HWND hwnd2move, const int new_width, const int new_height, bool incrementSize = false);
    API_IMPL void hideConsoleWindow();
    API_IMPL void showConsoleWindow();
    API_IMPL void closeWindow();
    API_IMPL void hide(const HWND hwnd_);
    API_IMPL void hide(const int id = -1, int flag = SW_HIDE);
    API_IMPL void show(const int id = -1, int flag = SW_SHOW);
    API_IMPL void show(const HWND hwnd_);
    API_IMPL void showMaximized(const int idWindow = -1);
    API_IMPL void showMinimized(const int idWindow = -1);
    API_IMPL void setMinSizeAllowed(const int width,const int height);
    API_IMPL void setMaxSizeAllowed(const int width,const int height);
    API_IMPL bool loadTextFileToRichEdit(const int idRichText, WCHAR *fileName);
    API_IMPL bool loadTextFileToRichEdit(HWND hwndRichText, WCHAR *fileName);
    API_IMPL bool loadTextFileToRichEdit(const int idRichText, const WCHAR *filter = nullptr);
    API_IMPL bool saveTextFileFromRichText(const int idRichText, WCHAR *fileName);
    API_IMPL bool messageBoxQuestion(const char *format, ...);
    API_IMPL void messageBox(const char *format, ...);
    API_IMPL const int getTabStopPixelSize();
    API_IMPL const void setTabStopPixelSize(int characters = 1, const int idComponent = -1);
  private:
    void moveHWND(HWND hwndToMove, int x, int y);
    void moveHWNDMeAndChilds(HWND hwndToMove, int x, int y);
    void moveTabByGroup(const int idTabControl, const int x, const int y);
    HICON getIcon();
    void filleDefaultExtraParam(COM_BETWEEN_WINP *comBetweenWinpChild, int idDest);
    HWND addToHwnd(const int idDest, COM_BETWEEN_WINP *comBetweenWinpChild);
    HWND addToHwnd(const int idDest, long *x, long *y, COM_BETWEEN_WINP *comBetweenWinpChild);
    static INT _Do_default_Drawer_BackGround(COM_BETWEEN_WINP *ptr);
    static INT _Do_default_Drawer(COM_BETWEEN_WINP *ptr, LPDRAWITEMSTRUCT lpdis);
    static bool exitNow(HWND windowHandle, const bool keyEsc);
    static bool hideWindowOnExit(HWND windowHandle);
    static void CALLBACK timerProc(HWND, UINT, UINT_PTR idEvent, DWORD);
    static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
  public:
    bool isVisible;
    bool doModalMode;
    bool preventMenuAlwaysShowing;
    HWND hwndInsertAfter;
    API_IMPL void getCursorPos(POINT *p);
    API_IMPL bool isUsingDoubleBuffer()const;
  protected:
    bool usingDoubleBuffer;
    int  dialogunitTabStopInPixel;
    char nameApplication[MAX_PATH];
    std::map<int, void *> lsObjectsContext;
    EVENTS_WIN32*                     callEventsManager;
    HWND                              hwnd;
    HWND                              hwndLastTrackBar;
    HWND                              hwndLastHover;
    HWND                              hwndLastPressed;
    DRAW *                            drawerDefault;
    POINT                             lastPosMouse;
    int                               idTimerHover;
    int                               adjustRectLeft, adjustRectTop;
    HICON                             iconApp;
    HMENU                             subMenu;
    HMENU                             sysTry_menu;
    bool                              hasTryIcon;
    bool                              isWin32Initialized;
    WINPLUS_TYPE_CURSOR               CURRENT_CURSOR;
    std::string                       stringRichText;
    OnKeyboardEvent                   onKeyboardDown;
    OnKeyboardEvent                   onKeyboardUp;
    OnParseRawInput                   onParseRawInput;
    OnMouseEventScroll                onScrollMouseEvent;
    OnMouseEvent                      onClickLeftMouse, onClickRightMouse, onClickMiddleMouse, onMouseMove;
    OnMouseEvent                      onReleaseLeftMouse, onReleaseRightMouse, onReleaseMiddleMouse;
    DWORD                             colorUnderling;
    DWORD                             colorText;
    DWORD                             colorTextTab;
    NOTIFYICONDATAA                   tnid;
    static std::vector<HWND>          lsDisabledRender;
    static std::vector<__MENU_DRAW *> lsAllMenus;
    int min_size_width,min_size_height;
    int max_size_width,max_size_height;
};
API_IMPL WINDOW *getWindow(HWND hwnd);
API_IMPL WINDOW *getLastWindow();
API_IMPL WINDOW *getFirstWindow();
API_IMPL const char *selectetDirectory(HWND hwnd, char *outDir);
API_IMPL std::vector<std::string> &openFileBoxMult(const char *extension_, const char *title,
                                                 bool enableReturnExtencion = true, bool enableAllFileType = false,
                                                 HWND hwnd = nullptr, const char *defaultNameInDialog = nullptr);
API_IMPL WCHAR *openFileBoxW(const WCHAR *extension, const WCHAR *title, bool enableReturnExtencion = true,
                           bool enableAllFileType = false, HWND hwnd = nullptr, const WCHAR *defaultNameInDialog = nullptr);
API_IMPL char *openFileBox(const char *extension_, const char *title_, bool enableReturnExtencion, bool enableAllFileType,
                         HWND hwnd, const char *defaultNameInDialog_, char *outFileName);
API_IMPL WCHAR *saveFileBoxW(WCHAR *extension, WCHAR *title, bool enableReturnExtencion = true,
                           bool enableAllFileType = false, HWND hwnd = nullptr, const WCHAR *defaultNameInDialog = nullptr);
API_IMPL char *saveFileBox(const char *extension_, const char *title_, const bool enableReturnExtencion,
                         const bool enableAllFileType, HWND hwnd, const char *defaultNameInDialog_, char *outFileName);
API_IMPL bool getColorFromDialogBox(uint8_t &red, uint8_t &green, uint8_t &blue, HWND hwnd = nullptr);
API_IMPL bool getFontFromDialogBox(LOGFONTA *fontOut, HWND hwnd = nullptr);
API_IMPL bool getFontFromDialogBox(LOGFONTW *fontOut, HWND hwnd = nullptr);
API_IMPL RECT getMenuRect(int idWindow, int myId);

API_IMPL char *getNameFromPath(const char *fileNamePath, const bool removeCharacterInvalids, char *primaryPartFromPath,
                             char *outFileName);
API_IMPL const char *getHeaderToResource();

API_IMPL bool saveToFileBinary(const char *fileName, void *header, DWORD sizeOfHeader, void *dataIn, DWORD sizeOfDataIn);

API_IMPL bool loadFromFileBynary(const char *fileName, void *header, DWORD sizeOfHeader, void *dataOut, DWORD sizeOfDataOut);
API_IMPL bool loadHeaderFromFileBynary(const char *fileName, void *header, DWORD sizeOfHeader);
API_IMPL void closeAllWindows();
}

struct __AUX_MONITOR_SELECT
{
    int           indexCmbSelectedeMonitor;
    int           idCmbSelectMonitor;
    int           idbntOk;
    int           idChkAskAboutMonitor;
    bool          askMeAgain;
    mbm::MONITOR *monitor;
    API_IMPL __AUX_MONITOR_SELECT();
    API_IMPL static void __0_onProcess(mbm::WINDOW *, mbm::DATA_EVENT &dataEvent);
    API_IMPL static void __0_onPressOkMonitor(mbm::WINDOW *w, mbm::DATA_EVENT &);
    API_IMPL static void __1_onCheckedDontAskAgain(mbm::WINDOW *w, mbm::DATA_EVENT &);
};

namespace mbm
{

bool selectMonitor(mbm::MONITOR *monitorOut);

class LAYOUT
{
  public:
    DWORD          widthRenderer;
    DWORD          heightRenderer;
    DWORD          widthWindow;
    DWORD          heightWindow;
    int            xGroupRenderer;
    int            yGroupRenderer;
    DWORD          xComponent;
    DWORD          yComponent;
    DWORD          maxWidthComponente;
    POINT          position;
    POINT          middleAbs;
    POINT          middleRenderer;
    bool           enableReziseWindow;
    bool           withoutBorder;
    OnEventWinPlus onEventWindow;
    API_IMPL bool init(const char *nameApp, mbm::WINDOW &window, int adjustRendererWidth = 0, int adjustRendererHeight = 0,
                     const bool hasMenu = false, const bool leftToRight = false, const int idResourceIcon = 0);

    API_IMPL LAYOUT();
    API_IMPL virtual ~LAYOUT();
    API_IMPL HWND getHwndRenderer();
    int idGroupRender;
  private:
    HWND hwndRenderer;
};
}

class __DRAW_SPLASH : public mbm::DRAW
{
  public:
    bool          enableEffectFade;
    uint8_t rProgress, gProgress, bProgress;
    int           ID_IMAGE;
    mbm::BMP      bmpSplash;
    mbm::BMP      bmpProgress;
    int           coutDownTimeOut;
    API_IMPL __DRAW_SPLASH(const int ID_IMAGE_RESOURCE, mbm::STATIC_IMAGE_RESOURCE *imageResource);
    API_IMPL virtual ~__DRAW_SPLASH();
    API_IMPL bool eraseBackGround(mbm::COMPONENT_INFO* component);
    API_IMPL bool render(mbm::COMPONENT_INFO &component);
    API_IMPL static void onTimeOutSplah(mbm::WINDOW *w, mbm::DATA_EVENT &);
  private:
    mbm::STATIC_IMAGE_RESOURCE *resource;
};

void __destroyMenu(void *extraParams);

namespace mbm
{

    API_IMPL void splash(const DWORD timeMiliSec, STATIC_IMAGE_RESOURCE &imageResource, uint8_t rgbProgres[3] = nullptr,
                   uint8_t colorKeiyng[3] = nullptr);
    API_IMPL void splash(const DWORD timeMiliSec, int ID_IMAGE_RESOURCE, uint8_t rgbProgres[3] = nullptr,
                   uint8_t colorKeiyng[3] = nullptr);
    API_IMPL void splash(const DWORD timeMiliSec, int ID_IMAGE_RESOURCE, int ID_IMAGE_PROGRESS,
                   uint8_t colorKeiyng[3] = nullptr);
    API_IMPL void splash(const int widthWindow, const int heightWindow, const DWORD timeMiliSec, int ID_IMAGE_RESOURCE,
                   int ID_IMAGE_PROGRESS, uint8_t colorKeiyng[3] = nullptr);
}

API_IMPL const int __getTabStopPixelSize(HWND hwnd);
namespace mbm
{
void __destroyOnExitAllListComBetweenWindows();
API_IMPL COM_BETWEEN_WINP* getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW* me,
    TYPE_WINDOWS_WINPLUS typeMe, void* extraParams_, const int idDest,
    USER_DRAWER* UserDrawer = nullptr);
};

#ifdef _DEBUG
    #ifdef _WIN32
        #ifdef __SELF__LEAK_D
            #undef __SELF__LEAK_D
            #undef new
            #define new DBG_NEW_OLD
        #endif
    #endif
#endif
#endif
#endif
