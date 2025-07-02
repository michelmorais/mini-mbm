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

#ifndef PLUS_WINDOWS_H
#define PLUS_WINDOWS_H

#ifndef NOMINMAX
    #define NOMINMAX
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

#define UNREFERENCED_PARAMETER(P) (P)
#define DBG_UNREFERENCED_PARAMETER(P) (P)
#define DBG_UNREFERENCED_LOCAL_VARIABLE(V) (V)

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
#include <map>
#include <assert.h>
#include <set>
#include <Richedit.h>
#include <windef.h>
#include <winbase.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <thread>

#ifdef __MINGW32__
#define TOOLINFO TTTOOLINFO
#define BS_PUSHBOX 0x0000000AL
typedef struct tagNMTTCUSTOMDRAW
{
    NMCUSTOMDRAW nmcd;
    UINT         uDrawFlags;
} NMTTCUSTOMDRAW, *LPNMTTCUSTOMDRAW;
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
const int __getTabStopPixelSize(HWND hwnd);

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

WCHAR *toWchar(const char *str, WCHAR *outText);
char *toChar(const WCHAR *wstr, char *outText);
void destroyListComBetweenWindows(HWND hwnd);

class STATIC_IMAGE_RESOURCE
{
  public:
    STATIC_IMAGE_RESOURCE(const uint32_t w, const uint32_t h, const uint32_t s, const char *nickName_,const uint32_t *d, const uint32_t c);
    const uint32_t  width;
    const uint32_t  height;
    const uint32_t  size;
    const char *        nickName;
    const uint32_t *data;
    const uint32_t  colorKeying;
};


void split(std::vector<std::string> &result, const char *in, const char delim);
const char *getLastErrWindows(const char *where, char *outMessage);
bool startUpWindows64(const char *name);
bool startUpWindows(const char *name);

class REGEDIT // regedit do windows. set "Project>Configuration Properties>Linker>Manifest File>UAC Execution Level" to
              // requireAdministrator
{
  public:
    REGEDIT();
    virtual ~REGEDIT();
    
    bool openKey(HKEY hRootKey, const wchar_t *strKey, const DWORD acess = KEY_ALL_ACCESS);
    bool openKey(HKEY hRootKey, const char *strKey, const DWORD acess = KEY_ALL_ACCESS);
    void setVal(LPCTSTR lpValue, DWORD data);
    DWORD getVal(LPCTSTR lpValue, DWORD valueNotFound);
    void closeKey();
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

int getRandomInt(const int min, const int max);
char getRandomChar(const char min, const char max);
float getRandomFloat(const float min, const float max);

}
namespace mbm
{
    void __destroyOnExitAllListComBetweenWindows();
};

void __initRandomSeed();

#ifndef PURE
    #define PURE = 0;
#endif

#if UNICODE
WCHAR *getNextClassNameWindow();
#else
char *getNextClassNameWindow();
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
    __TAB_GROUP_DESC(const int _index, const int _idDest);
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
    TRACK_BAR_INFO();
};

struct PROGRESS_BAR_INFO
{
    const bool vertical;
    float      minRange;
    float      maxRange;
    float      position;
    PROGRESS_BAR_INFO(const bool vertical_);
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
    uint32_t getAsUintFromCharPointer(uint8_t *adress);
};

namespace mbm
{
class WINDOW;
WINDOW *getWindow(HWND hwnd);
WINDOW *getLastWindow();
WINDOW *getFirstWindow();
void    closeAllWindows();

class BMP
{
  public:
    BMP();
    virtual ~BMP();
    void release();
    bool load(HWND hwnd, const int ID_RESOURCE);
    bool loadTrueColor(const char *fileName);
    bool load(const char *fileNameBitmap);
    bool load(mbm::STATIC_IMAGE_RESOURCE &imageResource);
    const int isLoaded()const;
    const int getWidth() const;
    const int getHeight() const;
    void draw(HDC hdc);
    void draw(HDC hdc, const int x, const int y);
    void draw(HDC hdc, const RECT &rect);
    void draw(HDC hdc, const int xPosition, const int yPosition, const int xSource, const int ySource,
                     const int width, const int height);
    bool createBitmap(int width, int heigth);
    bool createBitmap(int width, int heigth, const uint8_t *dataImage);
    bool updateData();
    HBITMAP getHBitmap() const;
    BITMAP *getBitmapInfo();
    uint8_t *getData() const;//RGB
    
  private:
    
    uint32_t getAsUintFromCharPointer(uint8_t *adress);
    
    HBITMAP        data;
    BITMAP         bitmapInfo;
    BITMAPINFO     bInfo;
    uint8_t *dataRGB;
    //--------------------------------------------------------------------------------------------
};

class EVENTS
{
  public:
    //--------------------------------------------------------------------------------------------
    virtual void onTouchDown(HWND w, int key, float x, float y) PURE;

    //--------------------------------------------------------------------------------------------
    virtual void onTouchUp(HWND w, int key, float x, float y) PURE;

    //--------------------------------------------------------------------------------------------
    virtual void onTouchMove(HWND w, float x, float y) PURE;

    //--------------------------------------------------------------------------------------------
    virtual void onTouchZoom(HWND w, float zoom) PURE;

    //--------------------------------------------------------------------------------------------
    virtual void onKeyDown(HWND w, int key) PURE;

    //--------------------------------------------------------------------------------------------
    virtual void onKeyUp(HWND w, int key) PURE;

    //--------------------------------------------------------------------------------------------
    virtual void onDoubleClick(HWND w, float x, float y, int key) PURE;
    //--------------------------------------------------------------------------------------------

    virtual void onResizeWindow(HWND w, int width, int height) PURE;
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
    DATA_EVENT();
    DATA_EVENT(int idComponent_, void *Data, USER_DRAWER *UserDrawer, const TYPE_WINDOWS_WINPLUS type_, const char *_myString);
    const int getAsInt();
    const float getAsFloat();
    const bool getAsBool();
    const char *getAsString();
    TIMER *getAsTimer();
    TRACK_BAR_INFO *getAsTrackBar();
    RADIO_GROUP *getAsRadio();
    SPIN_PARAMSi *getAsSpin();
    SPIN_PARAMSf *getAsSpinf();

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

const bool isNumeric(const char letter);
bool isNum(const char *numberAsString);
bool isNum(const WCHAR *numberAsString);
char *trimRight(char *stringSource);
char *trimLeft(char *stringSource);
char *trim(char *stringSource);

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
    MONITOR();
};

class MONITOR_MANAGER
{
  private:
    std::vector<MONITOR> lsMonitors;

  public:
    //---------------------------------------------------------------------------------------------------------------
    MONITOR_MANAGER();
    virtual ~MONITOR_MANAGER();
    void updateMonitors();
    long getWidthWindow(const DWORD indexMonitor = 0);
    long getHeightWindow(const DWORD indexMonitor = 0);
    POINT getPositionWindow(const DWORD indexMonitor = 0);
    DWORD getIndexMainMonitor();
    bool getMonitor(const DWORD indexMonitor, mbm::MONITOR *monitorOut);
    bool isMainMonitor(const DWORD indexMonitor = 0);
    DWORD getTotalMonitor();
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
    SPIN_PARAMSf(float minValue, float maxValue, float increment_, float currentPosition_, int precision_);
};

struct RADIO_GROUP
{
    const int     idRadio;
    const int     idParent;
    std::set<int> lsRadioGroup;
    bool          checked;
    RADIO_GROUP(const int id, const int _idParent);
    ~RADIO_GROUP();
};

struct TIMER
{
    int            timInMilisecond;
    int            times;
    int            idTimer;
    OnEventWinPlus onEventTimer;
    TIMER(int timeElapsed_inMiliSeconds, int idTimer_, OnEventWinPlus onEventTimer_);
};

struct EDIT_TEXT_DATA
{
    mbm::SPIN_PARAMSi *spin;
    mbm::SPIN_PARAMSf *spinf;
    const int          id;
    char *             text;
    uint32_t       len;
    EDIT_TEXT_DATA(const int _id);
    EDIT_TEXT_DATA(mbm::SPIN_PARAMSi *_spin, mbm::SPIN_PARAMSf *_spinf, const int _id);
    ~EDIT_TEXT_DATA();
};

class COM_BETWEEN_WINP
{
    friend class WINDOW;
    friend class COMPONENT_INFO;
    friend class KEY_BOARD_STATE;
    friend class DRAW;
    friend void        __destroyOnExitAllListComBetweenWindows();
    friend void destroyListComBetweenWindows(HWND hwnd);
    friend void destroyAlTimers(HWND hwnd);
    friend void destroyTimer(HWND hwnd, const int idTimer);
    friend WINDOW *getWindow(HWND hwnd);
    friend WINDOW *getLastWindow();
    friend WINDOW *getFirstWindow();
    friend void    closeAllWindows();
    friend RECT getMenuRect(int idWindow, int myId);
    friend COM_BETWEEN_WINP *getComBetweenWinp(const int id);
    friend COM_BETWEEN_WINP *getComBetweenWinp(const HWND hwnd);
    friend COM_BETWEEN_WINP *getComBetweenWinp(const HWND owerHwnd, const int id);
    friend COM_BETWEEN_WINP *getComBetweenWinpTryIcon(const HWND owerHwnd);
    friend COM_BETWEEN_WINP *getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *me,
                                                     TYPE_WINDOWS_WINPLUS typeMe, void *extraParams_, const int idDest,
                                                     USER_DRAWER *UserDrawer);

  public:
    int getId() const;
    mbm::WINDOW *getWindow();
    TYPE_WINDOWS_WINPLUS getType();
    HWND getHwnd();
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
  
    COM_BETWEEN_WINP(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *win, TYPE_WINDOWS_WINPLUS typeMe,
                     void *extraParams_, const int idOwner_, USER_DRAWER *UserDrawer);
    COM_BETWEEN_WINP(COM_BETWEEN_WINP *ncCopy);
    virtual ~COM_BETWEEN_WINP();
};

COM_BETWEEN_WINP *getComBetweenWinp(const int id);
COM_BETWEEN_WINP *getComBetweenWinp(const HWND owerHwnd, const int id);
COM_BETWEEN_WINP *getComBetweenWinp(const HWND hwnd);
COM_BETWEEN_WINP *getComBetweenWinpTryIcon(const HWND owerHwnd);
void destroyListComBetweenWindows(HWND hwnd);
void destroyAlTimers(HWND hwnd);
void destroyTimer(HWND hwnd, const int idTimer);

class COMPONENT_INFO;
typedef void(__cdecl *OnRenderComponent)(mbm::COMPONENT_INFO &component);
//-----------------------------------------------------------------------------------------------------------------
struct USER_DRAWER
{
    bool              enableHover;
    bool              enablePressed;
    virtual bool      render(COMPONENT_INFO & component) = 0;
    void *            that;
    DRAW *            draw;
    USER_DRAWER(void *That = nullptr, DRAW *Draw = nullptr);
};

struct USER_DATA :public USER_DRAWER
{
    USER_DATA();
    USER_DATA(void* newData);
    USER_DATA(void* newData,DRAW *Draw);
    virtual bool      render(COMPONENT_INFO & component);
    void setData(void* newData);
    void* getData();
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
    virtual ~COMPONENT_INFO();
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
        COLOR();
        COLOR(const COLORREF &c);
        COLOR(const uint8_t r, const uint8_t g, const uint8_t b);
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
    DRAW();
    DRAW(mbm::COMPONENT_INFO *component);
    virtual ~DRAW();
    HBRUSH createBrush(uint8_t r, uint8_t g, uint8_t b);
    void release(HBRUSH &hbrush);
    HPEN createPen(uint8_t r, uint8_t g, uint8_t b, int _stylePen = PS_SOLID, int width = 0);
    HPEN createPen(COLORREF color);
    void release(HPEN &hpen);
    HBRUSH createGradientBrush(COLORREF fromColor, COLORREF toColor, const RECT &rc, const bool horizontal = true,
                               const bool power2 = true, const bool reflected = true);
    void drawLine(const int initialX, const int initialY, const int finalX, const int finalY);
    void drawLine(const POINT &initialPoint, const POINT &finalPoint);
    void drawRectangle(const RECT &REct);
    void drawRectangle(const int x, const int y, const int w, const int h);
    void drawCircle(const POINT &point, const int ray);
    void drawCircle(const int initialX, const int initialY, const int ray);
    void drawElipse(const RECT &rect);
    HDC setHDC(HDC newHdc);
    void setFont(HFONT _hfont);
    static HFONT createFont(const char *pszFaceName = "Times New Roman", const int cHeight = 20, const int cWidth = 5,
                            const int cEscapement = 0, const int cOrientation = 0, const int cWeight = FW_NORMAL,
                            const DWORD bItalic = 0, const DWORD bUnderline = 0, const DWORD bStrikeOut = 0);
    void drawText(RECT *rect, const char *text, const bool bakgroundTransparente = true);
    void drawText(const int x, const int y, const char *text, const bool bakgroundTransparente = true);
    void drawTextRotated(const int x, const int y, HWND hwnd, const char *text, DWORD color_text, int angle,
                         const bool bakgroundTransparente = true);
    void drawText(const int x, const int y, const DWORD color, const char *text,
                         const bool bakgroundTransparente = true);
    void drawText(const RECT &rect, const DWORD color, const char *text, const bool bakgroundTransparente = true);
    void drawText(const int x, const int y, const uint8_t red, const uint8_t green,
                         const uint8_t blue, const char *text, const bool bakgroundTransparente = true);
    void drawText(const int x, const int y, const uint8_t red, const uint8_t green,
                         const uint8_t blue, const uint8_t redBack, const uint8_t greenBack,
                         const uint8_t blueBack, const char *text);
    void drawPoygon(const POINT *lpPoints, const int nCount);
    void drawRoundRect(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                              const int nWidth, const int nHeight);
    void drawRoundRect(const RECT &rect, const int nWidth, const int nHeight);
    void drawPie(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                        const int nXRadial1, const int nYRadial1, const int nXRadial2, const int nYRadial2);
    void selectRect(const RECT &rect);
    void drawFrameRect(const RECT &rect, HBRUSH brushColor);
    void drawChord(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                          const int nXRadial1, const int nYRadial1, const int nXRadial2, const int nYRadial2);
    void drawArc(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                        const int nXStartArc, const int nYStartArc, const int nXEndArc, const int nYEndArc);
    void drawArcTo(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                          const int nXStartArc, const int nYStartArc, const int nXEndArc, const int nYEndArc);
    void drawEdge(RECT rect, uint32_t edge = (BDR_RAISEDOUTER | BDR_SUNKENINNER), uint32_t flags = BF_RECT);
    void setArcDirection(const bool CLOCKWISE);
    void drawAngleArc(const int x, const int y, DWORD dwRadius, const float eStartAngle, const float eSweepAngle);
    void drawPolyBezier(const POINT *lppt, const DWORD cCount);
    void drawPolyBezierTo(const POINT *lppt, DWORD cCount);
    void setPenStyle(int style = PS_SOLID);
    void drawBmp(mbm::BMP &bmp, const int xPosition, const int yPosition);
    void drawBmp(mbm::BMP &bmp, const int xPosition, const int yPosition, const int xSource, const int ySource,
                        const int _width, const int _height);
    static SIZE getSizeText(const char *text, HWND hwnd);
  private:
    long _drawSingleLine(std::string &parcialText, int cx, const int cy);
    void _drawEndLineText(int x, int y, const char *text);
    void release();

  public:
    HGDIOBJ selectFontColor(const uint8_t red, const uint8_t green, const uint8_t blue);
    HGDIOBJ selectFontColor(const DWORD color);
    HGDIOBJ setDefaultColor(const uint8_t red = 255, const uint8_t green = 255,
                                   const uint8_t blue = 255);
    HGDIOBJ selectPenColor(const uint8_t red, const uint8_t green, const uint8_t blue);
    HPEN setPen(HPEN _myPen);
    HGDIOBJ setBrush(HGDIOBJ oldBrush);
    HGDIOBJ setBrush(HBRUSH _myBrush);
    HGDIOBJ selectBrushColor(const uint8_t red, const uint8_t green, const uint8_t blue);
    virtual bool render(COMPONENT_INFO &component) PURE;
    virtual bool eraseBackGround(COMPONENT_INFO *);// if true draw background (calls twice, 1° check component is null and  you must to return true, 2° check the component is not null, draw and return true.
    virtual int measureItem(COM_BETWEEN_WINP *, MEASUREITEMSTRUCT *);
    virtual void setCtlColor(HDC hdcStatic);
    void redrawWindow(HWND hwnd, BOOL eraseBck = 0);
    COMPONENT_INFO *getCurrentComponent();
    void present(HDC hdcDest, const int width, const int height);
    void present(HDC hdcDest, const int x, const int y, const int width, const int height);
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
    friend WCHAR *saveFileBoxW(WCHAR *extension, WCHAR *title, bool enableReturnExtencion, bool enableAllFileType,
                                      HWND hwnd, const WCHAR *defaultNameInDialog);
    friend WCHAR *openFileBoxW(const WCHAR *extension, const WCHAR *title, bool enableReturnExtencion,
                                      bool enableAllFileType, HWND hwnd, const WCHAR *defaultNameInDialog);
    friend std::vector<std::string> &openFileBoxMult(const char *extension_, const char *title,
                                                            bool enableReturnExtencion, bool enableAllFileType, HWND hwnd,
                                                            const char *defaultNameInDialog);
    friend COM_BETWEEN_WINP *getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *me,
                                                     TYPE_WINDOWS_WINPLUS typeMe, void *extraParams_, const int idDest,
                                                     USER_DRAWER *UserDrawer = nullptr);

  public:
    WINDOW();
    virtual ~WINDOW();
    
    volatile bool run;
    bool          neverClose;
    bool          askOnExit;
    bool          hideOnExit;
    bool          exitOnEsc;
    

    bool init(mbm::MONITOR &monitor, const char *nameApp, const bool enableResize = false,
                     const bool enableMaximizeButton = false, const bool enableMinimizeButton = false,
                     const bool maximized = false, OnEventWinPlus onEventWinPlus = nullptr, const bool withoutBorder = false,
                     DWORD ID_RESOURCE_ICON_APP = 0,
                    const bool doubleBuffer = true);

    bool init(const char *nameApp = nullptr, const int width = 0, const int height = 0, const long positionX = 0xffffff,
                     const long positionY = 0xffffff, const bool enableResize = false, const bool enableMaximizeButton = false,
                     const bool enableMinimizeButton = false, const bool maximized = false,
                     OnEventWinPlus onEventWinPlus = nullptr, const bool withoutBorder = false,
                     DWORD ID_RESOURCE_ICON_APP = 0,const bool doubleBuffer = true);
    void setNameAplication(const char *nameApp);
    const char *getNameAplication() const;
    static bool isEnableRender(HWND hwndIgnore);
    static void disableRender(HWND hwndIgnore);
    DRAW *getGrafics(const int idComponent) const;
    void setCallEventsManager(EVENTS *ptrCallEventsManager);
    uint32_t setObjectContext(void *YOUR_PTR_OBJECT, const uint32_t index);
    void *getObjectContext(const uint32_t index);
    void setCursor(WINPLUS_TYPE_CURSOR TYPE);
    WINPLUS_TYPE_CURSOR getCursor();
    void setNextCursor();
    void startTimerHover();

    virtual int enterLoop(OnEventWinPlus ptrLogic);
    virtual void doEvents();
    void refresh(const uint32_t idComponent, const int eraseBK);
    void refresh(const int eraseBK);

  private:
    static HHOOK hookMsgProc;
    static BOOL CALLBACK MessageBoxEnumProc(HWND hWnd, LPARAM lParam);
    static LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI ThreadModal(LPVOID OBJECT);
  public:
    virtual void doModal(mbm::WINDOW *parent, OnDoModal onDoModal = nullptr, const bool threadModal = true,
                         const bool disabelParentWindow = true);
    HWND getHwnd(const int id = -1) const;
    bool setDrawer(mbm::DRAW *draw, const int idComponent);
    bool setDrawer(mbm::DRAW *draw, const mbm::TYPE_WINDOWS_WINPLUS typeWindowWinPlus);
    bool setDrawer(mbm::DRAW *draw);
    void setTheme(mbm::DRAW *theme);
    mbm::DRAW *getDrawer(const int id);
    int addWindowChild(const char *title, long x, long y, long width, long height,
                              OnEventWinPlus onEventWinPlus = nullptr, const bool enableResize = true,
                              const bool enableMaximizeButton = true, const int idDest = -1, USER_DRAWER *UserDrawer = nullptr);
    int addLabel(const char *title, long x, long y, long width, long height, const int idDest = -1,OnEventWinPlus onGotClickeOrFocus = nullptr, USER_DRAWER *userDrawer = nullptr);
    bool isLoaded();
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

        __MENU_DRAW(const int idDest_);
        virtual ~__MENU_DRAW();

        void hideSubMenu();
        bool showSubMenu();
        bool show(HWND parentHwnd_, const int myId, const int width, const int height, const int diff_x, const int diff_y);
    };
    
    const __MENU_DRAW *getMenuInfo(const int idMenu);
    static void refreshMenu();
    static const bool isAnyMenuVisible();
    int addMenu(const char *title, OnEventWinPlus onSelectedSubMenu, const int idDest = -1, USER_DRAWER *UserDrawer = nullptr);
    int addSubMenu(const char *title, const int idMenu);
    int addStatusBar(const char *textStatusBar0, const uint32_t numberPartsIntoStatusBar,
                            const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    int addSpinInt(long x, long y, long width, long height, const int idDest = -1, long widthSpin = 0,
                          long heightSpin = 0, OnEventWinPlus onChangeValue = nullptr, int minValue = 0, int maxValue = 10,
                          int increment = 1, int currentPosition = 0, bool vertical = true, const bool enableWrite = true,
                          USER_DRAWER * UserDrawer = nullptr);
    int addSpinFloat(long x, long y, long width, long height, const int idDest, long widthSpin = 0,
                            long heightSpin = 0, float minValue = 0.0f, float maxValue = 10.0f, float increment = 0.5f,
                            float currentPosition = 1.0f, int precision = 2, bool vertical = true,
                            const bool enableWrite = true, OnEventWinPlus onChangedValue = nullptr, USER_DRAWER * UserDrawer = nullptr);
    int addScroll(long x, long y, long width, long height, int scrollSize = 10,
                         OnEventWinPlus onEventWindow = nullptr, const int idDest = -1, USER_DRAWER * UserDrawer = nullptr); // doesnt work
    int addTrayIcon(const int ID_RESOURCE_ICON, OnEventWinPlus onEventWindowByIdMenu, const char *tip,USER_DRAWER * UserDrawer = nullptr);
    int addTrayIcon(const char *fileNameIcon, OnEventWinPlus onEventWindowByIdMenu, const char *tip,
                           USER_DRAWER * UserDrawer = nullptr);
    int addTrayIcon(OnEventWinPlus onEventWindowByIdMenu, const char *tip, USER_DRAWER * UserDrawer = nullptr);
    int addMenuTrayIcon(const char *str, const int idMenuTryIcon = -1, const bool hasSubMenu = false,
                               const int position = 0, const bool doubleClicked = false, const bool breakMenu = false,
                               const bool checked = false, USER_DRAWER * UserDrawer = nullptr);
    int addSubMenuTrayIcon(const char *str, const int position = 0, USER_DRAWER * UserDrawer = nullptr);
    bool showBallonTrayIcon(const char *title, const char *message, int uTimeout, DWORD dwIcon = NIIF_INFO);
    bool setTextTrayIcon(const char *text);
    bool printLastErrWindows(const char *where = nullptr);
    int addToolTip(const char *tip, const int idDest = -1, USER_DRAWER *dataToolTip = nullptr);
    int addButton(const char *title, long x, long y, long width, long height, const int idDest = -1,
                         OnEventWinPlus onPressedByType = nullptr, USER_DRAWER * UserDrawer = nullptr);
    int addRadioButton(const char *title, long x, long y, long width, long height, const int idDest = -1,
                              OnEventWinPlus onPressedByType = nullptr, USER_DRAWER * UserDrawer = nullptr);
    int addGroupBox(const char *title, long x, long y, long width, long height, const int idDest = -1,
                           OnEventWinPlus onGotClickeOrFocus = nullptr,USER_DRAWER * UserDrawer = nullptr);
    void killTimer(const int idTimer);
    int addTimer(uint32_t timeMilliseconds, OnEventWinPlus onElapseTimer, USER_DRAWER * UserDrawer = nullptr);
    void setPositionProgressBar(const int IdComponent, int position);
    int getPositionProgressBar(const int IdComponent);
    int addProgressBar(long x, long y, long width, long height, const int idDest = -1, const bool vertical = false,
                              USER_DRAWER * UserDrawer = nullptr);
    void setDefaultPositionTrackBar(const int idTrackBar, const short defaultPosition);
    void setTrackBar(const int idTrackBar, const float position);
    void setMaxPositionTrackBar(const int idTrackBar, const short maxPosition);
    TRACK_BAR_INFO *getInfoTrack(const int idTrackBar);
    float getPositionTrackBar(const int idTrackBar);
    int addCombobox(long x, long y, long width, long height, OnEventWinPlus onPressedByType = nullptr,
                           const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    int addCheckBox(const char *title, long x, long y, long width, long height,
                           OnEventWinPlus onPressedByType = nullptr, const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    int addRichText(const char *textIntoRichText, long x, long y, long width, long height, const int idDest = -1,
                           OnEventWinPlus onPressedByType = nullptr, const bool vScroll = true, const bool hScroll = false,
                           USER_DRAWER * UserDrawer = nullptr);
    int addTextBox(const char *textIntoTextBox, long x, long y, long width, long height, const int idDest = -1,
                          OnEventWinPlus onPressedByType = nullptr, const bool isPassword = false, USER_DRAWER * UserDrawer = nullptr);
    int addListBox(long x, long y, long width, long height, OnEventWinPlus onPressedByType = nullptr,
                          const int idDest = -1, USER_DRAWER * UserDrawer = nullptr);
    int addTrackBar(long x, long y, long width, long height, const int idDest = -1,
                           OnEventWinPlus onChangeValue = nullptr, float minPosition = 0.0f, float maxPosition = 100.0f,
                           float defaultPosition = 50.0f, float tickSmall = 10.0f, float tickLarge = 25.0f,
                           const bool invertValueText = false, const bool trackBarVertical = false, USER_DRAWER * UserDrawer = nullptr);
    void SetWindowTrans(int percent);
    void RemoveWindowTrans();
  private:
    void hideDestinyNotVisible(const int id, HWND myHwnd);

    struct __DO_MODAL_OBJ
    {
        mbm::WINDOW *w;
        mbm::WINDOW *parent;
        OnDoModal    onDoModal;
        const bool   disabelParentWindow;
        __DO_MODAL_OBJ(mbm::WINDOW *me, mbm::WINDOW *myParent, OnDoModal onDoModalParent, const bool disabelParentWindow_);
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
    void setUserDrawer(int idComponent, USER_DRAWER *userDrawer); // user drawer para o componente
    USER_DRAWER *getUserDrawer(int idComponent); // recupera user drawer para do componente
    void setIndexTabByGroup(const int idTabControlByGroup, const int index,
                                   const bool callOnEventWindowByType = true);
    int addTabControlByGroup(long x, long y, long width, long height, long widthButton, long heightButton,
                                    OnEventWinPlus onEventWindowByIndexTab = nullptr, const int idDest = -1,
                                    const bool enableVisibleGroups = true, USER_DRAWER * UserDrawer = nullptr);
    int addTabByGroup(const char *title, const int idTabControlByGroup, const bool newCloumn = false,
                             const long newWidth = 0, USER_DRAWER *UserDataButton = nullptr, USER_DRAWER *UserDrawerGroup = nullptr);
    bool clear(const int idComponent);
    bool getRadioButtonState(const int idRadioButton);
    bool setNextFocus(const int idComponent, const int idComponentNextFocus);
    bool setRadioButtonState(const int idRadioButton);
    bool setCheckBox(const bool checked, const int idCheckBox);
    bool getCheckBoxState(const int idCheckBox);
    bool addText(const int idComponent, const char *text);
    bool removeText(const int idComponent, const int indexString);
    bool setSelectedIndex(const int idComponent, const int indexString);
    int getSelectedIndex(const int idComponent);
    int getTextCount(const int idComponent);
    mbm::SPIN_PARAMSf *getSpinf(const int idSpinf);
    mbm::SPIN_PARAMSi *getSpin(const int idSpin);
    bool updateSpin(const int idSpin);
    bool setFocus(const int idComponent = -1);
    void forceFocus();
    bool setText(const int IdComponent, const char *stringSource, int index = -1);
    bool getText(const int IdComponent, char *stringOut, const WORD sizeStringOut, int index = -1);
    int getTextLength(const int IdComponent, int index = -1);
    std::vector<std::string> *getStatusBar(const int idComponent);
    void setOnKeyboardDown(OnKeyboardEvent function);
    void setOnKeyboardUp(OnKeyboardEvent function);
    bool setOnParserRawInput(OnParseRawInput function);
    void setOnMoveMouseEvent(OnMouseEvent function);
    void setOnClickLeftMouse(OnMouseEvent function);
    void setOnReleaseLeftMouse(OnMouseEvent function);
    void setOnClickRightMouse(OnMouseEvent function);
    void setOnReleaseRightMouse(OnMouseEvent function);
    void setOnClickMiddleMouse(OnMouseEvent function);
    void setOnReleaseMiddleMouse(OnMouseEvent function);
    void setOnScrollMouseEvent(OnMouseEventScroll function);
    bool setMaxLength(const int idComponent, const uint32_t maxLength);
    bool setReadOnlyToRichText(const int idRichText, const bool value);
    void setAlwaysOnTop(const bool value, const bool hideMe = false);
    void setAlwaysOnTop(mbm::WINDOW *hwndParent);
    void setColorKeying(const uint8_t red, const uint8_t green, const uint8_t blue);
    void setColorKeying(const uint8_t red, const uint8_t green, const uint8_t blue,const int idComponent);
    void setPosition(const int x, const int y, const int id = -1);
    long getWidth(const int id = -1);
    WINDOW *getWindow(const HWND hwnd_);
    long getHeight(const int id = -1);
    RECT getRect(const int id = -1);
    RECT getRectAbsolute(const int id = -1);
    RECT getRectRelativeWindow(const int id = -1);
    void setSize(RECT &source, const bool inner = true);
    void resize(const int idComponent, const int x, const int y, const int new_width, const int new_height);
    void resize(const int idComponent, const int new_width, const int new_height);
    static void resize(HWND hwnd2move, const int new_width, const int new_height, bool incrementSize = false);
    void hideConsoleWindow();
    void showConsoleWindow();
    void closeWindow();
    void hide(const HWND hwnd_);
    void hide(const int id = -1, int flag = SW_HIDE);
    void show(const int id = -1, int flag = SW_SHOW);
    void show(const HWND hwnd_);
    void showMaximized(const int idWindow = -1);
    void showMinimized(const int idWindow = -1);
    void setMinSizeAllowed(const int width,const int height);
    void setMaxSizeAllowed(const int width,const int height);
    bool loadTextFileToRichEdit(const int idRichText, WCHAR *fileName);
    bool loadTextFileToRichEdit(HWND hwndRichText, WCHAR *fileName);
    bool loadTextFileToRichEdit(const int idRichText, const WCHAR *filter = nullptr);
    bool saveTextFileFromRichText(const int idRichText, WCHAR *fileName);
    bool messageBoxQuestion(const char *format, ...);
    void messageBox(const char *format, ...);
    const int getTabStopPixelSize();
    const void setTabStopPixelSize(int characters = 1, const int idComponent = -1);
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
    void getCursorPos(POINT *p);
    bool isUsingDoubleBuffer()const;
  protected:
    bool usingDoubleBuffer;
    int  dialogunitTabStopInPixel;
    char nameAplication[MAX_PATH];
    std::map<int, void *> lsObjectsContext;
    EVENTS *                          callEventsManager;
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
WINDOW *getWindow(HWND hwnd);
WINDOW *getLastWindow();
WINDOW *getFirstWindow();
const char *selectetDirectory(HWND hwnd, char *outDir);
std::vector<std::string> &openFileBoxMult(const char *extension_, const char *title,
                                                 bool enableReturnExtencion = true, bool enableAllFileType = false,
                                                 HWND hwnd = nullptr, const char *defaultNameInDialog = nullptr);
WCHAR *openFileBoxW(const WCHAR *extension, const WCHAR *title, bool enableReturnExtencion = true,
                           bool enableAllFileType = false, HWND hwnd = nullptr, const WCHAR *defaultNameInDialog = nullptr);
char *openFileBox(const char *extension_, const char *title_, bool enableReturnExtencion, bool enableAllFileType,
                         HWND hwnd, const char *defaultNameInDialog_, char *outFileName);
WCHAR *saveFileBoxW(WCHAR *extension, WCHAR *title, bool enableReturnExtencion = true,
                           bool enableAllFileType = false, HWND hwnd = nullptr, const WCHAR *defaultNameInDialog = nullptr);
char *saveFileBox(const char *extension_, const char *title_, const bool enableReturnExtencion,
                         const bool enableAllFileType, HWND hwnd, const char *defaultNameInDialog_, char *outFileName);
bool getColorFromDialogBox(uint8_t &red, uint8_t &green, uint8_t &blue, HWND hwnd = nullptr);
bool getFontFromDialogBox(LOGFONTA *fontOut, HWND hwnd = nullptr);
bool getFontFromDialogBox(LOGFONTW *fontOut, HWND hwnd = nullptr);
RECT getMenuRect(int idWindow, int myId);
COM_BETWEEN_WINP *getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus_, WINDOW *me,
                                          TYPE_WINDOWS_WINPLUS typeMe, void *extraParams_, const int idDest,
                                          USER_DRAWER *userDrawer);
char *getNameFromPath(const char *fileNamePath, const bool removeCharacterInvalids, char *primaryPartFromPath,
                             char *outFileName);
const char *getHeaderToResource();

bool saveToFileBinary(const char *fileName, void *header, DWORD sizeOfHeader, void *dataIn, DWORD sizeOfDataIn);

bool loadFromFileBynary(const char *fileName, void *header, DWORD sizeOfHeader, void *dataOut, DWORD sizeOfDataOut);
bool loadHeaderFromFileBynary(const char *fileName, void *header, DWORD sizeOfHeader);
void closeAllWindows();
}

struct __AUX_MONITOR_SELECT
{
    int           indexCmbSelectedeMonitor;
    int           idCmbSelectMonitor;
    int           idbntOk;
    int           idChkAskAboutMonitor;
    bool          askMeAgain;
    mbm::MONITOR *monitor;
    __AUX_MONITOR_SELECT();
    static void __0_onProcess(mbm::WINDOW *, mbm::DATA_EVENT &dataEvent);
    static void __0_onPressOkMonitor(mbm::WINDOW *w, mbm::DATA_EVENT &);
    static void __1_onCheckedDontAskAgain(mbm::WINDOW *w, mbm::DATA_EVENT &);
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
    bool init(const char *nameApp, mbm::WINDOW &window, int adjustRendererWidth = 0, int adjustRendererHeight = 0,
                     const bool hasMenu = false, const bool leftToRight = false, const int idResourceIcon = 0);

    LAYOUT();
    virtual ~LAYOUT();
    HWND getHwndRenderer();
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
    __DRAW_SPLASH(const int ID_IMAGE_RESOURCE, mbm::STATIC_IMAGE_RESOURCE *imageResource);
    virtual ~__DRAW_SPLASH();
    bool eraseBackGround(mbm::COMPONENT_INFO* component);
    bool render(mbm::COMPONENT_INFO &component);
    static void onTimeOutSplah(mbm::WINDOW *w, mbm::DATA_EVENT &);
  private:
    mbm::STATIC_IMAGE_RESOURCE *resource;
};

void __destroyMenu(void *extraParams);

namespace mbm
{

void splash(const DWORD timeMiliSec, STATIC_IMAGE_RESOURCE &imageResource, uint8_t rgbProgres[3] = nullptr,
                   uint8_t colorKeiyng[3] = nullptr);
void splash(const DWORD timeMiliSec, int ID_IMAGE_RESOURCE, uint8_t rgbProgres[3] = nullptr,
                   uint8_t colorKeiyng[3] = nullptr);
void splash(const DWORD timeMiliSec, int ID_IMAGE_RESOURCE, int ID_IMAGE_PROGRESS,
                   uint8_t colorKeiyng[3] = nullptr);
void splash(const int widthWindow, const int heightWindow, const DWORD timeMiliSec, int ID_IMAGE_RESOURCE,
                   int ID_IMAGE_PROGRESS, uint8_t colorKeiyng[3] = nullptr);
}

const int __getTabStopPixelSize(HWND hwnd);
namespace mbm
{
void __destroyOnExitAllListComBetweenWindows();
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
