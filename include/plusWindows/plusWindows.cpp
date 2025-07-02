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

#include "plusWindows.h"

const char*  WINPLUS_DIRSEPARATOR = "\\";

DWORD GetVersionDll(const char *lpszDllName)
{
    DWORD dwVersion = 0;

    // For security purposes, LoadLibrary should be provided with a fully qualified
    // path to the DLL. The lpszDllName variable should be tested to ensure that it
    // is a fully qualified path before it is used.
    HINSTANCE hinstDll = LoadLibraryA(lpszDllName);
    if (hinstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");

        // Because some DLLs might not implement this function, you must test for
        // it explicitly. Depending on the particular DLL, the lack of a DllGetVersion
        // function can be a useful indicator of the version.

        if (pDllGetVersion)
        {
            DLLVERSIONINFO dvi;
            ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);
            HRESULT hr = (*pDllGetVersion)(&dvi);
            if (SUCCEEDED(hr))
            {
                dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }
        FreeLibrary(hinstDll);
    }
    return dwVersion;
}

namespace mbm
{
    __NC_BORDERS::__NC_BORDERS(HWND hwnd, const bool invalidateRegion, __NC_BUTTONS * buttons_)
    {
        buttons    = buttons_;
        status     = false;
        hrgnLeft   = nullptr;
        hrgnRight  = nullptr;
        hrgnTop    = nullptr;
        hrgnBottom = nullptr;
        hrgnAll    = nullptr;
        memset(&rectLeft, 0, sizeof(RECT));
        memset(&rectRight, 0, sizeof(RECT));
        memset(&rectTop, 0, sizeof(RECT));
        memset(&rectBottom, 0, sizeof(RECT));

        memset(&rcClient, 0, sizeof(RECT));
        memset(&rcWind, 0, sizeof(RECT));

        POINT ptDiff;
        GetClientRect(hwnd, &rcClient);
        GetWindowRect(hwnd, &rcWind);
        ptDiff.x = (rcWind.right - rcWind.left) - rcClient.right;
        ptDiff.y = (rcWind.bottom - rcWind.top) - rcClient.bottom;

        sides = (ptDiff.x / 2);

        rectTop.left   = rcWind.left;
        rectTop.right  = rcWind.right;
        rectTop.top    = rcWind.top;
        rectTop.bottom = rcWind.top + ptDiff.y - sides;

        rectLeft.left   = rcWind.left;
        rectLeft.right  = rcWind.left + sides;
        rectLeft.top    = rcWind.top;
        rectLeft.bottom = rcWind.bottom;

        rectRight.left   = rcWind.right - sides;
        rectRight.right  = rcWind.right;
        rectRight.top    = rcWind.top;
        rectRight.bottom = rcWind.bottom;

        rectBottom.left   = rcWind.left;
        rectBottom.right  = rcWind.right;
        rectBottom.top    = rcWind.bottom - sides;
        rectBottom.bottom = rcWind.bottom;

        hrgnTop    = CreateRectRgnIndirect(&rectTop);
        hrgnLeft   = CreateRectRgnIndirect(&rectLeft);
        hrgnRight  = CreateRectRgnIndirect(&rectRight);
        hrgnBottom = CreateRectRgnIndirect(&rectBottom);

        hrgnAll           = CreateRectRgnIndirect(&rectLeft);
        int fnCombineMode = RGN_OR;
        int ret           = CombineRgn(hrgnAll, hrgnRight, hrgnLeft, fnCombineMode);
        if (ret != ERROR)
        {
            ret = CombineRgn(hrgnAll, hrgnAll, hrgnTop, fnCombineMode);
            if (ret != ERROR)
            {
                ret = CombineRgn(hrgnAll, hrgnAll, hrgnBottom, fnCombineMode);
            }
        }
        if (ret == ERROR)
        {
            Defaultresult(ret);
        }
        else if (invalidateRegion)
        {
            ret = OffsetRgn(hrgnAll, -rcWind.left, -rcWind.top - rectTop.bottom);
            if (ret != ERROR)
                RedrawWindow(hwnd, nullptr, hrgnAll, RDW_FRAME | RDW_INVALIDATE);
            else
                Defaultresult(ret);
        }
        status = (ret != ERROR);
    }

    __NC_BORDERS::~__NC_BORDERS()
    {
        if (hrgnLeft)
            DeleteObject(hrgnLeft);
        if (hrgnRight)
            DeleteObject(hrgnRight);
        if (hrgnTop)
            DeleteObject(hrgnTop);
        if (hrgnBottom)
            DeleteObject(hrgnBottom);
        if (hrgnAll)
            DeleteObject(hrgnAll);
        hrgnLeft   = nullptr;
        hrgnRight  = nullptr;
        hrgnTop    = nullptr;
        hrgnBottom = nullptr;
        hrgnAll    = nullptr;
    }

    __NC_BORDERS::__NC_BUTTONS::__NC_BUTTONS()
        {
            memset(&rectClose, 0, sizeof(rectClose));
            memset(&rectMaximize, 0, sizeof(rectMaximize));
            memset(&rectMinimize, 0, sizeof(rectMinimize));
            isHoverClose      = false;
            isHoverMax        = false;
            isHoverMin        = false;
            hasCloseButton    = false;
            hasMaximizeButton = false;
            hasMinimizeButton = false;
            distCloseRight    = 0;
            distMaxRight      = 0;
            distMinRight      = 0;
        }

    __NC_BORDERS::__NC_BUTTONS::~__NC_BUTTONS()
        {
            isHoverClose = false;
            isHoverMax   = false;
            isHoverMin   = false;
        }

    void __NC_BORDERS::Defaultresult(int ret)
    {
        switch (ret)
        {
            case NULLREGION: { printf("NULLREGION \n");
            }
            break;
            case SIMPLEREGION: { printf("SIMPLEREGION \n");
            }
            break;
            case COMPLEXREGION: { printf("COMPLEXREGION \n");
            }
            break;
            case ERROR: { printf("ERROR \n");
            }
            break;
        }
    }

    WCHAR *toWchar(const char *str, WCHAR *outText)
{
    if (str == nullptr)
        return nullptr;
    int    len      = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0) + 1;
    WCHAR *strLocal = new WCHAR[len];
    memset(strLocal, 0, sizeof(WCHAR) * len);
    MultiByteToWideChar(CP_ACP, 0, str, -1, (LPWSTR)strLocal, len - 1);
    if (outText)
    {
        wcscpy_s(outText, len, strLocal);
        delete[] strLocal;
        return outText;
    }
    return strLocal;
}

    char *toChar(const WCHAR *wstr, char *outText)
{
    if (wstr == nullptr)
        return nullptr;
    int   len      = wcslen(wstr) + 1;
    char *strLocal = new char[len];
    memset(strLocal, 0, sizeof(char) * len);
    int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr, len - 1, nullptr, 0, nullptr, nullptr);
    size_needed     = WideCharToMultiByte(CP_ACP, 0, wstr, len, strLocal, size_needed, nullptr, nullptr);
    if (outText)
    {
        strcpy_s(outText, len, strLocal);
        delete[] strLocal;
        return outText;
    }
    return strLocal;
}

    STATIC_IMAGE_RESOURCE::STATIC_IMAGE_RESOURCE(const unsigned int w, const unsigned int h, const unsigned int s, const char *nickName_,
                const unsigned int *d, const unsigned int c)
    : width(w), height(h), size(s), nickName(nickName_), data(d), colorKeying(c)
{
}

    void split(std::vector<std::string> &result, const char *in, const char delim)
{
    result.clear();
    if (in)
    {
        while (*in)
        {
            const char *next = strchr(in, delim);
            if (next)
            {
                result.push_back(std::string(in, next));
            }
            else
            {
                result.push_back(in);
            }
            in = (next ? next + 1 : "");
        }
    }
}

    const char *getLastErrWindows(const char *where, char *outMessage)
{
    DWORD lerr = GetLastError();
    if (lerr)
    {
        char *message;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lerr, 0, (char *)&message, 0,
                       nullptr);
        if (where)
        {
            if (outMessage)
                sprintf(outMessage, "Where:%s \nError:%s", where, message);
        }
        else
        {
            if (outMessage)
                sprintf(outMessage, "\n%s", message);
        }
        LocalFree(message);
        return outMessage;
    }
    return "Nenhum erro encontrado!";
}

    bool startUpWindows64(const char *name)
{
    char    myExe[MAX_PATH];
    WCHAR   myExeW[MAX_PATH];
    HMODULE HMod = GetModuleHandle(nullptr);
    GetModuleFileNameA(HMod, myExe, sizeof(myExe));
    GetModuleFileNameW(HMod, myExeW, sizeof(myExeW));

    HKEY hkey          = 0;
    LONG regOpenResult = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hkey);
    if (regOpenResult != ERROR_SUCCESS)
    {
        if (regOpenResult == ERROR_FILE_NOT_FOUND)
            printf("Key not found.\n");
        else
            printf("Error opening key.\n");
        return false;
    }
    std::string str = "\"";
    str += myExe;
    str += "\"";

    strcpy(myExe, str.c_str());

    long sta = RegSetValueExA(hkey, name, // This is the name that shows up in your registry
                              0, REG_SZ, (BYTE *)myExe, strlen(myExe));

    if (sta != ERROR_SUCCESS)
    {
        printf("Error opening key.\n");
        return false;
    }
    // RegSetValueExW(hkey ,L"startup",0,REG_SZ,(BYTE*)myExeW,0);
    RegCloseKey(hkey);
    return true;
}

    bool startUpWindows(const char *name)
{
    char    myExe[MAX_PATH];
    WCHAR   myExeW[MAX_PATH];
    HMODULE HMod = GetModuleHandle(nullptr);
    GetModuleFileNameA(HMod, myExe, sizeof(myExe));
    GetModuleFileNameW(HMod, myExeW, sizeof(myExeW));
    HKEY hkey          = 0;
    LONG regOpenResult = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                       //"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                                       "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hkey);
    if (regOpenResult != ERROR_SUCCESS)
    {
        if (regOpenResult == ERROR_FILE_NOT_FOUND)
            printf("Key not found.\n");
        else
            printf("Error opening key.\n");
        return false;
    }

    long sta = RegSetValueExA(hkey, name, // This is the name that shows up in your registry
                              0, REG_SZ, (BYTE *)myExe, strlen(myExe));

    if (sta != ERROR_SUCCESS)
    {
        printf("Error opening key.\n");
        return false;
    }
    // RegSetValueExW(hkey ,L"startup",0,REG_SZ,(BYTE*)myExeW,0);
    RegCloseKey(hkey);
    return true;
}

    REGEDIT::REGEDIT()
    {
        hKey = 0;
    }

    REGEDIT::~REGEDIT()
    {
        closeKey();
    }

    bool REGEDIT::openKey(HKEY hRootKey, const wchar_t *strKey, const DWORD acess)
    {
        LONG nError = RegOpenKeyExW(hRootKey, strKey, 0, acess, &hKey);
        if (nError == ERROR_FILE_NOT_FOUND)
        {
            std::cout << "Creating registry key: " << strKey << std::endl;
            nError = RegCreateKeyExW(hRootKey, strKey, 0, nullptr, REG_OPTION_NON_VOLATILE, acess, nullptr, &hKey, nullptr);
        }
        if (nError)
        {
            std::cout << "Error: " << nError << " Could not find or create " << strKey << std::endl;
            char str[255];
            sprintf(str, "Arquivo: %s linha:%d", __FILE__, __LINE__);
            printLastErrWindows(str);
        }
        return (hKey != 0);
    }

    bool REGEDIT::openKey(HKEY hRootKey, const char *strKey, const DWORD acess)
    {
        LONG nError = RegOpenKeyExA(hRootKey, strKey, 0, acess, &hKey);
        if (nError == ERROR_FILE_NOT_FOUND)
        {
            std::cout << "Creating registry key: " << strKey << std::endl;
            nError = RegCreateKeyExA(hRootKey, strKey, 0, nullptr, REG_OPTION_NON_VOLATILE, acess, nullptr, &hKey, nullptr);
        }
        if (nError)
        {
            std::cout << "Error: " << nError << " Could not find or create " << strKey << std::endl;
            char str[255];
            sprintf(str, "Arquivo: %s linha:%d", __FILE__, __LINE__);
            printLastErrWindows(str);
        }
        return (hKey != 0);
    }

    void REGEDIT::setVal(LPCTSTR lpValue, DWORD data)
    {
        if (hKey)
        {
            LONG nError = RegSetValueEx(hKey, lpValue, 0, REG_DWORD, (LPBYTE)&data, sizeof(DWORD));
            if (nError)
                std::cout << "Error: " << nError << " Could not set registry value: " << (char *)lpValue << std::endl;
        }
        else
        {
            std::cout << "Erro no opened HKEY. use openKey!" << std::endl;
        }
    }

    DWORD REGEDIT::getVal(LPCTSTR lpValue, DWORD valueNotFound)
    {
        if (hKey)
        {
            DWORD data   = 0;
            DWORD size   = sizeof(data);
            DWORD type   = REG_DWORD;
            LONG  nError = RegQueryValueEx(hKey, lpValue, nullptr, &type, (LPBYTE)&data, &size);
            if (nError == ERROR_FILE_NOT_FOUND)
                data = valueNotFound; // The value will be created and set to data next time SetVal() is called.
            else if (nError)
                std::cout << "Error: " << nError << " Could not get registry value " << (char *)lpValue << std::endl;
            return data;
        }
        else
        {
            std::cout << "Erro no opened HKEY. use openKey!" << std::endl;
            return 0;
        }
    }

    void REGEDIT::closeKey()
    {
        if (hKey)
            RegCloseKey(hKey);
        hKey = 0;
    }

    bool REGEDIT::printLastErrWindows(const char *where)
    {
        DWORD lerr = GetLastError();
        if (lerr)
        {
            char *message;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lerr, 0, (char *)&message,
                           0, nullptr);
            if (where)
                printf("%s %s\n", where, message);
            else
                printf("%s\n", message);
            LocalFree(message);
            return true;
        }
        return false;
    }

    int getRandomInt(const int min, const int max)
{
    return (int)(((double)rand() / (double)(0x7fff + 1)) * (max - min + 1) + min);
}

    char getRandomChar(const char min, const char max)
{
    return (char)(((double)rand() / (double)(0x7fff + 1)) * (max - min + 1) + min);
}

    float getRandomFloat(const float min, const float max)
{
    if (min >= max)
        return min;
    // get random float in [0, 1] interval
    float f = (rand() % 10000) * 0.0001f;
    // return float in [min, max] interval.
    return (f * (max - min)) + min;
}

    void __destroyOnExitAllListComBetweenWindows();
};

void __initRandomSeed()
{
    static bool wasInitSeed = false;
    if (!wasInitSeed)
    {
        wasInitSeed = true;
        SYSTEMTIME systime;
        ::GetLocalTime(&systime);
        srand(systime.wMilliseconds);
    }
}

#if UNICODE
    WCHAR *getNextClassNameWindow()
    {
        static WCHAR classNameRet[MAX_PATH] = {L"CLASS WINP "};
        static int   CountClassName         = 0;
        if (CountClassName == 0)
        {
            __initRandomSeed();
        }
        CountClassName++;
        WCHAR num[255];
        wsprintfW(num, L"%d", CountClassName);
        wcscpy(classNameRet, L"CLASS WINP ");
        wcscat(classNameRet, num);
        return classNameRet;
    }
#else
    char *getNextClassNameWindow()
    {
        static char classNameRet[MAX_PATH] = {"CLASS WINP "};
        static int  CountClassName         = 0;
        if (CountClassName == 0)
        {
            __initRandomSeed();
        }
        CountClassName++;
        char num[255];
        sprintf(num, "%d", CountClassName);
        strcpy(classNameRet, "CLASS WINP ");
        strcat(classNameRet, num);
        return classNameRet;
    }
#endif

    __TAB_GROUP_DESC::__TAB_GROUP_DESC(const int _index, const int _idDest) : index(_index), idDest(_idDest)
    {
        idTabControlByGroup = -1;
        displacementX       = index + 1;
        displacementY       = 0;
        idGroupTabBox       = -1;
        tabFather           = nullptr;
        tabSelected         = nullptr;
        x                   = 0;
        y                   = 0;
        width               = 100;
        height              = 25;
        widthButton         = 100;
        heightButton        = 25;
        enableVisibleGroups = true;
    }

    TRACK_BAR_INFO::TRACK_BAR_INFO()
    {
        minPosition      = 0.0f;
        maxPosition      = 0.0f;
        position         = 0.0f;
        tickLarge        = 0.0f;
        tickSmall        = 0.0f;
        increment        = 0.0f;
        defaultPosition  = 0.0f;
        positionInverted = 0.0f;
        isVertical       = false;
        invertMinMaxText = false;
    }

    PROGRESS_BAR_INFO::PROGRESS_BAR_INFO(const bool vertical_) : vertical(vertical_)
    {
        minRange = 0;
        maxRange = 100;
        position = 0;
    }

    unsigned int __HEADER_BMP::getAsUintFromCharPointer(unsigned char *adress)
    {
        unsigned int x;
        x = adress[3];
        x <<= 8;
        x |= adress[2];
        x <<= 8;
        x |= adress[1];
        x <<= 8;
        x |= adress[0];
        return x;
    }

namespace mbm
{
    BMP::BMP()
    {
        memset(&bInfo, 0, sizeof(bInfo));
        dataRGB = nullptr;
        data    = nullptr;
        memset(&bitmapInfo, 0, sizeof(BITMAP));
    }

    BMP::~BMP()
    {
        if (data)
            DeleteObject(data);
        data = nullptr;
        if (dataRGB)
            delete[] dataRGB;
        dataRGB = nullptr;
    }

    void BMP::release()
    {
        if (data)
            DeleteObject(data);
        data = nullptr;
        if (dataRGB)
            delete[] dataRGB;
        memset(&bInfo, 0, sizeof(bInfo));
        dataRGB = nullptr;
        data    = nullptr;
        memset(&bitmapInfo, 0, sizeof(BITMAP));
    }

    bool BMP::load(HWND hwnd, const int ID_RESOURCE)
    {
        if (data == nullptr)
        {
            data = LoadBitmap((HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), MAKEINTRESOURCE(ID_RESOURCE));
            if (data)
            {
                GetObject(data, sizeof(BITMAP), &bitmapInfo);
                HDC hdcscreen = GetDC(0);
                HDC hdc       = CreateCompatibleDC(hdcscreen);
                ReleaseDC(0, hdcscreen);
                BITMAPINFO info;
                memset(&info, 0, sizeof(info));
                info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

                GetDIBits(hdc, data, 0, 0, nullptr, &info, DIB_RGB_COLORS);

                DWORD sz = info.bmiHeader.biWidth * info.bmiHeader.biHeight * 4;
                if (dataRGB)
                    delete[] dataRGB;
                dataRGB = nullptr;
                dataRGB = new unsigned char[sz + 10];
                memset(dataRGB, 0, sz);

                // get pixel data in 32bit format
                info.bmiHeader.biSize        = sizeof(info.bmiHeader);
                info.bmiHeader.biBitCount    = 32;
                info.bmiHeader.biCompression = BI_RGB;
                info.bmiHeader.biHeight      = (info.bmiHeader.biHeight < 0)
                                              ? (-info.bmiHeader.biHeight)
                                              : (info.bmiHeader.biHeight); // correct the bottom-up ordering of lines
                GetDIBits(hdc, data, 0, info.bmiHeader.biHeight, (LPVOID)dataRGB, &info, DIB_RGB_COLORS);
                if (hdc)
                    DeleteDC(hdc);
                return true;
            }
            return false;
        }
        return true;
    }

    bool BMP::loadTrueColor(const char *fileName)
    {
        __HEADER_BMP   headerBMP;
        unsigned int   widthTemp = 1;
        unsigned int   zeroByte  = 0;
        unsigned int   index     = 0;
        if (!fileName)
            return false;
        FILE *fp = fopen(fileName, "rb");
        if (!fp)
            return false;
        fread(&headerBMP, sizeof(__HEADER_BMP), 1, fp);
        if (headerBMP.identy[0] != 'B')
        {
            fclose(fp);
            return false;
        }
        int  bitsPerPixels = getAsUintFromCharPointer(headerBMP.bitsPerPixels);
        bool is32Bits      = bitsPerPixels == 32;
        if (bitsPerPixels != 24 && bitsPerPixels != 32)
        {
            fclose(fp);
            return false;
        }
        if (getAsUintFromCharPointer(headerBMP.offSet) != 0x36)
        {
            fclose(fp);
            return false;
        }
        if (getAsUintFromCharPointer(headerBMP.compressed))
        {
            fclose(fp);
            return false;
        }
        unsigned int width  = getAsUintFromCharPointer(headerBMP.width);
        unsigned int height = getAsUintFromCharPointer(headerBMP.height);
        if ((unsigned long int)(width * height) >= 0xffffffff)
        {
            fclose(fp);
            return false;
        }
        if (!is32Bits)
        {
            if ((width * 3) % 4)
            {
                while (((width * 3) + zeroByte) % 4)
                    ++zeroByte;
            }
        }
		unsigned char *img   = new unsigned char[(width * height * 3) + 4];
        int i = getc(fp);
        while (i != EOF)
        {
            img[index++] = (unsigned char)i; // 1° byte: Azul
            i            = getc(fp);
            img[index++] = (unsigned char)i; // 2° byte: cor Verde
            i            = getc(fp);
            img[index++] = (unsigned char)i; // 3° byte: cor Vermelho
            i            = getc(fp);
            if (is32Bits)
                i = getc(fp);
            if (widthTemp == width)
            {
                widthTemp = 0;
                if (zeroByte)
                {
                    unsigned int byte_temp = (zeroByte - 1);
                    i                      = getc(fp);
                    while (byte_temp)
                    {
                        if (i != 0)
                        {
                            fclose(fp);
                            delete [] img;
                            return false;
                        }
                        --byte_temp;
                        i = getc(fp);
                    }
                }
            }
            widthTemp++;
        }
        fclose(fp);
        if (this->createBitmap(width, height, img))
        {
            delete[] img;
            return true;
        }
        delete[] img;
        return false;
    }

    bool BMP::load(const char *fileNameBitmap)
    {
        if (data == nullptr)
        {
            data = (HBITMAP)LoadImageA(GetModuleHandle(nullptr), fileNameBitmap, IMAGE_BITMAP, 0, 0,
                                       LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE);
            if (data)
            {
                GetObject(data, sizeof(BITMAP), &bitmapInfo);
                HDC hdcscreen = GetDC(0);
                HDC hdc       = CreateCompatibleDC(hdcscreen);
                ReleaseDC(0, hdcscreen);
                BITMAPINFO info;
                memset(&info, 0, sizeof(info));
                info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

                GetDIBits(hdc, data, 0, 0, nullptr, &info, DIB_RGB_COLORS);

                DWORD sz = info.bmiHeader.biWidth * info.bmiHeader.biHeight * 4;
                if (dataRGB)
                    delete[] dataRGB;
                dataRGB = nullptr;
                dataRGB = new unsigned char[sz + 10];
                memset(dataRGB, 0, sz);

                // get pixel data in 32bit format
                info.bmiHeader.biSize        = sizeof(info.bmiHeader);
                info.bmiHeader.biBitCount    = 32;
                info.bmiHeader.biCompression = BI_RGB;
                info.bmiHeader.biHeight      = (info.bmiHeader.biHeight < 0)
                                              ? (-info.bmiHeader.biHeight)
                                              : (info.bmiHeader.biHeight); // correct the bottom-up ordering of lines
                GetDIBits(hdc, data, 0, info.bmiHeader.biHeight, (LPVOID)dataRGB, &info, DIB_RGB_COLORS);
                if (hdc)
                    DeleteDC(hdc);
                return true;
            }
            return false;
        }
        return true;
    }

    bool BMP::load(mbm::STATIC_IMAGE_RESOURCE &imageResource)
    {
        if (data == nullptr)
        {
            if (imageResource.width <= 3 || imageResource.height <= 0 || imageResource.data == nullptr)
                return false;
            int diffw = imageResource.width % 4;
            memset(&bInfo, 0, sizeof(bInfo));
            BITMAPINFOHEADER &bih = bInfo.bmiHeader;
            bih.biSize            = sizeof(bih);
            bih.biWidth           = imageResource.width - diffw;
            bih.biHeight          = imageResource.height;
            bih.biPlanes          = 1;
            bih.biBitCount        = 24;
            bih.biCompression     = BI_RGB;
            bih.biSizeImage       = ((bih.biWidth * bih.biBitCount / 8 + 3) & 0xFFFFFFFC) * bih.biHeight;
            bih.biXPelsPerMeter   = 10000;
            bih.biYPelsPerMeter   = 10000;
            bih.biClrUsed         = 0;
            bih.biClrImportant    = 0;
            HDC hdcscreen         = GetDC(0);
            HDC hdc               = CreateCompatibleDC(hdcscreen);
            ReleaseDC(0, hdcscreen);
            void *bits;
            if (data)
                DeleteObject(data);
            data = nullptr;
            data = CreateDIBSection(hdc, (BITMAPINFO *)&bih, DIB_RGB_COLORS, &bits, 0, 0);
            BITMAP bitmap;
            memset(&bitmap, 0, sizeof(BITMAP));
            if (!GetObject(data, sizeof(BITMAP), &bitmap))
                return false;
            int w = bitmap.bmWidth;
            int h = bitmap.bmHeight;

            bInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bInfo.bmiHeader.biWidth       = w;
            bInfo.bmiHeader.biHeight      = -h;
            bInfo.bmiHeader.biPlanes      = 1;
            bInfo.bmiHeader.biBitCount    = 24;
            bInfo.bmiHeader.biCompression = BI_RGB;
            bInfo.bmiHeader.biSizeImage   = w * h * 3;
            if (dataRGB)
                delete[] dataRGB;
            dataRGB = nullptr;
            dataRGB = new unsigned char[bInfo.bmiHeader.biSizeImage + 10];
            if (diffw)
            {
                const int size = (int)imageResource.size;
                for (int x = 0, y = 0, i = 0; i < size; ++i)
                {
                    int indexDest          = (x * 3) + (y * w * 3);
                    dataRGB[indexDest]     = (unsigned char)((imageResource.data[i]));       // azul
                    dataRGB[indexDest + 1] = (unsigned char)((imageResource.data[i]) >> 8);  // verde
                    dataRGB[indexDest + 2] = (unsigned char)((imageResource.data[i]) >> 16); // vermelho
                    x++;
                    if (x > w)
                    {
                        x = 0;
                        y++;
                        i += diffw - 1;
                    }
                }
            }
            else
            {
                for (DWORD y = 0, x = 0; y < bInfo.bmiHeader.biSizeImage; y += 3, ++x)
                {
                    dataRGB[y]     = (unsigned char)((imageResource.data[x]));       // azul
                    dataRGB[y + 1] = (unsigned char)((imageResource.data[x]) >> 8);  // verde
                    dataRGB[y + 2] = (unsigned char)((imageResource.data[x]) >> 16); // vermelho
                }
            }
            int siz = SetDIBits(hdc, data, 0, h, dataRGB, &bInfo, DIB_RGB_COLORS);
            if (siz)
            {
                GetObject(data, sizeof(BITMAP), &bitmapInfo);
                if (hdc)
                    DeleteDC(hdc);
                return true;
            }
            if (hdc)
                DeleteDC(hdc);
            return false;
        }
        return true;
    }

    const int BMP::isLoaded()const
    {
        return (data != nullptr);
    }

    const int BMP::getWidth() const
    {
        return bitmapInfo.bmWidth;
    }

    const int BMP::getHeight() const
    {
        return bitmapInfo.bmHeight;
    }

    void BMP::draw(HDC hdc)
    {
        if (data)
        {
            if (hdc == nullptr)
            {
                HDC hdcscreen = GetDC(0);
                hdc           = CreateCompatibleDC(hdcscreen);
                ReleaseDC(0, hdcscreen);
            }
            HDC memDC = CreateCompatibleDC(hdc);
            SelectObject(memDC, data);
            BitBlt(hdc, 0, 0, bitmapInfo.bmWidth, bitmapInfo.bmHeight, memDC, 0, 0, SRCCOPY);
            DeleteDC(memDC);
        }
    }

    void BMP::draw(HDC hdc, const int x, const int y)
    {
        if (data)
        {
            if (hdc == nullptr)
            {
                HDC hdcscreen = GetDC(0);
                hdc           = CreateCompatibleDC(hdcscreen);
                ReleaseDC(0, hdcscreen);
            }
            HDC memDC = CreateCompatibleDC(hdc);
            SelectObject(memDC, data);
            BitBlt(hdc, x, y, bitmapInfo.bmWidth, bitmapInfo.bmHeight, memDC, 0, 0, SRCCOPY);
            DeleteDC(memDC);
        }
    }

    void BMP::draw(HDC hdc, const RECT &rect)
    {
        if (data)
        {
            if (hdc == nullptr)
            {
                HDC hdcscreen = GetDC(0);
                hdc           = CreateCompatibleDC(hdcscreen);
                ReleaseDC(0, hdcscreen);
            }
            HDC memDC = CreateCompatibleDC(hdc);
            SelectObject(memDC, data);
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;
            if (w > bitmapInfo.bmWidth)
                w = bitmapInfo.bmWidth;
            if (h > bitmapInfo.bmHeight)
                h = bitmapInfo.bmHeight;
            BitBlt(hdc, rect.left, rect.top, w, h, memDC, 0, 0, SRCCOPY);
            DeleteDC(memDC);
        }
    }

    void BMP::draw(HDC hdc, const int xPosition, const int yPosition, const int xSource, const int ySource,
                     const int width, const int height)
    {
        if (data)
        {
            if (hdc == nullptr)
            {
                HDC hdcscreen = GetDC(0);
                hdc           = CreateCompatibleDC(hdcscreen);
                ReleaseDC(0, hdcscreen);
            }
            HDC memDC = CreateCompatibleDC(hdc);
            SelectObject(memDC, data);
            BitBlt(hdc, xPosition, yPosition, width, height, memDC, xSource, ySource, SRCCOPY);
            DeleteDC(memDC);
        }
    }

    bool BMP::createBitmap(int width, int heigth)
    {
        if (data)
            return true;
        memset(&bInfo, 0, sizeof(bInfo));
        if (width <= 3 || heigth <= 0)
            return false;
        while (width % 4 != 0)
            width += 1;
        ZeroMemory(&bInfo, sizeof(bInfo));
        BITMAPINFOHEADER &bih = bInfo.bmiHeader;
        bih.biSize            = sizeof(bih);
        bih.biWidth           = width;
        bih.biHeight          = heigth;
        bih.biPlanes          = 1;
        bih.biBitCount        = 24;
        bih.biCompression     = BI_RGB;
        bih.biSizeImage       = ((bih.biWidth * bih.biBitCount / 8 + 3) & 0xFFFFFFFC) * bih.biHeight;
        bih.biXPelsPerMeter   = 10000;
        bih.biYPelsPerMeter   = 10000;
        bih.biClrUsed         = 0;
        bih.biClrImportant    = 0;
        HDC hdcscreen         = GetDC(0);
        HDC hdc               = CreateCompatibleDC(hdcscreen);
        ReleaseDC(0, hdcscreen);
        void *bits;
        if (data)
            DeleteObject(data);
        data = nullptr;
        data = CreateDIBSection(hdc, (BITMAPINFO *)&bih, DIB_RGB_COLORS, &bits, 0, 0);
        BITMAP bitmap;
        memset(&bitmap, 0, sizeof(BITMAP));
        if (!GetObject(data, sizeof(BITMAP), &bitmap))
            return false;
        int w = bitmap.bmWidth;
        int h = bitmap.bmHeight;

        bInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bInfo.bmiHeader.biWidth       = w;
        bInfo.bmiHeader.biHeight      = -h;
        bInfo.bmiHeader.biPlanes      = 1;
        bInfo.bmiHeader.biBitCount    = 24;
        bInfo.bmiHeader.biCompression = BI_RGB;
        bInfo.bmiHeader.biSizeImage   = w * h * 3;
        if (dataRGB)
            delete[] dataRGB;
        dataRGB = new unsigned char[bInfo.bmiHeader.biSizeImage + 10];
        for (DWORD y = 0; y < bInfo.bmiHeader.biSizeImage; y += 3)
        {
            dataRGB[y]     = 0xff; // azul
            dataRGB[y + 1] = 0x0;  // verde
            dataRGB[y + 2] = 0x0;  // vermelho
        }
        int siz = SetDIBits(hdc, data, 0, h, dataRGB, &bInfo, DIB_RGB_COLORS);
        if (siz)
        {
            GetObject(data, sizeof(BITMAP), &bitmapInfo);
            if (hdc)
                DeleteDC(hdc);
            return true;
        }
        if (hdc)
            DeleteDC(hdc);
        return false;
    }

    bool BMP::createBitmap(int width, int heigth, const unsigned char *dataImage)
    {
        if (data)
            return true;
        memset(&bInfo, 0, sizeof(bInfo));
        if (width <= 3 || heigth <= 0 || dataImage == nullptr)
            return false;
        while (width % 4 != 0)
            width += 1;
        ZeroMemory(&bInfo, sizeof(bInfo));
        BITMAPINFOHEADER &bih = bInfo.bmiHeader;
        bih.biSize            = sizeof(bih);
        bih.biWidth           = width;
        bih.biHeight          = heigth;
        bih.biPlanes          = 1;
        bih.biBitCount        = 24;
        bih.biCompression     = BI_RGB;
        bih.biSizeImage       = ((bih.biWidth * bih.biBitCount / 8 + 3) & 0xFFFFFFFC) * bih.biHeight;
        bih.biXPelsPerMeter   = 10000;
        bih.biYPelsPerMeter   = 10000;
        bih.biClrUsed         = 0;
        bih.biClrImportant    = 0;
        HDC hdcscreen         = GetDC(0);
        HDC hdc               = CreateCompatibleDC(hdcscreen);
        ReleaseDC(0, hdcscreen);
        void *bits;
        if (data)
            DeleteObject(data);
        data = nullptr;
        data = CreateDIBSection(hdc, (BITMAPINFO *)&bih, DIB_RGB_COLORS, &bits, 0, 0);
        BITMAP bitmap;
        memset(&bitmap, 0, sizeof(BITMAP));
        if (!GetObject(data, sizeof(BITMAP), &bitmap))
            return false;
        int w = bitmap.bmWidth;
        int h = bitmap.bmHeight;

        bInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bInfo.bmiHeader.biWidth       = w;
        bInfo.bmiHeader.biHeight      = -h;
        bInfo.bmiHeader.biPlanes      = 1;
        bInfo.bmiHeader.biBitCount    = 24;
        bInfo.bmiHeader.biCompression = BI_RGB;
        bInfo.bmiHeader.biSizeImage   = w * h * 3;
        if (dataRGB)
            delete[] dataRGB;
        dataRGB = nullptr;
        dataRGB = new unsigned char[bInfo.bmiHeader.biSizeImage + 10];
        for (DWORD y = 0; y < bInfo.bmiHeader.biSizeImage; y += 3)
        {
            dataRGB[y]     = dataImage[y];     // azul
            dataRGB[y + 1] = dataImage[y + 1]; // verde
            dataRGB[y + 2] = dataImage[y + 2]; // vermelho
        }
        int siz = SetDIBits(hdc, data, 0, h, dataRGB, &bInfo, DIB_RGB_COLORS);
        if (siz)
        {
            GetObject(data, sizeof(BITMAP), &bitmapInfo);
            if (hdc)
                DeleteDC(hdc);
            return true;
        }
        if (hdc)
            DeleteDC(hdc);
        return false;
    }

    bool BMP::updateData()
    {
        if (dataRGB && bitmapInfo.bmHeight)
        {
            HDC hdcscreen = GetDC(0);
            HDC hdc       = CreateCompatibleDC(hdcscreen);
            ReleaseDC(0, hdcscreen);
            int h   = bitmapInfo.bmHeight;
            int siz = SetDIBits(hdc, data, 0, h, dataRGB, &bInfo, DIB_RGB_COLORS);
            if (siz)
            {
                GetObject(data, sizeof(BITMAP), &bitmapInfo);
                if (hdc)
                    DeleteDC(hdc);
                return true;
            }
            if (hdc)
                DeleteDC(hdc);
        }
        return false;
    }

    HBITMAP BMP::getHBitmap() const
    {
        return data;
    }

    BITMAP * BMP::getBitmapInfo()
    {
        return &bitmapInfo;
    }

    unsigned char * BMP::getData() const // Retorna a data RGB (Ordem BGR)
    {
        return dataRGB;
    }

    unsigned int BMP::getAsUintFromCharPointer(unsigned char *adress)
    {
        unsigned int x;
        x = adress[3];
        x <<= 8;
        x |= adress[2];
        x <<= 8;
        x |= adress[1];
        x <<= 8;
        x |= adress[0];
        return x;
    }
    
    DATA_EVENT::DATA_EVENT() : type(mbm::WINPLUS_TYPE_NONE), myString(nullptr)
    {
        memset(_ret, 0, sizeof(_ret));
        idComponent = -1;
        data        = nullptr;
        userDrawer    = nullptr;
    }

    DATA_EVENT::DATA_EVENT(int idComponent_, void *Data, USER_DRAWER *UserDrawer, const TYPE_WINDOWS_WINPLUS type_, const char *_myString)
        : type(type_), myString(_myString)
    {
        memset(_ret, 0, sizeof(_ret));
        idComponent = idComponent_;
        data        = Data;
        userDrawer    = UserDrawer;
    }

    const int DATA_EVENT::getAsInt()
    {
        return this->getInt();
    }

    const float DATA_EVENT::getAsFloat()
    {
        return this->getFloat();
    }

    const bool DATA_EVENT::getAsBool()
    {
        return this->getBool();
    }

    const char * DATA_EVENT::getAsString()
    {
        return this->getString();
    }

    TIMER * DATA_EVENT::getAsTimer()
    {
        if (this->type == mbm::WINPLUS_TYPE_TIMER)
            return static_cast<TIMER *>(data);
        return nullptr;
    }

    TRACK_BAR_INFO * DATA_EVENT::getAsTrackBar()
    {
        if (this->type == mbm::WINPLUS_TYPE_TRACK_BAR)
            return static_cast<TRACK_BAR_INFO *>(data);
        return nullptr;
    }

    RADIO_GROUP * DATA_EVENT::getAsRadio()
    {
        if (this->type == mbm::WINPLUS_TYPE_RADIO_BOX)
            return static_cast<RADIO_GROUP *>(data);
        return nullptr;
    }

    SPIN_PARAMSi * DATA_EVENT::getAsSpin()
    {
        if (this->type == mbm::WINPLUS_TYPE_SPIN_INT)
            return static_cast<SPIN_PARAMSi *>(data);
        return nullptr;
    }

    SPIN_PARAMSf * DATA_EVENT::getAsSpinf()
    {
        if (this->type == mbm::WINPLUS_TYPE_SPIN_FLOAT)
            return static_cast<SPIN_PARAMSf *>(data);
        return nullptr;
    }

    const bool isNumeric(const char letter)
{
    if ((letter >= '0' && letter <= '9'))
        return true;
    return false;
}

    bool isNum(const char *numberAsString)
{
    if ((numberAsString[0] && !isNumeric(numberAsString[0]) &&
         (numberAsString[0] != '.' && numberAsString[0] != ',' && numberAsString[0] != '-' && numberAsString[0] != '+')))
        return false;
    for (int i = 1; numberAsString[i]; ++i)
    {
        if (!isNumeric(numberAsString[i]) && (numberAsString[i] != '.' && numberAsString[i] != ','))
            return false;
    }
    return true;
}

    bool isNum(const WCHAR *numberAsString)
{
    if ((numberAsString[0] && !(numberAsString[0] >= '0' && numberAsString[0] <= '9') &&
         (numberAsString[0] != '.' && numberAsString[0] != ',' && numberAsString[0] != '-' && numberAsString[0] != '+')))
        return false;
    for (int i = 1; numberAsString[i]; ++i)
    {
        if (!(numberAsString[i] >= '0' && numberAsString[i] <= '9') &&
            (numberAsString[i] != '.' && numberAsString[i] != ','))
            return false;
    }
    return true;
}

    char *trimRight(char *stringSource)
{
    char * stringPtr       = stringSource + strlen(stringSource) - 1;
    while ((stringPtr >= stringSource) && (*stringPtr == ' '))
    {
        *stringPtr-- = 0;
    }
    return stringSource;
}

    char *trimLeft(char *stringSource)
{
    char * stringPtr       = stringSource;
    while (*stringPtr == ' ')
    {
        (void)*stringPtr++;
    }
    return stringPtr;
}

    char *trim(char *stringSource)
{
    return trimRight(trimLeft(stringSource));
}

    MONITOR::MONITOR()
    {
        width      = 0;
        height     = 0;
        position.x = 0;
        position.y = 0;
        frequency  = 0;
        index      = 0;
        isPrimary  = false;
    }

    MONITOR_MANAGER::MONITOR_MANAGER()
    {
        __initRandomSeed();
    }

    MONITOR_MANAGER::~MONITOR_MANAGER()
    {
    }

    void MONITOR_MANAGER::updateMonitors()
    {
        DISPLAY_DEVICE dd;
        dd.cb          = sizeof(dd);
        DWORD   dev    = 0;
        DWORD   devMon = 0;
        DEVMODE devMod;
        memset(&devMod, 0, sizeof(DEVMODE));
        DWORD totalMonitors = 0;
        lsMonitors.clear();
        while (EnumDisplayDevices(0, dev, &dd, 0))
        {
            DISPLAY_DEVICE ddMon;
            MONITOR        monitor;
            ZeroMemory(&ddMon, sizeof(ddMon));
            ddMon.cb               = sizeof(ddMon);
            bool isDisplayPrimary  = false;
            bool displaySettingsOK = false;

            while (EnumDisplayDevices(dd.DeviceName, devMon, &ddMon, 0))
            {
                if (ddMon.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP &&
                    !(ddMon.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
                {
                    if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &devMod))
                    {
                        isDisplayPrimary  = true;
                        displaySettingsOK = true;
                        totalMonitors++;
                    }
                }
                devMon++;

                ZeroMemory(&ddMon, sizeof(ddMon));
                ddMon.cb = sizeof(ddMon);
            }
            if (!isDisplayPrimary && dd.DeviceName[0])
            {
                if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &devMod))
                {
                    displaySettingsOK = true;
                    totalMonitors++;
                }
            }
            if (displaySettingsOK)
            {
                monitor.frequency  = devMod.dmDisplayFrequency;
                monitor.height     = devMod.dmPelsHeight;
                monitor.width      = devMod.dmPelsWidth;
                monitor.position.x = devMod.dmPosition.x;
                monitor.position.y = devMod.dmPosition.y;
                monitor.isPrimary  = isDisplayPrimary;
                lsMonitors.push_back(monitor);
                lsMonitors[lsMonitors.size() - 1].index = lsMonitors.size() - 1;
            }
            ZeroMemory(&dd, sizeof(dd));
            dd.cb = sizeof(dd);
            dev++;
        }
    }

    long MONITOR_MANAGER::getWidthWindow(const DWORD indexMonitor)
    {
        if (indexMonitor < lsMonitors.size())
            return lsMonitors[indexMonitor].width;
        return 0;
    }

    long MONITOR_MANAGER::getHeightWindow(const DWORD indexMonitor)
    {
        if (indexMonitor < lsMonitors.size())
            return lsMonitors[indexMonitor].height;
        return 0;
    }

    POINT MONITOR_MANAGER::getPositionWindow(const DWORD indexMonitor)
    {
        if (indexMonitor < lsMonitors.size())
            return lsMonitors[indexMonitor].position;
        POINT p;
        p.x = 0;
        p.y = 0;
        return p;
    }

    DWORD MONITOR_MANAGER::getIndexMainMonitor()
    {
        const DWORD s = lsMonitors.size();
        for (DWORD i = 0; i < s; ++i)
        {
            if (lsMonitors[i].isPrimary)
                return i;
        }
        return 0;
    }

    bool MONITOR_MANAGER::getMonitor(const DWORD indexMonitor, mbm::MONITOR *monitorOut)
    {
        if (indexMonitor < lsMonitors.size() && monitorOut)
        {
            *monitorOut = lsMonitors[indexMonitor];
            return true;
        }
        return false;
    }

    bool MONITOR_MANAGER::isMainMonitor(const DWORD indexMonitor)
    {
        if (indexMonitor < lsMonitors.size())
            return lsMonitors[indexMonitor].isPrimary;
        return false;
    }

    DWORD MONITOR_MANAGER::getTotalMonitor()
    {
        return lsMonitors.size();
    }

    SPIN_PARAMSi::SPIN_PARAMSi(int minValue, int maxValue, int increment_, int currentPosition_)
    {
        min             = minValue;
        max             = maxValue;
        increment       = increment_;
        currentPosition = currentPosition_;
        if (currentPosition < min)
            currentPosition = min;
        if (currentPosition > max)
            currentPosition = max;
    }

    SPIN_PARAMSf::SPIN_PARAMSf(float minValue, float maxValue, float increment_, float currentPosition_, int precision_)
    {
        minf            = minValue;
        maxf            = maxValue;
        increment       = increment_;
        currentPosition = currentPosition_;
        precision       = precision_;
        if (currentPosition < minf)
            currentPosition = minf;
        if (currentPosition > maxf)
            currentPosition = maxf;
    }

    RADIO_GROUP::RADIO_GROUP(const int id, const int _idParent) : idRadio(id), idParent(_idParent)
    {
        checked = false;
    }

    RADIO_GROUP::~RADIO_GROUP()
    {
    }

    TIMER::TIMER(int timeElapsed_inMiliSeconds, int idTimer_, OnEventWinPlus onEventTimer_)
    {
        timInMilisecond = timeElapsed_inMiliSeconds;
        times           = 0;
        idTimer         = idTimer_;
        onEventTimer    = onEventTimer_;
    }

    EDIT_TEXT_DATA::EDIT_TEXT_DATA(const int _id) : id(_id)
    {
        spin  = nullptr;
        spinf = nullptr;
        text  = (char *)malloc(4); // int for message proc
        if(text)
            memset(text, 0, 4);
        len = 4;
    }

    EDIT_TEXT_DATA::EDIT_TEXT_DATA(mbm::SPIN_PARAMSi *_spin, mbm::SPIN_PARAMSf *_spinf, const int _id) : id(_id)
    {
        spin  = _spin;
        spinf = _spinf;
        text  = (char *)malloc(4); // int for message proc
        if(text)
            memset(text, 0, 4);
        len = 4;
    }

    EDIT_TEXT_DATA::~EDIT_TEXT_DATA()
    {
        if (text)
            free(text);
        text = nullptr;
    }

    int COM_BETWEEN_WINP::getId() const
    {
        return id;
    }

    mbm::WINDOW * COM_BETWEEN_WINP::getWindow()
    {
        return ptrWindow;
    }

    TYPE_WINDOWS_WINPLUS COM_BETWEEN_WINP::getType()
    {
        return this->typeWindowWinPlus;
    }

    HWND COM_BETWEEN_WINP::getHwnd()
    {
        return this->hwnd;
    }

    COM_BETWEEN_WINP::COM_BETWEEN_WINP(HWND owerHwnd_, OnEventWinPlus onEventWinPlus, WINDOW *win, TYPE_WINDOWS_WINPLUS typeMe,void *extraParams_, const int idOwner_, USER_DRAWER *UserDrawer)
        : id(COM_BETWEEN_WINP::lsComBetweenWinp.size())
    {
        this->hwnd = nullptr;
        COM_BETWEEN_WINP::lsComBetweenWinp.push_back(this);
        this->onEventWinPlus    = onEventWinPlus;
        this->owerHwnd          = owerHwnd_;
        this->ptrWindow         = win;
        this->typeWindowWinPlus = typeMe;
        this->extraParams       = extraParams_;
        this->idOwner           = idOwner_;
        this->idNextFocus       = -1;
        this->graphWin          = nullptr;
        this->_oldProc          = nullptr;
        this->userDrawer        = UserDrawer;
    }

    COM_BETWEEN_WINP::COM_BETWEEN_WINP(COM_BETWEEN_WINP *ncCopy) : id(ncCopy->id)
    {
        this->hwnd              = ncCopy->hwnd;
        this->onEventWinPlus    = ncCopy->onEventWinPlus;
        this->owerHwnd          = ncCopy->owerHwnd;
        this->ptrWindow         = ncCopy->ptrWindow;
        this->typeWindowWinPlus = ncCopy->typeWindowWinPlus;
        this->extraParams       = ncCopy->extraParams;
        this->idOwner           = ncCopy->idOwner;
        this->idNextFocus       = ncCopy->idNextFocus;
        this->graphWin          = ncCopy->graphWin;
        this->_oldProc          = ncCopy->_oldProc;
        this->userDrawer          = ncCopy->userDrawer;
    }

    COM_BETWEEN_WINP::~COM_BETWEEN_WINP()
    {
        if (this->_oldProc)
            SetWindowLong(this->hwnd, GWL_WNDPROC, long(this->_oldProc));
        switch (this->typeWindowWinPlus)
        {
            case WINPLUS_TYPE_WINDOWNC:
            {
                __NC_BORDERS::__NC_BUTTONS *nc = static_cast<__NC_BORDERS::__NC_BUTTONS *>(this->extraParams);
                if (nc)
                    delete nc;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_IMAGE:
            case WINPLUS_TYPE_WINDOW_MESSAGE_BOX:
            case WINPLUS_TYPE_WINDOW:
            case WINPLUS_TYPE_LABEL: {
            }
            break;
            case WINPLUS_TYPE_BUTTON: { this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_BUTTON_TAB:
            {
                __TAB_GROUP_DESC *thisTab = static_cast<__TAB_GROUP_DESC *>(this->extraParams);
                if (thisTab)
                    delete thisTab;
                thisTab           = nullptr;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_CHECK_BOX:
            {
                int *value = static_cast<int *>(this->extraParams);
                if (value)
                    delete value;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_RADIO_BOX:
            {
                RADIO_GROUP *radio = static_cast<RADIO_GROUP *>(this->extraParams);
                if (radio)
                    delete radio;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_COMBO_BOX:
            {
                std::vector<std::string *> *lsCombo = static_cast<std::vector<std::string *> *>(this->extraParams);
                if (lsCombo)
                {
                    for (unsigned int i = 0; i < lsCombo->size(); ++i)
                    {
                        std::string *strList = lsCombo->at(i);
                        delete strList;
                    }
                    delete lsCombo;
                }
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_LIST_BOX:
            {
                std::vector<std::string *> *lsListBox = static_cast<std::vector<std::string *> *>(this->extraParams);
                if (lsListBox)
                {
                    for (unsigned int i = 0; i < lsListBox->size(); ++i)
                    {
                        std::string *strList = lsListBox->at(i);
                        delete strList;
                    }
                    delete lsListBox;
                }
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_TEXT_BOX:
            {
                EDIT_TEXT_DATA *textData = static_cast<EDIT_TEXT_DATA *>(this->extraParams);
                if (textData)
                    delete textData;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_SCROLL: {
            }
            break;
            case WINPLUS_TYPE_SPIN_INT:
            {
                mbm::SPIN_PARAMSi *spin = static_cast<mbm::SPIN_PARAMSi *>(this->extraParams);
                if (spin)
                    delete spin;
                spin                 = nullptr;
                this->extraParams    = nullptr;
                const unsigned int i = this->id + 1;
                // Deleta o TextBox....
                COM_BETWEEN_WINP *ptr =
                    i < COM_BETWEEN_WINP::lsComBetweenWinp.size() ? COM_BETWEEN_WINP::lsComBetweenWinp[i] : nullptr;
                if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TEXT_BOX)
                {
                    delete ptr;
                    ptr                                   = nullptr;
                    COM_BETWEEN_WINP::lsComBetweenWinp[i] = nullptr;
                }
            }
            break;
            case WINPLUS_TYPE_SPIN_FLOAT:
            {
                mbm::SPIN_PARAMSf *spin = static_cast<mbm::SPIN_PARAMSf *>(this->extraParams);
                if (spin)
                    delete spin;
                spin                 = nullptr;
                this->extraParams    = nullptr;
                const unsigned int i = this->id + 1;
                // Deleta o TextBox....
                COM_BETWEEN_WINP *ptr =
                    i < COM_BETWEEN_WINP::lsComBetweenWinp.size() ? COM_BETWEEN_WINP::lsComBetweenWinp[i] : nullptr;
                if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TEXT_BOX)
                {
                    delete ptr;
                    ptr                                   = nullptr;
                    COM_BETWEEN_WINP::lsComBetweenWinp[i] = nullptr;
                }
            }
            break;
            case WINPLUS_TYPE_RICH_TEXT: { this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_CHILD_WINDOW: {
            }
            break;
            case WINPLUS_TYPE_GROUP_BOX: {
            }
            break;
            case WINPLUS_TYPE_PROGRESS_BAR:
            {
                PROGRESS_BAR_INFO *infoProgress = static_cast<PROGRESS_BAR_INFO *>(this->extraParams);
                if (infoProgress)
                    delete infoProgress;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_TIMER:
            {
                TIMER *timeElapsed = static_cast<TIMER *>(this->extraParams);
                if (timeElapsed)
                    delete timeElapsed;
                timeElapsed       = nullptr;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_TRACK_BAR:
            {
                TRACK_BAR_INFO *infoTrack = static_cast<TRACK_BAR_INFO *>(this->extraParams);
                if (infoTrack)
                    delete infoTrack;
                infoTrack         = nullptr;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_STATUS_BAR:
            {
                std::vector<std::string> *lsStatusBar = static_cast<std::vector<std::string> *>(this->extraParams);
                if (lsStatusBar)
                    delete lsStatusBar;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_MENU:
            {
                __destroyMenu(this->extraParams);
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_SUB_MENU: {
            }
            break;
            case WINPLUS_TYPE_GROUP_BOX_TAB: {
            }
            break;
            case WINPLUS_TYPE_TRY_ICON_MENU:
            {
                HMENU systray_menu = static_cast<HMENU>(this->extraParams);
                if (systray_menu)
                    DestroyMenu(systray_menu);
                systray_menu      = nullptr;
                this->extraParams = nullptr;
            }
            break;
            case WINPLUS_TYPE_TRY_ICON_SUB_MENU: {
            }
            break;
            case WINPLUS_TYPE_TOOL_TIP: {
            }
            break;
        }
    }

    COM_BETWEEN_WINP * getComBetweenWinp(const int id)
{
    if (id == -1)
    {
        if (COM_BETWEEN_WINP::lsComBetweenWinp.size())
        {
            for (unsigned int i = 0; i < COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
            {
                COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
                if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW)
                    return ptr;
            }
        }
        return nullptr;
    }
    else
    {
        if ((unsigned int)id < COM_BETWEEN_WINP::lsComBetweenWinp.size())
            return COM_BETWEEN_WINP::lsComBetweenWinp[id];
        return nullptr;
    }
}

    COM_BETWEEN_WINP * getComBetweenWinp(const HWND owerHwnd, const int id)
{
    for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
    {
        COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
        if (ptr && ptr->getId() == id && owerHwnd == ptr->owerHwnd)
        {
            return ptr;
        }
    }
    return nullptr;
}

    COM_BETWEEN_WINP * getComBetweenWinp(const HWND hwnd)
{
    if (hwnd == nullptr &&
        COM_BETWEEN_WINP::lsComBetweenWinp.size()) // probaly we want the next window avaliable (the last one)
    {
        for (int i = COM_BETWEEN_WINP::lsComBetweenWinp.size() - 1; i > 0; --i)
        {
            COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (ptr && ptr->hwnd && ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_WINDOW)
            {
                return ptr;
            }
        }
    }
    for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
    {
        COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
        if (ptr && ptr->hwnd == hwnd)
        {
            return ptr;
        }
    }
    return nullptr;
}

    COM_BETWEEN_WINP * getComBetweenWinpTryIcon(const HWND owerHwnd)
    {
        for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
        {
            COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if ((ptr && ptr->owerHwnd == owerHwnd && ((ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_TRY_ICON_MENU) ||
                                                      (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_TRY_ICON_SUB_MENU))))
            {
                return ptr;
            }
        }
        return nullptr;
    }

    void destroyListComBetweenWindows(HWND hwnd)
    {
        for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
        {
            COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (ptr && (ptr->hwnd == hwnd || ptr->owerHwnd == hwnd))
            {
                delete ptr;
                COM_BETWEEN_WINP::lsComBetweenWinp[i] = nullptr;
            }
        }
    }

    void destroyAlTimers(HWND hwnd)
    {
        for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
        {
            COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (ptr && (ptr->hwnd == nullptr || ptr->owerHwnd == hwnd))
            {
                if (ptr->typeWindowWinPlus == WINPLUS_TYPE_TIMER)
                {
                    KillTimer(ptr->owerHwnd, ptr->id);
                    delete ptr;
                    ptr                                   = nullptr;
                    COM_BETWEEN_WINP::lsComBetweenWinp[i] = nullptr;
                }
            }
        }
    }

    void destroyTimer(HWND hwnd, const int idTimer)
{
    for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
    {
        COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
        if ((ptr && ptr->hwnd == nullptr) || (ptr && ptr->owerHwnd == hwnd))
        {
            if (ptr->typeWindowWinPlus == WINPLUS_TYPE_TIMER && idTimer == ptr->id)
            {
                KillTimer(ptr->owerHwnd, ptr->id);
                delete ptr;
                ptr                                   = nullptr;
                COM_BETWEEN_WINP::lsComBetweenWinp[i] = nullptr;
                break;
            }
        }
    }
}

    USER_DRAWER::USER_DRAWER(void *That, DRAW *Draw)
    {
        enableHover       = true;
        enablePressed     = true;
        that              = That;
        draw              = Draw;
    }

    USER_DATA::USER_DATA()
    {
    }

    USER_DATA::USER_DATA(void* newData):USER_DRAWER(newData,nullptr)
    {   
    }

    USER_DATA::USER_DATA(void* newData,DRAW *Draw):USER_DRAWER(newData,Draw)
    {   
    }

    bool      USER_DATA::render(COMPONENT_INFO & ) 
    { 
        return false;
    }
    void USER_DATA::setData(void* newData)
    {
        this->that = newData;
    }
    void* USER_DATA::getData()
    {
        return this->that;
    }

    COMPONENT_INFO::COMPONENT_INFO(COM_BETWEEN_WINP *ptr, const LPDRAWITEMSTRUCT _lpdis, const HDC validHDC, const bool _isHover,const bool _isPressed, USER_DRAWER *UserDrawer)
        : onEventWinPlus(ptr->onEventWinPlus), hwnd(ptr->hwnd), owerHwnd(ptr->owerHwnd),
          typeWindowWinPlus(ptr->typeWindowWinPlus), id(ptr->id), idOwner(ptr->idOwner), idNextFocus(ptr->idNextFocus),
          lpdis(_lpdis), isHover(_isHover), hdc(validHDC), isPressed(_isPressed), userDrawer(UserDrawer)
    {
        this->ptrWindow   = ptr->ptrWindow;
        this->extraParams = ptr->extraParams;
        this->rect        = _lpdis->rcItem;
        if (this->userDrawer)
            this->userDrawer->draw = ptr->graphWin;
        COMPONENT_INFO::setCursorPos(this);
    }

    COMPONENT_INFO::COMPONENT_INFO(COM_BETWEEN_WINP *ptr, const bool _isHover, const bool _isPressed, USER_DRAWER *UserDrawer)
        : onEventWinPlus(ptr->onEventWinPlus), hwnd(ptr->hwnd), owerHwnd(ptr->owerHwnd),
          typeWindowWinPlus(ptr->typeWindowWinPlus), id(ptr->id), idOwner(ptr->idOwner), idNextFocus(ptr->idNextFocus),
          lpdis(nullptr), isHover(_isHover), hdc(nullptr), isPressed(_isPressed), userDrawer(UserDrawer)
    {
        this->ptrWindow   = ptr->ptrWindow;
        this->extraParams = ptr->extraParams;
        if (this->userDrawer)
            this->userDrawer->draw = ptr->graphWin;
        memset(&this->rect, 0, sizeof(this->rect));
        COMPONENT_INFO::setCursorPos(this);
    }

    COMPONENT_INFO::COMPONENT_INFO(COM_BETWEEN_WINP *ptr, PAINTSTRUCT *ps, const HDC validHDC, const bool _isHover, const bool _isPressed,USER_DRAWER *UserDrawer)
        : onEventWinPlus(ptr->onEventWinPlus), hwnd(ptr->hwnd), owerHwnd(ptr->owerHwnd),
          typeWindowWinPlus(ptr->typeWindowWinPlus), id(ptr->id), idOwner(ptr->idOwner), idNextFocus(ptr->idNextFocus),
          lpdis(nullptr), isHover(_isHover), hdc(validHDC), isPressed(_isPressed), userDrawer(UserDrawer)
    {
        this->ptrWindow   = ptr->ptrWindow;
        this->extraParams = ptr->extraParams;
        this->rect        = ps->rcPaint;
        if (this->userDrawer)
            this->userDrawer->draw = ptr->graphWin;
        COMPONENT_INFO::setCursorPos(this);
    }

    COMPONENT_INFO::COMPONENT_INFO(COM_BETWEEN_WINP *ptr, RECT *rect, const HDC validHDC, const bool _isHover, const bool _isPressed,USER_DRAWER *UserDrawer)
        : onEventWinPlus(ptr->onEventWinPlus), hwnd(ptr->hwnd), owerHwnd(ptr->owerHwnd),
          typeWindowWinPlus(ptr->typeWindowWinPlus), id(ptr->id), idOwner(ptr->idOwner), idNextFocus(ptr->idNextFocus),
          lpdis(nullptr), isHover(_isHover), hdc(validHDC), isPressed(_isPressed), userDrawer(UserDrawer)
    {
        this->ptrWindow   = ptr->ptrWindow;
        this->extraParams = ptr->extraParams;
        if (this->userDrawer)
            this->userDrawer->draw = ptr->graphWin;
        this->rect               = *rect;
        COMPONENT_INFO::setCursorPos(this);
    }

    COMPONENT_INFO::~COMPONENT_INFO(){};

    void COMPONENT_INFO::setCursorPos(COMPONENT_INFO * that)
    {
        that->ptrWindow->getCursorPos(&that->mouse);
        MapWindowPoints(HWND_DESKTOP, that->hwnd, &that->mouse, 1);
    }

    DRAW::COLOR::COLOR()
        {
            red   = 255;
            green = 255;
            blue  = 255;
        }

    DRAW::COLOR::COLOR(const COLORREF &c)
        {
            this->red   = GetRValue(c);
            this->green = GetGValue(c);
            this->blue  = GetBValue(c);
        }

    DRAW::COLOR::COLOR(const unsigned char r, const unsigned char g, const unsigned char b)
        {
            red   = r;
            green = g;
            blue  = b;
        }
        
    DRAW::DRAW()
    {
        hdcBack             = nullptr;
        brush               = nullptr;
        penColor            = nullptr;
        font                = nullptr;
        stylePen            = PS_SOLID;
        infoActualComponent = nullptr;
        myPtrBrush          = nullptr;
        myPtrPen            = nullptr;
        myPtrFont           = nullptr;
        dwRop               = SRCCOPY;
        useTranparency      = false;
        hBrushBackGround    = nullptr;
        colorKeying         = RGB(0, 0, 0);
    }

    DRAW::DRAW(mbm::COMPONENT_INFO *component)
    {
        brush               = nullptr;
        penColor            = nullptr;
        font                = nullptr;
        stylePen            = PS_SOLID;
        infoActualComponent = component;
        this->hdcBack       = component ? component->hdc : nullptr;
        myPtrBrush          = nullptr;
        myPtrPen            = nullptr;
        myPtrFont           = nullptr;
        dwRop               = SRCCOPY;
        useTranparency      = false;
        hBrushBackGround    = nullptr;
        colorKeying         = RGB(0, 0, 0);
    }

    DRAW::~DRAW()
    {
        release();
    }

    HBRUSH DRAW::createBrush(unsigned char r, unsigned char g, unsigned char b)
    {
        HBRUSH hbrush = CreateSolidBrush(RGB(r, g, b));
        return hbrush;
    }

    void DRAW::release(HBRUSH &hbrush)
    {
        if(hbrush)
            DeleteObject(hbrush);
        hbrush = 0;
    }

    void DRAW::release(HPEN &hpen)
    {
        if(hpen)
            DeleteObject(hpen);
        hpen = 0;
    }

    HPEN DRAW::createPen(unsigned char r, unsigned char g, unsigned char b, int _stylePen, int width)
    {
        HPEN hpen = CreatePen(_stylePen, width, RGB(r, g, b));
        return hpen;
    }

    HPEN DRAW::createPen(COLORREF color)
    {
        HPEN hpen = CreatePen(PS_SOLID, 0, color);
        return hpen;
    }

    HBRUSH DRAW::createGradientBrush(COLORREF fromColor, COLORREF toColor, const RECT &rc, const bool horizontal,const bool power2, const bool reflected)
    {
        if (fromColor == toColor)
        {
            HBRUSH solidBrush = CreateSolidBrush(fromColor);
            return solidBrush;
        }
        if (this->hdcBack == nullptr)
            return nullptr;
        HBRUSH    tmpBrush = nullptr;
        HDC       hdcmem   = CreateCompatibleDC(this->hdcBack);
        const int width    = power2 ? (rc.right - rc.left) + ((rc.right - rc.left) % 4) + 4 : (rc.right - rc.left);
        const int height   = power2 ? (rc.bottom - rc.top) + ((rc.bottom - rc.top) % 4) + 4 : (rc.bottom - rc.top);

        HBITMAP hbitmap = CreateCompatibleBitmap(this->hdcBack, width, height);
        if (hbitmap == nullptr)
            return nullptr;
        HGDIOBJ original = SelectObject(hdcmem, hbitmap);

        const int r1 = GetRValue(fromColor);
        const int r2 = GetRValue(toColor);
        const int g1 = GetGValue(fromColor);
        const int g2 = GetGValue(toColor);
        const int b1 = GetBValue(fromColor);
        const int b2 = GetBValue(toColor);
        if (horizontal)
        {
            if (reflected)
            {
                const int halfHeight = height / 2;
                for (int i = 0; i < halfHeight; ++i)
                {
                    RECT      temp;
                    const int r = int(r1 + double(i * (r2 - r1) / halfHeight));
                    const int g = int(g1 + double(i * (g2 - g1) / halfHeight));
                    const int b = int(b1 + double(i * (b2 - b1) / halfHeight));
                    tmpBrush    = CreateSolidBrush(RGB(r, g, b));
                    temp.left   = 0;
                    temp.right  = width;
                    temp.top    = i;
                    temp.bottom = i + 1;
                    FillRect(hdcmem, &temp, tmpBrush);
                    DeleteObject(tmpBrush);
                }
                for (int i = halfHeight; i < height; ++i)
                {
                    RECT      temp;
                    const int r = int(r1 + double((height - i) * (r2 - r1) / halfHeight));
                    const int g = int(g1 + double((height - i) * (g2 - g1) / halfHeight));
                    const int b = int(b1 + double((height - i) * (b2 - b1) / halfHeight));
                    tmpBrush    = CreateSolidBrush(RGB(r, g, b));
                    temp.left   = 0;
                    temp.right  = width;
                    temp.top    = i;
                    temp.bottom = i + 1;
                    FillRect(hdcmem, &temp, tmpBrush);
                    DeleteObject(tmpBrush);
                }
            }
            else
            {
                for (int i = 0; i < height; ++i)
                {
                    RECT      temp;
                    const int r = int(r1 + double(i * (r2 - r1) / height));
                    const int g = int(g1 + double(i * (g2 - g1) / height));
                    const int b = int(b1 + double(i * (b2 - b1) / height));
                    tmpBrush    = CreateSolidBrush(RGB(r, g, b));
                    temp.left   = 0;
                    temp.right  = width;
                    temp.top    = i;
                    temp.bottom = i + 1;
                    FillRect(hdcmem, &temp, tmpBrush);
                    DeleteObject(tmpBrush);
                }
            }
        }
        else
        {
            if (reflected)
            {
                const int halfWidth = width / 2;
                for (int i = 0; i < halfWidth; ++i)
                {
                    RECT      temp;
                    const int r = int(r1 + double(i * (r2 - r1) / halfWidth));
                    const int g = int(g1 + double(i * (g2 - g1) / halfWidth));
                    const int b = int(b1 + double(i * (b2 - b1) / halfWidth));
                    tmpBrush    = CreateSolidBrush(RGB(r, g, b));
                    temp.left   = i;
                    temp.right  = i + 1;
                    temp.top    = 0;
                    temp.bottom = height;
                    FillRect(hdcmem, &temp, tmpBrush);
                    DeleteObject(tmpBrush);
                }
                for (int i = halfWidth; i < width; ++i)
                {
                    RECT      temp;
                    const int r = int(r1 + double((width - i) * (r2 - r1) / halfWidth));
                    const int g = int(g1 + double((width - i) * (g2 - g1) / halfWidth));
                    const int b = int(b1 + double((width - i) * (b2 - b1) / halfWidth));
                    tmpBrush    = CreateSolidBrush(RGB(r, g, b));
                    temp.left   = i;
                    temp.right  = i + 1;
                    temp.top    = 0;
                    temp.bottom = height;
                    FillRect(hdcmem, &temp, tmpBrush);
                    DeleteObject(tmpBrush);
                }
            }
            else
            {
                for (int i = 0; i < width; ++i)
                {
                    RECT      temp;
                    const int r = int(r1 + double(i * (r2 - r1) / width));
                    const int g = int(g1 + double(i * (g2 - g1) / width));
                    const int b = int(b1 + double(i * (b2 - b1) / width));
                    tmpBrush    = CreateSolidBrush(RGB(r, g, b));
                    temp.left   = i;
                    temp.right  = i + 1;
                    temp.top    = 0;
                    temp.bottom = height;
                    FillRect(hdcmem, &temp, tmpBrush);
                    DeleteObject(tmpBrush);
                }
            }
        }
        HBRUSH pattern = CreatePatternBrush(hbitmap);
        SelectObject(hdcmem, original);
        DeleteDC(hdcmem);
        DeleteObject(tmpBrush);
        DeleteObject(hbitmap);
        return pattern;
    }

    void DRAW::drawLine(const int initialX, const int initialY, const int finalX, const int finalY)
    {
        MoveToEx(hdcBack, initialX, initialY, nullptr);
        LineTo(hdcBack, finalX, finalY);
        LineTo(hdcBack, initialX, initialY);
    }

    void DRAW::drawLine(const POINT &initialPoint, const POINT &finalPoint)
    {
        MoveToEx(hdcBack, initialPoint.x, initialPoint.y, nullptr);
        LineTo(hdcBack, finalPoint.x, finalPoint.y);
        LineTo(hdcBack, initialPoint.x, initialPoint.y);
    }

    void DRAW::drawRectangle(const RECT &REct)
    {
        Rectangle(hdcBack, REct.left, REct.top, REct.right, REct.bottom);
    }

    void DRAW::drawRectangle(const int x, const int y, const int w, const int h)
    {
        Rectangle(hdcBack, x, y, x + w, y + h);
    }

    void DRAW::drawCircle(const POINT &point, const int ray)
    {
        Ellipse(hdcBack, point.x - ray, point.y - ray, point.x + ray, point.y + ray);
    }

    void DRAW::drawCircle(const int initialX, const int initialY, const int ray)
    {
        Ellipse(hdcBack, initialX - ray, initialY - ray, initialX + ray, initialY + ray);
    }

    void DRAW::drawElipse(const RECT &rect)
    {
        Ellipse(hdcBack, rect.left, rect.top, rect.right, rect.bottom);
    }

    HDC DRAW::setHDC(HDC newHdc)
    {
        HDC old = hdcBack;
        hdcBack = newHdc;
        return old;
    }

    void DRAW::setFont(HFONT _hfont)
    {
        this->myPtrFont = _hfont;
    }

    HFONT DRAW::createFont(const char *pszFaceName, const int cHeight, const int cWidth,const int cEscapement, const int cOrientation, const int cWeight,const DWORD bItalic, const DWORD bUnderline, const DWORD bStrikeOut)
    {
        HFONT hFont = CreateFontA(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut,
                                  ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_ROMAN, (LPCSTR)pszFaceName);
        return hFont;
    }

    void DRAW::drawText(RECT *rect, const char *text, const bool bakgroundTransparente)
    {
        if (text)
        {
            HGDIOBJ original = nullptr;
            if (this->myPtrFont)
                original = SelectObject(hdcBack, this->myPtrFont);
            else
                selectFontColor(0, 0, 0);
            if (bakgroundTransparente)
                SetBkMode(hdcBack, TRANSPARENT);
            else
                SetBkMode(hdcBack, OPAQUE);
            this->_drawEndLineText(rect->left, rect->top, text);
            if (original)
                SelectObject(hdcBack, original);
        }
    }

    void DRAW::drawText(const int x, const int y, const char *text, const bool bakgroundTransparente)
    {
        if (text)
        {
            HGDIOBJ original = nullptr;
            if (this->myPtrFont)
                original = SelectObject(hdcBack, this->myPtrFont);
            else
                selectFontColor(0, 0, 0);
            if (bakgroundTransparente)
                SetBkMode(hdcBack, TRANSPARENT);
            else
                SetBkMode(hdcBack, OPAQUE);
            this->_drawEndLineText(x, y, text);
            if (original)
                SelectObject(hdcBack, original);
        }
    }

    void DRAW::drawTextRotated(const int x, const int y, HWND hwnd, const char *text, DWORD color_text, int angle,const bool bakgroundTransparente)
    {
        HFONT hfont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        if (hfont)
        {
            LOGFONT lFont;
            memset(&lFont, 0, sizeof(lFont));
            int ret = GetObject(hfont, sizeof(lFont), &lFont);
            if (ret == sizeof(lFont))
            {
                lFont.lfEscapement = (angle * 10);
                HFONT hfnt         = CreateFontIndirect(&lFont);
                if (hfnt)
                {
                    if (bakgroundTransparente)
                        SetBkMode(hdcBack, TRANSPARENT);
                    else
                        SetBkMode(hdcBack, OPAQUE);
                    HGDIOBJ hOld = SelectObject(this->hdcBack, hfnt);
                    SetTextColor(hdcBack, color_text);
                    this->_drawEndLineText(x, y, text);
                    DeleteObject(hfnt);
                    SelectObject(this->hdcBack, hOld);
                }
            }
        }
    }

    void DRAW::drawText(const int x, const int y, const DWORD color, const char *text,const bool bakgroundTransparente)
    {
        if (text)
        {
            HGDIOBJ original = nullptr;
            if (this->myPtrFont)
            {
                original = SelectObject(hdcBack, this->myPtrFont);
                SetTextColor(hdcBack, color);
            }
            else
                selectFontColor(color);
            if (bakgroundTransparente)
                SetBkMode(hdcBack, TRANSPARENT);
            else
                SetBkMode(hdcBack, OPAQUE);
            this->_drawEndLineText(x, y, text);
            if (original)
                SelectObject(hdcBack, original);
        }
    }

    void DRAW::drawText(const RECT &rect, const DWORD color, const char *text, const bool bakgroundTransparente)
    {
        if (text)
        {
            HGDIOBJ original = nullptr;
            if (this->myPtrFont)
            {
                original = SelectObject(hdcBack, this->myPtrFont);
                SetTextColor(hdcBack, color);
            }
            else
                selectFontColor(color);
            if (bakgroundTransparente)
                SetBkMode(hdcBack, TRANSPARENT);
            else
                SetBkMode(hdcBack, OPAQUE);
            this->_drawEndLineText(rect.left, rect.top, text);
            SelectObject(hdcBack, original);
        }
    }

    void DRAW::drawText(const int x, const int y, const unsigned char red, const unsigned char green,const unsigned char blue, const char *text, const bool bakgroundTransparente)
    {
        if (text)
        {
            HGDIOBJ original = nullptr;
            if (this->myPtrFont)
            {
                original = SelectObject(hdcBack, this->myPtrFont);
                SetTextColor(hdcBack, RGB(red, green, blue));
            }
            else
                selectFontColor(red, green, blue);
            if (bakgroundTransparente)
                SetBkMode(hdcBack, TRANSPARENT);
            else
                SetBkMode(hdcBack, OPAQUE);
            this->_drawEndLineText(x, y, text);
            if (original)
                SelectObject(hdcBack, original);
        }
    }

    void DRAW::drawText(const int x, const int y, const unsigned char red, const unsigned char green,
                         const unsigned char blue, const unsigned char redBack, const unsigned char greenBack,
                         const unsigned char blueBack, const char *text)
    {
        if (text)
        {
            HGDIOBJ original = nullptr;
            if (this->myPtrFont)
            {
                original = SelectObject(hdcBack, this->myPtrFont);
                SetTextColor(hdcBack, RGB(red, green, blue));
            }
            else
                selectFontColor(red, green, blue);
            COLORREF colorBack = RGB(redBack, greenBack, blueBack);
            SetBkColor(hdcBack, colorBack);
            this->_drawEndLineText(x, y, text);
            if (original)
                SelectObject(hdcBack, original);
        }
    }

    void DRAW::drawPoygon(const POINT *lpPoints, const int nCount)
    {
        Polygon(hdcBack, lpPoints, nCount);
    }

    void DRAW::drawRoundRect(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                              const int nWidth, const int nHeight)
    {
        RoundRect(hdcBack, nLeftRect, nTopRect, nRightRect, nBottomRect, nWidth, nHeight);
    }

    void DRAW::drawRoundRect(const RECT &rect, const int nWidth, const int nHeight)
    {
        RoundRect(hdcBack, rect.left, rect.top, rect.right, rect.bottom, nWidth, nHeight);
    }

    void DRAW::drawPie(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                        const int nXRadial1, const int nYRadial1, const int nXRadial2, const int nYRadial2)
    {
        Pie(hdcBack, nLeftRect, nTopRect, nRightRect, nBottomRect, nXRadial1, nYRadial1, nXRadial2, nYRadial2);
    }

    void DRAW::selectRect(const RECT &rect)
    {
        InvertRect(hdcBack, &rect);
    }

    void DRAW::drawFrameRect(const RECT &rect, HBRUSH brushColor)
    {
        FrameRect(hdcBack, &rect, brushColor);
    }

    void DRAW::drawChord(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                          const int nXRadial1, const int nYRadial1, const int nXRadial2, const int nYRadial2)
    {
        Chord(hdcBack, nLeftRect, nTopRect, nRightRect, nBottomRect, nXRadial1, nYRadial1, nXRadial2, nYRadial2);
    }

    void DRAW::drawArc(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                        const int nXStartArc, const int nYStartArc, const int nXEndArc, const int nYEndArc)
    {
        Arc(hdcBack, nLeftRect, nTopRect, nRightRect, nBottomRect, nXStartArc, nYStartArc, nXEndArc, nYEndArc);
    }
    void DRAW::drawArcTo(const int nLeftRect, const int nTopRect, const int nRightRect, const int nBottomRect,
                          const int nXStartArc, const int nYStartArc, const int nXEndArc, const int nYEndArc)
    {
        ArcTo(hdcBack, nLeftRect, nTopRect, nRightRect, nBottomRect, nXStartArc, nYStartArc, nXEndArc, nYEndArc);
    }
    void DRAW::drawEdge(RECT rect, unsigned int edge, unsigned int flags)
    {
        DrawEdge(hdcBack, &rect, edge, flags);
    }
    void DRAW::setArcDirection(const bool CLOCKWISE)
    {
        SetArcDirection(hdcBack, CLOCKWISE ? AD_CLOCKWISE : AD_COUNTERCLOCKWISE);
    }
    void DRAW::drawAngleArc(const int x, const int y, DWORD dwRadius, const float eStartAngle, const float eSweepAngle)
    {
        AngleArc(hdcBack, x, y, dwRadius, eStartAngle, eSweepAngle);
    }
    void DRAW::drawPolyBezier(const POINT *lppt, const DWORD cCount)
    {
        PolyBezier(hdcBack, lppt, cCount);
    }
    void DRAW::drawPolyBezierTo(const POINT *lppt, DWORD cCount)
    {
        PolyBezierTo(hdcBack, lppt, cCount);
    }
    void DRAW::setPenStyle(int style)
    {
        stylePen = style;
    }
    void DRAW::drawBmp(mbm::BMP &bmp, const int xPosition, const int yPosition)
    {
        bmp.draw(this->hdcBack, xPosition, yPosition);
    }
    void DRAW::drawBmp(mbm::BMP &bmp, const int xPosition, const int yPosition, const int xSource, const int ySource,
                        const int _width, const int _height)
    {
        bmp.draw(this->hdcBack, xPosition, yPosition, xSource, ySource, _width, _height);
    }
    SIZE DRAW::getSizeText(const char *text, HWND hwnd)
    {
        SIZE SizeRet = {0, 0};
        HDC  hdc     = GetDC(hwnd);
        if (hdc && text)
        {
            std::string myText(text);
            std::size_t pos = myText.find('\n');
            if (pos == std::string::npos)
            {
                int len = strlen(text);
                GetTextExtentPoint32A(hdc, text, len, &SizeRet);
            }
            else
            {
                int margin = LOWORD(SendMessageA(hwnd, EM_GETMARGINS, 0, 0));
                if (margin == 0)
                {
                    SIZE SizeMargin = {0, 0};
                    GetTextExtentPoint32A(hdc, "X", 1, &SizeMargin);
                    if (SizeMargin.cy)
                        margin = SizeMargin.cy / 8;
                    if (margin == 0)
                        margin = 2;
                }
                const int cx       = 4;
                int       cy       = 4;
                int       numLines = 1;
                do
                {
                    SIZE        Size        = {cx, cy};
                    std::string parcialText = myText.substr(0, pos);
                    GetTextExtentPoint32A(hdc, parcialText.c_str(), parcialText.size(), &Size);
                    myText = myText.substr(pos + 1);
                    pos    = myText.find('\n');
                    cy += Size.cy;
                    SizeRet.cx = Size.cx > SizeRet.cx ? Size.cx : SizeRet.cx;
                    SizeRet.cy += Size.cy;
                    numLines++;
                } while (pos != std::string::npos);
                if (myText.size())
                {
                    SIZE Size = {0, 0};
                    GetTextExtentPoint32A(hdc, myText.c_str(), myText.size(), &Size);
                    SizeRet.cx = Size.cx > SizeRet.cx ? Size.cx : SizeRet.cx;
                    SizeRet.cy += Size.cy;
                    numLines++;
                }
                SizeRet.cy += (margin * numLines);
            }
        }
        return SizeRet;
    }
    long DRAW::_drawSingleLine(std::string &parcialText, int cx, const int cy)
    {
        SIZE        Size = {cx, cy};
        std::size_t pos  = parcialText.find('\t');
        if (pos == std::string::npos)
        {
            const int   len = parcialText.size();
            const char *str = parcialText.c_str();
            TextOutA(hdcBack, cx, cy, str, len);
            GetTextExtentPoint32A(hdcBack, str, len, &Size);
            return Size.cy;
        }
        std::string              newString;
        std::vector<std::string> lsResult;
        split(lsResult, parcialText.c_str(), '\t');
        for (unsigned int i = 0; i < lsResult.size(); ++i)
        {
            newString += lsResult[i];
            if ((i + 1) < lsResult.size())
                newString += ' ';
        }
        return _drawSingleLine(newString, cx, cy);
    }
    void DRAW::_drawEndLineText(int x, int y, const char *text)
    {
        if (text)
        {
            std::string myText(text);
            std::size_t pos = myText.find('\n');
            if (pos == std::string::npos)
            {
                _drawSingleLine(myText, x, y);
            }
            else
            {
                std::vector<std::string> lsResult;
                split(lsResult, text, '\n');
                for (unsigned int i = 0; i < lsResult.size(); ++i)
                {
                    y += _drawSingleLine(lsResult[i], x, y);
                }
            }
        }
    }
    void DRAW::release()
    {
        if (penColor != nullptr)
            DeleteObject(penColor);
        if (hdcBack != nullptr)
            DeleteDC(hdcBack);
        if (font)
            DeleteObject(font);
        if (brush)
            DeleteObject(brush);
        if (hBrushBackGround)
            DeleteObject(hBrushBackGround);
        hBrushBackGround    = nullptr;
        brush               = nullptr;
        font                = nullptr;
        hdcBack             = nullptr;
        penColor            = nullptr;
        infoActualComponent = nullptr;
    }
    HGDIOBJ DRAW::selectFontColor(const unsigned char red, const unsigned char green, const unsigned char blue)
    {
        HGDIOBJ original = nullptr;
        if (this->myPtrPen)
        {
            original = SelectObject(hdcBack, this->myPtrPen);
            SetTextColor(hdcBack, RGB(red, green, blue));
        }
        else
        {
            if (this->font)
                DeleteObject(this->font);
            this->font = this->createFont();
            SetTextColor(hdcBack, RGB(red, green, blue));
            original = SelectObject(hdcBack, this->font);
        }
        return original;
    }
    HGDIOBJ DRAW::selectFontColor(const DWORD color)
    {
        HGDIOBJ original = nullptr;
        if (this->myPtrPen)
        {
            original = SelectObject(hdcBack, this->myPtrPen);
            SetTextColor(hdcBack, color);
        }
        else
        {
            if (this->font)
                DeleteObject(this->font);
            this->font = this->createFont();
            SetTextColor(hdcBack, color);
            original = SelectObject(hdcBack, this->font);
        }
        return original;
    }
    HGDIOBJ DRAW::setDefaultColor(const unsigned char red , const unsigned char green,
                                   const unsigned char blue )
    {
        this->selectPenColor(red, green, blue);
        HGDIOBJ original = this->selectBrushColor(red, green, blue);
        return original;
    }
    HGDIOBJ DRAW::selectPenColor(const unsigned char red, const unsigned char green, const unsigned char blue)
    {
        HGDIOBJ original = nullptr;
        if (this->myPtrPen)
            original = SelectObject(hdcBack, this->myPtrPen);
        else
        {
            if (penColor != nullptr)
                DeleteObject(penColor);
            penColor = CreatePen(stylePen, 0, RGB(red, green, blue));
            original = SelectObject(hdcBack, penColor);
        }
        return original;
    }
    HPEN DRAW::setPen(HPEN _myPen)
    {
        HPEN original  = nullptr;
        this->myPtrPen = _myPen;
        if (this->myPtrPen)
            original = (HPEN)SelectObject(hdcBack, this->myPtrPen);
        else
        {
            if (penColor != nullptr)
                DeleteObject(penColor);
            penColor = CreatePen(stylePen, 0, RGB(255, 255, 255));
            original = (HPEN)SelectObject(hdcBack, penColor);
        }
        return original;
    }
    HGDIOBJ DRAW::setBrush(HGDIOBJ oldBrush)
    {
        return SelectObject(hdcBack, oldBrush);
    }
    HGDIOBJ DRAW::setBrush(HBRUSH _myBrush)
    {
        HGDIOBJ original = nullptr;
        this->myPtrBrush = _myBrush;
        if (this->myPtrBrush)
            original = SelectObject(hdcBack, this->myPtrBrush);
        else
        {
            if (brush != nullptr)
                DeleteObject(brush);
            brush    = CreateSolidBrush(RGB(255, 255, 255));
            original = SelectObject(hdcBack, brush);
        }
        return original;
    }
    HGDIOBJ DRAW::selectBrushColor(const unsigned char red, const unsigned char green, const unsigned char blue)
    {
        HGDIOBJ original = nullptr;
        if (this->myPtrBrush)
            original = SelectObject(hdcBack, this->myPtrBrush);
        else
        {
            if (brush != nullptr)
                DeleteObject(brush);
            brush    = CreateSolidBrush(RGB(red, green, blue));
            original = SelectObject(hdcBack, brush);
        }
        return original;
    }
    bool DRAW::eraseBackGround(COMPONENT_INFO *)
    {
        return false;
    }

    int DRAW::measureItem(COM_BETWEEN_WINP *, MEASUREITEMSTRUCT *)
    {
        return 1;
    }
    void DRAW::setCtlColor(HDC hdcStatic)
    {
        SetBkColor(hdcStatic, RGB(255, 255, 255));
        SetTextColor(hdcStatic, RGB(0, 0, 0));
    }
    void DRAW::redrawWindow(HWND hwnd, BOOL eraseBck)
    {
        InvalidateRect(hwnd, nullptr, eraseBck);
    }
    COMPONENT_INFO * DRAW::getCurrentComponent()
    {
        return infoActualComponent;
    }
    void DRAW::present(HDC hdcDest, const int width, const int height)
    {
        if (useTranparency)
            TransparentBlt(hdcDest, 0, 0, width, height, hdcBack, 0, 0, width, height, colorKeying);
        else
            BitBlt(hdcDest, 0, 0, width, height, hdcBack, 0, 0, this->dwRop);
    }
    void DRAW::present(HDC hdcDest, const int x, const int y, const int width, const int height)
    {
        if (useTranparency)
            TransparentBlt(hdcDest, x, y, width, height, hdcBack, 0, 0, width, height, colorKeying);
        else
            BitBlt(hdcDest, x, y, width, height, hdcBack, 0, 0, this->dwRop);
    }
    void DRAW::doRenderBackBuffer(mbm::COM_BETWEEN_WINP *ptr, LPDRAWITEMSTRUCT lpdis, const bool isHover, const bool _isPressed)
    {
        if (lpdis)
        {
            const int      win_width  = lpdis->rcItem.right - lpdis->rcItem.left;
            const int      win_height = lpdis->rcItem.bottom - lpdis->rcItem.top;
            COMPONENT_INFO component(ptr, lpdis, lpdis->hDC, isHover, _isPressed,static_cast<USER_DRAWER *>(ptr->userDrawer));
            this->infoActualComponent = &component;
            this->hdcBack             = lpdis->hDC;
            this->render(component);
            this->present(lpdis->hDC, win_width, win_height);
        }
        else
        {
            PAINTSTRUCT    ps;
            HDC            hdc        = BeginPaint(ptr->hwnd, &ps);
            const int      win_width  = ps.rcPaint.right - ps.rcPaint.left;
            const int      win_height = ps.rcPaint.bottom - ps.rcPaint.top;
            HDC            hdcMem     = CreateCompatibleDC(hdc);
            HBITMAP        hbmMem     = CreateCompatibleBitmap(hdc, win_width, win_height);
            HANDLE         hOld       = SelectObject(hdcMem, hbmMem);
            COMPONENT_INFO component(ptr, &ps, hdcMem, isHover, _isPressed, static_cast<USER_DRAWER *>(ptr->userDrawer));
            this->infoActualComponent = &component;
            this->hdcBack             = hdcMem;
            this->render(component);
            this->present(ps.hdc, win_width, win_height);

            SelectObject(hdcMem, hOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(ptr->hwnd, &ps);
        }
        InvalidateChilds(ptr);
    }
    void DRAW::InvalidateChilds(COM_BETWEEN_WINP *ptr)
    {
        if(ptr->ptrWindow->isUsingDoubleBuffer() == false )
        {
            for (std::set<COM_BETWEEN_WINP *>::const_iterator it = ptr->myChilds.cbegin(); it != ptr->myChilds.cend(); ++it)
            {
                COM_BETWEEN_WINP *child = *it;
                InvalidateRect(child->hwnd, nullptr, 0);
                InvalidateChilds(child);
            }
        }
    }

    WINDOW::WINDOW()
    {
        static bool __atexit__ = true;
        if (__atexit__)
        {
            __atexit__ = false;
            atexit(__destroyOnExitAllListComBetweenWindows);
            INITCOMMONCONTROLSEX tagINIT;
            tagINIT.dwSize = sizeof(INITCOMMONCONTROLSEX);
            tagINIT.dwICC  = ICC_TAB_CLASSES;
            InitCommonControlsEx(&tagINIT);
        }
        usingDoubleBuffer        = false;
        preventMenuAlwaysShowing = false;
        dialogunitTabStopInPixel = 0;
        hwndLastTrackBar         = 0;
        doModalMode              = false;
        drawerDefault            = _winplusDefaultThemeDraw;
        idTimerHover             = -1;
        lastPosMouse.x           = 0;
        lastPosMouse.y           = 0;
        hwndLastHover            = nullptr;
        hwndLastPressed          = nullptr;
        hwnd                     = nullptr;
        iconApp                  = nullptr;
        adjustRectLeft           = 0;
        adjustRectTop            = 0;
        colorUnderling           = RGB(255, 255, 255);
        colorText                = RGB(0, 0, 0);
        colorTextTab             = RGB(255, 0, 0);
        sysTry_menu              = nullptr;
        callEventsManager        = nullptr;
        neverClose               = false;
        run                      = true;
        askOnExit                = true;
        hideOnExit               = false;
        exitOnEsc                = true;
        subMenu                  = 0;
        isWin32Initialized       = false;
        onScrollMouseEvent       = nullptr;
        onKeyboardDown           = nullptr;
        onKeyboardUp             = nullptr;
        onParseRawInput          = nullptr;
        onClickLeftMouse         = nullptr;
        onClickRightMouse        = nullptr;
        onClickMiddleMouse       = nullptr;
        onMouseMove              = nullptr;
        onReleaseLeftMouse       = nullptr;
        onReleaseRightMouse      = nullptr;
        onReleaseMiddleMouse     = nullptr;
        hasTryIcon               = false;
        isVisible                = false;
        hwndInsertAfter          = nullptr;
        memset(&tnid, 0, sizeof(tnid));
        tnid.cbSize    = sizeof(NOTIFYICONDATA);
        tnid.uID       = 1;
        CURRENT_CURSOR = WINPLUS_CURSOR_ARROW;
        min_size_width  = 0;
        min_size_height = 0;
        max_size_width  = 0;
        max_size_height = 0;
        strcpy(nameAplication, "WIN PLUS");
    }
    WINDOW::~WINDOW()
    {
        if (this->hasTryIcon)
        {
            this->hasTryIcon = false;
            Shell_NotifyIconA(NIM_DELETE, &tnid);
        }
        if (this->doModalMode)
        {
            this->doModalMode = false;
            Sleep(1000);
        }
        if (this->hwnd)
            DestroyWindow(this->hwnd);
        this->doEvents();
        if (this->hwnd)
            destroyListComBetweenWindows(this->hwnd);
    }
    bool WINDOW::init(mbm::MONITOR &monitor, const char *nameApp, const bool enableResize ,
                     const bool enableMaximizeButton , const bool enableMinimizeButton ,
                     const bool maximized , OnEventWinPlus onEventWinPlus, const bool withoutBorder,
                     DWORD ID_RESOURCE_ICON_APP,const bool doubleBuffer)
    {
        return init(nameApp, monitor.width, monitor.height, monitor.position.x, monitor.position.y, enableResize,
                    enableMaximizeButton, enableMinimizeButton, maximized, onEventWinPlus, withoutBorder,
                    ID_RESOURCE_ICON_APP,doubleBuffer);
    }

    bool WINDOW::init(const char *nameApp, const int width, const int height, const long positionX,
                     const long positionY, const bool enableResize, const bool enableMaximizeButton,
                     const bool enableMinimizeButton , const bool maximized,
                     OnEventWinPlus onEventWinPlus, const bool withoutBorder,
                     DWORD ID_RESOURCE_ICON_APP,const bool doubleBuffer)
    {
        if (isWin32Initialized)
            return true;
        if (nameApp)
            strcpy(nameAplication, nameApp);
#if UNICODE
        WCHAR *className = getNextClassNameWindow();
#else
        char *className = getNextClassNameWindow();
#endif
        this->hwnd = (HWND)GWL_HINSTANCE;
        WNDCLASSEX windowsClassLocal;
        RECT       WindowRect;
        WindowRect.left = (long)0;
        if (width)
            WindowRect.right = (long)width;
        else
            WindowRect.right = (long)GetSystemMetrics(SM_CXSCREEN);

        WindowRect.top = (long)0;
        if (height)
            WindowRect.bottom = (long)height;
        else
            WindowRect.bottom    = (long)GetSystemMetrics(SM_CYSCREEN);
        windowsClassLocal.cbSize = sizeof(windowsClassLocal);
        // windowsClassLocal.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS ;
        windowsClassLocal.style       = CS_DBLCLKS;
        windowsClassLocal.lpfnWndProc = WINDOW::WindowProc;
        windowsClassLocal.cbClsExtra  = 0;
        windowsClassLocal.cbWndExtra  = 0;
        windowsClassLocal.hInstance   = (HINSTANCE)GetWindowLongA(this->hwnd, GWL_HINSTANCE);
        if (ID_RESOURCE_ICON_APP != 0)
        {
            this->iconApp =
                (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(ID_RESOURCE_ICON_APP), IMAGE_ICON, 32, 32, 0);
            windowsClassLocal.hIcon = this->iconApp;
        }
        else
        {
            windowsClassLocal.hIcon = getIcon();
        }
        windowsClassLocal.hIconSm       = windowsClassLocal.hIcon;
        windowsClassLocal.hCursor       = LoadCursor(0, IDC_ARROW);
        windowsClassLocal.lpszMenuName  = 0;
        windowsClassLocal.lpszClassName = className;
        windowsClassLocal.hbrBackground = 0;

        if (!RegisterClassEx(&windowsClassLocal))
        {
            this->messageBox("Falha ao Registrar a windowsClassLocal!");
            return false;
        }

        int resizebleValue = 0;
        if (enableResize)
            resizebleValue = WS_THICKFRAME;
        int maxizeBox = 0, minimize = 0;
        if (enableMaximizeButton)
            maxizeBox = WS_MAXIMIZEBOX;
        if (enableMinimizeButton)
            minimize = WS_MINIMIZEBOX;
        DWORD dwExStyle, dwStyle;
        DWORD DB = doubleBuffer ? WS_EX_COMPOSITED : 0;//WS_EX_COMPOSITED = double-buffering
        this->usingDoubleBuffer = doubleBuffer;
        if (withoutBorder)
        {
            dwExStyle = WS_EX_APPWINDOW | DB ;//WS_EX_COMPOSITED = double-buffering
            dwStyle   = WS_POPUP | WS_CLIPCHILDREN;
        }
        else
        {
            dwExStyle = WS_EX_OVERLAPPEDWINDOW | DB ;//WS_EX_COMPOSITED = double buffer
            dwStyle   = resizebleValue | maxizeBox | minimize | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_BORDER |
                      WS_CLIPCHILDREN | WS_POPUP;
        }
        const int widthScreen  = GetSystemMetrics(SM_CXSCREEN);
        const int heightScreen = GetSystemMetrics(SM_CYSCREEN);
        AdjustWindowRectEx(&WindowRect, dwStyle, 0, dwExStyle);
        this->adjustRectLeft = WindowRect.left < 0 ? WindowRect.left * -1 : WindowRect.left;
        this->adjustRectTop  = WindowRect.top < 0 ? WindowRect.top * -1 : WindowRect.top;
        int wWin             = WindowRect.right - WindowRect.left;
        int hWin             = WindowRect.bottom - WindowRect.top;
        if (wWin > widthScreen)
            wWin = widthScreen;
        if (hWin > heightScreen)
            hWin = heightScreen;
#if UNICODE
        WCHAR *tmp_nameAplication = mbm::toWchar(nameAplication, nullptr);
        this->hwnd = CreateWindowExW(dwExStyle, className, tmp_nameAplication, dwStyle, 0, 0, wWin, hWin, nullptr, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
        delete[] tmp_nameAplication;
#else
        this->hwnd      = CreateWindowExA(dwExStyle, className, nameAplication, dwStyle, positionX, positionY, wWin, hWin, nullptr, nullptr,
                                     GetModuleHandleA(nullptr), nullptr);
#endif

        if (this->hwnd == nullptr)
        {
            this->messageBox("Erro ao Criar A windowsClassLocal");
            return false;
        }
        // adjust the size

        
        if(positionX == 0xffffff || positionY == 0xffffff || (positionX == 0 && positionY == 0))
        {
            const int posX = (int)((float)(widthScreen - wWin) * 0.5f);
            const int posY = (int)((float)(heightScreen - hWin) * 0.5f);
            MoveWindow(this->hwnd, (positionX == 0xffffff || positionX <= 0) ? posX : positionX,(positionY == 0xffffff || positionY <= 0) ? posY : positionY, wWin, hWin, false);
        }
        else
        {
            MoveWindow(this->hwnd, positionX ,positionY, wWin, hWin, false);
        }
        if (maximized)
            ShowWindow(this->hwnd, SIZE_MINIMIZED | SIZE_MAXIMIZED);
        else
            ShowWindow(this->hwnd, SIZE_RESTORED | SIZE_MINIMIZED);
        UpdateWindow(this->hwnd);
        this->run       = true;
        this->isVisible = true;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onEventWinPlus, this, WINPLUS_TYPE_WINDOW, nullptr, -1, nullptr);
        comBetweenWinp->hwnd = this->hwnd;
        isWin32Initialized   = true;
        SetForegroundWindow(this->hwnd);
        comBetweenWinp->graphWin              = this->drawerDefault;
        this->run                             = true;
        __NC_BORDERS::__NC_BUTTONS *ncButtons = new __NC_BORDERS::__NC_BUTTONS();
        ncButtons->hasMinimizeButton    = enableMinimizeButton;
        ncButtons->hasMaximizeButton    = enableMaximizeButton;
        ncButtons->hasCloseButton       = true;
        COM_BETWEEN_WINP *          nc =
            getNewComBetween(this->hwnd, onEventWinPlus, this, WINPLUS_TYPE_WINDOWNC, ncButtons, -1, nullptr);
        nc->hwnd                 = comBetweenWinp->hwnd;
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(WinNCProc));
        nc->_oldProc             = comBetweenWinp->_oldProc;
        nc->graphWin             = this->drawerDefault;
        dwStyle                  = GetWindowLong(this->hwnd, GWL_STYLE);
        ncButtons->hasCloseButton    = (WS_SYSMENU & dwStyle) ? true : false;
        InvalidateRect(hwnd, 0, 1);
        RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
        this->doEvents();
        return true;
    }
    void WINDOW::setNameAplication(const char *nameApp)
    {
        if (nameApp)
            strcpy(nameAplication, nameApp);
    }
    const char * WINDOW::getNameAplication() const
    {
        return nameAplication;
    }
    bool WINDOW::isEnableRender(HWND hwndIgnore)
    {
        for (unsigned int i = 0; i < mbm::WINDOW::lsDisabledRender.size(); ++i)
        {
            HWND htemp = mbm::WINDOW::lsDisabledRender[i];
            if (hwndIgnore == htemp)
                return false;
        }
        return true;
    }
    void WINDOW::disableRender(HWND hwndIgnore)
    {
        for (unsigned int i = 0; i < mbm::WINDOW::lsDisabledRender.size(); ++i)
        {
            HWND htemp = mbm::WINDOW::lsDisabledRender[i];
            if (hwndIgnore == htemp)
                return;
        }
        InvalidateRect(hwndIgnore, 0, 1);
        mbm::COM_BETWEEN_WINP *ptr = getComBetweenWinp(hwndIgnore);
        if (ptr && ptr->ptrWindow)
            ptr->ptrWindow->doEvents();
        InvalidateRect(hwndIgnore, 0, 1);
        mbm::WINDOW::lsDisabledRender.push_back(hwndIgnore);
    }
    DRAW * WINDOW::getGrafics(const int idComponent) const
    {
        mbm::COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
            return ptr->graphWin;
        return nullptr;
    }
    void WINDOW::setCallEventsManager(EVENTS *ptrCallEventsManager)
    {
        callEventsManager = ptrCallEventsManager;
    }
    unsigned int WINDOW::setObjectContext(void *YOUR_PTR_OBJECT, const unsigned int index)
    {
        if (index != 0xffffffff)
        {
            assert(this->lsObjectsContext[index] == nullptr || this->lsObjectsContext[index] == YOUR_PTR_OBJECT);
            this->lsObjectsContext[index] = YOUR_PTR_OBJECT;
            return index;
        }
        const unsigned int newIndex      = this->lsObjectsContext.size();
        this->lsObjectsContext[newIndex] = (YOUR_PTR_OBJECT);
        return newIndex;
    }
    void * WINDOW::getObjectContext(const unsigned int index)
    {
        if (index != 0xffffffff)
            return lsObjectsContext[index];
        return lsObjectsContext[0];
    }
    void WINDOW::setCursor(WINPLUS_TYPE_CURSOR TYPE)
    {
        if (TYPE != this->CURRENT_CURSOR)
        {
            this->CURRENT_CURSOR = TYPE;
            SetCursor(0);
        }
    }
    WINPLUS_TYPE_CURSOR WINDOW::getCursor()
    {
        return this->CURRENT_CURSOR;
    }
    void WINDOW::setNextCursor()
    {
        if ((CURRENT_CURSOR + 1) > WINPLUS_CURSOR_HELP)
            CURRENT_CURSOR = (WINPLUS_TYPE_CURSOR)0;
        else
            CURRENT_CURSOR = (WINPLUS_TYPE_CURSOR)(CURRENT_CURSOR + (WINPLUS_TYPE_CURSOR)1);
    }
    void WINDOW::startTimerHover()
    {
        if (this->idTimerHover == -1)
            this->idTimerHover = this->addTimer(100, _onTimeHover, nullptr);
    }
    int WINDOW::enterLoop(OnEventWinPlus ptrLogic)
    {
        if (this->hwnd == nullptr)
            return 0;
        MSG messageMain;
        ZeroMemory(&messageMain, sizeof(MSG));
        this->startTimerHover();
        this->refresh(1);
        while (run || this->doModalMode)
        {
            if (PeekMessage(&messageMain, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&messageMain);
                DispatchMessage(&messageMain);
                if (mbm::WINDOW::isEnableRender(messageMain.hwnd) == false)
                    mbm::WINDOW::WindowProc(messageMain.hwnd, messageMain.message, messageMain.wParam,
                                            messageMain.lParam);
            }
            if (ptrLogic)
            {
                DATA_EVENT data;
                ptrLogic(this, data);
            }
            Sleep(0);
        }
        if (!this->run && this->hasTryIcon)
        {
            this->hasTryIcon = false;
            Shell_NotifyIconA(NIM_DELETE, &tnid);
        }
        return messageMain.wParam;
    }

    void WINDOW::doEvents()
    {
        if (this->hwnd)
        {
            MSG messageMain;
            ZeroMemory(&messageMain, sizeof(MSG));
            this->startTimerHover();
            unsigned int maxLoop = 100;
            while (PeekMessage(&messageMain, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&messageMain);
                DispatchMessage(&messageMain);
                if (mbm::WINDOW::isEnableRender(messageMain.hwnd) == false)
                    mbm::WINDOW::WindowProc(messageMain.hwnd, messageMain.message, messageMain.wParam,
                                            messageMain.lParam);
                for (unsigned int i = 0; i < COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
                {
                    COM_BETWEEN_WINP *otherWindow = COM_BETWEEN_WINP::lsComBetweenWinp[i];
                    if (otherWindow && otherWindow->ptrWindow && otherWindow->hwnd)
                    {
                        if (PeekMessage(&messageMain, otherWindow->hwnd, 0, 0, PM_REMOVE))
                        {
                            TranslateMessage(&messageMain);
                            DispatchMessage(&messageMain);
                            if (mbm::WINDOW::isEnableRender(otherWindow->hwnd) == false)
                                mbm::WINDOW::WindowProc(otherWindow->hwnd, messageMain.message, messageMain.wParam,
                                                        messageMain.lParam);
                        }
                    }
                }
                if (maxLoop-- == 0)
                    break;
            }
            Sleep(0);
        }
    }

    void WINDOW::refresh(const unsigned int idComponent, const int eraseBK)
    {
        if (idComponent < mbm::COM_BETWEEN_WINP::lsComBetweenWinp.size())
        {
            mbm::COM_BETWEEN_WINP *ptr = mbm::COM_BETWEEN_WINP::lsComBetweenWinp[idComponent];
            if (ptr)
            {
                InvalidateRect(ptr->hwnd, 0, eraseBK);
                if (this->usingDoubleBuffer == false && ptr->ptrWindow->isUsingDoubleBuffer() == false &&
                    (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_GROUP_BOX_TAB ||
                    ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_BUTTON_TAB))
                {
                    __TAB_GROUP_DESC *tabFather = static_cast<__TAB_GROUP_DESC *>(ptr->extraParams);
                    if (tabFather)
                        this->setIndexTabByGroup(tabFather->idTabControlByGroup, tabFather->index, false);
                }
            }
        }
    }
    void WINDOW::refresh(const int eraseBK)
    {
        for (unsigned int i = 0, s = mbm::COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
        {
            mbm::COM_BETWEEN_WINP *ptr = mbm::COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (ptr && (ptr->hwnd == this->hwnd || ptr->owerHwnd == this->hwnd))
            {
                if (mbm::WINDOW::isEnableRender(ptr->hwnd))
                    InvalidateRect(ptr->hwnd, 0, eraseBK);
            }
        }
    }
    BOOL CALLBACK WINDOW::MessageBoxEnumProc(HWND hWnd, LPARAM lParam)
    {
        const int _MBOK          = 1;
        const int _MBCancel      = 2;
        const int _MBAbort       = 3;
        const int _MBRetry       = 4;
        const int _MBIgnore      = 5;
        const int _MBYes         = 6;
        const int _MBNo          = 7;
        char      className[255] = "";
        GetClassNameA(hWnd, className, sizeof(className));
        mbm::COM_BETWEEN_WINP *dialogBox = (mbm::COM_BETWEEN_WINP *)lParam;
        if (strcmp(className, "Button") == 0)
        {
            int ctlId = GetDlgCtrlID(hWnd);
            switch (ctlId)
            {
                case _MBOK:
                case _MBCancel:
                case _MBAbort:
                case _MBRetry:
                case _MBIgnore:
                case _MBYes:
                case _MBNo:
                {
                    const int s = SendMessageA(hWnd, WM_GETTEXTLENGTH, 0, 0) + 4;
                    if (s > 4)
                    {
                        char *text = new char[s];
                        if (GetWindowTextA(hWnd, text, s))
                        {
                            text[s - 1]   = 0;
                            char *newText = new char[s];
                            memset(newText, 0, s);
                            for (int i = 0, j = 0; i < s; ++i)
                            {
                                if (text[i] != '&')
                                {
                                    newText[j] = text[i];
                                    j++;
                                }
                            }
                            SetWindowTextA(hWnd, newText);
                            delete[] newText;
                        }
                        delete[] text;
                    }
                    COM_BETWEEN_WINP *comBetweenWinp =
                        getNewComBetween(dialogBox->hwnd, nullptr, dialogBox->ptrWindow, WINPLUS_TYPE_BUTTON, nullptr,
                                         dialogBox->id, dialogBox->userDrawer);
                    comBetweenWinp->hwnd = hWnd;
                    comBetweenWinp->ptrWindow->hideDestinyNotVisible(dialogBox->id, comBetweenWinp->hwnd);
                    comBetweenWinp->graphWin = dialogBox->ptrWindow->drawerDefault;
                    comBetweenWinp->_oldProc =
                        (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(MessageBoxProc));
                    dialogBox->myChilds.insert(comBetweenWinp);
                    InvalidateRect(hWnd, 0, 1);
                }
                break;
                default: SetWindowTextA(hWnd, "whata"); break;
            }
        }
        else if (strcmp(className, "Static") == 0)
        {
            if (dialogBox->extraParams == nullptr) // dialog open file
                return 1;
            COM_BETWEEN_WINP *comBetweenWinp = nullptr;
            const int         s              = SendMessageA(hWnd, WM_GETTEXTLENGTH, 0, 0) + 4;
            if (s > 4)
            {
                char *text = new char[s];
                if (GetWindowTextA(hWnd, text, s))
                {
                    text[s - 1]   = 0;
                    RECT rect     = {0, 0, 0, 0};
                    RECT rectMain = {0, 0, 0, 0};
                    GetWindowRect(hWnd, &rect);
                    SIZE sz     = mbm::DRAW::getSizeText(text, hWnd);
                    rect.right  = rect.left + sz.cx;
                    rect.bottom = rect.top + sz.cy;
                    MapWindowRect(HWND_DESKTOP, dialogBox->hwnd, &rect);
                    MoveWindow(hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0);
                    GetClientRect(dialogBox->hwnd, &rectMain);
                    const int sw1 = rectMain.right;
                    const int sw2 = rect.right;
                    if ((sw1 - 16) < sw2)
                    {
                        GetWindowRect(dialogBox->hwnd, &rectMain);
                        rectMain.right += 16 + (sw2 - sw1);
                        MoveWindow(dialogBox->hwnd, rectMain.left, rectMain.top, rectMain.right - rectMain.left,
                                   rectMain.bottom - rectMain.top, 1);
                    }
                }
                text[s - 1]   = 0;
                char *newText = new char[s];
                memset(newText, 0, s);
                for (int i = 0, j = 0; i < s; ++i)
                {
                    if (text[i] != '&')
                    {
                        newText[j] = text[i];
                        j++;
                    }
                }
                SetWindowTextA(hWnd, newText);
                delete[] newText;
                delete[] text;
            }
            else
            {
                int *pFlag = static_cast<int *>(dialogBox->extraParams);
                {
					#define hasBit_On(v, bit) ((v & bit) == bit)
                    int   flag  = *pFlag;
                    HICON hIcon = nullptr;
                    if (hasBit_On(flag, MB_ICONINFORMATION) || hasBit_On(flag, MB_ICONASTERISK))
                    {
                        hIcon = LoadIcon(nullptr, IDI_INFORMATION);
                    }
                    else if (hasBit_On(flag, MB_ICONEXCLAMATION) || hasBit_On(flag, MB_ICONWARNING))
                    {
                        hIcon = LoadIcon(nullptr, IDI_WARNING);
                    }
                    else if (hasBit_On(flag, MB_ICONSTOP) || hasBit_On(flag, MB_ICONERROR) ||
                             hasBit_On(flag, MB_ICONHAND))
                    {
                        hIcon = LoadIcon(nullptr, IDI_ERROR);
                    }
                    else if (hasBit_On(flag, MB_ICONQUESTION))
                    {
                        hIcon = LoadIcon(nullptr, IDI_QUESTION);
                    }
                    if (hIcon)
                    {
                        ICONINFO iconinfo = {0};
                        if (GetIconInfo(hIcon, &iconinfo))
                        {
                            HBITMAP hBitmap = iconinfo.hbmColor;
                            HBRUSH  pattern = CreatePatternBrush(hBitmap);
                            if (pattern)
                            {
                                comBetweenWinp =
                                    getNewComBetween(dialogBox->hwnd, nullptr, dialogBox->ptrWindow, WINPLUS_TYPE_IMAGE,
                                                     pattern, dialogBox->id, dialogBox->userDrawer);
                                resize(hWnd, 1, 1, true);
                            }
                        }
                    }
                }
            }
            if (comBetweenWinp == nullptr)
                comBetweenWinp = getNewComBetween(dialogBox->hwnd, nullptr, dialogBox->ptrWindow, WINPLUS_TYPE_LABEL, nullptr,
                                                  dialogBox->id, dialogBox->userDrawer);
            comBetweenWinp->hwnd = hWnd;
            if (comBetweenWinp->ptrWindow)
                comBetweenWinp->ptrWindow->hideDestinyNotVisible(dialogBox->id, comBetweenWinp->hwnd);
            comBetweenWinp->graphWin =
                dialogBox->ptrWindow ? dialogBox->ptrWindow->drawerDefault : _winplusDefaultThemeDraw;
            comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(MessageBoxProc));
            dialogBox->myChilds.insert(comBetweenWinp);
            InvalidateRect(hWnd, 0, 1);
        }
        return 1;
    }
    LRESULT CALLBACK WINDOW::GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode < 0) // do not process message
            return CallNextHookEx(mbm::WINDOW::hookMsgProc, nCode, wParam, lParam);
        PCWPRETSTRUCT msg = (PCWPRETSTRUCT)(lParam);
        if (msg->message == WM_INITDIALOG)
        {
            char className[255];
            GetClassNameA(msg->hwnd, className, sizeof(className));
            if (strcmp(className, "#32770") == 0)
            {
                if (COM_BETWEEN_WINP::lsComBetweenWinp.size())
                {
                    mbm::COM_BETWEEN_WINP *dialogBox =
                        mbm::COM_BETWEEN_WINP::lsComBetweenWinp[COM_BETWEEN_WINP::lsComBetweenWinp.size() - 1];
                    if (dialogBox->typeWindowWinPlus == mbm::WINPLUS_TYPE_WINDOW_MESSAGE_BOX)
                    {
                        if (dialogBox->graphWin == nullptr)
                            dialogBox->graphWin               = _winplusDefaultThemeDraw;
                        __NC_BORDERS::__NC_BUTTONS *ncButtons = new __NC_BORDERS::__NC_BUTTONS();
                        dialogBox->hwnd                       = msg->hwnd;
                        dialogBox->graphWin =
                            dialogBox->ptrWindow ? dialogBox->ptrWindow->drawerDefault : _winplusDefaultThemeDraw;
                        COM_BETWEEN_WINP *nc =
                            getNewComBetween(dialogBox->owerHwnd, dialogBox->onEventWinPlus, dialogBox->ptrWindow,
                                             WINPLUS_TYPE_WINDOWNC, ncButtons, -1, nullptr);
                        nc->hwnd            = dialogBox->hwnd;
                        dialogBox->_oldProc = (WNDPROC)SetWindowLong(dialogBox->hwnd, GWL_WNDPROC, long(WinNCProc));
                        nc->_oldProc        = dialogBox->_oldProc;
                        nc->graphWin =
                            dialogBox->ptrWindow ? dialogBox->ptrWindow->drawerDefault : _winplusDefaultThemeDraw;
                        EnumChildWindows(msg->hwnd, MessageBoxEnumProc, (LPARAM)dialogBox);
                        return TRUE;
                    }
                }
            }
        }
        return CallNextHookEx(mbm::WINDOW::hookMsgProc, nCode, wParam, lParam);
    }
    DWORD WINAPI WINDOW::ThreadModal(LPVOID OBJECT)
    {
        __DO_MODAL_OBJ *obj          = static_cast<__DO_MODAL_OBJ *>(OBJECT);
        mbm::WINDOW *   w            = obj->w;
        mbm::WINDOW *   parent       = obj->parent;
        OnDoModal       onDoModalObj = obj->onDoModal;
        w->hwndInsertAfter           = nullptr;
        w->hwndLastHover             = nullptr;
        w->hwndLastPressed           = nullptr;
        w->hwndLastTrackBar          = nullptr;
        w->hideOnExit                = true;
        w->askOnExit                 = false;
        w->exitOnEsc                 = false;

        if (parent)
        {
            parent->setFocus();
            parent->doEvents();
        }
        w->setFocus();
        w->doEvents();
        w->forceFocus();
        w->refresh(1);

        while (w->doModalMode && w->isVisible)
        {
            w->doEvents();
            if (onDoModalObj)
                onDoModalObj(w);
            Sleep(0);
        }
        if (parent)
        {
            parent->hwndInsertAfter = nullptr;
            if (obj->disabelParentWindow)
            {
                EnableWindow(parent->hwnd, TRUE);
                w->hwndInsertAfter = parent->hwnd;
            }
            parent->refresh(1);
            parent->forceFocus();
            parent->hwndLastHover    = nullptr;
            parent->hwndLastPressed  = nullptr;
            parent->hwndLastTrackBar = nullptr;
        }
        delete obj;
        obj = nullptr;
        return 0;
    }
    void WINDOW::doModal(mbm::WINDOW *parent, OnDoModal onDoModal, const bool threadModal,
                         const bool disabelParentWindow)
    {
        if (this->hwnd)
        {
            this->run         = true;
            this->doModalMode = true;
            this->show();
            this->doEvents();
            if (parent)
            {
                parent->setFocus();
                parent->doEvents();
                this->doEvents();
                if (disabelParentWindow)
                {
                    EnableWindow(parent->hwnd, FALSE);
                    this->setAlwaysOnTop(parent);
                }
                parent->setFocus();
                parent->doEvents();
                this->doEvents();
            }
            if (threadModal)
            {
                __DO_MODAL_OBJ *obj = new __DO_MODAL_OBJ(this, parent, onDoModal, disabelParentWindow);
                CreateThread(0, 0, ThreadModal, obj, 0, nullptr);
            }
            else
            {
                this->startTimerHover();
                this->refresh(1);
                while (this->doModalMode && this->isVisible)
                {
                    this->doEvents();
                    if (onDoModal)
                        onDoModal(this);
                }
                if (parent)
                {
                    parent->hwndInsertAfter = nullptr;
                    if (disabelParentWindow)
                    {
                        EnableWindow(parent->hwnd, TRUE);
                    }
                    parent->refresh(1);
                    parent->forceFocus();
                    parent->hwndLastHover    = nullptr;
                    parent->hwndLastPressed  = nullptr;
                    parent->hwndLastTrackBar = nullptr;
                }
            }
        }
    }
    HWND WINDOW::getHwnd(const int id) const
    {
        if (id <= 0)
            return this->hwnd;
        mbm::COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
        if (ptr)
            return ptr->hwnd;
        return nullptr;
    }
    bool WINDOW::setDrawer(mbm::DRAW *draw, const int idComponent)
    {
        mbm::COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (draw && ptr)
        {
            ptr->graphWin = draw;
            return true;
        }
        return false;
    }
    bool WINDOW::setDrawer(mbm::DRAW *draw, const mbm::TYPE_WINDOWS_WINPLUS typeWindowWinPlus)
    {
        bool founded = false;
        for (unsigned int i = 0; i < COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
        {

            mbm::COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (draw && ptr && ptr->typeWindowWinPlus == typeWindowWinPlus)
            {
                founded       = true;
                ptr->graphWin = draw;
            }
        }
        return founded;
    }
    bool WINDOW::setDrawer(mbm::DRAW *draw)
    {
        bool founded = false;
        for (unsigned int i = 0; i < COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
        {
            mbm::COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (draw && ptr)
            {
                founded       = true;
                ptr->graphWin = draw;
            }
        }
        this->drawerDefault = draw;
        return founded;
    }
    void WINDOW::setTheme(mbm::DRAW *theme)
    {
        if(theme && this->drawerDefault != theme)
        {
            for( auto & ptr : COM_BETWEEN_WINP::lsComBetweenWinp)
            {
                ptr->graphWin = theme;
            }
            this->drawerDefault = theme;
            for( auto & ptr : COM_BETWEEN_WINP::lsComBetweenWinp)
            {
                this->refresh(ptr->id,1);
            }
        }
    }
    mbm::DRAW * WINDOW::getDrawer(const int id)
    {
        mbm::COM_BETWEEN_WINP *ptr = nullptr;
        if (id == -1)
            ptr = COM_BETWEEN_WINP::lsComBetweenWinp.size() ? COM_BETWEEN_WINP::lsComBetweenWinp[0] : nullptr;
        else if (id < 0)
            return nullptr;
        else if ((unsigned int)id < COM_BETWEEN_WINP::lsComBetweenWinp.size())
            ptr = COM_BETWEEN_WINP::lsComBetweenWinp[id];
        return ptr ? ptr->graphWin : nullptr;
    }
    int WINDOW::addWindowChild(const char *title, long x, long y, long width, long height,
                              OnEventWinPlus onEventWinPlus, const bool enableResize,
                              const bool enableMaximizeButton, const int idDest , USER_DRAWER *UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onEventWinPlus, this, WINPLUS_TYPE_CHILD_WINDOW, nullptr, idDest, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        int resizebleValue = 0;
        if (enableResize)
            resizebleValue = WS_THICKFRAME;
        int maxizeBox      = 0;
        if (enableMaximizeButton)
            maxizeBox   = WS_MAXIMIZEBOX;
        DWORD dwExStyle = WS_EX_APPWINDOW;
        DWORD dwStyle   = WS_CHILD | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE |
                        resizebleValue | maxizeBox;
#if UNICODE
        WCHAR tmp_nameAplication[] = L"Child";
        WCHAR className[256]       = L"";
        GetClassNameW(this->hwnd, className, 255);
        if (className[0] == 0)
            return -1;
        comBetweenWinp->hwnd = CreateWindowExW(dwExStyle, className, tmp_nameAplication, dwStyle, x, y, width, height,
                                               addToHwnd(idDest, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                                               GetModuleHandleW(nullptr), nullptr);
#else
        char tmp_nameAplication[] = "Child";
        char className[256]       = "";
        GetClassNameA(this->hwnd, className, 255);
        if (className[0] == 0)
            return -1;
        comBetweenWinp->hwnd = CreateWindowExA(dwExStyle, className, tmp_nameAplication, dwStyle, x, y, width, height,
                                               addToHwnd(idDest, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                                               GetModuleHandleA(nullptr), nullptr);
#endif
        comBetweenWinp->graphWin = this->drawerDefault;
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        __NC_BORDERS::__NC_BUTTONS *ncButtons = new __NC_BORDERS::__NC_BUTTONS();
        COM_BETWEEN_WINP *          nc =
            getNewComBetween(this->hwnd, onEventWinPlus, this, WINPLUS_TYPE_WINDOWNC, ncButtons, idDest, nullptr);
        nc->hwnd                 = comBetweenWinp->hwnd;
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(WinNCProc));
        nc->_oldProc             = comBetweenWinp->_oldProc;
        nc->graphWin             = this->drawerDefault;
        InvalidateRect(comBetweenWinp->hwnd, 0, 1);
        RedrawWindow(comBetweenWinp->hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SendMessageA(comBetweenWinp->hwnd, WM_SETTEXT, 0, (LPARAM)title);
        return comBetweenWinp->getId();
    }
    int WINDOW::addLabel(const char *title, long x, long y, long width, long height, const int idDest ,
                        OnEventWinPlus onGotClickeOrFocus,USER_DRAWER *userDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onGotClickeOrFocus, this, WINPLUS_TYPE_LABEL, nullptr, idDest, userDrawer);
        comBetweenWinp->hwnd =
            CreateWindowA("static", title, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    bool WINDOW::isLoaded()
    {
        return (isWin32Initialized) || (this->hwnd != nullptr);
    }
    
        WINDOW::__MENU_DRAW::__MENU_DRAW(const int idDest_) : idDest(idDest_)
        {
            idMenu            = -1;
            minSize[0]        = 0;
            minSize[1]        = 0;
            idSubMenu         = -1;
            sizeSubMenuDrawed = 0;
            diffX             = 0;
            diffY             = 0;
            parentHwnd        = nullptr;
            hwndSubMenu       = nullptr;
            hwnd              = nullptr;
            onSelectedSubMenu = nullptr;
            child             = nullptr;
            isSubMenuVisible  = false;
            indexClickedMenu  = 0;
        }
        WINDOW::__MENU_DRAW::~__MENU_DRAW()
        {
            if (this->hwnd)
                DestroyWindow(this->hwnd);
            this->hwnd = nullptr;
            if (this->hwndSubMenu)
                DestroyWindow(this->hwndSubMenu);
            this->hwndSubMenu = nullptr;
        }
        void WINDOW::__MENU_DRAW::hideSubMenu()
        {
            if (this->hwndSubMenu && isSubMenuVisible)
            {
                isSubMenuVisible = false;
                ShowWindow(this->hwndSubMenu, SW_HIDE);
                InvalidateRect(this->parentHwnd, 0, 0);
            }
        }
        bool WINDOW::__MENU_DRAW::showSubMenu()
        {
            if (this->hwndSubMenu == nullptr || sizeSubMenuDrawed != this->lsSubMenusTitles.size())
            {
                COM_BETWEEN_WINP *ptrOwner = getComBetweenWinp(idDest);
                RECT              rect;
                GetClientRect(this->hwnd, &rect);
                MapWindowRect(this->hwnd, ptrOwner->hwnd, &rect);
                SIZE szMax = {0, 0};
                lsSubMenusHeight.clear();
                for (unsigned int i = 0; i < this->lsSubMenusTitles.size(); ++i)
                {
                    SIZE sz = mbm::DRAW::getSizeText(this->lsSubMenusTitles[i].c_str(), this->hwnd);
                    if (sz.cx > szMax.cx)
                        szMax.cx = sz.cx;
                    if (sz.cy > szMax.cy)
                        szMax.cy = sz.cy;
                    lsSubMenusHeight.push_back(0);
                }
                szMax.cx   = (szMax.cx + diffX) < minSize[0] ? (minSize[0] - diffX) : szMax.cx;
                szMax.cy   = (szMax.cy + diffY) < minSize[1] ? (minSize[1] - diffY) : szMax.cy;
                int width  = szMax.cx + diffX;
                int height = szMax.cy * this->lsSubMenusTitles.size() + (diffY * this->lsSubMenusTitles.size());
                if (width == 0 || height == 0)
                    return false;
                int div = height / this->lsSubMenusTitles.size();
                for (unsigned int i = 0; i < lsSubMenusHeight.size(); ++i)
                {
                    lsSubMenusHeight[i] = (i + 1) * div;
                }
                DWORD             dwExStyle         = WS_EX_APPWINDOW;
                DWORD             dwStyle           = WS_POPUP | WS_CLIPCHILDREN;
                COM_BETWEEN_WINP *comBetweenSubMenu = nullptr;
                if (this->hwndSubMenu)
                {
                    comBetweenSubMenu = getComBetweenWinp(this->hwndSubMenu);
                    if (comBetweenSubMenu)
                    {
                        comBetweenSubMenu->hwnd        = nullptr;
                        comBetweenSubMenu->graphWin    = nullptr;
                        comBetweenSubMenu->extraParams = nullptr;
                    }
                    DestroyWindow(this->hwndSubMenu);
                    this->hwndSubMenu = nullptr;
                }
#if UNICODE
                WCHAR tmp_nameAplication[] = L"Menu";
                WCHAR className[256]       = L"";
                GetClassNameW(parentHwnd, className, 255);
                if (className[0] == 0)
                    return false;
                this->hwndSubMenu = CreateWindowExW(dwExStyle, className, tmp_nameAplication, dwStyle, rect.left,
                                                    rect.bottom, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
#else
                char tmp_nameAplication[] = "Menu";
                char className[256]       = "";
                GetClassNameA(parentHwnd, className, 255);
                if (className[0] == 0)
                    return false;
                this->hwndSubMenu = CreateWindowExA(dwExStyle, className, tmp_nameAplication, dwStyle, rect.left,
                                                    rect.bottom, width, height, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
#endif
                sizeSubMenuDrawed = this->lsSubMenusTitles.size();
                if (comBetweenSubMenu == nullptr)
                    comBetweenSubMenu = getNewComBetween(this->parentHwnd, onSelectedSubMenu, ptrOwner->ptrWindow,
                                                         WINPLUS_TYPE_SUB_MENU, this, idDest, ptrOwner->userDrawer);
                comBetweenSubMenu->graphWin    = ptrOwner->ptrWindow->drawerDefault;
                comBetweenSubMenu->hwnd        = this->hwndSubMenu;
                comBetweenSubMenu->extraParams = this;
                idSubMenu                      = comBetweenSubMenu->id;
                SetParent(comBetweenSubMenu->hwnd, parentHwnd);
                ShowWindow(comBetweenSubMenu->hwnd, SW_SHOWNORMAL);
                UpdateWindow(comBetweenSubMenu->hwnd);
            }
            isSubMenuVisible = true;
            ShowWindow(this->hwndSubMenu, SW_SHOWNORMAL);
            InvalidateRect(this->hwndSubMenu, 0, 1);
            return this->hwndSubMenu != nullptr;
        }
        bool WINDOW::__MENU_DRAW::show(HWND parentHwnd_, const int myId, const int width, const int height, const int diff_x, const int diff_y)
        {
            if (!this->hwnd)
            {
                diffX            = diff_x;
                diffY            = diff_y;
                parentHwnd       = parentHwnd_;
                RECT  WindowRect = getMenuRect(idDest, myId);
                DWORD dwExStyle  = WS_EX_APPWINDOW;
                DWORD dwStyle    = WS_POPUP | WS_CLIPCHILDREN;

#if UNICODE
                WCHAR className[256] = L"";
                GetClassNameW(parentHwnd, className, 255);
                if (className[0] == 0)
                    return false;
                WCHAR tmp_nameAplication[] = L"Menu";
                this->hwnd = CreateWindowExW(dwExStyle, className, tmp_nameAplication, dwStyle, WindowRect.right,
                                             WindowRect.top, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
#else
                char tmp_nameAplication[] = "Menu";
                char className[256]       = "";
                GetClassNameA(parentHwnd, className, 255);
                if (className[0] == 0)
                    return false;
                this->hwnd = CreateWindowExA(dwExStyle, className, tmp_nameAplication, dwStyle, WindowRect.right,
                                             WindowRect.top, width, height, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
#endif
                minSize[0] = width;
                minSize[1] = height;
                SetParent(this->hwnd, parentHwnd);
                ShowWindow(this->hwnd, SIZE_RESTORED | SIZE_MINIMIZED);
                UpdateWindow(this->hwnd);
            }
            return this->hwnd != nullptr;
        }
    const WINDOW::__MENU_DRAW * WINDOW::getMenuInfo(const int idMenu)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idMenu);
        if (ptr == nullptr || ptr->typeWindowWinPlus != WINPLUS_TYPE_MENU)
        {
            __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr ? ptr->extraParams: nullptr);
            return menu;
        }
        return nullptr;
    }
    void WINDOW::refreshMenu()
    {
        for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
        {
            mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
            if (menu && menu->isSubMenuVisible)
                InvalidateRect(menu->hwndSubMenu, 0, 1);
        }
    }
    const bool WINDOW::isAnyMenuVisible()
    {
        for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
        {
            mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
            if (menu && menu->isSubMenuVisible)
                return true;
        }
        return false;
    }
    int WINDOW::addMenu(const char *title, OnEventWinPlus onSelectedSubMenu, const int idDest, USER_DRAWER *UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        __MENU_DRAW *menu       = new __MENU_DRAW(idDest);
        menu->onSelectedSubMenu = onSelectedSubMenu;
        menu->title             = title ? title : "";
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onSelectedSubMenu, this, WINPLUS_TYPE_MENU, menu, idDest, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        comBetweenWinp->graphWin = this->drawerDefault;
        if (comBetweenWinp->graphWin)
        {
            SIZE              size = mbm::DRAW::getSizeText(title, this->hwnd);
            MEASUREITEMSTRUCT measure;
            memset(&measure, 0, sizeof(measure));
            measure.CtlType    = ODT_MENU;
            measure.itemHeight = size.cy;
            measure.itemWidth  = size.cx;
            measure.itemData   = (ULONG_PTR)title;
            if (comBetweenWinp->graphWin->measureItem(comBetweenWinp, &measure))
            {
                if (menu->show(this->hwnd, comBetweenWinp->id, measure.itemWidth, measure.itemHeight,
                               measure.itemWidth - size.cx, measure.itemHeight - size.cy) &&
                    title)
                {
                    this->setText(comBetweenWinp->id, title);
                }
            }
        }
        comBetweenWinp->hwnd = menu->hwnd;
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        lsAllMenus.push_back(menu);
        menu->idMenu = comBetweenWinp->getId();
        return menu->idMenu;
    }
    int WINDOW::addSubMenu(const char *title, const int idMenu)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idMenu);
        if (ptr == nullptr || ptr->typeWindowWinPlus != WINPLUS_TYPE_MENU)
            return -1;
        __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
        menu->lsSubMenusTitles.push_back(title ? title : "");
        return menu->lsSubMenusTitles.size() - 1;
    }
    int WINDOW::addStatusBar(const char *textStatusBar0, const unsigned int numberPartsIntoStatusBar,
                            const int idDest, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, nullptr, this, WINPLUS_TYPE_STATUS_BAR, nullptr, idDest, UserDrawer);

        int  sizeBar = GetSystemMetrics(SM_CYCAPTION);
        RECT rect    = getRect(-1);
        int  Wheight = rect.bottom - rect.top;
        int  mywidth = rect.right - rect.left;

        comBetweenWinp->hwnd =
            CreateWindowA("static", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, rect.left, Wheight - sizeBar,
                          mywidth, sizeBar, addToHwnd(idDest, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), nullptr);

        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(StatusProc));
        SendMessageA(comBetweenWinp->hwnd, SB_SETPARTS, numberPartsIntoStatusBar ? numberPartsIntoStatusBar : 1, 0);
        SendMessageA(comBetweenWinp->hwnd, SB_SETTEXTA, 0, (LPARAM)textStatusBar0);
        return comBetweenWinp->getId();
    }
    int WINDOW::addSpinInt(long x, long y, long width, long height, const int idDest, long widthSpin,
                          long heightSpin, OnEventWinPlus onChangeValue, int minValue, int maxValue,
                          int increment, int currentPosition, bool vertical, const bool enableWrite,
                          USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        mbm::SPIN_PARAMSi *spinValues = new mbm::SPIN_PARAMSi(minValue, maxValue, increment, currentPosition);
        currentPosition               = spinValues->currentPosition;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onChangeValue, this, WINPLUS_TYPE_SPIN_INT, spinValues, idDest, UserDrawer);
        if (!widthSpin)
            widthSpin = 20;
        if (!heightSpin)
            heightSpin = height;
        width -= widthSpin;
        int V    = 0;
        int diff = 0;
        if (!vertical)
            V = UDS_HORZ;
        else
            diff = 3;

        HWND hwndTo = addToHwnd(idDest, &x, &y, comBetweenWinp);
        comBetweenWinp->hwnd =
            CreateUpDownControl(WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | V, x + width + diff, y, widthSpin, heightSpin,
                                hwndTo, comBetweenWinp->getId(), (HINSTANCE)GetWindowLongA(hwnd, GWL_HINSTANCE), nullptr,
                                maxValue, minValue, currentPosition);
        if (comBetweenWinp->hwnd == nullptr)
        {
            delete spinValues;
            return -1;
        }
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(UDProc));
        char num[100];
        sprintf(num, "%d", currentPosition);
        int readOnly = 0;
        if (!enableWrite)
            readOnly                   = ES_READONLY;
        mbm::COM_BETWEEN_WINP *ptrDest = mbm::getComBetweenWinp(idDest);
        COM_BETWEEN_WINP *     comBetweenWinp2 =
            getNewComBetween(this->hwnd, nullptr, this, WINPLUS_TYPE_TEXT_BOX, nullptr, idDest, UserDrawer);
        comBetweenWinp2->hwnd = CreateWindowA("EDIT", num, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
                                              x, y, width + diff, height, hwndTo, (HMENU)comBetweenWinp2->getId(),
                                              (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp2->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        hideDestinyNotVisible(idDest, comBetweenWinp2->hwnd);
        comBetweenWinp2->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp2->hwnd, GWL_WNDPROC, long(EditProc));
        SetParent(comBetweenWinp2->hwnd, this->hwnd);
        EDIT_TEXT_DATA *data         = new EDIT_TEXT_DATA(comBetweenWinp2->getId());
        comBetweenWinp2->extraParams = data;
        data->spin                   = spinValues;
        if (ptrDest)
        {
            ptrDest->myChilds.insert(comBetweenWinp2);
            comBetweenWinp2->graphWin = this->drawerDefault;
        }
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        SetParent(comBetweenWinp2->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addSpinFloat(long x, long y, long width, long height, const int idDest, long widthSpin,
                            long heightSpin, float minValue, float maxValue, float increment,
                            float currentPosition, int precision, bool vertical,
                            const bool enableWrite, OnEventWinPlus onChangedValue, USER_DRAWER * UserDrawer)
    {
        char num[100];
        if (this->hwnd == nullptr)
            return -1;
        mbm::SPIN_PARAMSf *spinValues = new mbm::SPIN_PARAMSf(minValue, maxValue, increment, currentPosition, precision);
        currentPosition               = spinValues->currentPosition;
        sprintf(num, "%0.*f", spinValues->precision, currentPosition);
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onChangedValue, this, WINPLUS_TYPE_SPIN_FLOAT, spinValues, idDest, UserDrawer);
        if (!widthSpin)
            widthSpin = 20;
        if (!heightSpin)
            heightSpin = height;
        width -= widthSpin;
        int V    = 0;
        int diff = 0;
        if (!vertical)
            V = UDS_HORZ;
        else
            diff             = 3;
        HWND hwndTo          = addToHwnd(idDest, &x, &y, comBetweenWinp);
        comBetweenWinp->hwnd = CreateUpDownControl(WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | V, x + width + diff, y,
                                                   widthSpin, heightSpin, hwndTo, comBetweenWinp->getId(),
                                                   (HINSTANCE)GetWindowLongA(hwnd, GWL_HINSTANCE), nullptr, 100, 0, 10);
        if (comBetweenWinp->hwnd == nullptr)
        {
            delete spinValues;
            return -1;
        }
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(UDProc));
        int readOnly             = 0;
        if (!enableWrite)
            readOnly                   = ES_READONLY;
        mbm::COM_BETWEEN_WINP *ptrDest = mbm::getComBetweenWinp(idDest);
        COM_BETWEEN_WINP *     comBetweenWinp2 =
            getNewComBetween(this->hwnd, nullptr, this, WINPLUS_TYPE_TEXT_BOX, nullptr, idDest, UserDrawer);
        comBetweenWinp2->hwnd = CreateWindowA("EDIT", num, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
                                              x, y, width + diff, height, hwndTo, (HMENU)comBetweenWinp2->getId(),
                                              (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp2->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        hideDestinyNotVisible(idDest, comBetweenWinp2->hwnd);
        comBetweenWinp2->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp2->hwnd, GWL_WNDPROC, long(EditProc));
        SetParent(comBetweenWinp2->hwnd, this->hwnd);
        EDIT_TEXT_DATA *data         = new EDIT_TEXT_DATA(comBetweenWinp2->getId());
        comBetweenWinp2->extraParams = data;
        data->spinf                  = spinValues;
        if (ptrDest)
        {
            ptrDest->myChilds.insert(comBetweenWinp2);
            comBetweenWinp2->graphWin = this->drawerDefault;
        }
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        SetParent(comBetweenWinp2->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addScroll(long x, long y, long width, long height, int scrollSize,
                         OnEventWinPlus onEventWindow, const int idDest, USER_DRAWER * UserDrawer) // doesnt work
    {
        if (this->hwnd == nullptr)
            return -1;
        SCROLLINFO *sc = new SCROLLINFO;
        sc->cbSize     = sizeof(SCROLLINFO);
        sc->fMask      = SIF_ALL;
        sc->nMin       = 0;
        sc->nMax       = 100;
        sc->nPage      = scrollSize; // tamanho do scroll
        sc->nPos       = 25;
        sc->nTrackPos  = 75;
        if ((int)sc->nPage > sc->nMax)
            sc->nPage = sc->nMax;
        if ((int)sc->nPage < sc->nMin)
            sc->nPage = sc->nMin;
        // int iHThumb = GetSystemMetrics(SM_CXHTHUMB);
        // int iVThumb = GetSystemMetrics(SM_CYVTHUMB);
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onEventWindow, this, WINPLUS_TYPE_SCROLL, sc, idDest, UserDrawer);
        const bool isHorizontal = width > height;
        DWORD      st           = 0;
        if (isHorizontal)
            st = SBS_HORZ | SBS_TOPALIGN | WS_HSCROLL;
        else
            st               = SBS_VERT | SBS_LEFTALIGN | WS_VSCROLL;
        comBetweenWinp->hwnd = CreateWindowExW(0, L"SCROLLBAR", nullptr, WS_CHILD | WS_VISIBLE | st, x, y, width, height,
                                               addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                                               (HINSTANCE)GetWindowLongA(this->hwnd, GWL_HINSTANCE), nullptr);

        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);

        comBetweenWinp->_oldProc =
            (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(ScrollProc)); // doesnt work
        sc->fMask = SIF_ALL;
        if (isHorizontal)
            SetScrollInfo(comBetweenWinp->hwnd, SB_HORZ, sc, true);
        else
            SetScrollInfo(comBetweenWinp->hwnd, SB_VERT, sc, true);
        // SendMessageA(comBetweenWinp->hwnd,SBM_ENABLE_ARROWS,ESB_DISABLE_LEFT,0);
        return comBetweenWinp->getId();
    }
    int WINDOW::addTrayIcon(const int ID_RESOURCE_ICON, OnEventWinPlus onEventWindowByIdMenu, const char *tip,
                           USER_DRAWER * UserDrawer)
    {
        if (hasTryIcon)
            return 1;
        BOOL  res;
        HICON hicon;

#ifdef NIM_SETVERSION
        tnid.uVersion = 0;
        res           = Shell_NotifyIconA(NIM_SETVERSION, &tnid);
#endif

        tnid.cbSize           = sizeof(NOTIFYICONDATA);
        tnid.hWnd             = this->hwnd;
        tnid.uID              = 1; /* unique within this systray use */
        tnid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tnid.uCallbackMessage = WM_SYSTRAY;

        tnid.hIcon = hicon =
            LoadIcon((HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), MAKEINTRESOURCE(ID_RESOURCE_ICON));
        if (hicon == nullptr)
            return -1;
        hasTryIcon = true;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onEventWindowByIdMenu, this, WINPLUS_TYPE_TRY_ICON_MENU,
                             static_cast<void *>(CreatePopupMenu()), -1, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        this->sysTry_menu    = static_cast<HMENU>(comBetweenWinp->extraParams);
        comBetweenWinp->hwnd = this->hwnd;
        strcpy(tnid.szTip, tip);
        strcpy(tnid.szInfoTitle, "Title");
        strcpy(tnid.szInfo, "ballon");
        res = Shell_NotifyIconA(NIM_ADD, &tnid);
        if (!res)
        {
            delete comBetweenWinp;
            DestroyIcon(hicon);
            return -1;
        }
        DestroyIcon(hicon);
        return comBetweenWinp->getId();
    }
    int WINDOW::addTrayIcon(const char *fileNameIcon, OnEventWinPlus onEventWindowByIdMenu, const char *tip,
                           USER_DRAWER * UserDrawer)
    {
        if (hasTryIcon)
            return 1;
        BOOL  res;
        HICON hicon;

#ifdef NIM_SETVERSION
        tnid.uVersion = 0;
        res           = Shell_NotifyIconA(NIM_SETVERSION, &tnid);
#endif

        tnid.cbSize           = sizeof(NOTIFYICONDATA);
        tnid.hWnd             = this->hwnd;
        tnid.uID              = 1; /* unique within this systray use */
        tnid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tnid.uCallbackMessage = WM_SYSTRAY;

        hicon = (HICON)LoadImageA(GetModuleHandle(nullptr), fileNameIcon, IMAGE_ICON, 0, 0,
                                  LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE);

        tnid.hIcon = hicon;
        if (hicon == nullptr)
            return -1;
        hasTryIcon = true;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onEventWindowByIdMenu, this, WINPLUS_TYPE_TRY_ICON_MENU,
                             static_cast<void *>(CreatePopupMenu()), -1, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        this->sysTry_menu    = static_cast<HMENU>(comBetweenWinp->extraParams);
        comBetweenWinp->hwnd = this->hwnd;
        strcpy(tnid.szTip, tip);
        strcpy(tnid.szInfoTitle, "Title");
        strcpy(tnid.szInfo, "ballon");
        res = Shell_NotifyIconA(NIM_ADD, &tnid);
        if (!res)
        {
            delete comBetweenWinp;
            DestroyIcon(hicon);
            return -1;
        }
        DestroyIcon(hicon);
        return comBetweenWinp->getId();
    }
    int WINDOW::addTrayIcon(OnEventWinPlus onEventWindowByIdMenu, const char *tip, USER_DRAWER * UserDrawer)
    {
        if (hasTryIcon)
            return 1;
        BOOL res;
#ifdef NIM_SETVERSION
        tnid.uVersion = 0;
        res           = Shell_NotifyIconA(NIM_SETVERSION, &tnid);
#endif

        tnid.cbSize           = sizeof(NOTIFYICONDATA);
        tnid.hWnd             = this->hwnd;
        tnid.uID              = 1; /* unique within this systray use */
        tnid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tnid.uCallbackMessage = WM_SYSTRAY;

        tnid.hIcon = this->getIcon();
        if (tnid.hIcon == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onEventWindowByIdMenu, this, WINPLUS_TYPE_TRY_ICON_MENU,
                             static_cast<void *>(CreatePopupMenu()), -1, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        this->sysTry_menu    = static_cast<HMENU>(comBetweenWinp->extraParams);
        comBetweenWinp->hwnd = this->hwnd;
        strcpy(tnid.szTip, tip);
        strcpy(tnid.szInfoTitle, "Title");
        strcpy(tnid.szInfo, "ballon");
        res = Shell_NotifyIconA(NIM_ADD, &tnid);
        if (!res)
        {
            delete comBetweenWinp;
            return -1;
        }
        hasTryIcon = true;
        return comBetweenWinp->getId();
    }
    int WINDOW::addMenuTrayIcon(const char *str, const int idMenuTryIcon, const bool hasSubMenu,
                               const int position, const bool doubleClicked, const bool breakMenu,
                               const bool checked , USER_DRAWER *UserDrawer)
    {
        COM_BETWEEN_WINP *ptr      = getComBetweenWinpTryIcon(this->hwnd);
        HMENU *           pSubmenu = nullptr;
        if (ptr == nullptr || ptr->typeWindowWinPlus != WINPLUS_TYPE_TRY_ICON_MENU)
            return -1;
        HMENU systray_menu = static_cast<HMENU>(ptr->extraParams);
        if (systray_menu == nullptr)
            return -1;
        if (idMenuTryIcon != -1)
        {
            COM_BETWEEN_WINP *ptrSubMenu = getComBetweenWinp(idMenuTryIcon);
            if (ptrSubMenu && ptrSubMenu->typeWindowWinPlus == WINPLUS_TYPE_TRY_ICON_SUB_MENU)
                pSubmenu = static_cast<HMENU *>(ptrSubMenu->extraParams);
        }
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, ptr->onEventWinPlus, this, WINPLUS_TYPE_TRY_ICON_SUB_MENU, nullptr, -1, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        comBetweenWinp->hwnd = this->hwnd;
        if (pSubmenu)
        {
            MENUITEMINFOA menu;
            HMENU *       pmenu         = new HMENU();
            *pmenu                      = CreateMenu();
            comBetweenWinp->extraParams = pmenu;
            memset(&menu, 0, sizeof(MENUITEMINFOA));
            menu.cbSize      = sizeof(MENUITEMINFOA);
            int subMenu_flag = 0;
            if (hasSubMenu)
                subMenu_flag = MIIM_SUBMENU;
            menu.fMask       = MIIM_ID | MIIM_STRING | subMenu_flag;
            menu.fType       = MFT_STRING;
            menu.fState      = MFS_ENABLED;
            if (str)
            {
                menu.dwItemData = strlen(str);
                menu.dwTypeData = (LPSTR)str;
            }
            menu.wID      = comBetweenWinp->id;
            menu.hSubMenu = *pmenu;
            InsertMenuItemA(*pSubmenu, position, true, &menu);
            SetMenu(this->hwnd, systray_menu);
            SetMenu(this->hwnd, menu.hSubMenu);
            DrawMenuBar(this->hwnd);
            return comBetweenWinp->getId();
        }
        int _subMenu = 0;
        if (breakMenu)
            _subMenu = MF_MENUBARBREAK | _subMenu;
        if (checked)
            _subMenu = MFS_CHECKED | _subMenu;
        if (hasSubMenu)
        {
            HMENU *pmenu                = new HMENU();
            *pmenu                      = CreateMenu();
            comBetweenWinp->extraParams = pmenu;
            _subMenu                    = MFT_RADIOCHECK | _subMenu;
            MENUITEMINFOA menu;
            memset(&menu, 0, sizeof(MENUITEMINFOA));
            menu.cbSize      = sizeof(MENUITEMINFOA);
            int subMenu_flag = 0;
            if (hasSubMenu)
                subMenu_flag = MIIM_SUBMENU;
            menu.fMask       = MIIM_ID | MIIM_STRING | subMenu_flag;
            menu.fType       = MFT_STRING;
            menu.fState      = MFS_ENABLED;
            if (str)
            {
                menu.dwItemData = strlen(str);
                menu.dwTypeData = (LPSTR)str;
            }
            menu.wID      = comBetweenWinp->id;
            menu.hSubMenu = *pmenu;
            InsertMenuItemA(systray_menu, position, true, &menu);
            SetMenu(this->hwnd, systray_menu);
            SetMenu(this->hwnd, menu.hSubMenu);
            DrawMenuBar(this->hwnd);
        }
        else
        {
            AppendMenuA(systray_menu, MF_ENABLED | _subMenu, comBetweenWinp->getId(), str);
            if (doubleClicked)
                SetMenuDefaultItem(systray_menu, comBetweenWinp->getId(), FALSE);
        }
        return comBetweenWinp->getId();
    }
    int WINDOW::addSubMenuTrayIcon(const char *str, const int position, USER_DRAWER * UserDrawer)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinpTryIcon(this->hwnd);
        if (ptr == nullptr || ptr->typeWindowWinPlus != WINPLUS_TYPE_TRY_ICON_MENU)
            return -1;
        HMENU *pmenu       = new HMENU();
        *pmenu             = CreateMenu();
        HMENU systray_menu = static_cast<HMENU>(ptr->extraParams);
        if (systray_menu == nullptr)
        {
            delete pmenu;
            return -1;
        }
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, ptr->onEventWinPlus, this, WINPLUS_TYPE_TRY_ICON_SUB_MENU, pmenu, -1, UserDrawer);
        if (comBetweenWinp == nullptr)
        {
            return -1;
        }
        comBetweenWinp->hwnd = this->hwnd;
        MENUITEMINFOA menu;
        memset(&menu, 0, sizeof(MENUITEMINFOA));
        menu.cbSize = sizeof(MENUITEMINFOA);
        menu.fMask  = MIIM_ID | MIIM_STRING | MIIM_SUBMENU;
        menu.fType  = MFT_STRING;
        menu.fState = MFS_ENABLED;
        if (str)
        {
            menu.dwItemData = strlen(str);
            menu.dwTypeData = (LPSTR)str;
        }
        menu.wID      = comBetweenWinp->id;
        menu.hSubMenu = *pmenu;
        InsertMenuItemA(systray_menu, position, true, &menu);
        // AppendMenuA(menu.hSubMenu, MF_ENABLED, comBetweenWinp->getId(),str);
        // InsertMenuItemA(menu.hSubMenu,position,true,&menu);
        SetMenu(this->hwnd, systray_menu);
        SetMenu(this->hwnd, menu.hSubMenu);
        DrawMenuBar(this->hwnd);
        return comBetweenWinp->getId();
    }
    bool WINDOW::showBallonTrayIcon(const char *title, const char *message, int uTimeout, DWORD dwIcon)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinpTryIcon(this->hwnd);
        if (ptr == nullptr || ptr->typeWindowWinPlus != WINPLUS_TYPE_TRY_ICON_MENU)
            return false;
        tnid.uFlags = NIF_INFO;
        if (message && strlen(message) < 256)
            strcpy(tnid.szInfo, message);
        else if (message)
            strncpy(tnid.szInfo, message, 255);
        else
            strcpy(tnid.szInfo, "Sua mensagem!");

        if (title && strlen(title) < 64)
            strcpy(tnid.szInfoTitle, title);
        else if (title)
            strncpy(tnid.szInfoTitle, title, 63);
        else
            strcpy(tnid.szInfoTitle, "Titulo!");
        tnid.dwInfoFlags = dwIcon;
        if (uTimeout < 5)
            uTimeout = 5;
        if (uTimeout > 30)
            uTimeout  = 30;
        tnid.uTimeout = uTimeout * 1000;

        NOTIFYICONDATAW tnidw;
        memset(&tnidw, 0, sizeof(NOTIFYICONDATAW));
        tnidw.cbSize           = sizeof(NOTIFYICONDATAW);
        tnidw.dwInfoFlags      = tnid.dwInfoFlags;
        tnidw.dwState          = tnid.dwState;
        tnidw.dwStateMask      = tnid.dwStateMask;
        tnidw.guidItem         = tnid.guidItem;
        tnidw.hBalloonIcon     = tnid.hBalloonIcon;
        tnidw.hIcon            = tnid.hIcon;
        tnidw.hWnd             = tnid.hWnd;
        tnidw.uCallbackMessage = tnid.uCallbackMessage;
        tnidw.uFlags           = tnid.uFlags;
        tnidw.uID              = tnid.uID;
        tnidw.uTimeout         = tnid.uTimeout;
        tnidw.uVersion         = tnid.uVersion;
        WCHAR  textOut[1024]   = L"";
        WCHAR *szTmp           = toWchar(tnid.szInfo, textOut);
		if(szTmp)
			wcscpy(tnidw.szInfo, szTmp);
		else
			memset(tnidw.szInfo,0,sizeof(tnidw.szInfo));

        szTmp = toWchar(tnid.szInfoTitle, textOut);
		if(szTmp)
			wcscpy(tnidw.szInfoTitle, szTmp);
		else
			memset(tnidw.szInfoTitle,0,sizeof(tnidw.szInfoTitle));

        szTmp = toWchar(tnid.szTip, textOut);
		if(szTmp)
			wcscpy(tnidw.szTip, szTmp);
		else
			memset(tnidw.szTip,0,sizeof(tnidw.szTip));

        BOOL bSuccess  = Shell_NotifyIconW(NIM_MODIFY, &tnidw);
        tnid.szInfo[0] = 0;
        return (bSuccess = !0);
    }
    bool WINDOW::setTextTrayIcon(const char *text)
    {
        tnid.uFlags = NIF_TIP;
        strcpy(tnid.szTip, text);
        return Shell_NotifyIconA(NIM_MODIFY, &tnid) != 0;
    }
    bool WINDOW::printLastErrWindows(const char *where )
    {
        DWORD lerr = GetLastError();
        if (lerr)
        {
            char *message = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lerr, 0, (char *)&message,
                           0, nullptr);
            if (where)
                printf("%s %s\n", where, message);
            else
                printf("%s\n", message);
            LocalFree(message);
            return true;
        }
        return false;
    }
    int WINDOW::addToolTip(const char *tip, const int idDest , USER_DRAWER *dataToolTip)
    {
        if (this->hwnd == nullptr || tip == nullptr)
            return -1;
        COM_BETWEEN_WINP *ptrDest = mbm::getComBetweenWinp(idDest);
        if (ptrDest == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(ptrDest->hwnd, nullptr, this, WINPLUS_TYPE_TOOL_TIP, nullptr, idDest, dataToolTip);
        if (comBetweenWinp == nullptr)
            return -1;
        HINSTANCE g_hInst = (HINSTANCE)GetWindowLongA(this->hwnd, GWL_HWNDPARENT);
        // Create a tooltip.
        comBetweenWinp->hwnd =
            CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, ptrDest->hwnd, nullptr, g_hInst, nullptr);
        SetWindowPos(comBetweenWinp->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        // Set up "tool" information. In this case, the "tool" is the entire parent window.
        TOOLINFOA ti = {0};
        ti.cbSize    = sizeof(TOOLINFOA);
        ti.uFlags    = TTF_SUBCLASS;
        ti.hwnd      = ptrDest->hwnd;
        ti.hinst     = g_hInst;
        ti.lpszText  = (char *)tip;
        GetClientRect(ptrDest->hwnd, &ti.rect);
        // Associate the tooltip with the "tool" window.
        SendMessageA(comBetweenWinp->hwnd, TTM_ADDTOOLA, 0, (LPARAM)&ti);
        RECT margin = {0, 0, 10, 0};
        SendMessageA(comBetweenWinp->hwnd, TTM_SETMARGIN, 0, (LPARAM)&margin);
        int maxWidth = 1024;
        SendMessageA(comBetweenWinp->hwnd, TTM_SETMAXTIPWIDTH, 0, (LPARAM)&maxWidth);

        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(ToolTipProc));
        comBetweenWinp->graphWin = this->drawerDefault;
        return comBetweenWinp->getId();
    }
    int WINDOW::addButton(const char *title, long x, long y, long width, long height, const int idDest,
                         OnEventWinPlus onPressedByType, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_BUTTON, nullptr, idDest, UserDrawer);
        comBetweenWinp->hwnd =
            CreateWindowA("STATIC", title, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLongA(this->hwnd, GWL_HWNDPARENT), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addRadioButton(const char *title, long x, long y, long width, long height, const int idDest,
                              OnEventWinPlus onPressedByType, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_RADIO_BOX, nullptr, idDest, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        comBetweenWinp->hwnd =
            CreateWindowA("button", title, WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_OWNERDRAW, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addGroupBox(const char *title, long x, long y, long width, long height, const int idDest,
                           OnEventWinPlus onGotClickeOrFocus,USER_DRAWER * UserDrawer )
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onGotClickeOrFocus, this, WINPLUS_TYPE_GROUP_BOX, nullptr, idDest, UserDrawer);
        comBetweenWinp->hwnd =
            CreateWindowA("button", title, WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_OWNERDRAW, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    void WINDOW::killTimer(const int idTimer)
    {
        destroyTimer(this->hwnd, idTimer);
    }
    int WINDOW::addTimer(unsigned int timeMilliseconds, OnEventWinPlus onElapseTimer, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        TIMER *           timeElapsed = new TIMER(timeMilliseconds, -1, onElapseTimer);
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onElapseTimer, this, WINPLUS_TYPE_TIMER, timeElapsed, -1, UserDrawer);
        if (comBetweenWinp == nullptr || comBetweenWinp->owerHwnd == nullptr)
            return -1;
        comBetweenWinp->hwnd     = nullptr;
        comBetweenWinp->owerHwnd = this->hwnd;
        if (!SetTimer(this->hwnd, comBetweenWinp->getId(), timeMilliseconds, timerProc))
            return -1;
        timeElapsed->idTimer = comBetweenWinp->getId();
        return timeElapsed->idTimer;
    }
    void WINDOW::setPositionProgressBar(const int IdComponent, int position)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(IdComponent);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_PROGRESS_BAR)
        {
            position = position > 100 ? 100 : position;
            position = position < 0 ? 0 : position;
            SendMessageA(ptr->hwnd, PBM_SETPOS, position, 0);
            InvalidateRect(ptr->hwnd, 0, 0);
        }
    }
    int WINDOW::getPositionProgressBar(const int IdComponent)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(IdComponent);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_PROGRESS_BAR)
        {
            int position = SendMessageA(ptr->hwnd, PBM_GETPOS, 0, 0);
            return position;
        }
        return 0;
    }
    int WINDOW::addProgressBar(long x, long y, long width, long height, const int idDest, const bool vertical,
                              USER_DRAWER * UserDrawer )
    {
        if (this->hwnd == nullptr)
            return -1;
        PROGRESS_BAR_INFO *infoProgress = new PROGRESS_BAR_INFO(vertical);
        COM_BETWEEN_WINP * comBetweenWinp =
            getNewComBetween(this->hwnd, nullptr, this, WINPLUS_TYPE_PROGRESS_BAR, infoProgress, idDest, UserDrawer);
        char className[256];
        className[0] = 0;
        HWND hwndTo  = addToHwnd(idDest, comBetweenWinp);
        GetClassNameA(hwndTo, className, 255);
        if (className[0] == 0)
            return -1;
        // int verticalStyle = 0;
        // if(vertical)
        //  verticalStyle = PBS_VERTICAL;

        comBetweenWinp->hwnd =
            CreateWindowA("static", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        comBetweenWinp->graphWin = this->drawerDefault;
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(ProgressBarWindowProc));
        SendMessageA(comBetweenWinp->hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageA(comBetweenWinp->hwnd, PBM_SETSTEP, 1, 0);
        return comBetweenWinp->getId();
    }
    void WINDOW::setDefaultPositionTrackBar(const int idTrackBar, const short defaultPosition)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idTrackBar);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TRACK_BAR)
        {
            TRACK_BAR_INFO *infoTrackBar  = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
            infoTrackBar->defaultPosition = defaultPosition;
            DATA_EVENT data(ptr->id, infoTrackBar, ptr->userDrawer, mbm::WINPLUS_TYPE_TRACK_BAR, nullptr);
            if (ptr->onEventWinPlus)
                ptr->onEventWinPlus(ptr->ptrWindow, data);
            InvalidateRect(ptr->hwnd, 0, 0);
        }
    }
    void WINDOW::setTrackBar(const int idTrackBar, const float position)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idTrackBar);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TRACK_BAR)
        {
            TRACK_BAR_INFO *infoTrackBar = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
            if (infoTrackBar->invertMinMaxText)
                infoTrackBar->position = infoTrackBar->maxPosition - infoTrackBar->minPosition - position;
            else
                infoTrackBar->position = position;
            DATA_EVENT data(ptr->id, infoTrackBar, ptr->userDrawer, mbm::WINPLUS_TYPE_TRACK_BAR, nullptr);
            if (ptr->onEventWinPlus)
                ptr->onEventWinPlus(ptr->ptrWindow, data);
            InvalidateRect(ptr->hwnd, 0, 0);
        }
    }
    void WINDOW::setMaxPositionTrackBar(const int idTrackBar, const short maxPosition)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idTrackBar);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TRACK_BAR)
        {
            TRACK_BAR_INFO *infoTrackBar = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
            infoTrackBar->maxPosition    = maxPosition;
            DATA_EVENT data(ptr->id, infoTrackBar, ptr->userDrawer, mbm::WINPLUS_TYPE_TRACK_BAR, nullptr);
            if (ptr->onEventWinPlus)
                ptr->onEventWinPlus(ptr->ptrWindow, data);
            InvalidateRect(ptr->hwnd, 0, 0);
        }
    }
    TRACK_BAR_INFO * WINDOW::getInfoTrack(const int idTrackBar)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idTrackBar);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TRACK_BAR)
            return static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
        return nullptr;
    }
    float WINDOW::getPositionTrackBar(const int idTrackBar)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idTrackBar);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TRACK_BAR)
        {
            TRACK_BAR_INFO *info = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
            if (info->invertMinMaxText)
                return info->positionInverted;
            return info->position;
        }
        return 0.0f;
    }
    int WINDOW::addCombobox(long x, long y, long width, long height, OnEventWinPlus onPressedByType,
                           const int idDest, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_COMBO_BOX, nullptr, idDest, UserDrawer);

        if (comBetweenWinp == nullptr)
            return -1;
        if (height < 50)
            height = 50;
        height += 50;
        comBetweenWinp->hwnd =
            CreateWindowA("combobox", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED, x, y, width,
                          height, addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);

        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(ComboProc));
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addCheckBox(const char *title, long x, long y, long width, long height,
                           OnEventWinPlus onPressedByType, const int idDest, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_CHECK_BOX, nullptr, idDest, UserDrawer);
        comBetweenWinp->hwnd =
            CreateWindowA("button", title, WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_OWNERDRAW, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addRichText(const char *textIntoRichText, long x, long y, long width, long height, const int idDest ,
                           OnEventWinPlus onPressedByType, const bool vScroll, const bool hScroll,
                           USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        // static DWORD version  = GetVersion("ComCtl32.dll");
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_RICH_TEXT, nullptr, idDest, UserDrawer);
        int nScrool = 0;
        if (vScroll)
            nScrool = nScrool | WS_VSCROLL;
        if (hScroll)
            nScrool          = nScrool | WS_HSCROLL;
        comBetweenWinp->hwnd = CreateWindowA(
            "EDIT", "", WS_CHILD | WS_VISIBLE | nScrool | ES_LEFT | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL, x, y,
            width, height, addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
            (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        this->setTabStopPixelSize(1, comBetweenWinp->getId());
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(RichTextProc));
        if (textIntoRichText)
            SendMessageA(comBetweenWinp->hwnd, WM_SETTEXT, MAKEWPARAM(0, -1), (LPARAM)textIntoRichText);
        SetParent(comBetweenWinp->hwnd, this->hwnd);

        return comBetweenWinp->getId();
    }
    int WINDOW::addTextBox(const char *textIntoTextBox, long x, long y, long width, long height, const int idDest ,
                          OnEventWinPlus onPressedByType, const bool isPassword, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_TEXT_BOX, nullptr, idDest, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        int password = 0;
        if (isPassword)
            password = ES_PASSWORD;
        comBetweenWinp->hwnd =
            CreateWindowA("EDIT", textIntoTextBox,
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL | password | WS_TABSTOP, x, y,
                          width, height, addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(EditProc));
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        EDIT_TEXT_DATA *data        = new EDIT_TEXT_DATA(comBetweenWinp->getId());
        comBetweenWinp->extraParams = data;
        return comBetweenWinp->getId();
    }
    int WINDOW::addListBox(long x, long y, long width, long height, OnEventWinPlus onPressedByType,
                          const int idDest, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onPressedByType, this, WINPLUS_TYPE_LIST_BOX, nullptr, idDest, UserDrawer);

        if (comBetweenWinp == nullptr)
            return -1;
        comBetweenWinp->hwnd =
            CreateWindowA("listbox", "title", WS_CHILD | WS_VISIBLE | LBS_OWNERDRAWFIXED | LBS_STANDARD, x, y, width,
                          height < 50 ? 50 : height, addToHwnd(idDest, &x, &y, comBetweenWinp),
                          (HMENU)comBetweenWinp->getId(), (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    int WINDOW::addTrackBar(long x, long y, long width, long height, const int idDest,
                           OnEventWinPlus onChangeValue , float minPosition, float maxPosition,
                           float defaultPosition , float tickSmall , float tickLarge,
                           const bool invertValueText, const bool trackBarVertical, USER_DRAWER * UserDrawer )
    {
        if (this->hwnd == nullptr)
            return -1;
        TRACK_BAR_INFO *infoTrack   = new TRACK_BAR_INFO();
        infoTrack->maxPosition      = maxPosition;
        infoTrack->minPosition      = minPosition;
        infoTrack->position         = defaultPosition;
        infoTrack->tickLarge        = tickLarge;
        infoTrack->tickSmall        = tickSmall;
        infoTrack->isVertical       = trackBarVertical;
        infoTrack->invertMinMaxText = invertValueText;
        infoTrack->defaultPosition  = defaultPosition;
        assert(infoTrack->maxPosition > infoTrack->minPosition);
        COM_BETWEEN_WINP *comBetweenWinp =
            getNewComBetween(this->hwnd, onChangeValue, this, WINPLUS_TYPE_TRACK_BAR, infoTrack, idDest, UserDrawer);
        comBetweenWinp->hwnd =
            CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, x, y, width, height,
                          addToHwnd(idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                          (HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), nullptr);
        if (comBetweenWinp->hwnd == nullptr)
            return -1;
        hideDestinyNotVisible(idDest, comBetweenWinp->hwnd);
        comBetweenWinp->_oldProc = (WNDPROC)SetWindowLong(comBetweenWinp->hwnd, GWL_WNDPROC, long(TrackProc));
        SetParent(comBetweenWinp->hwnd, this->hwnd);
        return comBetweenWinp->getId();
    }
    void WINDOW::SetWindowTrans(int percent)
    {
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)((255 * percent) / 100), LWA_ALPHA);
    }
    void WINDOW::RemoveWindowTrans()
    {
        // Remove WS_EX_LAYERED from this window styles
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        // Ask the window and its children to repaint
        RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    void WINDOW::hideDestinyNotVisible(const int id, HWND myHwnd)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_GROUP_BOX_TAB ||
                ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_BUTTON_TAB)
            {
                ShowWindow(myHwnd, SW_HIDE);
            }
        }
    }
        WINDOW::__DO_MODAL_OBJ::__DO_MODAL_OBJ(mbm::WINDOW *me, mbm::WINDOW *myParent, OnDoModal onDoModalParent, const bool disabelParentWindow_)
            : disabelParentWindow(disabelParentWindow_)
        {
            w         = me;
            parent    = myParent;
            onDoModal = onDoModalParent;
        }
    LRESULT WINDOW::TrackProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr && ptr->ptrWindow)
        {
            switch (message)
            {
                case WM_PAINT:
                {
                    if (ptr->graphWin)
                        return _Do_default_Drawer(ptr, nullptr);
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_DRAWITEM:
                {
                    if (ptr->graphWin)
                        return _Do_default_Drawer(ptr, nullptr);
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_LBUTTONDOWN:
                {
                    ptr->ptrWindow->hwndLastTrackBar = window;
                    InvalidateRect(window, 0, 0);
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_MOUSEMOVE:
                {
                    if (ptr->onEventWinPlus)
                    {
                        TRACK_BAR_INFO *infoTrack = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
                        DATA_EVENT      data(ptr->id, infoTrack, ptr->userDrawer, mbm::WINPLUS_TYPE_TRACK_BAR, nullptr);
                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                    }
                    if (ptr->ptrWindow->hwndLastTrackBar == window)
                    {
                        ptr->ptrWindow->hwndLastPressed = window;
                        InvalidateRect(window, 0, 0);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_RBUTTONUP:
                case WM_LBUTTONUP:
                {
                    if (ptr->onEventWinPlus)
                    {
                        TRACK_BAR_INFO *infoTrack = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
                        DATA_EVENT      data(ptr->id, infoTrack, ptr->userDrawer, mbm::WINPLUS_TYPE_TRACK_BAR, nullptr);
                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                    }
                    ptr->ptrWindow->hwndLastTrackBar = nullptr;
                    InvalidateRect(window, 0, 0);
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::StatusProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            switch (message)
            {
                case SB_SETPARTS:
                {
                    std::vector<std::string> *lsStatusBar = static_cast<std::vector<std::string> *>(ptr->extraParams);
                    int                       parts       = wParam;
                    if ((int)lsStatusBar->size() != parts)
                        lsStatusBar->resize(parts);
                    InvalidateRect(window, nullptr, 0);
                    return 0;
                }
                break;
                case WM_SETTEXT:
                case SB_SETTEXTA:
                {
                    std::vector<std::string> *lsStatusBar = static_cast<std::vector<std::string> *>(ptr->extraParams);
                    const char *              text        = (const char *)(lParam);
                    const int                 index       = wParam;
                    if ((unsigned int)index < lsStatusBar->size())
                    {
                        (*lsStatusBar)[index] = text;
                        InvalidateRect(window, nullptr, 0);
                        return 0;
                    }
                    return 1;
                }
                break;
                case WM_NCPAINT:
                case WM_PAINT:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_DRAWITEM:
                {
                    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, lpdis);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::MessageBoxProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            switch (message)
            {
                case WM_NCPAINT:
                case WM_PAINT:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_DRAWITEM:
                {
                    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, lpdis);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::ScrollProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) // doesnt work
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            switch (message)
            {
                case WM_VSCROLL:
                {
                    if (ptr->extraParams)
                    {
                        SCROLLINFO *sc       = static_cast<SCROLLINFO *>(ptr->extraParams);
                        int         position = HIWORD(wParam);
                        int         request  = LOWORD(wParam);
                        std::string str;

                        switch (request)
                        {
                            case SB_BOTTOM: { str += "SB_BOTTOM";
                            }
                            break;
                            case SB_ENDSCROLL: { str += "SB_ENDSCROLL";
                            }
                            break;
                            case SB_LINEDOWN:
                            {
                                str += "SB_LINEDOWN";
                                sc->nPos++;
                            }
                            break;
                            case SB_LINEUP:
                            {
                                str += "SB_LINEUP";
                                sc->nPos--;
                            }
                            break;
                            case SB_PAGEDOWN:
                            {
                                str += "SB_PAGEDOWN";
                                sc->nPos += sc->nPage;
                            }
                            break;
                            case SB_PAGEUP:
                            {
                                str += "SB_PAGEUP";
                                sc->nPos -= sc->nPage;
                            }
                            break;
                            case SB_THUMBPOSITION:
                            {
                                str += "SB_THUMBPOSITION";
                                sc->nPos = position;
                            }
                            break;
                            case SB_THUMBTRACK: { str += "SB_THUMBTRACK";
                            }
                            break;
                            case SB_TOP: { str += "SB_TOP";
                            }
                            break;
                        }
                        char text[255];
                        sprintf(text, "Position %d :%10.10s handle:%ld\n", position, str.c_str(), lParam);
                        OutputDebugStringA(text);
                        if (sc->nPos > sc->nMax)
                            sc->nPos = sc->nMax;
                        if (sc->nPos < sc->nMin)
                            sc->nPos = sc->nMin;
                        SetScrollInfo(ptr->hwnd, SB_VERT, sc, true);
                        if (ptr->onEventWinPlus)
                        {
                            DATA_EVENT data(ptr->id, nullptr, ptr->userDrawer, WINPLUS_TYPE_SCROLL, nullptr);
                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                        }
                    }
                }
                break;
                case WM_HSCROLL:
                {
                    if (ptr->extraParams)
                    {
                        SCROLLINFO *sc       = static_cast<SCROLLINFO *>(ptr->extraParams);
                        int         position = HIWORD(wParam);
                        int         request  = LOWORD(wParam);
                        std::string str;

                        switch (request)
                        {
                            case SB_BOTTOM: { str += "SB_BOTTOM";
                            }
                            break;
                            case SB_ENDSCROLL: { str += "SB_ENDSCROLL";
                            }
                            break;
                            case SB_LINEDOWN:
                            {
                                str += "SB_LINEDOWN";
                                sc->nPos++;
                            }
                            break;
                            case SB_LINEUP:
                            {
                                str += "SB_LINEUP";
                                sc->nPos--;
                            }
                            break;
                            case SB_PAGEDOWN:
                            {
                                str += "SB_PAGEDOWN";
                                sc->nPos += sc->nPage;
                            }
                            break;
                            case SB_PAGEUP:
                            {
                                str += "SB_PAGEUP";
                                sc->nPos -= sc->nPage;
                            }
                            break;
                            case SB_THUMBPOSITION:
                            {
                                str += "SB_THUMBPOSITION";
                                sc->nPos = position;
                            }
                            break;
                            case SB_THUMBTRACK: { str += "SB_THUMBTRACK";
                            }
                            break;
                            case SB_TOP: { str += "SB_TOP";
                            }
                            break;
                        }
                        char text[255];
                        sprintf(text, "Position %d :%10.10s handle:%ld\n", position, str.c_str(), lParam);
                        OutputDebugStringA(text);
                        if (sc->nPos > sc->nMax)
                            sc->nPos = sc->nMax;
                        if (sc->nPos < sc->nMin)
                            sc->nPos = sc->nMin;
                        SetScrollInfo(ptr->hwnd, SB_VERT, sc, true);
                        if (ptr->onEventWinPlus)
                        {
                            DATA_EVENT data(ptr->id, nullptr, ptr->userDrawer, WINPLUS_TYPE_SCROLL, nullptr);
                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                        }
                    }
                }
                break;
                case WM_NCPAINT:
                case WM_PAINT:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_DRAWITEM:
                {
                    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, lpdis);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::WinNCProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
        if (ptr)
        {
            switch (message)
            {
                case WM_NCLBUTTONUP:
                case WM_NCLBUTTONDOWN:
                case WM_NCLBUTTONDBLCLK:
                {
                    COM_BETWEEN_WINP *ptrPrevious = ptr;
                    ptr                           = getComBetweenWinp(ptr->id + 1);
					if(ptr == nullptr)
						return 0;
                    if (ptr->typeWindowWinPlus != WINPLUS_TYPE_WINDOWNC)
                        return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    __NC_BORDERS::__NC_BUTTONS *ncButtons = (__NC_BORDERS::__NC_BUTTONS *)ptr->extraParams;
                    LRESULT                     ret       = 0;
                    if (message != WM_NCLBUTTONDBLCLK)
                        ret = CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    if (ncButtons->isHoverClose)
                    {
                        if (ptrPrevious->typeWindowWinPlus == WINPLUS_TYPE_WINDOW_MESSAGE_BOX)
                            PostMessageA(windowHandle, WM_CLOSE, 0, 0);
                        else if (ptrPrevious->typeWindowWinPlus == WINPLUS_TYPE_CHILD_WINDOW)
                            ptrPrevious->ptrWindow->hide(ptrPrevious->hwnd);
                        else
                            PostMessageA(windowHandle, WM_QUIT, 0, 0);
                    }
                    if (ncButtons->isHoverMax || message == WM_NCLBUTTONDBLCLK)
                    {
                        WINDOWPLACEMENT p;
                        memset(&p, 0, sizeof(p));
                        GetWindowPlacement(windowHandle, &p);
                        if (message == WM_NCLBUTTONDBLCLK)
                        {
                            if (ncButtons->hasMaximizeButton == false)
                            {
                                RedrawWindow(windowHandle, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                                return ret;
                            }
                        }
                        if (p.showCmd == SW_SHOWMAXIMIZED)
                            ShowWindow(windowHandle, SW_RESTORE);
                        else
                            ShowWindow(windowHandle, SW_MAXIMIZE);
                        ncButtons->isHoverMax = false;
                        RedrawWindow(windowHandle, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                    }
                    else if (ncButtons->isHoverMin)
                    {
                        ShowWindow(windowHandle, SW_MINIMIZE);
                        ncButtons->isHoverMin = false;
                    }
                    RedrawWindow(windowHandle, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                    return ret;
                }
                break;
                case WM_NCMOUSEMOVE:
                {
                    LRESULT ret = CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    RedrawWindow(windowHandle, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                    return ret;
                }
                break;
                case WM_MOVE:
                {
                    __NC_BORDERS nc(windowHandle, true, nullptr);
                    if (nc.status == false)
                        return 1;
                    if (ptr->typeWindowWinPlus == WINPLUS_TYPE_CHILD_WINDOW)
                        RedrawWindow(ptr->owerHwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                    else
                        InvalidateRect(ptr->hwnd,nullptr,0);
                    return 0;
                }
                break;
                case WM_MOVING:
                {
                    __NC_BORDERS nc(windowHandle, true, nullptr);
                    if (nc.status == false)
                        return 1;
                    if (ptr->typeWindowWinPlus == WINPLUS_TYPE_CHILD_WINDOW)
                        RedrawWindow(ptr->owerHwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                }
                break;
                case WM_NCHITTEST:
                {
                    ptr = getComBetweenWinp(ptr->id + 1);
					if(ptr == nullptr)
						return 0;
                    if (ptr->typeWindowWinPlus != WINPLUS_TYPE_WINDOWNC)
                        return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    __NC_BORDERS::__NC_BUTTONS *ncButtons = (__NC_BORDERS::__NC_BUTTONS *)ptr->extraParams;
                    UINT ret = CallWindowProc(ptr->_oldProc, windowHandle, WM_NCHITTEST, wParam, lParam);
                    // If the mouse is in the caption, then check to
                    // see if it is over one of our buttons
                    if (ret == HTCAPTION)
                    {
                        POINT pt   = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                        RECT  rect = {0, 0, 0, 0};
                        GetWindowRect(windowHandle, &rect);
                        pt.x -= rect.left;
                        pt.y -= rect.top;
                        if (PtInRect(&ncButtons->rectClose, pt))
                            ncButtons->isHoverClose = true;
                        else
                            ncButtons->isHoverClose = false;
                        if (PtInRect(&ncButtons->rectMaximize, pt))
                            ncButtons->isHoverMax = true;
                        else
                            ncButtons->isHoverMax = false;
                        if (PtInRect(&ncButtons->rectMinimize, pt))
                            ncButtons->isHoverMin = true;
                        else
                            ncButtons->isHoverMin = false;
                    }
                    return ret;
                }
                break;
                case WM_SETTEXT:
                case WM_NCACTIVATE:
                {
                    DWORD dwStyle = GetWindowLong(windowHandle, GWL_STYLE);
                    // Turn OFF WS_VISIBLE, so that WM_NCACTIVATE does not
                    // paint our window caption...
                    SetWindowLong(windowHandle, GWL_STYLE, dwStyle & ~WS_VISIBLE);
                    // Do the default thing..
                    UINT ret = CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    // Restore the original style
                    SetWindowLong(windowHandle, GWL_STYLE, dwStyle);
                    return ret;
                }
                break;
                case WM_PAINT:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        INT ret = _Do_default_Drawer(ptr, nullptr);
                        return ret;
                    }
                    return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                }
                break;
                case WM_NCPAINT:
                {
                    ptr = getComBetweenWinp(ptr->id + 1);
					if(ptr == nullptr)
						return 0;
                    if (ptr->typeWindowWinPlus != WINPLUS_TYPE_WINDOWNC || ptr->ptrWindow == nullptr ||
                        ptr->ptrWindow->run == false || ptr->ptrWindow->isVisible == false)
                    {
                        return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    }
                    __NC_BORDERS ncBorder(windowHandle, false, (__NC_BORDERS::__NC_BUTTONS *)ptr->extraParams);
                    if (ncBorder.status == false)
                        return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    HDC hdc = GetDCEx(windowHandle, ncBorder.hrgnAll,
                                      DCX_WINDOW | DCX_CACHE | DCX_INTERSECTRGN | DCX_LOCKWINDOWUPDATE);
                    if (hdc == nullptr)
                        return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                    const int      win_width  = ncBorder.rcWind.right - ncBorder.rcWind.left;
                    const int      win_height = ncBorder.rcWind.bottom - ncBorder.rcWind.top;
                    HDC            hdcMem     = CreateCompatibleDC(hdc);
                    HBITMAP        hbmMem     = CreateCompatibleBitmap(hdc, win_width, win_height);
                    HANDLE         hOld       = SelectObject(hdcMem, hbmMem);

                    struct USER_DRAWER_NC : public USER_DRAWER
                    {
                        USER_DRAWER_NC(void *That , DRAW *Draw ):USER_DRAWER(That,Draw)
                        {
                        }
                        virtual bool render(COMPONENT_INFO &) {return false;}//not used for NC
                    };
                    USER_DRAWER_NC userDrawerNc(&ncBorder, ptr->graphWin);

                    ptr->graphWin->hdcBack = hdcMem;
                    mbm::COMPONENT_INFO component(ptr, &ncBorder.rcWind, hdcMem, false, false, &userDrawerNc);
                    ptr->graphWin->infoActualComponent = &component;
                    ptr->graphWin->render(*ptr->graphWin->infoActualComponent);
                    ptr->graphWin->present(hdc, win_width, win_height);
                    SelectObject(hdcMem, hOld);
                    DeleteObject(hbmMem);
                    DeleteDC(hdcMem);
                    ReleaseDC(windowHandle, hdc);
                    RedrawWindow(windowHandle, &ncBorder.rcWind, ncBorder.hrgnAll, RDW_UPDATENOW);
					ptr->graphWin->infoActualComponent = nullptr;
                    /*for(unsigned int i=0; i< COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
                    {
                        COM_BETWEEN_WINP* ptrWinChild = COM_BETWEEN_WINP::lsComBetweenWinp[i];
                        if(ptrWinChild && (ptrWinChild->idOwner == -1 || ptrWinChild->owerHwnd == ptr->hwnd))
                        {
                            InvalidateRect(ptrWinChild->hwnd,0,0);
                        }
                    }*/
                    return 0;
                }
                default: { return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
                }
            }
            return CallWindowProc(ptr->_oldProc, windowHandle, message, wParam, lParam);
        }
        return 1;
    }
    LRESULT WINDOW::ComboProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            switch (message)
            {
                case WM_MOUSEWHEEL:
                case WM_VSCROLL:
                case WM_HSCROLL:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_KEYDOWN:
                case WM_KEYUP:
                {
                    int r = CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                    InvalidateRect(window, nullptr, 0);
                    return r;
                }
                break;
                case WM_PAINT:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_DRAWITEM:
                {
                    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, lpdis);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::UDProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            switch (message)
            {
                case WM_MOUSEMOVE:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_KEYDOWN:
                {
                    int r = CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                    InvalidateRect(window, nullptr, 0);
                    return r;
                }
                break;
                case WM_PAINT:
                case WM_DRAWITEM:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::ToolTipProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            switch (message)
            {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_KEYDOWN:
                {
                    int r = CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                    InvalidateRect(window, nullptr, 0);
                    return r;
                }
                break;
                case WM_PAINT:
                case WM_DRAWITEM:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::ProgressBarWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_PROGRESS_BAR)
        {
            PROGRESS_BAR_INFO *infoProgress = static_cast<PROGRESS_BAR_INFO *>(ptr->extraParams);
            switch (message)
            {
                case PBM_DELTAPOS:
                case PBM_GETBARCOLOR:
                case PBM_GETBKCOLOR: {
                }
                break;
                case PBM_GETPOS:
                {
                    int pos = (int)infoProgress->position;
                    return pos;
                }
                break;
                case PBM_GETRANGE:
                {
                    int    oldMax = (int)infoProgress->maxRange;
                    int    oldMin = (int)infoProgress->minRange;
                    LPARAM ret    = MAKELPARAM(oldMin, oldMax);
                    return ret;
                }
                break;
                case PBM_GETSTATE:
                case PBM_GETSTEP:
                case PBM_SETBARCOLOR:
                case PBM_SETBKCOLOR:
                case PBM_SETMARQUEE: {
                }
                break;
                case PBM_SETPOS:
                {
                    int old                = (int)infoProgress->position;
                    infoProgress->position = (float)((int)wParam);
                    return old;
                }
                break;
                case PBM_SETRANGE:
                {
                    int oldMax             = (int)infoProgress->maxRange;
                    int oldMin             = (int)infoProgress->minRange;
                    int min                = LOWORD(lParam);
                    int max                = HIWORD(lParam);
                    infoProgress->maxRange = (float)max;
                    infoProgress->minRange = (float)min;
                    LPARAM ret             = MAKELPARAM(oldMin, oldMax);
                    return ret;
                }
                case PBM_SETRANGE32:
                case PBM_SETSTATE:
                case PBM_SETSTEP:
                case PBM_STEPIT: {
                }
                break;
                case WM_DRAWITEM:
                case WM_PAINT: { return _Do_default_Drawer(ptr, nullptr);
                }
                case WM_ERASEBKGND: { return _Do_default_Drawer_BackGround(ptr);
                }
            }
            return CallWindowProcW((WNDPROC)ptr->_oldProc, window, message, wParam, lParam);
        }
        return 0;
    }
    LRESULT WINDOW::RichTextProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            if (ptr->_oldProc == nullptr)
                return 0;
            switch (message)
            {

                case WM_CHAR:
                {
                    switch (wParam)
                    {
                        case '\t':
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                CallWindowProc(ptr->_oldProc, window, message, (WPARAM)' ', lParam);
                            }
                            return 0;
                        }
                        break;
                        default:
                        {
                            DWORD iStart = 0;
                            DWORD iEnd   = 0;
                            SendMessageA(window, EM_GETSEL, (WPARAM)&iStart, (LPARAM)&iEnd);
                            if (iStart == iEnd && isalnum(wParam))
                            {
                                RECT       rect   = {0, 0, 10000, 10000};
                                const int  iCaret = (int)SendMessageA(window, EM_LINEINDEX, (WPARAM)-1, 0);
                                const int  iLine  = SendMessageA(window, EM_LINEFROMCHAR, (WPARAM)-1, 0);
                                const WORD length = (WORD)SendMessageA(window, EM_LINELENGTH, iCaret, 0) + 1;
                                GetClientRect(window, &rect);
                                char *myText = new char[length + 4];
                                memset(myText, 0, length + 4);
                                WORD *des         = (WORD *)myText;
                                *des              = (length);
                                const WORD copied = (WORD)SendMessageA(window, EM_GETLINE, (WPARAM)iLine, (LPARAM)myText);
                                if (copied)
                                {
                                    const SIZE mySize = mbm::DRAW::getSizeText(myText, window);
                                    const int  width  = rect.right - rect.left;
                                    delete[] myText;
                                    if (mySize.cx >= width)
                                    {
                                        const int nextPos        = iCaret + length + 1;
                                        const int totalCaracters = SendMessageA(window, WM_GETTEXTLENGTH, (WPARAM)-1, 0);
                                        if (nextPos > totalCaracters)
                                            SendMessageA(window, WM_CHAR, (WPARAM)VK_RETURN, 0);
                                        SendMessage(window, EM_SETSEL, (WPARAM)nextPos, (LPARAM)nextPos);
                                        const int iNextLine = SendMessageA(window, EM_LINEFROMCHAR, iStart + 2, 0);
                                        if (iNextLine > iLine)
                                            SendMessageA(window, WM_CHAR, wParam, 0);
                                        InvalidateRect(window, 0, 0);
                                        return 0;
                                    }
                                }
                                else
                                {
                                    delete[] myText;
                                }
                            }
                        }
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_MOUSEWHEEL:
                case WM_VSCROLL:
                case WM_HSCROLL:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_KEYDOWN:
                {
                    int r = CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                    InvalidateRect(window, nullptr, 0);
                    return r;
                }
                break;
                case WM_PAINT:
                case WM_DRAWITEM:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    LRESULT WINDOW::EditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(window);
        if (ptr)
        {
            if (ptr->_oldProc == nullptr)
                return 0;
            EDIT_TEXT_DATA *data   = static_cast<EDIT_TEXT_DATA *>(ptr->extraParams);
            const bool      isSpin = data && (data->spin != nullptr || data->spinf != nullptr);
            switch (message)
            {
                case WM_CHAR:
                {
                    if (!isSpin)
                        return (CallWindowProc(ptr->_oldProc, window, message, wParam, lParam));
                    char c = (char)wParam;
                    switch (c)
                    {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                        case '-':
                        case VK_DELETE:
                        case VK_BACK: return (CallWindowProc(ptr->_oldProc, window, message, wParam, lParam));
                        case ',': return (CallWindowProc(ptr->_oldProc, window, message, WPARAM('.'), lParam));
                        default: return 0;
                    }
                }
                break;
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_KEYDOWN:
                {
                    int r = CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                    InvalidateRect(window, nullptr, 0);
                    return r;
                }
                break;
                case WM_PAINT:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                case WM_DRAWITEM:
                {
                    if (ptr->ptrWindow && ptr->graphWin)
                    {
                        return _Do_default_Drawer(ptr, nullptr);
                    }
                    return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
                break;
                default: { return CallWindowProc(ptr->_oldProc, window, message, wParam, lParam);
                }
            }
        }
        return 0;
    }
    void WINDOW::_onTimeHover(mbm::WINDOW *w, DATA_EVENT &)
    {
        POINT p = {0, 0};
        if (GetCursorPos(&p))
        {
            if (w->lastPosMouse.x != p.x && w->lastPosMouse.y != p.y)
            {
                w->lastPosMouse = p;
                HWND hwndPoint  = WindowFromPoint(p);
                if (hwndPoint && hwndPoint != w->hwndLastHover)
                {
                    if (w->hwndLastTrackBar)
                    {
                        COM_BETWEEN_WINP *ptrTrackBar = getComBetweenWinp(w->hwndLastTrackBar);
                        if (ptrTrackBar && ptrTrackBar->onEventWinPlus)
                        {
                            TRACK_BAR_INFO *infoTrack = static_cast<TRACK_BAR_INFO *>(ptrTrackBar->extraParams);
                            DATA_EVENT      data(ptrTrackBar->id, infoTrack, ptrTrackBar->userDrawer,
                                            mbm::WINPLUS_TYPE_TRACK_BAR, nullptr);
                            ptrTrackBar->onEventWinPlus(ptrTrackBar->ptrWindow, data);
                        }
                        InvalidateRect(w->hwndLastTrackBar, nullptr, 0);
                        w->hwndLastTrackBar = nullptr;
                    }
                    HWND old         = w->hwndLastHover;
                    w->hwndLastHover = hwndPoint;
                    InvalidateRect(old, nullptr, 0);
                    LPARAM pMouse = MAKELPARAM(p.x, p.y);
                    SendMessageA(w->hwnd, WM_MOUSEHOVER, (WPARAM)w->hwndLastHover, pMouse);
                }
                else if (hwndPoint != w->hwndLastHover)
                {
                    HWND old         = w->hwndLastHover;
                    w->hwndLastHover = hwndPoint;
                    InvalidateRect(old, nullptr, 0);
                }
            }
        }
        else if (w->hwndLastHover)
        {
            HWND old         = w->hwndLastHover;
            w->hwndLastHover = nullptr;
            InvalidateRect(old, nullptr, 0);
        }
    }
    void WINDOW::setUserDrawer(int idComponent, USER_DRAWER *userDrawer) // user drawer para o componente
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
            ptr->userDrawer = userDrawer;
    }
    USER_DRAWER * WINDOW::getUserDrawer(int idComponent) // recupera user drawer para do componente
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
            return ptr->userDrawer;
        return nullptr;
    }
    void WINDOW::setIndexTabByGroup(const int idTabControlByGroup, const int index,
                                   const bool callOnEventWindowByType)
    {
        COM_BETWEEN_WINP *comBetweenWinpTabByGroupControl = mbm::getComBetweenWinp(idTabControlByGroup);
        if (comBetweenWinpTabByGroupControl == nullptr)
            return;
        if (comBetweenWinpTabByGroupControl->typeWindowWinPlus != WINPLUS_TYPE_BUTTON_TAB)
            return;
        if (comBetweenWinpTabByGroupControl->extraParams == nullptr)
            return;
        __TAB_GROUP_DESC *tabFather = static_cast<__TAB_GROUP_DESC *>(comBetweenWinpTabByGroupControl->extraParams);
        tabFather->tabSelected      = nullptr;
        for (unsigned int i = 0; i < tabFather->lsTabChilds.size(); ++i)
        {
            __TAB_GROUP_DESC *tabChild     = tabFather->lsTabChilds[i];
            COM_BETWEEN_WINP *c_GroupChild = mbm::getComBetweenWinp(tabChild->idGroupTabBox);
            if (tabChild->index == index)
            {
                tabFather->tabSelected = tabChild;
                if (tabFather->enableVisibleGroups)
                    this->show(c_GroupChild->hwnd);
                else
                    this->hide(c_GroupChild->hwnd);
            }
            else
            {
                this->hide(c_GroupChild->hwnd);
            }
            for (unsigned int j = 0; j < tabChild->lsHwndComponents.size(); ++j)
            {
                HWND hwndChild = *tabChild->lsHwndComponents[j];
                if (hwndChild)
                {
                    if (tabChild->index == index)
                        this->show(hwndChild);
                    else
                        this->hide(hwndChild);
                }
            }
        }
        InvalidateRect(comBetweenWinpTabByGroupControl->owerHwnd, 0, 0);
        if (callOnEventWindowByType && comBetweenWinpTabByGroupControl->onEventWinPlus && tabFather->tabSelected)
        {
            DATA_EVENT data(comBetweenWinpTabByGroupControl->id, tabFather->tabSelected,
                            comBetweenWinpTabByGroupControl->userDrawer, mbm::WINPLUS_TYPE_BUTTON_TAB, nullptr);
            comBetweenWinpTabByGroupControl->onEventWinPlus(comBetweenWinpTabByGroupControl->ptrWindow, data);
        }
        for (unsigned int i = 0; i < tabFather->lsTabChilds.size(); ++i)
        {
            __TAB_GROUP_DESC *tabChild = tabFather->lsTabChilds[i];
            int               idButton = tabChild->idGroupTabBox - 1;
            COM_BETWEEN_WINP *ptrBtn   = mbm::getComBetweenWinp(idButton);
            if (ptrBtn && ptrBtn->typeWindowWinPlus == WINPLUS_TYPE_BUTTON_TAB)
            {
                InvalidateRect(ptrBtn->hwnd, 0, 0);
            }
        }
    }
    int WINDOW::addTabControlByGroup(long x, long y, long width, long height, long widthButton, long heightButton,
                                    OnEventWinPlus onEventWindowByIndexTab, const int idDest ,
                                    const bool enableVisibleGroups, USER_DRAWER * UserDrawer)
    {
        if (this->hwnd == nullptr)
            return -1;
        if (widthButton <= 0)
            widthButton           = 100;
        __TAB_GROUP_DESC *tabDesc = new __TAB_GROUP_DESC(-1, idDest);

        COM_BETWEEN_WINP *comBetweenWinp = getNewComBetween(this->hwnd, onEventWindowByIndexTab, this,
                                                            WINPLUS_TYPE_BUTTON_TAB, tabDesc, tabDesc->idDest, UserDrawer);
        if (comBetweenWinp == nullptr)
            return -1;
        tabDesc->width               = width;
        tabDesc->height              = height;
        tabDesc->widthButton         = widthButton;
        tabDesc->heightButton        = heightButton;
        tabDesc->x                   = x;
        tabDesc->y                   = y;
        tabDesc->enableVisibleGroups = enableVisibleGroups;
        tabDesc->idTabControlByGroup = comBetweenWinp->getId();
        return tabDesc->idTabControlByGroup;
    }
    int WINDOW::addTabByGroup(const char *title, const int idTabControlByGroup, const bool newCloumn ,
                             const long newWidth, USER_DRAWER *UserDataButton, USER_DRAWER *UserDrawerGroup )
    {
        if (this->hwnd == nullptr)
            return -1;
        COM_BETWEEN_WINP *comBetweenWinpTabByGroupControl = mbm::getComBetweenWinp(idTabControlByGroup);
        if (comBetweenWinpTabByGroupControl == nullptr)
            return -1;
        if (comBetweenWinpTabByGroupControl->typeWindowWinPlus != WINPLUS_TYPE_BUTTON_TAB)
            return -1;
        if (comBetweenWinpTabByGroupControl->extraParams == nullptr)
            return -1;
        __TAB_GROUP_DESC *tabFather = static_cast<__TAB_GROUP_DESC *>(comBetweenWinpTabByGroupControl->extraParams);
        if (newWidth)
            tabFather->widthButton = newWidth;
        long width                 = tabFather->width;
        long height                = tabFather->height;
        long widthButton           = tabFather->widthButton;
        long heightButton          = tabFather->heightButton;
        long x                     = tabFather->x;
        long y                     = tabFather->y;

        __TAB_GROUP_DESC *tabChild    = new __TAB_GROUP_DESC(tabFather->lsTabChilds.size(), tabFather->idDest);
        tabChild->idTabControlByGroup = tabFather->idTabControlByGroup;
        if (newCloumn)
        {
            tabFather->displacementX = 0;
            tabFather->displacementY++;
        }
        // button
        {
            COM_BETWEEN_WINP *comBetweenWinp =
                getNewComBetween(this->hwnd, comBetweenWinpTabByGroupControl->onEventWinPlus, this,
                                 WINPLUS_TYPE_BUTTON_TAB, tabChild, tabFather->idDest, UserDataButton);

            comBetweenWinp->hwnd =
                CreateWindowA("STATIC", title, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
                              (tabFather->displacementX * widthButton) + x, (tabFather->displacementY * heightButton) + y,
                              widthButton, heightButton, addToHwnd(tabFather->idDest, &x, &y, comBetweenWinp),
                              (HMENU)comBetweenWinp->getId(), (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);
            SetParent(comBetweenWinp->hwnd, this->hwnd);
            tabFather->displacementX++;
            hideDestinyNotVisible(tabFather->idDest, comBetweenWinp->hwnd);
        }
        // Group box
        {
            if (newCloumn)
                moveTabByGroup(idTabControlByGroup, 0, heightButton);
            COM_BETWEEN_WINP *comBetweenWinp =
                getNewComBetween(this->hwnd, comBetweenWinpTabByGroupControl->onEventWinPlus, this,
                                 WINPLUS_TYPE_GROUP_BOX_TAB, tabChild, tabFather->idDest, UserDrawerGroup);
            comBetweenWinp->hwnd =
                CreateWindowA("STATIC", title, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, x,
                              y + heightButton + (tabFather->displacementY * heightButton), width, height,
                              addToHwnd(tabFather->idDest, &x, &y, comBetweenWinp), (HMENU)comBetweenWinp->getId(),
                              (HINSTANCE)GetWindowLong(this->hwnd, GWL_HINSTANCE), nullptr);

            if (comBetweenWinp->hwnd == nullptr)
                return -1;
            hideDestinyNotVisible(tabFather->idDest, comBetweenWinp->hwnd);
            tabChild->idGroupTabBox = comBetweenWinp->getId();
            SetParent(comBetweenWinp->hwnd, this->hwnd);
            hideDestinyNotVisible(tabFather->idDest, comBetweenWinp->hwnd);
        }
        tabFather->lsTabChilds.push_back(tabChild);
        tabChild->tabFather = tabFather;
        this->setIndexTabByGroup(idTabControlByGroup, 0, false);
        return tabChild->idGroupTabBox;
    }
    bool WINDOW::clear(const int idComponent)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_LIST_BOX)
            {
                std::vector<std::string *> *lsListBox = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                for (unsigned int i = 0; i < lsListBox->size(); ++i)
                {
                    std::string *strList = lsListBox->at(i);
                    delete strList;
                }
                lsListBox->clear();
                bool ret = SUCCEEDED(SendMessageA(ptr->hwnd, LB_RESETCONTENT, 0, 0));
                InvalidateRect(ptr->hwnd, 0, 0);
                return ret;
            }
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_COMBO_BOX)
            {
                std::vector<std::string *> *lsCombo = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                if (lsCombo)
                {
                    for (unsigned int i = 0; i < lsCombo->size(); ++i)
                    {
                        std::string *strList = lsCombo->at(i);
                        delete strList;
                    }
                    lsCombo->clear();
                }
                bool ret = SUCCEEDED(SendMessageA(ptr->hwnd, CB_RESETCONTENT, 0, 0));
                InvalidateRect(ptr->hwnd, 0, 0);
                return ret;
            }
            else
            {
                return this->setText(idComponent, "");
            }
        }
        return false;
    }
    bool WINDOW::getRadioButtonState(const int idRadioButton)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idRadioButton);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_RADIO_BOX)
        {
            RADIO_GROUP *radio = static_cast<RADIO_GROUP *>(ptr->extraParams);
            return radio->checked;
        }
        return false;
    }
    bool WINDOW::setNextFocus(const int idComponent, const int idComponentNextFocus)
    {
        COM_BETWEEN_WINP *ptr     = getComBetweenWinp(idComponent);
        COM_BETWEEN_WINP *ptrNext = getComBetweenWinp(idComponentNextFocus);
        if (ptr && ptrNext)
        {
            ptr->idNextFocus = ptrNext->id;
            return true;
        }
        return false;
    }
    bool WINDOW::setRadioButtonState(const int idRadioButton)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idRadioButton);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_RADIO_BOX)
        {
            RADIO_GROUP *radio = static_cast<RADIO_GROUP *>(ptr->extraParams);
            if (radio)
            {
                radio->checked = true;
                InvalidateRect(ptr->hwnd, nullptr, 0);
                for (std::set<int>::const_iterator it = radio->lsRadioGroup.cbegin(); it != radio->lsRadioGroup.cend();
                     ++it)
                {
                    const int         id         = *it;
                    COM_BETWEEN_WINP *otherRadio = getComBetweenWinp(id);
                    if (otherRadio && otherRadio->id != ptr->id)
                    {
                        RADIO_GROUP *radio2 = static_cast<RADIO_GROUP *>(otherRadio->extraParams);
                        if (radio2)
                        {
                            radio2->checked = false;
                            InvalidateRect(otherRadio->hwnd, nullptr, 0);
                        }
                    }
                }
                DATA_EVENT data(ptr->id, radio, ptr->userDrawer, mbm::WINPLUS_TYPE_RADIO_BOX, nullptr);
                if (ptr->onEventWinPlus)
                    ptr->onEventWinPlus(ptr->ptrWindow, data);
            }
        }
        return false;
    }
    bool WINDOW::setCheckBox(const bool checked, const int idCheckBox)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idCheckBox);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_CHECK_BOX)
        {
            int *value = static_cast<int *>(ptr->extraParams);
            *value     = checked ? 1 : 0;
            InvalidateRect(ptr->hwnd, 0, 0);
            return true;
        }
        return false;
    }
    bool WINDOW::getCheckBoxState(const int idCheckBox)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idCheckBox);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_CHECK_BOX)
        {
            const int value = *static_cast<int *>(ptr->extraParams);
            return value ? true : false;
        }
        return false;
    }
    bool WINDOW::addText(const int idComponent, const char *text)
    {
        if (text == nullptr)
            return false;
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_COMBO_BOX)
            {
                std::vector<std::string *> *lsCombo = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                lsCombo->push_back(new std::string(text));
                const char *newAdressString = lsCombo->at(lsCombo->size() - 1)->c_str();
                return SUCCEEDED(SendMessageA(ptr->hwnd, CB_ADDSTRING, 0, (long)newAdressString));
            }
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_LIST_BOX)
            {
                std::vector<std::string *> *lsListBox = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                lsListBox->push_back(new std::string(text));
                const char *newPtrAdressStr = lsListBox->at(lsListBox->size() - 1)->c_str();
                return SUCCEEDED(SendMessageA(ptr->hwnd, LB_ADDSTRING, 0, (long)newPtrAdressStr));
            }
        }
        return false;
    }
    bool WINDOW::removeText(const int idComponent, const int indexString)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            int index = indexString;
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_COMBO_BOX)
            {
                if (index <= -1)
                    index                           = (int)SendMessageA(ptr->hwnd, CB_GETCURSEL, 0, 0);
                std::vector<std::string *> *lsCombo = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                if ((unsigned int)index < lsCombo->size())
                {
                    std::string *strList = lsCombo->at(index);
                    lsCombo->erase(lsCombo->begin() + index);
                    delete strList;
                }
                bool ret = SUCCEEDED(SendMessageA(ptr->hwnd, CB_DELETESTRING, index, 0));
                InvalidateRect(ptr->hwnd, 0, 0);
                return ret;
            }
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_LIST_BOX)
            {
                if (index <= -1)
                    index                             = (int)SendMessageA(ptr->hwnd, LB_GETCURSEL, 0, 0);
                std::vector<std::string *> *lsListBox = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                if ((unsigned int)index < lsListBox->size())
                {
                    std::string *strList = lsListBox->at(index);
                    lsListBox->erase(lsListBox->begin() + index);
                    delete strList;
                }
                bool ret = SUCCEEDED(SendMessageA(ptr->hwnd, LB_DELETESTRING, index, 0));
                InvalidateRect(ptr->hwnd, 0, 0);
                return ret;
            }
        }
        return false;
    }
    bool WINDOW::setSelectedIndex(const int idComponent, const int indexString)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_COMBO_BOX)
            {
                bool ret SUCCEEDED(SendMessageA(ptr->hwnd, CB_SETCURSEL, indexString, 0));
                InvalidateRect(ptr->hwnd, 0, 0);
                return ret;
            }
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_LIST_BOX)
            {
                bool ret = SUCCEEDED(SendMessageA(ptr->hwnd, LB_SETCURSEL, indexString, 0));
                InvalidateRect(ptr->hwnd, 0, 0);
                return ret;
            }
        }
        return false;
    }
    int WINDOW::getSelectedIndex(const int idComponent)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_COMBO_BOX)
                return (int)SendMessageA(ptr->hwnd, CB_GETCURSEL, 0, 0);
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_LIST_BOX)
                return (int)SendMessageA(ptr->hwnd, LB_GETCURSEL, 0, 0);
            else
            {
                const int iCaret = SendMessageA(ptr->hwnd, EM_LINEINDEX, (WPARAM)-1, 0);
                return iCaret;
            }
        }
        return -1;
    }
    int WINDOW::getTextCount(const int idComponent)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_COMBO_BOX)
                return (int)SendMessageA(ptr->hwnd, CB_GETCOUNT, 0, 0);
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_LIST_BOX)
                return (int)SendMessageA(ptr->hwnd, LB_GETCOUNT, 0, 0);
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_RICH_TEXT)
                return (int)SendMessageA(ptr->hwnd, EM_GETLINECOUNT, 0, 0);
        }
        return 0;
    }
    mbm::SPIN_PARAMSf * WINDOW::getSpinf(const int idSpinf)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idSpinf);
        if (ptr && ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_SPIN_FLOAT)
        {
            SPIN_PARAMSf *editData = static_cast<SPIN_PARAMSf *>(ptr->extraParams);
            return editData;
        }
        return 0;
    }
    mbm::SPIN_PARAMSi * WINDOW::getSpin(const int idSpin)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idSpin);
        if (ptr && ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_SPIN_INT)
        {
            SPIN_PARAMSi *editData = static_cast<SPIN_PARAMSi *>(ptr->extraParams);
            return editData;
        }
        return nullptr;
    }
    bool WINDOW::updateSpin(const int idSpin)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idSpin);
        if (ptr)
        {
            if (ptr->typeWindowWinPlus == WINPLUS_TYPE_SPIN_INT)
            {
                mbm::SPIN_PARAMSi *spin = static_cast<SPIN_PARAMSi *>(ptr->extraParams);
                if (spin)
                {
                    char num[255];
                    if (spin->currentPosition > spin->max)
                        spin->currentPosition = spin->max;
                    else if (spin->currentPosition < spin->min)
                        spin->currentPosition = spin->min;
                    sprintf(num, "%d", spin->currentPosition);
                    ptr->ptrWindow->setText(ptr->id + 1, num);
                    return true;
                }
            }
            else if (ptr->typeWindowWinPlus == WINPLUS_TYPE_SPIN_FLOAT)
            {
                mbm::SPIN_PARAMSf *spinf = static_cast<mbm::SPIN_PARAMSf *>(ptr->extraParams);
                if (spinf)
                {
                    char num[255];
                    if (spinf->currentPosition > spinf->maxf)
                        spinf->currentPosition = spinf->maxf;
                    else if (spinf->currentPosition < spinf->minf)
                        spinf->currentPosition = spinf->minf;
                    sprintf(num, "%0.*f", spinf->precision, spinf->currentPosition);
                    this->setText(ptr->id + 1, num);
                    return true;
                }
            }
        }
        return false;
    }
    bool WINDOW::setFocus(const int idComponent )
    {
        if (idComponent > -1)
        {
            COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
            if (ptr)
                return SetFocus(ptr->hwnd) == ptr->hwnd;
            return false;
        }
        return SetFocus(this->hwnd) == this->hwnd;
    }
    void WINDOW::forceFocus()
    {
        EnableWindow(this->hwnd, TRUE);
        SetForegroundWindow(this->hwnd);
        SetCapture(this->hwnd);
        SetFocus(this->hwnd);
        SetActiveWindow(this->hwnd);
        BringWindowToTop(this->hwnd);
    }
    bool WINDOW::setText(const int IdComponent, const char *stringSource, int index )
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(IdComponent);
        if (ptr)
        {
            switch (ptr->typeWindowWinPlus)
            {
                case WINPLUS_TYPE_TOOL_TIP:
                {
                    TOOLINFOA ti = {0};
                    ti.cbSize    = sizeof(TOOLINFOA);
                    ti.uFlags    = TTF_SUBCLASS;
                    ti.hwnd      = ptr->owerHwnd;
                    ti.hinst     = (HINSTANCE)GetWindowLongA(this->hwnd, GWL_HWNDPARENT);
                    ti.lpszText  = (char *)stringSource;
                    GetClientRect(ptr->owerHwnd, &ti.rect);
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, TTM_UPDATETIPTEXTA, 0, (LPARAM)&ti)));
                }
                break;
                case WINPLUS_TYPE_WINDOW_MESSAGE_BOX:
                case WINPLUS_TYPE_WINDOW:
                case WINPLUS_TYPE_WINDOWNC:
                case WINPLUS_TYPE_LABEL:
                case WINPLUS_TYPE_BUTTON:
                case WINPLUS_TYPE_CHECK_BOX:
                case WINPLUS_TYPE_RADIO_BOX:
                case WINPLUS_TYPE_BUTTON_TAB:
                {
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_SETTEXT, 0, (LPARAM)stringSource)));
                }
                break;
                case WINPLUS_TYPE_COMBO_BOX:
                {
                    if (index == -1)
                        index = SendMessageA(ptr->hwnd, CB_GETCURSEL, 0, 0);
                    return SUCCEEDED(SendMessageA(ptr->hwnd, CB_SETCURSEL, index, 0));
                }
                break;
                case WINPLUS_TYPE_LIST_BOX:
                {
                    if (index == -1)
                        index = SendMessageA(ptr->hwnd, LB_GETCURSEL, 0, 0);
                    return SUCCEEDED(SendMessageA(ptr->hwnd, LB_SETCURSEL, index, 0));
                }
                break;
                case WINPLUS_TYPE_TEXT_BOX:
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_SETTEXT, index < 0 ? 0 : index, (LPARAM)stringSource)));
                    break;
                case WINPLUS_TYPE_SCROLL:
                case WINPLUS_TYPE_SPIN_INT:
                {
                    int                num  = std::atoi(stringSource);
                    mbm::SPIN_PARAMSi *spin = static_cast<mbm::SPIN_PARAMSi *>(ptr->extraParams);
                    if (spin)
                    {
                        if (num >= spin->min && num <= spin->max)
                        {
                            spin->currentPosition = num;
                            this->updateSpin(ptr->id);
                            return true;
                        }
                    }
                    return false;
                }
                break;
                case WINPLUS_TYPE_SPIN_FLOAT:
                {
                    float              num  = (float)atof(stringSource);
                    mbm::SPIN_PARAMSf *spin = static_cast<mbm::SPIN_PARAMSf *>(ptr->extraParams);
                    if (spin)
                    {
                        if (num >= spin->minf && num <= spin->maxf)
                        {
                            spin->currentPosition = num;
                            this->updateSpin(ptr->id);
                            return true;
                        }
                    }
                    return false;
                }
                break;
                case WINPLUS_TYPE_RICH_TEXT:
                {
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_SETTEXT, index, (LPARAM)stringSource)));
                }
                break;
                case WINPLUS_TYPE_CHILD_WINDOW:
                case WINPLUS_TYPE_GROUP_BOX:
                case WINPLUS_TYPE_GROUP_BOX_TAB:
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_SETTEXT, 0, (LPARAM)stringSource)));
                case WINPLUS_TYPE_TIMER:
                case WINPLUS_TYPE_IMAGE: return false;
                case WINPLUS_TYPE_TRACK_BAR:
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_SETTEXT, 0, (LPARAM)stringSource)));
                case WINPLUS_TYPE_STATUS_BAR:
                {
                    std::vector<std::string> *lsStatusBar = static_cast<std::vector<std::string> *>(ptr->extraParams);
                    index                                 = index < 0 ? 0 : index;
                    if (index < (int)lsStatusBar->size())
                    {
                        (*lsStatusBar)[index] = stringSource;
                        InvalidateRect(ptr->hwnd, 0, 0);
                        return true;
                    }
                }
                break;
                case WINPLUS_TYPE_TRY_ICON_MENU:
                case WINPLUS_TYPE_TRY_ICON_SUB_MENU:
                    tnid.uFlags = NIF_TIP;
                    strcpy(tnid.szTip, stringSource);
                    SetWindowTextA(HWND(IdComponent), stringSource);
                    return Shell_NotifyIconA(NIM_MODIFY, &tnid) != 0;
                    break;
                case WINPLUS_TYPE_MENU:
                {
                    __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                    if (menu)
                    {
                        return (SUCCEEDED(SendMessageA(menu->hwnd, WM_SETTEXT, 0, (LPARAM)stringSource)));
                    }
                }
                break;
                case WINPLUS_TYPE_SUB_MENU:
                {
                    __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                    if (menu)
                    {
                        if (index < (int)menu->lsSubMenusTitles.size())
                        {
                            menu->lsSubMenusTitles[index] = stringSource ? stringSource : "";
                            InvalidateRect(menu->hwndSubMenu, 0, 0);
                            return true;
                        }
                    }
                }
                break;
                default: return false;
            }
        }
        return false;
    }
    bool WINDOW::getText(const int IdComponent, char *stringOut, const WORD sizeStringOut, int index)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(IdComponent);
        if (ptr)
        {
            switch (ptr->typeWindowWinPlus)
            {
                case WINPLUS_TYPE_IMAGE: return false;
                case WINPLUS_TYPE_WINDOW_MESSAGE_BOX:
                case WINPLUS_TYPE_WINDOW:
                case WINPLUS_TYPE_WINDOWNC:
                case WINPLUS_TYPE_LABEL:
                case WINPLUS_TYPE_BUTTON:
                case WINPLUS_TYPE_BUTTON_TAB:
                case WINPLUS_TYPE_CHECK_BOX:
                case WINPLUS_TYPE_RADIO_BOX:
                case WINPLUS_TYPE_TOOL_TIP:
                {
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_GETTEXT, sizeStringOut, (LPARAM)stringOut)));
                }
                break;
                case WINPLUS_TYPE_COMBO_BOX:
                {
                    if (index == -1)
                        index = SendMessageA(ptr->hwnd, CB_GETCURSEL, 0, 0);
                    if (index > -1)
                    {
                        int *ret = (int *)SendMessageA(ptr->hwnd, CB_GETITEMDATA, index, (LPARAM)stringOut);
                        if ((int)ret == -1)
                            return false;
                        const char *text = (const char *)(ret);
                        if (ret && text && strlen(text) < sizeStringOut)
                        {
                            strcpy(stringOut, text);
                            return true;
                        }
                    }
                    return false;
                }
                break;
                case WINPLUS_TYPE_LIST_BOX:
                {
                    if (index == -1)
                        index = SendMessageA(ptr->hwnd, LB_GETCURSEL, 0, 0);
                    return SUCCEEDED(SendMessageA(ptr->hwnd, LB_GETTEXT, (WPARAM)index, (LPARAM)stringOut));
                }
                break;
                case WINPLUS_TYPE_TEXT_BOX:
                {
                    index = index < 0 ? 0 : index;
                    memset(stringOut, 0, sizeStringOut);
                    WORD *des  = (WORD *)stringOut;
                    *des       = (sizeStringOut - 1);
                    int Length = SendMessageA(ptr->hwnd, EM_GETLINE, 0, (LPARAM)stringOut);
                    if (Length == 1)
                        stringOut[1] = 0;
                    else if (Length && (Length < sizeStringOut))
                        stringOut[Length] = 0;
                    else if (Length && sizeStringOut)
                        stringOut[sizeStringOut - 1] = 0;
                    return Length > 0;
                }
                break;
                case WINPLUS_TYPE_SCROLL:
                case WINPLUS_TYPE_SPIN_INT:
                {
                    mbm::SPIN_PARAMSi *spin = static_cast<mbm::SPIN_PARAMSi *>(ptr->extraParams);
                    if (spin)
                    {
                        snprintf(stringOut, sizeStringOut, "%d", spin->currentPosition);
                        return true;
                    }
                    return false;
                }
                break;
                case WINPLUS_TYPE_SPIN_FLOAT:
                {
                    mbm::SPIN_PARAMSf *spin = static_cast<mbm::SPIN_PARAMSf *>(ptr->extraParams);
                    if (spin)
                    {
                        snprintf(stringOut, sizeStringOut, "%0.*f", spin->precision, spin->currentPosition);
                        return true;
                    }
                    return false;
                }
                break;
                case WINPLUS_TYPE_RICH_TEXT:
                {
                    if (index <= -1)
                        return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_GETTEXT, sizeStringOut, (LPARAM)stringOut)));
                    const WORD length = (WORD)SendMessageA(ptr->hwnd, EM_LINELENGTH, index, 0);
                    if (length && sizeStringOut)
                    {
                        char *myText = new char[length + 4];
                        memset(myText, 0, length + 4);
                        WORD *des         = (WORD *)myText;
                        *des              = (length);
                        const WORD copied = (WORD)SendMessageA(ptr->hwnd, EM_GETLINE, (WPARAM)index, (LPARAM)myText);
                        if (copied == length)
                        {
                            strncpy_s(stringOut, sizeStringOut, myText, sizeStringOut);
                            delete[] myText;
                            return true;
                        }
                        delete[] myText;
                    }
                    return false;
                }
                break;
                case WINPLUS_TYPE_CHILD_WINDOW:
                case WINPLUS_TYPE_GROUP_BOX:
                case WINPLUS_TYPE_GROUP_BOX_TAB:
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_GETTEXT, sizeStringOut, (LPARAM)stringOut)));
                case WINPLUS_TYPE_PROGRESS_BAR:
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_GETTEXT, sizeStringOut, (LPARAM)stringOut)));
                case WINPLUS_TYPE_TIMER: return false;
                case WINPLUS_TYPE_TRACK_BAR:
                    return (SUCCEEDED(SendMessageA(ptr->hwnd, WM_GETTEXT, sizeStringOut, (LPARAM)stringOut)));
                case WINPLUS_TYPE_STATUS_BAR:
                {
                    std::vector<std::string> *lsStatusBar = static_cast<std::vector<std::string> *>(ptr->extraParams);
                    index                                 = index < 0 ? 0 : index;
                    if ((unsigned int)index < lsStatusBar->size())
                    {
                        const char *text = (*lsStatusBar)[index].c_str();
                        strncpy(stringOut, text, sizeStringOut);
                        return true;
                    }
                }
                break;
                case WINPLUS_TYPE_TRY_ICON_MENU:
                case WINPLUS_TYPE_TRY_ICON_SUB_MENU:
                    for (int i = 0; i < sizeof(tnid.szTip) && i < sizeStringOut && tnid.szTip[i]; ++i)
                    {
                        stringOut[i] = (char)tnid.szTip[i];
                    }
                    return (tnid.szTip[0] != 0);
                    break;
                case WINPLUS_TYPE_MENU:
                {
                    __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                    if (menu)
                    {
                        return (SUCCEEDED(SendMessageA(menu->hwnd, WM_GETTEXT, sizeStringOut, (LPARAM)stringOut)));
                    }
                }
                break;
                case WINPLUS_TYPE_SUB_MENU:
                {
                    __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                    if (menu)
                    {
                        if (index <= -1)
                        {
                            std::string strSubMenu;
                            for (unsigned int i = 0; i < menu->lsSubMenusTitles.size(); ++i)
                            {
                                strSubMenu += menu->lsSubMenusTitles[i];
                                if ((i + 1) < menu->lsSubMenusTitles.size())
                                    strSubMenu += "\n";
                            }
                            strncpy(stringOut, strSubMenu.c_str(), sizeStringOut);
                            return true;
                        }
                        else if (index < (int)menu->lsSubMenusTitles.size())
                        {
                            const char *title = menu->lsSubMenusTitles[index].c_str();
                            strncpy(stringOut, title, sizeStringOut);
                            return true;
                        }
                    }
                }
                break;
            }
        }
        return false;
    }
    int WINDOW::getTextLength(const int IdComponent, int index )
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(IdComponent);
        if (ptr)
        {
            switch (ptr->typeWindowWinPlus)
            {
                case WINPLUS_TYPE_SPIN_INT:
                {
                    mbm::SPIN_PARAMSi *spin = static_cast<mbm::SPIN_PARAMSi *>(ptr->extraParams);
                    if (spin)
                    {
                        char stringOut[1024] = "";
                        snprintf(stringOut, 1024, "%d", spin->currentPosition);
                        return strlen(stringOut);
                    }
                    return 0;
                }
                break;
                case WINPLUS_TYPE_SPIN_FLOAT:
                {
                    mbm::SPIN_PARAMSf *spin = static_cast<mbm::SPIN_PARAMSf *>(ptr->extraParams);
                    if (spin)
                    {
                        char stringOut[1024] = "";
                        snprintf(stringOut, 1024, "%0.*f", spin->precision, spin->currentPosition);
                        return strlen(stringOut);
                    }
                    return 0;
                }
                break;
                case WINPLUS_TYPE_IMAGE: return 0;
                case WINPLUS_TYPE_WINDOW_MESSAGE_BOX:
                case WINPLUS_TYPE_WINDOW:
                case WINPLUS_TYPE_WINDOWNC:
                case WINPLUS_TYPE_LABEL:
                case WINPLUS_TYPE_BUTTON:
                case WINPLUS_TYPE_BUTTON_TAB:
                case WINPLUS_TYPE_CHECK_BOX:
                case WINPLUS_TYPE_RADIO_BOX:
                case WINPLUS_TYPE_LIST_BOX:
                case WINPLUS_TYPE_TEXT_BOX:
                case WINPLUS_TYPE_SCROLL:
                case WINPLUS_TYPE_RICH_TEXT:
                case WINPLUS_TYPE_CHILD_WINDOW:
                case WINPLUS_TYPE_GROUP_BOX:
                case WINPLUS_TYPE_GROUP_BOX_TAB:
                case WINPLUS_TYPE_PROGRESS_BAR:
                case WINPLUS_TYPE_TIMER:
                case WINPLUS_TYPE_TRACK_BAR:
                {
                    index = index < 0 ? 0 : index;
                    return (SendMessageA(ptr->hwnd, WM_GETTEXTLENGTH, index, 0) + 4);
                }
                break;
                case WINPLUS_TYPE_STATUS_BAR:
                {
                    index                                 = index < 0 ? 0 : index;
                    std::vector<std::string> *lsStatusBar = static_cast<std::vector<std::string> *>(ptr->extraParams);
                    if ((unsigned int)index < lsStatusBar->size())
                        return lsStatusBar->at(index).size();
                }
                break;
                case WINPLUS_TYPE_TRY_ICON_MENU:
                case WINPLUS_TYPE_TRY_ICON_SUB_MENU:
                case WINPLUS_TYPE_MENU:
                {
                    __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                    if (menu)
                    {
                        return (SendMessageA(menu->hwnd, WM_GETTEXTLENGTH, index, 0) + 4);
                    }
                }
                break;
                case WINPLUS_TYPE_SUB_MENU:
                {
                    __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                    if (menu)
                    {
                        index = index < 0 ? 0 : index;
                        if (index < (int)menu->lsSubMenusTitles.size())
                        {
                            return menu->lsSubMenusTitles[index].size();
                        }
                    }
                }
                break;
                case WINPLUS_TYPE_TOOL_TIP:
                {
                    index = index < 0 ? 0 : index;
                    return (SendMessageA(ptr->hwnd, WM_GETTEXTLENGTH, index, 0) + 4);
                }
                break;
                case WINPLUS_TYPE_COMBO_BOX:
                {
                    if (index == -1)
                        index = SendMessageA(ptr->hwnd, CB_GETCURSEL, 0, 0);
                    if (index > -1)
                    {
                        std::vector<std::string *> *lsCombo = static_cast<std::vector<std::string *> *>(ptr->extraParams);
                        if (lsCombo && index < (int)lsCombo->size())
                            return lsCombo->at(index)->size() + 4;
                        int len = (int)SendMessageA(ptr->hwnd, CB_GETLBTEXTLEN, index, 0) + 4;
                        return len;
                    }
                    return 0;
                }
                break;
            }
        }
        return 0;
    }
    std::vector<std::string> * WINDOW::getStatusBar(const int idComponent)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_STATUS_BAR)
            return static_cast<std::vector<std::string> *>(ptr->extraParams);
        return nullptr;
    }
    void WINDOW::setOnKeyboardDown(OnKeyboardEvent function)
    {
        onKeyboardDown = function;
    }
    void WINDOW::setOnKeyboardUp(OnKeyboardEvent function)
    {
        onKeyboardUp = function;
    }
    bool WINDOW::setOnParserRawInput(OnParseRawInput function)
    {
        RAWINPUTDEVICE rid[1];
        rid[0].usUsagePage = 1;
        rid[0].usUsage     = 4; // Joystick
        rid[0].dwFlags     = RIDEV_EXINPUTSINK;
        rid[0].hwndTarget  = this->getHwnd();

        const int index      = 0;
        const int numDevices = 1;
        if (!RegisterRawInputDevices(&rid[index], numDevices, sizeof(RAWINPUTDEVICE)))
            return false;
        this->onParseRawInput = function;
        return true;
    }
    void WINDOW::setOnMoveMouseEvent(OnMouseEvent function)
    {
        onMouseMove = function;
    }
    void WINDOW::setOnClickLeftMouse(OnMouseEvent function)
    {
        onClickLeftMouse = function;
    }
    void WINDOW::setOnReleaseLeftMouse(OnMouseEvent function)
    {
        onReleaseLeftMouse = function;
    }
    void WINDOW::setOnClickRightMouse(OnMouseEvent function)
    {
        onClickRightMouse = function;
    }
    void WINDOW::setOnReleaseRightMouse(OnMouseEvent function)
    {
        onReleaseRightMouse = function;
    }
    void WINDOW::setOnClickMiddleMouse(OnMouseEvent function)
    {
        onClickMiddleMouse = function;
    }
    void WINDOW::setOnReleaseMiddleMouse(OnMouseEvent function)
    {
        onReleaseMiddleMouse = function;
    }
    void WINDOW::setOnScrollMouseEvent(OnMouseEventScroll function)
    {
        onScrollMouseEvent = function;
    }
    bool WINDOW::setMaxLength(const int idComponent, const unsigned int maxLength)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr && maxLength > 0)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_RICH_TEXT)
                return SUCCEEDED(SendMessageA(ptr->hwnd, EM_SETLIMITTEXT, maxLength, 0));
            else if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_TEXT_BOX)
                return SUCCEEDED(SendMessageA(ptr->hwnd, EM_LIMITTEXT, (WPARAM)maxLength, (LPARAM)0));
            return SUCCEEDED(SendMessageA(ptr->hwnd, EM_LIMITTEXT, (WPARAM)maxLength, (LPARAM)0));
        }
        return false;
    }
    bool WINDOW::setReadOnlyToRichText(const int idRichText, const bool value)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idRichText);
        if (ptr)
            return SUCCEEDED(SendMessageA(ptr->hwnd, EM_SETREADONLY, value, 0));
        return false;
    }
    void WINDOW::setAlwaysOnTop(const bool value, const bool hideMe)
    {
        isVisible   = true;
        DWORD uFlag = SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE;
        if (value)
            SetWindowPos(this->hwnd, HWND_TOPMOST, 0, 0, 0, 0, uFlag);
        else
            SetWindowPos(this->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, uFlag);
        if (hideMe)
            this->hide();
    }
    void WINDOW::setAlwaysOnTop(mbm::WINDOW *hwndParent)
    {
        if (hwndParent)
            hwndParent->hwndInsertAfter = this->hwnd;
    }
    void WINDOW::setColorKeying(const unsigned char red, const unsigned char green, const unsigned char blue)
    {
        SetWindowLong(this->hwnd, GWL_EXSTYLE, GetWindowLong(this->hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(this->hwnd, RGB(red, green, blue), 0, LWA_COLORKEY);
    }
    void WINDOW::setColorKeying(const unsigned char red, const unsigned char green, const unsigned char blue,
                               const int idComponent)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            SetWindowLong(ptr->hwnd, GWL_EXSTYLE, GetWindowLong(ptr->hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(ptr->hwnd, RGB(red, green, blue), 0, LWA_COLORKEY);
        }
    }
    void WINDOW::setPosition(const int x, const int y, const int id)
    {
        RECT rect = getRect(id);
        HWND h    = this->getHwnd(id);
        MoveWindow(h, x, y, rect.right - rect.left, rect.bottom - rect.top, true);
    }
    long WINDOW::getWidth(const int id )
    {
        RECT rect;
        if (id == -1)
        {
            GetClientRect(this->hwnd, &rect);
            return rect.right;
        }
        else
        {
            COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
            if (ptr)
            {
                GetClientRect(ptr->hwnd, &rect);
                return rect.right;
            }
        }
        return 0;
    }
    WINDOW * WINDOW::getWindow(const HWND hwnd_)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(hwnd_);
        if (ptr)
            return ptr->ptrWindow;
        return nullptr;
    }
    long WINDOW::getHeight(const int id)
    {
        RECT rect;
        if (id == -1)
        {
            GetClientRect(this->hwnd, &rect);
            return rect.bottom;
        }
        else
        {
            COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
            if (ptr)
            {
                GetClientRect(ptr->hwnd, &rect);
                return rect.bottom;
            }
        }
        return 0;
    }
    RECT WINDOW::getRect(const int id )
    {
        RECT rect = {0, 0, 0, 0};
        if (id == -1)
            GetClientRect(this->hwnd, &rect);
        else
        {
            COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
            if (ptr)
                GetClientRect(ptr->hwnd, &rect);
        }
        return rect;
    }
    RECT WINDOW::getRectAbsolute(const int id)
    {
        RECT rect = {0, 0, 0, 0};
        if (id == -1)
            GetWindowRect(this->hwnd, &rect);
        else
        {
            COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
            if (ptr)
                GetWindowRect(ptr->hwnd, &rect);
        }
        return rect;
    }
    RECT WINDOW::getRectRelativeWindow(const int id )
    {
        RECT rect = {0, 0, 0, 0};
        if (id == -1)
            GetWindowRect(this->hwnd, &rect);
        else
        {
            COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
            if (ptr)
            {
                RECT rectAbsolute;
                GetWindowRect(this->hwnd, &rectAbsolute);
                GetWindowRect(ptr->hwnd, &rect);
                rect.left -= rectAbsolute.left + this->adjustRectLeft;
                rect.bottom -= rectAbsolute.top + this->adjustRectTop;
                rect.top -= rectAbsolute.top + this->adjustRectTop;
                rect.right -= rectAbsolute.left + this->adjustRectLeft;
            }
        }
        return rect;
    }
    void WINDOW::setSize(RECT &source, const bool inner)
    {
        MoveWindow(this->hwnd, source.left, source.top, source.right - source.left, source.bottom - source.top, true);
        if (inner)
        {
            mbm::COM_BETWEEN_WINP *ptr = getComBetweenWinp(-1);
            RECT                   comp;
            if (GetClientRect(this->hwnd, &comp))
            {
                source.right += (comp.right - comp.left) - (source.right - source.left);
                source.bottom += (source.bottom - source.top) - (comp.bottom - comp.top);
                MoveWindow(this->hwnd, source.left, source.top, source.right - source.left, source.bottom - source.top,
                           true);
                if (ptr && ptr->graphWin)
                    ptr->graphWin->redrawWindow(this->hwnd);
            }
        }
    }
    void WINDOW::resize(const int idComponent, const int x, const int y, const int new_width, const int new_height)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            MoveWindow(ptr->hwnd, x, y, new_width, new_height, true);
            if (ptr->graphWin)
                ptr->graphWin->redrawWindow(ptr->hwnd);
        }
    }
    void WINDOW::resize(const int idComponent, const int new_width, const int new_height)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idComponent);
        if (ptr)
        {
            RECT comp = getRectRelativeWindow(idComponent);
            if (comp.right && comp.left && comp.top && comp.bottom)
            {
                MoveWindow(ptr->hwnd, comp.left, comp.top, new_width, new_height, true);
                if (ptr->graphWin)
                    ptr->graphWin->redrawWindow(ptr->hwnd);
            }
        }
    }
    void WINDOW::resize(HWND hwnd2move, const int new_width, const int new_height, bool incrementSize)
    {
        RECT rect = {0, 0, 0, 0};
        if (GetClientRect(hwnd2move, &rect))
        {
            WINDOWPLACEMENT p;
            memset(&p, 0, sizeof(p));
            p.length = sizeof(WINDOWPLACEMENT);
            if (GetWindowPlacement(hwnd2move, &p))
            {
                int nx = p.rcNormalPosition.left;
                int ny = p.rcNormalPosition.top;
                if (incrementSize)
                {
                    const int nWidth  = rect.right - rect.left + new_width;
                    const int nHeight = rect.bottom - rect.top + new_height;
                    MoveWindow(hwnd2move, nx, ny, nWidth, nHeight, true);
                }
                else
                {
                    const int nWidth  = new_width;
                    const int nHeight = new_height;
                    MoveWindow(hwnd2move, nx, ny, nWidth, nHeight, true);
                }
            }
        }
    }
    void WINDOW::hideConsoleWindow()
    {
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole, SW_HIDE);
    }
    void WINDOW::showConsoleWindow()
    {
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole, SW_SHOW | SW_NORMAL);
    }
    void WINDOW::closeWindow()
    {
        SendMessage(this->hwnd, WM_CLOSE, 0, 0);
    }
    void WINDOW::hide(const HWND hwnd_)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(hwnd_);
        if (ptr)
            this->hide(ptr->id);
    }
    void WINDOW::hide(const int id, int flag)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
        if (id == -1)
        {
            ShowWindow(this->hwnd, flag);
            isVisible = false;
            if (ptr)
            {
                for (std::set<COM_BETWEEN_WINP *>::const_iterator it = ptr->myChilds.cbegin(); it != ptr->myChilds.cend();
                     ++it)
                {
                    COM_BETWEEN_WINP *child = *it;
                    this->hide(child->id);
                }
            }
            InvalidateRect(this->hwnd, 0, 0);
        }
        else if (ptr)
        {
            ShowWindow(ptr->hwnd, flag);
            if (ptr->typeWindowWinPlus == WINPLUS_TYPE_SPIN_INT || ptr->typeWindowWinPlus == WINPLUS_TYPE_SPIN_FLOAT)
            {
                ptr = getComBetweenWinp(id + 1);
                if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TEXT_BOX)
                    ShowWindow(ptr->hwnd, flag);
            }
            for (std::set<COM_BETWEEN_WINP *>::const_iterator it = ptr->myChilds.cbegin(); it != ptr->myChilds.cend();
                 ++it)
            {
                COM_BETWEEN_WINP *child = *it;
                this->hide(child->id);
            }
            InvalidateRect(ptr->hwnd, 0, 0);
        }
    }
    void WINDOW::show(const int id, int flag)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(id);
        if (id == -1)
        {
            ShowWindow(this->hwnd, flag);
            isVisible = true;
            if (ptr)
            {
                for (std::set<COM_BETWEEN_WINP *>::const_iterator it = ptr->myChilds.cbegin(); it != ptr->myChilds.cend();
                     ++it)
                {
                    COM_BETWEEN_WINP *child = *it;
                    show(child->id);
                }
            }
            InvalidateRect(this->hwnd, 0, 0);
        }
        else if (ptr)
        {
            ShowWindow(ptr->hwnd, flag);
            if (ptr->typeWindowWinPlus == WINPLUS_TYPE_SPIN_INT || ptr->typeWindowWinPlus == WINPLUS_TYPE_SPIN_FLOAT)
            {
                ptr = getComBetweenWinp(id + 1);
                if (ptr && ptr->typeWindowWinPlus == WINPLUS_TYPE_TEXT_BOX)
                    ShowWindow(ptr->hwnd, flag);
            }
            for (std::set<COM_BETWEEN_WINP *>::const_iterator it = ptr->myChilds.cbegin(); it != ptr->myChilds.cend();
                 ++it)
            {
                COM_BETWEEN_WINP *child = *it;
                show(child->id);
            }
            InvalidateRect(ptr->hwnd, 0, 0);
        }
    }
    void WINDOW::show(const HWND hwnd_)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(hwnd_);
        if (ptr)
            this->show(ptr->id, SW_SHOW | SW_NORMAL);
    }
    void WINDOW::showMaximized(const int idWindow )
    {
        this->show(idWindow, SW_SHOW | SW_MAXIMIZE);
    }
    void WINDOW::showMinimized(const int idWindow )
    {
        this->show(idWindow, SW_FORCEMINIMIZE);
    }
    void WINDOW::setMinSizeAllowed(const int width,const int height)
    {
        this->min_size_width  = width;
        this->min_size_height = height;
    }
    void WINDOW::setMaxSizeAllowed(const int width,const int height)
    {
        this->max_size_width  = width;
        this->max_size_height = height;
    }
    bool WINDOW::loadTextFileToRichEdit(const int idRichText, WCHAR *fileName)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idRichText);
        if (!ptr)
            return false;
        return loadTextFileToRichEdit(ptr->hwnd, fileName);
    }
    bool WINDOW::loadTextFileToRichEdit(HWND hwndRichText, WCHAR *fileName)
    {
        HANDLE hFile;
        BOOL   bSuccess = FALSE;

        hFile = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD dwFileSize;

            dwFileSize = GetFileSize(hFile, nullptr);
            if (dwFileSize != 0xFFFFFFFF)
            {
                char *pszFileText;

                pszFileText = (char *)GlobalAlloc(GPTR, dwFileSize + 1);
                if (pszFileText != nullptr)
                {
                    DWORD dwRead;

                    if (ReadFile(hFile, pszFileText, dwFileSize, &dwRead, nullptr))
                    {
                        pszFileText[dwFileSize] = 0;
                        if (SetWindowTextA(hwndRichText, pszFileText))
                            bSuccess = TRUE;
                    }
                    GlobalFree(pszFileText);
                }
            }
            CloseHandle(hFile);
        }
        return bSuccess > 0;
    }
    bool WINDOW::loadTextFileToRichEdit(const int idRichText, const WCHAR *filter)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idRichText);
        if (!ptr)
            return false;
        OPENFILENAMEW ofn;
        WCHAR         fileName[MAX_PATH] = L"";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner   = this->hwnd;
        if (filter == nullptr)
            ofn.lpstrFilter = L"Arquivos textos (*.txt)\0*.txt\0 Todos tipos de arquivo (*.*)\0*.*\0";
        else
            ofn.lpstrFilter = filter;
        ofn.lpstrFile       = fileName;
        ofn.nMaxFile        = MAX_PATH;
        ofn.Flags           = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt     = L"txt";

        if (GetOpenFileNameW(&ofn))
        {
            return loadTextFileToRichEdit(ptr->hwnd, fileName);
        }
        return false;
    }
    bool WINDOW::saveTextFileFromRichText(const int idRichText, WCHAR *fileName)
    {
        COM_BETWEEN_WINP *ptr = getComBetweenWinp(idRichText);
        if (!ptr)
            return false;
        HANDLE hFile;
        BOOL   bSuccess = FALSE;

        hFile = CreateFileW(fileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD dwTextLength;

            dwTextLength = GetWindowTextLengthA(ptr->hwnd);

            if (dwTextLength > 0)
            {
                char *pszText;
                DWORD dwBufferSize = dwTextLength + 1;

                pszText = (char *)GlobalAlloc(GPTR, dwBufferSize);
                if (pszText != nullptr)
                {
                    if (GetWindowTextA(ptr->hwnd, pszText, dwBufferSize))
                    {
                        DWORD dwWritten;

                        if (WriteFile(hFile, pszText, dwTextLength, &dwWritten, nullptr))
                            bSuccess = TRUE;
                    }
                    GlobalFree(pszText);
                }
            }
            CloseHandle(hFile);
        }
        return bSuccess > 0;
    }
    bool WINDOW::messageBoxQuestion(const char *format, ...)
    {
        bool    ret = false;
        va_list args;
        va_start(args, format);
        int   count  = vsnprintf(nullptr, 0, format, args);
        char *buffer = new char[count + 1];
        vsprintf(buffer, format, args);
        va_end(args);
        int flag = MB_YESNO | MB_ICONQUESTION;
        if (this->hwnd == nullptr)
        {
            mbm::WINDOW *win = mbm::getFirstWindow();
            if (win && win != this)
            {
                ret                      = win->messageBoxQuestion(buffer);
                mbm::WINDOW::hookMsgProc = nullptr;
                delete[] buffer;
                return ret;
            }
        }
        mbm::COM_BETWEEN_WINP *me    = mbm::getComBetweenWinp(this->hwnd);
        mbm::COM_BETWEEN_WINP *owner = mbm::getNewComBetween(this->hwnd ? this->hwnd : me->hwnd, me->onEventWinPlus, this,
                                                             WINPLUS_TYPE_WINDOW_MESSAGE_BOX, &flag, me->id, nullptr);
        if (owner)
        {
            const DWORD idThread     = GetCurrentThreadId();
            mbm::WINDOW::hookMsgProc = SetWindowsHookExA(WH_CALLWNDPROCRET, GetMsgProc, 0, idThread);
            bool visible = me->ptrWindow->isVisible;
            if(visible == false)
                me->ptrWindow->show();
            ret                      = MessageBoxA(me->hwnd, buffer, me->ptrWindow->nameAplication, flag) == IDYES;
            UnhookWindowsHookEx(mbm::WINDOW::hookMsgProc);
            if(visible == false)
                me->ptrWindow->hide();
        }
        else
        {
            ret = MessageBoxA(this->hwnd, buffer, this->nameAplication, flag) == IDYES;
        }
        mbm::WINDOW::hookMsgProc = nullptr;
        delete[] buffer;
        return ret;
    }
    void WINDOW::messageBox(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        int   count  = vsnprintf(nullptr, 0, format, args);
        char *buffer = new char[count + 1];
        vsprintf(buffer, format, args);
        va_end(args);
        if (this->hwnd == nullptr)
        {
            mbm::WINDOW *win = mbm::getFirstWindow();
            if (win && win != this)
            {
                win->messageBox(buffer);
                mbm::WINDOW::hookMsgProc = nullptr;
                delete[] buffer;
                return;
            }
        }
        int                    flag  = MB_OK | MB_ICONINFORMATION;
        mbm::COM_BETWEEN_WINP *me    = mbm::getComBetweenWinp(this->hwnd);
        mbm::COM_BETWEEN_WINP *owner = mbm::getNewComBetween(this->hwnd ? this->hwnd : me->hwnd, me->onEventWinPlus, this,
                                                             WINPLUS_TYPE_WINDOW_MESSAGE_BOX, &flag, me->id, nullptr);
        if (owner)
        {
            bool visible = me->ptrWindow->isVisible;
            if(visible == false)
                me->ptrWindow->show();
            const DWORD idThread     = GetCurrentThreadId();
            mbm::WINDOW::hookMsgProc = SetWindowsHookExA(WH_CALLWNDPROCRET, GetMsgProc, 0, idThread);
            MessageBoxA(hwnd, buffer, me->ptrWindow->nameAplication, flag);
            UnhookWindowsHookEx(mbm::WINDOW::hookMsgProc);
            if(visible == false)
                me->ptrWindow->hide();
        }
        else
        {
            MessageBoxA(hwnd, buffer, this->nameAplication, flag);
        }
        mbm::WINDOW::hookMsgProc = nullptr;
        delete[] buffer;
    }
    const int WINDOW::getTabStopPixelSize()
    {
        if (dialogunitTabStopInPixel == 0)
            setTabStopPixelSize();
        return dialogunitTabStopInPixel;
    }
    const void WINDOW::setTabStopPixelSize(int characters , const int idComponent )
    {
        const char *alpha           = "ABCDEFGHIJKLMNOPKRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        const int   len             = strlen(alpha);
        const SIZE  sz              = mbm::DRAW::getSizeText(alpha, this->hwnd);
        const int   charactersSpace = (sz.cx / len);
        const int   tabSpace        = charactersSpace * characters;

        const long baseunit            = GetDialogBaseUnits();
        const int  baseunitX           = LOWORD(baseunit);
        const int  sizeInPointsDLU_X   = MulDiv(tabSpace, 4, baseunitX);
        this->dialogunitTabStopInPixel = tabSpace;
        if (idComponent == -1)
        {
            for (unsigned int i = 0, s = mbm::COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
            {
                mbm::COM_BETWEEN_WINP *ptr = mbm::COM_BETWEEN_WINP::lsComBetweenWinp[i];
                if (ptr)
                {
                    SendMessageA(ptr->hwnd, EM_SETTABSTOPS, (WPARAM)1, (LPARAM)&sizeInPointsDLU_X);
                }
            }
        }
        else
        {

            if ((unsigned int)idComponent < mbm::COM_BETWEEN_WINP::lsComBetweenWinp.size())
            {
                mbm::COM_BETWEEN_WINP *ptr = mbm::COM_BETWEEN_WINP::lsComBetweenWinp[idComponent];
                if (ptr)
                {
                    SendMessageA(ptr->hwnd, EM_SETTABSTOPS, (WPARAM)1, (LPARAM)&sizeInPointsDLU_X);
                }
            }
        }
    }
    void WINDOW::moveHWND(HWND hwndToMove, int x, int y)
    {
        RECT rect = {0, 0, 0, 0};
        if (GetClientRect(hwndToMove, &rect))
        {
            WINDOWPLACEMENT p;
            memset(&p, 0, sizeof(p));
            p.length = sizeof(WINDOWPLACEMENT);
            if (GetWindowPlacement(hwndToMove, &p))
            {
                int nx      = x + p.rcNormalPosition.left;
                int ny      = y + p.rcNormalPosition.top;
                int nWidth  = rect.right - rect.left;
                int nHeight = rect.bottom - rect.top;
                MoveWindow(hwndToMove, nx, ny, nWidth, nHeight, true);
            }
        }
    }
    void WINDOW::moveHWNDMeAndChilds(HWND hwndToMove, int x, int y)
    {
        moveHWND(hwndToMove, x, y);
        COM_BETWEEN_WINP *ptrComponet = getComBetweenWinp(hwndToMove);
        for (std::set<COM_BETWEEN_WINP *>::const_iterator it = ptrComponet->myChilds.cbegin();
             it != ptrComponet->myChilds.cend(); ++it)
        {
            COM_BETWEEN_WINP *ptrChild = *it;
            for (std::set<COM_BETWEEN_WINP *>::const_iterator that = ptrChild->myChilds.cbegin();
                 that != ptrChild->myChilds.cend(); ++that)
            {
                moveHWNDMeAndChilds(ptrChild->hwnd, x, y);
            }
            moveHWND(ptrChild->hwnd, x, y);
        }
    }
    void WINDOW::moveTabByGroup(const int idTabControl, const int x, const int y)
    {
        COM_BETWEEN_WINP *ptrFather = getComBetweenWinp(idTabControl);
        if (ptrFather && ptrFather->typeWindowWinPlus == WINPLUS_TYPE_BUTTON_TAB && ptrFather->extraParams)
        {
            __TAB_GROUP_DESC *tabFather = static_cast<__TAB_GROUP_DESC *>(ptrFather->extraParams);
            for (unsigned int i = 0; i < tabFather->lsTabChilds.size(); ++i)
            {
                __TAB_GROUP_DESC *tabGroup = tabFather->lsTabChilds[i];
                COM_BETWEEN_WINP *ptrGroup = getComBetweenWinp(tabGroup->idGroupTabBox);
                RECT              rect     = {0, 0, 0, 0};
                if (GetClientRect(ptrGroup->hwnd, &rect))
                {
                    WINDOWPLACEMENT p;
                    memset(&p, 0, sizeof(p));
                    p.length = sizeof(WINDOWPLACEMENT);
                    if (GetWindowPlacement(ptrGroup->hwnd, &p))
                    {
                        int nx      = x + p.rcNormalPosition.left;
                        int ny      = y + p.rcNormalPosition.top;
                        int nWidth  = rect.right - rect.left;
                        int nHeight = rect.bottom - rect.top;
                        MoveWindow(ptrGroup->hwnd, nx, ny, nWidth, nHeight, true);
                    }
                }
                for (unsigned int j = 0; j < tabGroup->lsHwndComponents.size(); ++j)
                {
                    HWND hwndChildGroup = *tabGroup->lsHwndComponents[j];
                    if (hwndChildGroup)
                    {
                        moveHWNDMeAndChilds(hwndChildGroup, x, y);
                    }
                }
            }
        }
    }
    HICON WINDOW::getIcon()
    {
        if (iconApp)
            return iconApp;
        BYTE ANDmaskIcon[] = {
            0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03,

            0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03,

            0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03,

            0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03,

            0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xFC, 0x3F, 0x03, 0xC0, 0xC0, 0x03, 0x03, 0xC0, 0xC0, 0x03, 0x03,

            0xC0, 0xC0, 0x03, 0x03, 0xC0, 0xFF, 0xFF, 0x03, 0xC0, 0xFF, 0xFF, 0x03, 0xC0, 0xFF, 0xFF, 0x03,

            0xC0, 0xFF, 0xFF, 0x03, 0xC0, 0xFF, 0xFF, 0x03, 0xC0, 0xFF, 0xFF, 0x03, 0xC0, 0xFF, 0xFF, 0x03,

            0xC0, 0xFF, 0xFF, 0x03, 0xC0, 0xFF, 0xFF, 0x03, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00};

        BYTE XORmaskIcon[] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        iconApp = CreateIcon(0, 32, 32, 1, 1, ANDmaskIcon, XORmaskIcon);
        return iconApp;
    }
    void WINDOW::filleDefaultExtraParam(COM_BETWEEN_WINP *comBetweenWinpChild, int idDest)
    {
        switch (comBetweenWinpChild->typeWindowWinPlus)
        {
            case mbm::WINPLUS_TYPE_IMAGE:
            case mbm::WINPLUS_TYPE_WINDOW_MESSAGE_BOX:
            case mbm::WINPLUS_TYPE_WINDOW:
            case mbm::WINPLUS_TYPE_WINDOWNC: break;
            case mbm::WINPLUS_TYPE_LABEL: break;
            case mbm::WINPLUS_TYPE_BUTTON: { comBetweenWinpChild->extraParams = nullptr;
            }
            break;
            case mbm::WINPLUS_TYPE_BUTTON_TAB: break;
            case mbm::WINPLUS_TYPE_CHECK_BOX:
            {
                int *value                       = new int;
                *value                           = 0;
                comBetweenWinpChild->extraParams = static_cast<void *>(value);
            }
            break;
            case mbm::WINPLUS_TYPE_RADIO_BOX:
            {
                RADIO_GROUP *radio               = new RADIO_GROUP(comBetweenWinpChild->id, idDest);
                comBetweenWinpChild->extraParams = static_cast<void *>(radio);
                for (unsigned int i = 0; i < COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
                {
                    COM_BETWEEN_WINP *ptrRadio = COM_BETWEEN_WINP::lsComBetweenWinp[i];
                    if (ptrRadio && ptrRadio->typeWindowWinPlus == WINPLUS_TYPE_RADIO_BOX &&
                        ptrRadio->id != comBetweenWinpChild->id)
                    {
                        RADIO_GROUP *radioOther = static_cast<RADIO_GROUP *>(ptrRadio->extraParams);
                        if (radioOther && radioOther->idParent == radio->idParent)
                        {
                            radioOther->lsRadioGroup.insert(comBetweenWinpChild->id);
                            radio->lsRadioGroup.insert(radioOther->idRadio);
                        }
                    }
                }
            }
            break;
            case mbm::WINPLUS_TYPE_COMBO_BOX:
            {
                std::vector<std::string *> *lsComboBox = new std::vector<std::string *>();
                comBetweenWinpChild->extraParams       = static_cast<void *>(lsComboBox);
            }
            break;
            case mbm::WINPLUS_TYPE_LIST_BOX:
            {
                std::vector<std::string *> *lsListBox = new std::vector<std::string *>();
                comBetweenWinpChild->extraParams      = static_cast<void *>(lsListBox);
            }
            break;
            case mbm::WINPLUS_TYPE_TEXT_BOX: break;
            case mbm::WINPLUS_TYPE_SCROLL: break;
            case mbm::WINPLUS_TYPE_SPIN_INT: break;
            case mbm::WINPLUS_TYPE_SPIN_FLOAT: break;
            case mbm::WINPLUS_TYPE_RICH_TEXT: break;
            case mbm::WINPLUS_TYPE_CHILD_WINDOW: break;
            case mbm::WINPLUS_TYPE_GROUP_BOX: break;
            case mbm::WINPLUS_TYPE_PROGRESS_BAR: break;
            case mbm::WINPLUS_TYPE_TIMER: break;
            case mbm::WINPLUS_TYPE_TRACK_BAR: break;
            case mbm::WINPLUS_TYPE_STATUS_BAR:
            {
                std::vector<std::string> *lsStatusBar = new std::vector<std::string>();
                comBetweenWinpChild->extraParams      = static_cast<void *>(lsStatusBar);
            }
            break;
            case mbm::WINPLUS_TYPE_MENU: break;
            case mbm::WINPLUS_TYPE_SUB_MENU: break;
            case mbm::WINPLUS_TYPE_GROUP_BOX_TAB: break;
            case mbm::WINPLUS_TYPE_TRY_ICON_MENU: break;
            case mbm::WINPLUS_TYPE_TRY_ICON_SUB_MENU: break;
            case mbm::WINPLUS_TYPE_TOOL_TIP: break;
            default: break;
        }
    }
    HWND WINDOW::addToHwnd(const int idDest, COM_BETWEEN_WINP *comBetweenWinpChild)
    {
        HWND hwndDest = this->hwnd;
        if (this->drawerDefault)
            comBetweenWinpChild->graphWin = this->drawerDefault;
        filleDefaultExtraParam(comBetweenWinpChild, idDest);
        if (idDest != -1)
        {
            mbm::COM_BETWEEN_WINP *ptr = getComBetweenWinp(idDest);
            if (ptr)
            {
                ptr->myChilds.insert(comBetweenWinpChild);
                return ptr->hwnd;
            }
        }
        return hwndDest;
    }
    HWND WINDOW::addToHwnd(const int idDest, long *x, long *y, COM_BETWEEN_WINP *comBetweenWinpChild)
    {
        HWND hwndDest = this->hwnd;
        if (idDest != -1)
        {
            mbm::COM_BETWEEN_WINP *ptr = mbm::getComBetweenWinp(idDest);
            if (ptr)
            {
                ptr->myChilds.insert(comBetweenWinpChild);
                mbm::TYPE_WINDOWS_WINPLUS type = comBetweenWinpChild->typeWindowWinPlus;
                if ((type == WINPLUS_TYPE_BUTTON || type == WINPLUS_TYPE_SPIN_INT || type == WINPLUS_TYPE_SPIN_FLOAT ||
                     type == WINPLUS_TYPE_RADIO_BOX || type == WINPLUS_TYPE_TEXT_BOX || type == WINPLUS_TYPE_RICH_TEXT ||
                     type == WINPLUS_TYPE_LIST_BOX || type == WINPLUS_TYPE_COMBO_BOX || type == WINPLUS_TYPE_LABEL ||
                     type == WINPLUS_TYPE_SCROLL || type == WINPLUS_TYPE_CHECK_BOX || type == WINPLUS_TYPE_TRACK_BAR ||
                     type == WINPLUS_TYPE_BUTTON_TAB || type == WINPLUS_TYPE_GROUP_BOX) &&
                    (ptr->typeWindowWinPlus == WINPLUS_TYPE_GROUP_BOX ||
                     ptr->typeWindowWinPlus == WINPLUS_TYPE_GROUP_BOX_TAB))
                {
                    WINDOWPLACEMENT p;
                    p.length = sizeof(WINDOWPLACEMENT);
                    if (GetWindowPlacement(ptr->hwnd, &p))
                    {
                        *x += p.rcNormalPosition.left;
                        *y += p.rcNormalPosition.top;
                        if (ptr->typeWindowWinPlus == WINPLUS_TYPE_GROUP_BOX_TAB)
                        {
                            *y += 10;
                        }
                    }
                    filleDefaultExtraParam(comBetweenWinpChild, idDest);
                    if (this->drawerDefault)
                        comBetweenWinpChild->graphWin = this->drawerDefault;
                    return this->hwnd;
                }
                else
                {
                    ptr = mbm::getComBetweenWinp(idDest);
                    if (ptr)
                        ptr->myChilds.insert(comBetweenWinpChild);
                    hwndDest = this->getHwnd(idDest);
                }
            }
        }
        if (this->drawerDefault)
            comBetweenWinpChild->graphWin = this->drawerDefault;
        filleDefaultExtraParam(comBetweenWinpChild, idDest);
        return hwndDest;
    }
    INT WINDOW::_Do_default_Drawer_BackGround(COM_BETWEEN_WINP *ptr)
    {
        if (ptr && ptr->graphWin)
        {
            ptr->graphWin->infoActualComponent = nullptr;
            if (ptr->ptrWindow == nullptr || ptr->ptrWindow->run == false || ptr->ptrWindow->isVisible == false || ptr->graphWin->eraseBackGround(nullptr) == false)
            {
                return 1;
            }
            PAINTSTRUCT ps;
            memset(&ps, 0, sizeof(ps));
            HDC hdc = BeginPaint(ptr->hwnd, &ps);
            GetClientRect(ptr->hwnd, &ps.rcPaint);
            const int win_width  = ps.rcPaint.right - ps.rcPaint.left;
            const int win_height = ps.rcPaint.bottom - ps.rcPaint.top;
            HDC       hdcMem     = CreateCompatibleDC(hdc);
            HBITMAP   hbmMem     = CreateCompatibleBitmap(hdc, win_width, win_height);
            HANDLE    hOld       = SelectObject(hdcMem, hbmMem);

            ptr->graphWin->hdcBack = hdcMem;
            mbm::COMPONENT_INFO component(ptr, &ps, hdcMem, false, false, static_cast<mbm::USER_DRAWER *>(ptr->userDrawer));
            ptr->graphWin->infoActualComponent = &component;
            ptr->graphWin->eraseBackGround(&component);
            ptr->graphWin->present(ps.hdc, win_width, win_height);
            SelectObject(hdcMem, hOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(ptr->hwnd, &ps);
			ptr->graphWin->infoActualComponent = nullptr;
        }
        return 1;
    }
    INT WINDOW::_Do_default_Drawer(COM_BETWEEN_WINP *ptr, LPDRAWITEMSTRUCT lpdis)
    {
        if (ptr == nullptr || mbm::WINDOW::isEnableRender(ptr->hwnd) == false)
            return 1;
        if (ptr->ptrWindow == nullptr || ptr->ptrWindow->run == false || ptr->ptrWindow->isVisible == false)
        {
            return 1;
        }
        bool isPressed = false;
        bool isHover   = ptr->ptrWindow->hwndLastHover == ptr->hwnd;
        switch (ptr->typeWindowWinPlus)
        {
            case mbm::WINPLUS_TYPE_CHECK_BOX:
            {
                int *value = static_cast<int *>(ptr->extraParams);
                isPressed  = (*value == 1);
            }
            break;
            case mbm::WINPLUS_TYPE_RADIO_BOX:
            {
                RADIO_GROUP *radio = static_cast<RADIO_GROUP *>(ptr->extraParams);
                isPressed          = radio->checked;
            }
            break;
            case mbm::WINPLUS_TYPE_BUTTON_TAB:
            {
                __TAB_GROUP_DESC *ptrTab      = static_cast<__TAB_GROUP_DESC *>(ptr->extraParams);
                __TAB_GROUP_DESC *tabSelected = ptrTab->tabFather->tabSelected;
                isPressed                     = ptrTab->index == (tabSelected ? tabSelected->index : -1);
            }
            break;
            default: { isPressed = ptr->ptrWindow->hwndLastPressed == ptr->hwnd;
            }
            break;
        }

        if (isPressed)
            ptr->ptrWindow->hwndLastPressed = nullptr;
        if (ptr->graphWin)
        {
            ptr->graphWin->doRenderBackBuffer(ptr, lpdis, isHover, isPressed);
            if (ptr->ptrWindow->preventMenuAlwaysShowing &&  ptr->typeWindowWinPlus != WINPLUS_TYPE_MENU && ptr->typeWindowWinPlus != WINPLUS_TYPE_SUB_MENU)
            {
                for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
                {
                    mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
                    if (menu)
                        menu->hideSubMenu();
                }
            }
            return TRUE;
        }
        return 1;
    }
    bool WINDOW::exitNow(HWND windowHandle, const bool keyEsc)
    {
        mbm::WINDOW *ptrWin = mbm::getWindow(windowHandle);
        if (ptrWin)
        {
            if (ptrWin->neverClose)
                return false;
            if (ptrWin->exitOnEsc && keyEsc)
            {
                mbm::WINPLUS_TYPE_CURSOR cursor = ptrWin->getCursor();
                ptrWin->setCursor(mbm::WINPLUS_CURSOR_ARROW);
                if (ptrWin->neverClose)
                    return false;
                if (ptrWin->askOnExit)
                {
                    if (ptrWin->messageBoxQuestion("DESEJA SAIR?"))
                    {
                        ptrWin->run = false;
                        closeAllWindows();
                        if (windowHandle && *(int *)windowHandle)
                            DestroyWindow(windowHandle);
                        return true;
                    }
                }
                else
                {
                    ptrWin->run = false;
                    closeAllWindows();
                    if (windowHandle && *(int *)windowHandle)
                        DestroyWindow(windowHandle);
                    return true;
                }
                ptrWin->setCursor(cursor);
            }
            else if (!keyEsc)
            {
                ptrWin->run = false;
                closeAllWindows();
                ptrWin->run = false;
                return true;
            }
            if (ptrWin->neverClose)
                return false;
        }
        return false;
    }
    bool WINDOW::hideWindowOnExit(HWND windowHandle)
    {
        mbm::WINDOW *ptrWin = mbm::getWindow(windowHandle);
        if (ptrWin)
        {
            if (ptrWin->hideOnExit)
            {
                ptrWin->hide();
                return true;
            }
        }
        return false;
    }
    void WINDOW::timerProc(HWND, UINT, UINT_PTR idEvent, DWORD)
    {
        COM_BETWEEN_WINP *ptr = mbm::getComBetweenWinp(idEvent);
        if (ptr && ptr->extraParams)
        {
            if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_TIMER)
            {
                TIMER *ptrTime = static_cast<TIMER *>(ptr->extraParams);
                ptrTime->times += 1;
                if (ptrTime->onEventTimer)
                {
                    DATA_EVENT data(idEvent, ptrTime, ptr->userDrawer, mbm::WINPLUS_TYPE_TIMER, nullptr);
                    ptrTime->onEventTimer(ptr->ptrWindow, data);
                }
            }
        }
    }
    LRESULT WINDOW::WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLOREDIT:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->graphWin)
                {
                    HDC hdcStatic = (HDC)wParam;
                    ptr->graphWin->setCtlColor(hdcStatic);
                    return 0;
                }
            }
            break;
            case WM_ERASEBKGND:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                return _Do_default_Drawer_BackGround(ptr);
            }
            break;
            case WM_MEASUREITEM:
            {
                if (wParam == 0) // menu
                {
                    MEASUREITEMSTRUCT *measureItem = (MEASUREITEMSTRUCT *)lParam;
                    const int          si          = COM_BETWEEN_WINP::lsComBetweenWinp.size();
                    COM_BETWEEN_WINP * ptr         = si ? COM_BETWEEN_WINP::lsComBetweenWinp[si - 1] : 0;
                    if (ptr && ptr->graphWin)
                    {
                        return ptr->graphWin->measureItem(ptr, measureItem);
                    }
                    return 1;
                }
                else // combo box or list box
                {
                    MEASUREITEMSTRUCT *measureItem = (MEASUREITEMSTRUCT *)lParam;
                    COM_BETWEEN_WINP * ptr         = getComBetweenWinp(wParam);
                    if (ptr && ptr->graphWin)
                    {
                        if (measureItem->itemID == -1)
                            return ptr->graphWin->measureItem(ptr, measureItem);
                        return ptr->graphWin->measureItem(ptr, measureItem);
                    }
                    return 0;
                }
            }
            break;
            case WM_DRAWITEM:
            {
                LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam; // item drawing information
                if (lpdis)
                {
                    COM_BETWEEN_WINP *ptr = getComBetweenWinp(lpdis->hwndItem);
                    return _Do_default_Drawer(ptr, lpdis);
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_INPUT:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    HRAWINPUT phRawInput = (HRAWINPUT)lParam;
                    if (ptr->ptrWindow->onParseRawInput)
                        return ptr->ptrWindow->onParseRawInput(ptr->ptrWindow, phRawInput);
                }
            }
                return 0;
            //------------------------------------------------------------------------------------------------------------
            case WM_PAINT:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr)
                    return _Do_default_Drawer(ptr, nullptr);
                else
                    return 0;
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_MOUSEHOVER:
            {
                HWND              hwndChild = (HWND)wParam;
                COM_BETWEEN_WINP *ptr       = getComBetweenWinp(hwndChild);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->onMouseMove)
                        ptr->ptrWindow->onMouseMove(ptr->ptrWindow, x, y);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onTouchMove(ptr->hwnd, (float)x, (float)y);
                    InvalidateRect(ptr->ptrWindow->hwndLastHover, nullptr, 0);
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_MOUSEMOVE:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    if (lParam)
                    {
                        const int x = GET_X_LPARAM(lParam);
                        const int y = GET_Y_LPARAM(lParam);

                        if (ptr->ptrWindow->lastPosMouse.x != x && ptr->ptrWindow->lastPosMouse.y != y)
                        {
                            ptr->ptrWindow->lastPosMouse.x = x;
                            ptr->ptrWindow->lastPosMouse.y = y;
                            if (windowHandle != ptr->ptrWindow->hwndLastHover)
                            {
                                if (ptr->ptrWindow->hwndLastHover)
                                {
                                    InvalidateRect(ptr->ptrWindow->hwndLastHover, nullptr, 0);
                                }
                                ptr->ptrWindow->hwndLastHover = windowHandle;
                                InvalidateRect(windowHandle, nullptr, 0);
                            }
                        }
                        if (ptr->ptrWindow->onMouseMove)
                            ptr->ptrWindow->onMouseMove(ptr->ptrWindow, x, y);
                        if (ptr->ptrWindow->callEventsManager)
                            ptr->ptrWindow->callEventsManager->onTouchMove(ptr->hwnd, (float)x, (float)y);
                    }
                    if (ptr->typeWindowWinPlus == WINPLUS_TYPE_MENU)
                    {
                        for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
                        {
                            mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
                            if (menu && menu->idMenu != ptr->id)
                            {
                                menu->hideSubMenu();
                            }
                        }
                    }
                    else if (ptr->typeWindowWinPlus == WINPLUS_TYPE_SUB_MENU)
                    {
                        InvalidateRect(ptr->hwnd, nullptr, 0);
                    }
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_LBUTTONDOWN:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    if (ptr->typeWindowWinPlus == WINPLUS_TYPE_MENU)
                    {
                        __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                        if (menu)
                        {
                            menu->showSubMenu();
                        }
                    }
                    else if (ptr->typeWindowWinPlus == WINPLUS_TYPE_SUB_MENU)
                    {
                        __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                        if (menu && menu->onSelectedSubMenu)
                        {
                            const int y = GET_Y_LPARAM(lParam);
                            const int s = menu->lsSubMenusHeight.size();
                            if (s)
                            {
                                int index = -1;
                                for (int i = 0; i < s; ++i)
                                {
                                    int        pos     = i * menu->minSize[1];
                                    int        height  = menu->lsSubMenusHeight[i];
                                    const bool isHover = y < height && y > pos;
                                    if (isHover)
                                    {
                                        index = i;
                                        break;
                                    }
                                }
                                if (index != -1)
                                {
                                    menu->indexClickedMenu = index;
                                    mbm::DATA_EVENT data_event(ptr->id, menu, ptr->userDrawer, mbm::WINPLUS_TYPE_SUB_MENU,
                                                               nullptr);
                                    menu->onSelectedSubMenu(ptr->ptrWindow, data_event);
                                }
                            }
                            menu->hideSubMenu();
                        }
                    }
                    else
                    {
                        const int x = GET_X_LPARAM(lParam);
                        const int y = GET_Y_LPARAM(lParam);
                        if (ptr->ptrWindow->onClickLeftMouse)
                            ptr->ptrWindow->onClickLeftMouse(ptr->ptrWindow, x, y);
                        if (ptr->ptrWindow->callEventsManager)
                            ptr->ptrWindow->callEventsManager->onTouchDown(ptr->hwnd, 0, (float)x, (float)y);
                        for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
                        {
                            mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
                            if (menu)
                                menu->hideSubMenu();
                        }
                    }
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_LBUTTONUP:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->onReleaseLeftMouse)
                        ptr->ptrWindow->onReleaseLeftMouse(ptr->ptrWindow, x, y);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onTouchUp(ptr->hwnd, 0, (float)x, (float)y);
                }
                InvalidateRect(windowHandle, nullptr, 0);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_LBUTTONDBLCLK:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onDoubleClick(ptr->hwnd, (float)x, (float)y, 0);
                }
                InvalidateRect(windowHandle, nullptr, 0);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_RBUTTONDOWN:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->onClickRightMouse)
                        ptr->ptrWindow->onClickRightMouse(ptr->ptrWindow, x, y);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onTouchDown(ptr->hwnd, 1, (float)x, (float)y);
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_RBUTTONUP:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->onReleaseRightMouse)
                        ptr->ptrWindow->onReleaseRightMouse(ptr->ptrWindow, x, y);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onTouchUp(ptr->hwnd, 1, (float)x, (float)y);
                }
                InvalidateRect(windowHandle, nullptr, 0);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_RBUTTONDBLCLK:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onDoubleClick(ptr->hwnd, (float)x, (float)y, 1);
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_MBUTTONDOWN:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->onClickMiddleMouse)
                        ptr->ptrWindow->onClickMiddleMouse(ptr->ptrWindow, x, y);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onTouchDown(ptr->hwnd, 2, (float)x, (float)y);
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_MBUTTONUP:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int x = GET_X_LPARAM(lParam);
                    const int y = GET_Y_LPARAM(lParam);
                    if (ptr->ptrWindow->onReleaseMiddleMouse)
                        ptr->ptrWindow->onReleaseMiddleMouse(ptr->ptrWindow, x, y);
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onTouchUp(ptr->hwnd, 2, (float)x, (float)y);
                }
                InvalidateRect(windowHandle, nullptr, 0);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_MOUSEWHEEL:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    if (delta > 0)
                    {
                        if (ptr->ptrWindow->onScrollMouseEvent)
                            ptr->ptrWindow->onScrollMouseEvent(ptr->ptrWindow, true);
                        if (ptr->ptrWindow->callEventsManager)
                            ptr->ptrWindow->callEventsManager->onTouchZoom(ptr->hwnd, 1.0f);
                    }
                    else if (delta < 0)
                    {
                        if (ptr->ptrWindow->onScrollMouseEvent)
                            ptr->ptrWindow->onScrollMouseEvent(ptr->ptrWindow, false);
                        if (ptr->ptrWindow->callEventsManager)
                            ptr->ptrWindow->callEventsManager->onTouchZoom(ptr->hwnd, -1.0f);
                    }
                }
                InvalidateRect(windowHandle, nullptr, 0);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_SETCURSOR:
            {
                if (LOWORD(lParam) == HTCLIENT)
                {
                    COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                    if (ptr && ptr->ptrWindow)
                    {
                        switch (ptr->ptrWindow->CURRENT_CURSOR)
                        {
                            case WINPLUS_CURSOR_ARROW: SetCursor(LoadCursor(nullptr, IDC_ARROW)); break;
                            case WINPLUS_CURSOR_IBEAM: SetCursor(LoadCursor(nullptr, IDC_IBEAM)); break;
                            case WINPLUS_CURSOR_WAIT: SetCursor(LoadCursor(nullptr, IDC_WAIT)); break;
                            case WINPLUS_CURSOR_CROSS: SetCursor(LoadCursor(nullptr, IDC_CROSS)); break;
                            case WINPLUS_CURSOR_UPARROW: SetCursor(LoadCursor(nullptr, IDC_UPARROW)); break;
                            case WINPLUS_CURSOR_SIZENWSE: SetCursor(LoadCursor(nullptr, IDC_SIZENWSE)); break;
                            case WINPLUS_CURSOR_SIZENESW: SetCursor(LoadCursor(nullptr, IDC_SIZENESW)); break;
                            case WINPLUS_CURSOR_SIZEWE: SetCursor(LoadCursor(nullptr, IDC_SIZEWE)); break;
                            case WINPLUS_CURSOR_SIZENS: SetCursor(LoadCursor(nullptr, IDC_SIZENS)); break;
                            case WINPLUS_CURSOR_SIZEALL: SetCursor(LoadCursor(nullptr, IDC_SIZEALL)); break;
                            case WINPLUS_CURSOR_NO: SetCursor(LoadCursor(nullptr, IDC_NO)); break;
                            case WINPLUS_CURSOR_HAND: SetCursor(LoadCursor(nullptr, IDC_HAND)); break;
                            case WINPLUS_CURSOR_APPSTARTING: SetCursor(LoadCursor(nullptr, IDC_APPSTARTING)); break;
                            case WINPLUS_CURSOR_WITHOUT: SetCursor(0); break;
                            case WINPLUS_CURSOR_HELP: SetCursor(LoadCursor(nullptr, IDC_HELP)); break;
                            default: return 0;
                        }
                        SetWindowLong(windowHandle, DWL_MSGRESULT, 1);
                        return true;
                    }
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_KEYDOWN:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                {
                    if (ptr->ptrWindow->onKeyboardDown)
                        ptr->ptrWindow->onKeyboardDown(ptr->ptrWindow, (int)(wParam));
                    if (ptr->ptrWindow->callEventsManager)
                        ptr->ptrWindow->callEventsManager->onKeyDown(ptr->hwnd, (int)(wParam));
                }
                if (wParam == VK_ESCAPE)
                {
                    if (!exitNow(windowHandle, true))
                        return 0;
                }
                return 0;
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_KEYUP:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow && ptr->ptrWindow->onKeyboardUp)
                    ptr->ptrWindow->onKeyboardUp(ptr->ptrWindow, (int)(wParam));
                if (ptr && ptr->ptrWindow->callEventsManager)
                    ptr->ptrWindow->callEventsManager->onKeyUp(ptr->hwnd, (int)(wParam));
                return 0;
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_SETFOCUS:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr)
                {
                    if (ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_CHILD_WINDOW)
                        InvalidateRect(ptr->hwnd, nullptr, 0);
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_KILLFOCUS:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->typeWindowWinPlus == mbm::WINPLUS_TYPE_CHILD_WINDOW)
                    InvalidateRect(ptr->hwnd, nullptr, 0);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_WINDOWPOSCHANGING:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                LPWINDOWPOS       pos = (LPWINDOWPOS)lParam;
                if (pos != nullptr && ptr)
                {
                    if (((pos->flags & SWP_NOMOVE) == 0) && ((pos->flags & SWP_NOSIZE) == 0))
                    {
                        if (pos->x == 0 && pos->y == 0 && pos->cx == GetSystemMetrics(SM_CXSCREEN) &&
                            pos->cy == GetSystemMetrics(SM_CYSCREEN))
                        {

                            pos->x  = GetSystemMetrics(SM_XVIRTUALSCREEN);
                            pos->y  = GetSystemMetrics(SM_YVIRTUALSCREEN);
                            pos->cx = GetSystemMetrics(SM_CXMAXIMIZED);
                            pos->cy = GetSystemMetrics(SM_CYMAXIMIZED);
                        }
                    }
                }

                if (ptr && ptr->ptrWindow && ptr->ptrWindow->hwndInsertAfter)
                {
                    WINDOWPOS *p       = (WINDOWPOS *)lParam;
                    p->hwndInsertAfter = ptr->ptrWindow->hwndInsertAfter;
                    p->flags &= ~SWP_NOZORDER;
                    return 0;
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_NOTIFY:
            {
                int               command = LOWORD(wParam);
                COM_BETWEEN_WINP *ptr     = getComBetweenWinp(command);
                if (wParam == TTN_POP || wParam == TTN_SHOW || command == (int)TTN_POP || command == (int)TTN_SHOW)
                {
                    LPNMHDR pnmh = (LPNMHDR)lParam;
                    pnmh->code   = 0;
                }
                if ((command == (int)NM_CUSTOMDRAW) || (wParam == (WPARAM)NM_CUSTOMDRAW))
                {
                    LPNMTTCUSTOMDRAW lpNMCustomDraw = (LPNMTTCUSTOMDRAW)lParam;
                    lpNMCustomDraw->uDrawFlags      = 0;
                }
                COM_BETWEEN_WINP *ptrToolTip = getComBetweenWinp((int)lParam);
                if (ptrToolTip)
                {
                    LPNMTTCUSTOMDRAW lpNMCustomDraw = (LPNMTTCUSTOMDRAW)lParam;
                    lpNMCustomDraw->uDrawFlags      = 0;
                }
                for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
                {
                    mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
                    if (menu)
                        menu->hideSubMenu();
                }
                if (ptr && ptr->ptrWindow)
                {
                    switch (ptr->typeWindowWinPlus)
                    {
                        case WINPLUS_TYPE_SPIN_INT:
                        {
                            NMUPDOWN *lpnmud = (LPNMUPDOWN)lParam;
                            if (lpnmud == nullptr)
                                break;
                            mbm::SPIN_PARAMSi *spin = (mbm::SPIN_PARAMSi *)ptr->extraParams;
                            if (spin)
                            {
                                char num[255];
                                RECT rect = {0, 0, 0, 0};
                                GetWindowRect(ptr->hwnd, &rect);
                                int half  = (rect.bottom - rect.top) / 2 + rect.top;
                                int delta = ptr->ptrWindow->lastPosMouse.y > half ? -1 : 1;
                                spin->currentPosition += (spin->increment * delta);
                                if (spin->currentPosition > spin->max)
                                    spin->currentPosition = spin->max;
                                else if (spin->currentPosition < spin->min)
                                    spin->currentPosition = spin->min;
                                sprintf(num, "%d", spin->currentPosition);
                                ptr->ptrWindow->setText(ptr->id + 1, num);
                                LPARAM _lParam = (LPARAM)MAKELONG((short)50, 0);
                                SendMessageA(ptr->hwnd, UDM_SETPOS, 0, _lParam);
                                if (ptr->onEventWinPlus)
                                {
                                    DATA_EVENT data(ptr->id, spin, ptr->userDrawer, mbm::WINPLUS_TYPE_SPIN_INT, nullptr);
                                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                                }
                            }
                        }
                        break;
                        case WINPLUS_TYPE_SPIN_FLOAT:
                        {
                            NMUPDOWN *lpnmud = (LPNMUPDOWN)lParam;
                            if (lpnmud == nullptr)
                                break;
                            mbm::SPIN_PARAMSf *spinf = (mbm::SPIN_PARAMSf *)ptr->extraParams;
                            if (spinf)
                            {
                                char num[255];
                                spinf->currentPosition += (spinf->increment * (float)lpnmud->iDelta);
                                if (spinf->currentPosition > spinf->maxf)
                                    spinf->currentPosition = spinf->maxf;
                                else if (spinf->currentPosition < spinf->minf)
                                    spinf->currentPosition = spinf->minf;
                                sprintf(num, "%0.*f", spinf->precision, spinf->currentPosition);
                                ptr->ptrWindow->setText(ptr->id + 1, num);
                                LPARAM _lParam = (LPARAM)MAKELONG((short)50, 0);
                                SendMessageA(ptr->hwnd, UDM_SETPOS, 0, _lParam);
                                if (ptr->onEventWinPlus)
                                {
                                    DATA_EVENT data(ptr->id, spinf, ptr->userDrawer, WINPLUS_TYPE_SPIN_FLOAT, nullptr);
                                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                                }
                            }
                        }
                        break;
                        case WINPLUS_TYPE_TRACK_BAR:
                        {
                            if (ptr->onEventWinPlus)
                            {
                                TRACK_BAR_INFO *infoTrack = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
                                DATA_EVENT      data(ptr->id, infoTrack, ptr->userDrawer, WINPLUS_TYPE_TRACK_BAR, nullptr);
                                ptr->onEventWinPlus(ptr->ptrWindow, data);
                            }
                        }
                        break;
                        default: break;
                    }
                }
            }
                return 0;
            //------------------------------------------------------------------------------------------------------------
            case WM_SYSTRAY:
            {
                if (lParam == WM_RBUTTONUP)
                {
                    POINT cursorpos;
                    GetCursorPos(&cursorpos);
                    PostMessage(windowHandle, WM_SYSTRAY2, cursorpos.x, cursorpos.y);
                }
                else if (lParam == WM_LBUTTONDBLCLK)
                {
                    COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                    if (ptr)
                    {
                        /* Run the default menu item. */
                        UINT menuitem = GetMenuDefaultItem(ptr->ptrWindow->sysTry_menu, FALSE, 0);
                        if (menuitem != (UINT)-1)
                            PostMessage(windowHandle, WM_COMMAND, menuitem, 0);
                    }
                }
                else if (1029 == lParam) // what
                {
                    COM_BETWEEN_WINP *ptr = getComBetweenWinpTryIcon(windowHandle);
                    if (ptr && ptr->onEventWinPlus)
                    {
                        int        n = 0;
                        DATA_EVENT data(ptr->id, &n, ptr->userDrawer, mbm::WINPLUS_TYPE_TRY_ICON_MENU, nullptr);
                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                    }
                }
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_SYSTRAY2:
            {
                COM_BETWEEN_WINP *ptr            = getComBetweenWinp(windowHandle);
                static int        menuinprogress = 0;
                if (ptr && !menuinprogress)
                {
                    menuinprogress = 1;

                    SetForegroundWindow(windowHandle);
                    TrackPopupMenu(ptr->ptrWindow->sysTry_menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                                   wParam, lParam, 0, windowHandle, nullptr);
                    menuinprogress = 0;
                }
            }
            break;
            case WM_USER:
            {
                // COM_BETWEEN_WINP* ptr = getComBetweenWinp(windowHandle);
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_COMMAND:
                // case WM_SYSCOMMAND:

                {
                    int               command           = LOWORD(wParam);
                    int               notificationCodes = HIWORD(wParam);
                    COM_BETWEEN_WINP *ptr               = getComBetweenWinp(command);
                    if (ptr)
                    {
                        for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
                        {
                            mbm::WINDOW::__MENU_DRAW *menu = mbm::WINDOW::lsAllMenus[i];
                            if (menu)
                                menu->hideSubMenu();
                        }
                        switch (ptr->typeWindowWinPlus)
                        {
                            case WINPLUS_TYPE_BUTTON_TAB:
                            {
                                if (ptr->extraParams)
                                {
                                    __TAB_GROUP_DESC *ptrTab = static_cast<__TAB_GROUP_DESC *>(ptr->extraParams);
                                    InvalidateRect(ptr->hwnd, nullptr, FALSE);
                                    if (ptr->ptrWindow)
                                       ptr->ptrWindow->setIndexTabByGroup(ptrTab->idTabControlByGroup, ptrTab->index,true);
                                }
                            }
                            break;
                            case WINPLUS_TYPE_WINDOWNC:
                            case WINPLUS_TYPE_WINDOW:
                            case WINPLUS_TYPE_WINDOW_MESSAGE_BOX:
                            case WINPLUS_TYPE_IMAGE: 
                            { 
                                    InvalidateRect(ptr->hwnd, nullptr, FALSE);
                            }
                            break;
                            
                            case WINPLUS_TYPE_BUTTON:
                                switch (notificationCodes)
                                {
                                    case BN_CLICKED:
                                    {
                                        ptr->ptrWindow->hwndLastPressed = ptr->hwnd;
                                        InvalidateRect(ptr->hwnd, nullptr, 0);
                                        if (ptr->onEventWinPlus)
                                        {
                                            int        n = BN_CLICKED;
                                            DATA_EVENT data(ptr->id, &n, ptr->userDrawer, WINPLUS_TYPE_BUTTON, nullptr);
                                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                                        }
                                    }
                                    break;
                                }
                                break;
                            case WINPLUS_TYPE_CHECK_BOX:
                            {
                                if (notificationCodes == BN_CLICKED)
                                {
                                    int *value = static_cast<int *>(ptr->extraParams);
                                    int  v     = *value;
                                    *value     = !v;
                                    DATA_EVENT data(ptr->id, value, ptr->userDrawer, WINPLUS_TYPE_CHECK_BOX, nullptr);
                                    if (ptr->onEventWinPlus)
                                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                                    InvalidateRect(ptr->hwnd, nullptr, 0);
                                }
                                return 1;
                            }
                            break;
                            case WINPLUS_TYPE_RADIO_BOX:
                            {
                                if (notificationCodes == BN_CLICKED)
                                {
                                    RADIO_GROUP *radio = static_cast<RADIO_GROUP *>(ptr->extraParams);
                                    if (radio)
                                    {
                                        radio->checked = true;
                                        InvalidateRect(ptr->hwnd, nullptr, 0);
                                        for (std::set<int>::const_iterator it = radio->lsRadioGroup.cbegin();
                                             it != radio->lsRadioGroup.cend(); ++it)
                                        {
                                            const int         id         = *it;
                                            COM_BETWEEN_WINP *otherRadio = getComBetweenWinp(id);
                                            if (otherRadio && otherRadio->id != ptr->id)
                                            {
                                                RADIO_GROUP *radio2 = static_cast<RADIO_GROUP *>(otherRadio->extraParams);
                                                if (radio2)
                                                {
                                                    radio2->checked = false;
                                                    InvalidateRect(otherRadio->hwnd, nullptr, 0);
                                                }
                                            }
                                        }
                                        DATA_EVENT data(ptr->id, radio, ptr->userDrawer, WINPLUS_TYPE_RADIO_BOX, nullptr);
                                        if (ptr->onEventWinPlus)
                                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                                    }
                                }
                            }
                            break;
                            case WINPLUS_TYPE_COMBO_BOX:
                                switch (notificationCodes)
                                {
                                    case CBN_SELCHANGE:
                                    {
                                        if (ptr->onEventWinPlus)
                                        {
                                            int        index = ptr->ptrWindow->getSelectedIndex(ptr->id);
                                            DATA_EVENT data(ptr->id, &index, ptr->userDrawer, WINPLUS_TYPE_COMBO_BOX, nullptr);
                                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                                        }
                                    }
                                    break;
                                }
                                break;
                            case WINPLUS_TYPE_LIST_BOX:
                            {
                                switch (notificationCodes)
                                {
                                    case LBN_DBLCLK:
                                    case LBN_SELCHANGE:
                                    {
                                        if (ptr->onEventWinPlus)
                                        {
                                            int        index = ptr->ptrWindow->getSelectedIndex(ptr->id);
                                            DATA_EVENT data(ptr->id, &index, ptr->userDrawer, WINPLUS_TYPE_LIST_BOX, nullptr);
                                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                                        }
                                    }
                                    break;
                                }
                            }
                            break;
                            case WINPLUS_TYPE_RICH_TEXT:
                            {
                                switch (notificationCodes)
                                {
                                    case EN_CHANGE:
                                    {
                                        if (ptr->onEventWinPlus)
                                        {
                                            const int length = SendMessageA(ptr->hwnd, WM_GETTEXTLENGTH, 0, 0);
                                            if (length)
                                            {
                                                char *myText = new char[length + 4];
                                                memset(myText, 0, length + 4);
                                                const WORD copied =
                                                    (WORD)SendMessageA(ptr->hwnd, WM_GETTEXT, length + 1, (LPARAM)myText);
                                                if (copied > 0)
                                                {
                                                    DATA_EVENT data(ptr->id, (void *)myText, ptr->userDrawer,
                                                                    mbm::WINPLUS_TYPE_RICH_TEXT, myText);
                                                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                                                }
                                                delete[] myText;
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                            break;
                            case WINPLUS_TYPE_TEXT_BOX:
                            {
                                switch (notificationCodes)
                                {
                                    case EN_CHANGE:
                                    {
                                        if (ptr->extraParams == nullptr)
                                            break; // not created yet
                                        EDIT_TEXT_DATA *editData = static_cast<EDIT_TEXT_DATA *>(ptr->extraParams);
                                        unsigned int len = SendMessageA(ptr->hwnd, EM_LINELENGTH, 0, 0);
                                        if (len > editData->len)
                                        {
                                            char *tmp = (char *)realloc(editData->text, len + 1);
                                            if (tmp == nullptr)
                                            {
                                                free(editData->text);
                                                editData->text = nullptr;
                                                break;
                                            }
                                            else
                                                editData->text = tmp;
                                            if (!editData->len)
                                            {
                                                if(editData->text)
                                                    memset(editData->text, 0, len + 1);
                                            }
                                            editData->len = len + 1;
                                        }
                                        ((WORD *)editData->text)[0] = (WORD)editData->len - 1;
                                        int Length = SendMessageA(ptr->hwnd, EM_GETLINE, 0, (LPARAM)editData->text);
                                        if (Length < (int)editData->len)
                                            editData->text[Length] = 0;
                                        if (editData->spin != nullptr || editData->spinf != nullptr)
                                        {
                                            mbm::COM_BETWEEN_WINP *spinPtr = getComBetweenWinp(ptr->id - 1);
                                            if (spinPtr == nullptr)
                                                break;
                                            if (spinPtr->typeWindowWinPlus != WINPLUS_TYPE_SPIN_FLOAT &&
                                                spinPtr->typeWindowWinPlus != WINPLUS_TYPE_SPIN_INT)
                                                break;
                                            if (Length > 0)
                                            {
                                                if (editData->spin)
                                                {
                                                    int num = std::atoi(editData->text);
                                                    if (num == editData->spin->currentPosition)
                                                        break;
                                                    else if (num >= editData->spin->min && num <= editData->spin->max)
                                                        editData->spin->currentPosition = num;
                                                    else
                                                    {
                                                        if (num > editData->spin->max)
                                                            editData->spin->currentPosition = editData->spin->max;
                                                        else
                                                            editData->spin->currentPosition = editData->spin->min;
                                                        sprintf(editData->text, "%d", editData->spin->currentPosition);
                                                        ptr->ptrWindow->setText(ptr->id, editData->text);
                                                    }
                                                    if (spinPtr->onEventWinPlus)
                                                    {
                                                        DATA_EVENT data(ptr->id, editData->spin, ptr->userDrawer,
                                                                        WINPLUS_TYPE_SPIN_INT, editData->text);
                                                        spinPtr->onEventWinPlus(spinPtr->ptrWindow, data);
                                                    }
                                                }
                                                else if (editData->spinf)
                                                {
                                                    float num = (float)atof(editData->text);
                                                    if (fabs(num - editData->spinf->currentPosition) <=
                                                        FLT_EPSILON *
                                                            fmax(fabs(num), fabs(editData->spinf->currentPosition)))
                                                        // if(num == editData->spinf->currentPosition)
                                                        break;
                                                    else if (num >= editData->spinf->minf && num <= editData->spinf->maxf)
                                                        editData->spinf->currentPosition = num;
                                                    else
                                                    {
                                                        if (num > editData->spinf->maxf)
                                                            editData->spinf->currentPosition = editData->spinf->maxf;
                                                        else
                                                            editData->spinf->currentPosition = editData->spinf->minf;
                                                        switch (editData->spinf->precision)
                                                        {
                                                            case 1:
                                                                sprintf(editData->text, "%0.1f",
                                                                        editData->spinf->currentPosition);
                                                                break;
                                                            case 2:
                                                                sprintf(editData->text, "%0.2f",
                                                                        editData->spinf->currentPosition);
                                                                break;
                                                            case 3:
                                                                sprintf(editData->text, "%0.3f",
                                                                        editData->spinf->currentPosition);
                                                                break;
                                                            case 4:
                                                                sprintf(editData->text, "%0.4f",
                                                                        editData->spinf->currentPosition);
                                                                break;
                                                            case 5:
                                                                sprintf(editData->text, "%0.5f",
                                                                        editData->spinf->currentPosition);
                                                                break;
                                                            case 6:
                                                                sprintf(editData->text, "%0.6f",
                                                                        editData->spinf->currentPosition);
                                                                break;
                                                            default:
                                                                sprintf(editData->text, "%f",
                                                                        editData->spinf->currentPosition);
                                                        }
                                                        ptr->ptrWindow->setText(ptr->id, editData->text);
                                                    }
                                                    if (spinPtr->onEventWinPlus)
                                                    {
                                                        DATA_EVENT data(ptr->id, editData->spinf, ptr->userDrawer,
                                                                        WINPLUS_TYPE_SPIN_FLOAT, editData->text);
                                                        spinPtr->onEventWinPlus(spinPtr->ptrWindow, data);
                                                    }
                                                }
                                            }
                                        }
                                        else if (ptr->onEventWinPlus)
                                        {
                                            DATA_EVENT data(ptr->id, editData, ptr->userDrawer, WINPLUS_TYPE_TEXT_BOX,
                                                            editData->text);
                                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                                        }
                                    }
                                    break;
                                }
                            }
                            break;
                            case WINPLUS_TYPE_TRY_ICON_MENU:
                            case WINPLUS_TYPE_TRY_ICON_SUB_MENU:
                            {
                                if (ptr->onEventWinPlus)
                                {
                                    DATA_EVENT data(ptr->id, nullptr, ptr->userDrawer, WINPLUS_TYPE_TRY_ICON_MENU, nullptr);
                                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                                }
                            }
                            break;
                            case WINPLUS_TYPE_MENU:
                            {
                                __MENU_DRAW *menu = static_cast<__MENU_DRAW *>(ptr->extraParams);
                                if (menu)
                                {
                                    menu->showSubMenu();
                                }
                            }
							break;
                            case WINPLUS_TYPE_SUB_MENU:
                            {
                                if (ptr->onEventWinPlus)
                                {
                                    DATA_EVENT data(ptr->id, nullptr, ptr->userDrawer, WINPLUS_TYPE_SUB_MENU, nullptr);
                                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                                }
                            }
                            break;
                            case WINPLUS_TYPE_TRACK_BAR:
                            {
                                ptr->ptrWindow->hwndLastPressed = ptr->hwnd;
                                if (ptr->onEventWinPlus)
                                {
                                    TRACK_BAR_INFO *infoTrack = static_cast<TRACK_BAR_INFO *>(ptr->extraParams);
                                    DATA_EVENT      data(ptr->id, infoTrack, ptr->userDrawer, WINPLUS_TYPE_TRACK_BAR, nullptr);
                                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                                }
                                InvalidateRect(ptr->hwnd, 0, 0);
                            }
                            break;
                            default://WINPLUS_TYPE_GROUP_BOX,  WINPLUS_TYPE_GROUP_BOX_TAB, WINPLUS_TYPE_LABEL
                            {
                                if(notificationCodes == BN_SETFOCUS)
                                {
                                    if(ptr->onEventWinPlus)
                                    {
                                        int        n = BN_SETFOCUS;
                                        DATA_EVENT data(ptr->id, &n, ptr->userDrawer, ptr->typeWindowWinPlus, "focus");
                                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                                    }
                                }
                                else if(notificationCodes == BN_CLICKED)
                                {
                                    if(ptr->onEventWinPlus)
                                    {
                                        int        n = BN_CLICKED;
                                        DATA_EVENT data(ptr->id, &n, ptr->userDrawer, ptr->typeWindowWinPlus, "clicked");
                                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                                    }
                                    if(lParam)
                                    {
                                        POINT mouse;
                                        ptr->ptrWindow->getCursorPos(&mouse);
                                        MapWindowPoints(HWND_DESKTOP, ptr->hwnd, &mouse, 1);

                                        if (ptr->ptrWindow->onClickLeftMouse)
                                            ptr->ptrWindow->onClickLeftMouse(ptr->ptrWindow, mouse.x, mouse.y);
                                        if (ptr->ptrWindow->callEventsManager)
                                            ptr->ptrWindow->callEventsManager->onTouchDown( ptr->hwnd,0, (float)mouse.x, (float)mouse.y);
                                    }
                                }
                                else if(notificationCodes == BN_DOUBLECLICKED)
                                {
                                    if(ptr->onEventWinPlus)
                                    {
                                        int        n = BN_CLICKED;
                                        DATA_EVENT data(ptr->id, &n, ptr->userDrawer, ptr->typeWindowWinPlus, "doubleclicked");
                                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                                    }
                                    if(lParam && ptr->ptrWindow->callEventsManager)
                                    {
                                        POINT mouse;
                                        ptr->ptrWindow->getCursorPos(&mouse);
                                        MapWindowPoints(HWND_DESKTOP, ptr->hwnd, &mouse, 1);
                                        ptr->ptrWindow->callEventsManager->onDoubleClick(ptr->hwnd,(float)mouse.x, (float)mouse.y,0);
                                    }
                                }
                            }
                            break;
                        }
                    }
                    EndDialog(windowHandle, command);
                    return 0;
                }
                break;
            //------------------------------------------------------------------------------------------------------------
            case WM_GETMINMAXINFO:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
                if(ptr && lpMMI)
                {
                    WINDOW* win = ptr->getWindow();
                    if(win)
                    {
                        if(win->min_size_width > 0)
                            lpMMI->ptMinTrackSize.x = win->min_size_width;
                        if(win->min_size_height > 0)
                            lpMMI->ptMinTrackSize.y = win->min_size_height;
                        if(win->max_size_width > 0)
                            lpMMI->ptMaxTrackSize.x = win->max_size_width;
                        if(win->max_size_height > 0)
                            lpMMI->ptMaxTrackSize.y = win->max_size_height;
                    }
                }
            }
            break;
            case WM_SIZE:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr &&
                    (ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW || ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOWNC ||
                     ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW_MESSAGE_BOX))
                {
                    InvalidateRect(windowHandle, 0, 0);
                    if (ptr->onEventWinPlus)
                    {
                        int p = -1;
                        switch (wParam)
                        {
                            case SIZE_RESTORED:
                            case SIZE_MINIMIZED:
                            case SIZE_MAXSHOW:
                            case SIZE_MAXIMIZED:
                            case SIZE_MAXHIDE: p = (int)wParam; break;
                            default: p           = -1;
                        }
                        DATA_EVENT data(ptr->id, &p, ptr->userDrawer, ptr->typeWindowWinPlus, nullptr);
                        ptr->onEventWinPlus(ptr->ptrWindow, data);
                    }
                }
                for (unsigned int i = 0; i < COM_BETWEEN_WINP::lsComBetweenWinp.size(); ++i)
                {
                    COM_BETWEEN_WINP *ptrStatus = COM_BETWEEN_WINP::lsComBetweenWinp[i];
                    if (ptrStatus && ptrStatus->ptrWindow && ptrStatus->typeWindowWinPlus == WINPLUS_TYPE_STATUS_BAR)
                    {
                        const int  sizeBar = GetSystemMetrics(SM_CYCAPTION);
                        const RECT rect    = ptrStatus->ptrWindow->getRect(-1);
                        const int  Wheight = rect.bottom - rect.top;
                        const int  mywidth = rect.right - rect.left;
                        ptrStatus->ptrWindow->resize(ptrStatus->id, 0, Wheight - sizeBar, mywidth, sizeBar);
                    }
                }

                if (ptr && ptr->ptrWindow->callEventsManager && (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED))
                {
                    POINT pt   = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                    if(pt.x > 0 && pt.y > 0)//maybe only minimized
                    {
                        ptr->ptrWindow->callEventsManager->onResizeWindow(ptr->hwnd,pt.x, pt.y);
                    }
                }
                return 0;
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_QUERYENDSESSION:
            case WM_QUIT:
            case WM_CLOSE:
            {
                if (hideWindowOnExit(windowHandle))
                {
                    COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                    if (ptr && (ptr->typeWindowWinPlus == WINPLUS_TYPE_CHILD_WINDOW ||
                                ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW ||
                                ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOWNC ||
                                ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW_MESSAGE_BOX))
                    {
                        if (ptr->onEventWinPlus)
                        {
                            int        p = message;
                            DATA_EVENT data(ptr->id, &p, ptr->userDrawer, ptr->typeWindowWinPlus, nullptr);
                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                        }
                        ptr->ptrWindow->isVisible = false;
                    }
                    return 0;
                }
                WCHAR strAux[MAX_PATH] = L"";
                GetWindowTextW(windowHandle, strAux, MAX_PATH - 1);
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr)
                {
                    if (ptr->typeWindowWinPlus == WINPLUS_TYPE_CHILD_WINDOW)
                    {
                        if (ptr->onEventWinPlus)
                        {
                            int        p = message;
                            DATA_EVENT data(ptr->id, &p, ptr->userDrawer, WINPLUS_TYPE_CHILD_WINDOW, nullptr);
                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                        }
                        ptr->ptrWindow->hide(ptr->hwnd);
                        ptr->ptrWindow->isVisible = false;
                        return 0;
                    }
                    else if (ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW ||
                             ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOWNC ||
                             ptr->typeWindowWinPlus == WINPLUS_TYPE_WINDOW_MESSAGE_BOX)
                    {
                        ptr->ptrWindow->isVisible = false;
                        if (ptr->onEventWinPlus)
                        {
                            int        p = message;
                            DATA_EVENT data(ptr->id, &p, ptr->userDrawer, ptr->typeWindowWinPlus, nullptr);
                            ptr->onEventWinPlus(ptr->ptrWindow, data);
                            return 0;
                        }
                    }
                }
                if (!exitNow(windowHandle, false))
                    return 0;
                ptr = getComBetweenWinp(windowHandle);
                if (ptr)
                    ptr->ptrWindow->run = false;
            }
            break;
            //------------------------------------------------------------------------------------------------------------
            case WM_ENDSESSION:
            {
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->onEventWinPlus)
                {
                    int        p = message;
                    DATA_EVENT data(ptr->id, &p, ptr->userDrawer, ptr->typeWindowWinPlus, nullptr);
                    ptr->onEventWinPlus(ptr->ptrWindow, data);
                }
            }
            break;
            case WM_DESTROY:
            {
                destroyAlTimers(windowHandle);
                COM_BETWEEN_WINP *ptr = getComBetweenWinp(windowHandle);
                if (ptr && ptr->ptrWindow)
                    ptr->ptrWindow->run = false;
            }
                return 0;
                break;
                //------------------------------------------------------------------------------------------------------------
        }
        return DefWindowProc(windowHandle, message, wParam, lParam);
    }
    void WINDOW::getCursorPos(POINT *p)
    {
        if (p)
            *p = this->lastPosMouse;
    }

    bool WINDOW::isUsingDoubleBuffer()const
    {
        return usingDoubleBuffer;
    }
  
    WINDOW *getWindow(HWND hwnd)
    {
        std::vector<COM_BETWEEN_WINP *>::const_iterator i;
        i = COM_BETWEEN_WINP::lsComBetweenWinp.cbegin();
        while (i != COM_BETWEEN_WINP::lsComBetweenWinp.cend())
        {
            COM_BETWEEN_WINP *ptrCom = (*i);
            if (ptrCom && ptrCom->hwnd == hwnd)
                return ptrCom->ptrWindow;
            else
                ++i;
        }
        return nullptr;
    }
    WINDOW *getLastWindow()
    {
        const int s = COM_BETWEEN_WINP::lsComBetweenWinp.size();
        if (s)
        {
            for (int i = s - 1; i > 0; --i)
            {
                COM_BETWEEN_WINP *ptrCom = (COM_BETWEEN_WINP::lsComBetweenWinp[i]);
                if (ptrCom && ptrCom->hwnd && ptrCom->typeWindowWinPlus == mbm::WINPLUS_TYPE_WINDOW && ptrCom->ptrWindow &&
                    ptrCom->ptrWindow->run)
                    return ptrCom->ptrWindow;
            }
        }
        return nullptr;
    }
    WINDOW *getFirstWindow()
    {
        const unsigned int s = COM_BETWEEN_WINP::lsComBetweenWinp.size();
        if (s)
        {
            for (unsigned int i = 0; i < s; ++i)
            {
                COM_BETWEEN_WINP *ptrCom = (COM_BETWEEN_WINP::lsComBetweenWinp[i]);
                if (ptrCom && ptrCom->hwnd && ptrCom->typeWindowWinPlus == mbm::WINPLUS_TYPE_WINDOW && ptrCom->ptrWindow &&
                    ptrCom->ptrWindow->run)
                    return ptrCom->ptrWindow;
            }
        }
        return nullptr;
    }
    const char *selectetDirectory(HWND hwnd, char *outDir)
    {
        BROWSEINFOA  bi;
        LPITEMIDLIST pidl;
        LPMALLOC     lpMalloc = nullptr;
        memset(&bi, 0, sizeof(bi));
        if (outDir == nullptr)
            return nullptr;
        // Get a pointer to the shell memory allocator
        if (SHGetMalloc(&lpMalloc) != S_OK)
        {
            MessageBoxA(nullptr, ("Error opening browse window"), ("ERROR"), MB_OK);
            return nullptr;
        }
        bi.hwndOwner      = hwnd;
        bi.pidlRoot       = nullptr;
        bi.pszDisplayName = outDir;
        bi.lpszTitle      = "Select a folder";
        bi.ulFlags        = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_VALIDATE | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
        bi.lpfn           = nullptr;
        bi.lParam         = 0;
        //this does not make any sense however I realize that when folder dialog is supposed to appear, it does not until press ALT in the keyboard...
        keybd_event(VK_LMENU, 0, KEYEVENTF_EXTENDEDKEY, 0);
		keybd_event(VK_LMENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        //end bizarre behavior
        pidl              = SHBrowseForFolderA(&bi);
        if (pidl)
        {
            // Copy the path directory to the buffer
            if (SHGetPathFromIDListA(pidl, outDir))
            {
                lpMalloc->Free(pidl);
                return outDir;
            }
        }
        return nullptr;
    }
std::vector<std::string> &openFileBoxMult(const char *extension_, const char *title,
                                                 bool enableReturnExtencion , bool enableAllFileType ,
                                                 HWND hwnd, const char *defaultNameInDialog)
{
    OPENFILENAMEA dialogBox;
    char          extension[256]={};
    strcpy(extension, extension_);
    char        filtersFiles[MAX_PATH * 100] = "";
    static char path[MAX_PATH * 100];
    char        extencionTemp[500] = "";
    memset(path, 0, MAX_PATH * 100);
    ZeroMemory(&dialogBox, sizeof(dialogBox));
    dialogBox.lStructSize = sizeof(dialogBox);
    dialogBox.hwndOwner   = hwnd;

    if (strlen(extension) == 0)
    {
        strcpy(filtersFiles, "Abrir Arquivos(*");
        strcat(filtersFiles, extension);
        strcat(filtersFiles, ")$*");
        strcat(filtersFiles, extension);
    }
    else
        strcpy(filtersFiles, "Todos Tipos De Arquivo(*.*)$*.*$");

    if (enableAllFileType && strlen(extension) > 0)
        strcat(filtersFiles, "$Todos Tipos De Arquivo(*.*)$*.*$");

    for (int x = 0; x < MAX_PATH * 100; ++x)
    {
        if (filtersFiles[x] == '$')
            filtersFiles[x] = '\0';
    }

    if (strlen(extension) > 0)
    {
        const int len = strlen(extension);
        for (unsigned char x = 0, y = 0; x < 50 && x < len; ++x)
        {
            if (extension[x] != '.')
            {
                extencionTemp[y] = extension[x];
                y++;
            }
        }
    }

    dialogBox.lpstrFilter = filtersFiles;
    dialogBox.lpstrFile   = path;
    dialogBox.nMaxFile    = MAX_PATH * 100;
    dialogBox.lpstrDefExt = extencionTemp;

    dialogBox.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST |
                      OFN_ALLOWMULTISELECT;
    if (title != nullptr)
        dialogBox.lpstrTitle = title;
    for (DWORD i = (MAX_PATH * 100) - 1; i && filtersFiles[i]; --i)
    {
        filtersFiles[i] = 0;
    }
    if (defaultNameInDialog)
    {
        strcpy(dialogBox.lpstrFile, defaultNameInDialog);
    }
    static std::vector<std::string> ret;
    ret.clear();

    if (!GetOpenFileNameA(&dialogBox))
        return ret;

    if (path[0] == 0)
        return ret;
    char *directory = path;
    int   lenStr    = strlen(directory);
    while (lenStr + 1 < sizeof(path) && path[lenStr + 1])
    {
        char        tmp[1024];
        const char *newFile = &path[lenStr + 1];
        snprintf(tmp,sizeof(tmp), "%s%c%s", directory, WINPLUS_DIRSEPARATOR[0], newFile);
        lenStr = strlen(newFile) + lenStr + 1;
        if (!enableReturnExtencion)
        {
            int e, pointIndex = 0;
            for (e = 0; tmp[e]; ++e)
            {
                if (tmp[e] == '.')
                    pointIndex = e;
            }
            if (pointIndex)
            {
                for (e     = pointIndex; tmp[e]; ++e)
                    tmp[e] = 0;
            }
        }
        ret.push_back(tmp);
    }
    if (ret.size() == 0 && directory[0] != 0)
    {
        if (!enableReturnExtencion)
        {
            int e, pointIndex = 0;
            for (e = 0; directory[e]; ++e)
            {
                if (directory[e] == '.')
                    pointIndex = e;
            }
            if (pointIndex)
            {
                for (e           = pointIndex; directory[e]; ++e)
                    directory[e] = 0;
            }
        }
        ret.push_back(directory);
    }
    return ret;
}
    WCHAR *openFileBoxW(const WCHAR *extension, const WCHAR *title, bool enableReturnExtencion ,
                           bool enableAllFileType , HWND hwnd, const WCHAR *defaultNameInDialog)
{
    const DWORD   MAX_PATH_FILE = 1024;
    OPENFILENAMEW dialogBox;
    WCHAR         filtersFiles[MAX_PATH_FILE]  = L"";
    static WCHAR  path[MAX_PATH_FILE]          = L"";
    WCHAR         extencionTemp[MAX_PATH_FILE] = L"";
    ZeroMemory(&dialogBox, sizeof(dialogBox));
    dialogBox.lStructSize = sizeof(dialogBox);
    dialogBox.hwndOwner   = hwnd;

    memset(filtersFiles, 0, sizeof(filtersFiles));
    memset(path, 0, sizeof(path));

    if (extension != nullptr)
    {
        wcscpy(filtersFiles, L"Abrir Arquivos(*");
        wcscat(filtersFiles, extension);
        wcscat(filtersFiles, L")$*");
        wcscat(filtersFiles, extension);
    }
    else
    {
        wcscpy(filtersFiles, L"Todos Tipos De Arquivo(*.*)$*.*$");
    }

    if (enableAllFileType && extension != nullptr)
        wcscat(filtersFiles, L"$Todos Tipos De Arquivo(*.*)$*.*$");

    for (int x = 0; x < 1024; ++x)
    {
        if (filtersFiles[x] == '$')
            filtersFiles[x] = '\0';
    }
    memset(extencionTemp, 0, sizeof(extencionTemp));

    if (extension != nullptr)
    {
        const int len = wcslen(extension);
        for (int x = 0, y = 0; x < sizeof(extencionTemp) && x < len; ++x)
        {
            if (extension[x] != '.')
            {
                extencionTemp[y] = extension[x];
                y++;
            }
        }
    }

    dialogBox.lpstrFilter = filtersFiles;
    dialogBox.lpstrFile   = path;
    dialogBox.nMaxFile    = MAX_PATH_FILE;
    dialogBox.lpstrDefExt = extencionTemp;
    dialogBox.Flags       = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST;
    if (title != nullptr)
        dialogBox.lpstrTitle        = title;
    filtersFiles[MAX_PATH_FILE - 1] = 0;
    if (defaultNameInDialog)
        wcscpy(dialogBox.lpstrFile, defaultNameInDialog);

    if (!GetOpenFileNameW(&dialogBox))
        return nullptr;

    if (path[0] == 0)
        return nullptr;
    if (!enableReturnExtencion)
    {
        int e, pointIndex = 0;
        for (e = 0; path[e]; ++e)
        {
            if (path[e] == '.')
                pointIndex = e;
        }
        if (pointIndex)
        {
            for (e      = pointIndex; path[e]; ++e)
                path[e] = 0;
        }
    }
    return path;
}
    char *openFileBox(const char *extension_, const char *title_, bool enableReturnExtencion, bool enableAllFileType,
                         HWND hwnd, const char *defaultNameInDialog_, char *outFileName)
{
    WCHAR *extension           = mbm::toWchar(extension_, nullptr);
    WCHAR *title               = mbm::toWchar(title_, nullptr);
    WCHAR *defaultNameInDialog = mbm::toWchar(defaultNameInDialog_, nullptr);
    WCHAR *wret = openFileBoxW(extension, title, enableReturnExtencion, enableAllFileType, hwnd, defaultNameInDialog);
    delete[] extension;
    delete[] title;
    delete[] defaultNameInDialog;
    if (wret == nullptr)
        return nullptr;
    char *ret = mbm::toChar(wret, nullptr);
    if (ret && outFileName)
    {
        strcpy(outFileName, ret);
        delete [] ret;
        ret = outFileName;
    }
    return ret;
}
    WCHAR *saveFileBoxW(WCHAR *extension, WCHAR *title, bool enableReturnExtencion ,
                           bool enableAllFileType , HWND hwnd , const WCHAR *defaultNameInDialog )
{
    const DWORD   MAX_PATH_FILE = 1024;
    OPENFILENAMEW dialogBox;
    WCHAR         filtersFiles[MAX_PATH_FILE]  = L"";
    static WCHAR  path[MAX_PATH_FILE]          = L"";
    WCHAR         extencionTemp[MAX_PATH_FILE] = L"";
    ZeroMemory(&dialogBox, sizeof(dialogBox));
    dialogBox.lStructSize = sizeof(dialogBox);
    dialogBox.hwndOwner   = hwnd;

    memset(path, 0, sizeof(path));

    if (extension != nullptr)
    {
        wcscpy(filtersFiles, L"Salvar Arquivo(*");
        wcscat(filtersFiles, extension);
        wcscat(filtersFiles, L")$*");
        wcscat(filtersFiles, extension);
    }
    else
    {
        wcscpy(filtersFiles, L"Todos Tipos De Arquivo(*.*)$*.*$");
    }

    if (enableAllFileType && extension != nullptr)
        wcscat(filtersFiles, L"$Todos Tipos De Arquivo(*.*)$*.*$");

    for (int x = 0; x < MAX_PATH_FILE; ++x)
    {
        if (filtersFiles[x] == '$')
            filtersFiles[x] = '\0';
    }

    memset(extencionTemp, 0, sizeof(extencionTemp));

    if (extension != nullptr)
    {
        const int len = wcslen(extension);
        for (int x = 0, y = 0; x < MAX_PATH_FILE && x < len; ++x)
        {
            if (extension[x] != '.')
            {
                extencionTemp[y] = extension[x];
                y++;
            }
        }
    }

    dialogBox.lpstrFilter = filtersFiles;
    dialogBox.lpstrFile   = path;
    dialogBox.nMaxFile    = MAX_PATH_FILE;
    dialogBox.lpstrDefExt = extencionTemp;
    dialogBox.Flags       = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    if (title != nullptr)
        dialogBox.lpstrTitle        = title;
    filtersFiles[MAX_PATH_FILE - 1] = 0;
    if (defaultNameInDialog)
    {
        wcscpy(dialogBox.lpstrFile, defaultNameInDialog);
    }

    if (!GetSaveFileNameW(&dialogBox))
        return nullptr;

    if (path[0] == 0)
        return nullptr;

    if (!enableReturnExtencion)
    {
        int e, pointIndex = 0;
        for (e = 0; path[e]; ++e)
        {
            if (path[e] == '.')
                pointIndex = e;
        }
        if (pointIndex)
        {
            for (e      = pointIndex; path[e]; ++e)
                path[e] = 0;
        }
    }
    return path;
}
    char *saveFileBox(const char *extension_, const char *title_, const bool enableReturnExtencion,
                         const bool enableAllFileType, HWND hwnd, const char *defaultNameInDialog_, char *outFileName)
{
    WCHAR *extension           = mbm::toWchar(extension_, nullptr);
    WCHAR *title               = mbm::toWchar(title_, nullptr);
    WCHAR *defaultNameInDialog = mbm::toWchar(defaultNameInDialog_, nullptr);
    WCHAR *wret = saveFileBoxW(extension, title, enableReturnExtencion, enableAllFileType, hwnd, defaultNameInDialog);
    delete[] extension;
    delete[] title;
    delete[] defaultNameInDialog;
    if (wret == nullptr)
        return nullptr;
    char *ret = mbm::toChar(wret, nullptr);
    if (ret && outFileName)
    {
        strcpy(outFileName, ret);
        delete [] ret;
        ret = outFileName;
    }
    return ret;
}
    bool getColorFromDialogBox(unsigned char &red, unsigned char &green, unsigned char &blue, HWND hwnd )
{
    CHOOSECOLORA CS;
    COLORREF     ref = 0;
    CS.lStructSize   = sizeof(CHOOSECOLORA);
    CS.hwndOwner     = hwnd;
    CS.hInstance     = 0;
    CS.rgbResult     = 0;
    CS.lpCustColors  = &ref;
    CS.Flags         = CC_SOLIDCOLOR | CC_FULLOPEN;
    if (ChooseColorA(&CS))
    {
        unsigned char *pRgb = 0;
        pRgb                = (unsigned char *)(&CS.rgbResult);
        red                 = *pRgb;
        pRgb++;
        green = *pRgb;
        pRgb++;
        blue = *pRgb;
        return true;
    }
    return false;
}
    bool getFontFromDialogBox(LOGFONTA *fontOut, HWND hwnd )
{
    if (fontOut == nullptr)
        return false;
    CHOOSEFONTA font;
    font.lStructSize = sizeof(CHOOSEFONTA);
    font.hwndOwner   = hwnd;
    font.hDC         = 0;
    font.lpLogFont   = fontOut;
    font.Flags       = CF_INITTOLOGFONTSTRUCT;
    if (ChooseFontA(&font))
        return true;
    return false;
}
    bool getFontFromDialogBox(LOGFONTW *fontOut, HWND hwnd )
{
    if (fontOut == nullptr)
        return false;
    static CHOOSEFONTW font;
    font.lStructSize = sizeof(CHOOSEFONTW);
    font.hwndOwner   = hwnd;
    font.hDC         = 0;
    font.lpLogFont   = fontOut;
    font.Flags       = CF_INITTOLOGFONTSTRUCT;
    if (ChooseFontW(&font))
        return true;
    return false;
}
    RECT getMenuRect(int idWindow, int myId)
{

    std::vector<RECT *> lsRect;
    COM_BETWEEN_WINP *  owner = getComBetweenWinp(idWindow);
    if (owner)
    {
        for (unsigned int i = 0, s = COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
        {
            COM_BETWEEN_WINP *ptr = COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (ptr && ptr->getId() != myId &&
                (ptr->typeWindowWinPlus == WINPLUS_TYPE_MENU || ptr->typeWindowWinPlus == WINPLUS_TYPE_SUB_MENU) &&
                ptr->owerHwnd == owner->hwnd)
            {
                RECT *rect = new RECT;
                GetClientRect(ptr->hwnd, rect);
                lsRect.push_back(rect);
                MapWindowRect(ptr->hwnd, owner->hwnd, rect);
            }
        }
    }
    if (lsRect.size())
    {
        RECT rectOut;
        rectOut.left   = 99999999;
        rectOut.right  = -99999999;
        rectOut.top    = 99999999;
        rectOut.bottom = -99999999;
        for (unsigned int i = 0, s = lsRect.size(); i < s; ++i)
        {
            RECT *rect = lsRect[i];
            if (rect->left < rectOut.left)
                rectOut.left = rect->left;
            if (rect->top < rectOut.top)
                rectOut.top = rect->top;
            if (rect->right > rectOut.right)
                rectOut.right = rect->right;
            if (rect->bottom > rectOut.bottom)
                rectOut.bottom = rect->bottom;
            delete rect;
            lsRect[i] = nullptr;
        }
        return rectOut;
    }
    else
    {
        RECT rectOut;
        rectOut.left   = 0;
        rectOut.right  = 0;
        rectOut.top    = 0;
        rectOut.bottom = 0;
        return rectOut;
    }
}
    COM_BETWEEN_WINP *getNewComBetween(HWND owerHwnd_, OnEventWinPlus onEventWinPlus_, WINDOW *me,
                                          TYPE_WINDOWS_WINPLUS typeMe, void *extraParams_, const int idDest,
                                          USER_DRAWER *userDrawer)
{
    COM_BETWEEN_WINP *comBetweenWinp =
        new COM_BETWEEN_WINP(owerHwnd_, onEventWinPlus_, me, typeMe, extraParams_, idDest, userDrawer);
    mbm::COM_BETWEEN_WINP *ptrDest = mbm::getComBetweenWinp(idDest);
    if (ptrDest && ptrDest->typeWindowWinPlus == WINPLUS_TYPE_GROUP_BOX_TAB && ptrDest->extraParams)
    {
        __TAB_GROUP_DESC *tabGroup = static_cast<__TAB_GROUP_DESC *>(ptrDest->extraParams);
        tabGroup->lsHwndComponents.push_back(&comBetweenWinp->hwnd);
    }

    if (typeMe == WINPLUS_TYPE_TEXT_BOX || typeMe == WINPLUS_TYPE_SPIN_FLOAT || typeMe == WINPLUS_TYPE_SPIN_INT ||
        typeMe == WINPLUS_TYPE_CHECK_BOX || typeMe == WINPLUS_TYPE_COMBO_BOX || typeMe == WINPLUS_TYPE_LIST_BOX ||
        typeMe == WINPLUS_TYPE_RADIO_BOX || typeMe == WINPLUS_TYPE_BUTTON || typeMe == WINPLUS_TYPE_TRACK_BAR)
    {
        comBetweenWinp->idNextFocus = comBetweenWinp->id + 1;
    }
    return comBetweenWinp;
}

    char *getNameFromPath(const char *fileNamePath, const bool removeCharacterInvalids, char *primaryPartFromPath,
                             char *outFileName)
{
    if (fileNamePath == nullptr || outFileName == nullptr)
        return 0;
    strcpy(outFileName, fileNamePath);
    char *lastFounded = outFileName;
    char *token       = strtok(outFileName, WINPLUS_DIRSEPARATOR);
    if (token && primaryPartFromPath)
        strcpy(primaryPartFromPath, fileNamePath);
    while (token != nullptr)
    {
        lastFounded = token;
        token       = strtok(nullptr, WINPLUS_DIRSEPARATOR);
    }
    if (lastFounded)
        strcpy(outFileName, lastFounded);
    if (primaryPartFromPath)
    {
        std::string str = primaryPartFromPath;
        int         i   = str.find(outFileName);
        if (i > -1)
        {
            primaryPartFromPath[i] = 0;
            int j                  = strlen(primaryPartFromPath);
            if (j && primaryPartFromPath[j - 1] && primaryPartFromPath[j - 1] == WINPLUS_DIRSEPARATOR[0])
            {
                primaryPartFromPath[j - 1] = 0;
            }
        }
    }
    if (removeCharacterInvalids)
    {
        for (int count = 0; outFileName[count]; ++count)
        {
            if ((outFileName[count] >= 65 && outFileName[count] <= 90) ||
                (outFileName[count] >= 97 && outFileName[count] <= 122))
            {
                // Podem ser declarados
                continue;
            }
            else
            {
                if (outFileName[count] == '.')
                {
                    outFileName[count] = 0;
                    break;
                }
                else
                    outFileName[count] = '_';
            }
        }
    }
    return outFileName;
}

    const char *getHeaderToResource()
{
    static const char headerResource[] = {
        "\t/*------------------------------------------------------------------------------------------|\n"
        "\t|*******************************************************************************************|\n"
        "\t|-------------------------------------------------------------------------------------------|\n"
        "\t|              Arquivo gerado automaticamente                                               |\n"
        "\t|              FAZ PARTE DA FRAMEWORK MBM PARA A CRIAÇÃO DE UM GAME OU APLICAÇÃO GRÁFICA    |\n"
        "\t|              NÃO modifique este arquivo! DO NOT Modify this file!                         |\n"
        "\t|              E-Mail msn (michel.braz.morais@gmail.com)                                    |\n"
        "\t|-------------------------------------------------------------------------------------------|\n"
        "\t|*******************************************************************************************|\n"
        "\t-------------------------------------------------------------------------------------------*/\n\n"};
    return headerResource;
}

    bool saveToFileBinary(const char *fileName, void *header, DWORD sizeOfHeader, void *dataIn, DWORD sizeOfDataIn)
{
    if (fileName == nullptr)
        return false;
    if (dataIn == nullptr)
        return false;
    if (!sizeOfDataIn)
        return false;
    FILE *file = fopen(fileName, "wb");
    if (!file)
        return false;
    if (sizeOfHeader && header)
    {
        if (!fwrite(header, sizeOfHeader, 1, file))
        {
            fclose(file);
            file = nullptr;
            return false;
        }
    }
    if (fwrite(dataIn, sizeOfDataIn, 1, file))
    {
        fclose(file);
        file = nullptr;
        return true;
    }
    return false;
}

    bool loadFromFileBynary(const char *fileName, void *header, DWORD sizeOfHeader, void *dataOut, DWORD sizeOfDataOut)
{
    if (fileName == nullptr)
        return false;
    if (dataOut == nullptr)
        return false;
    if (!sizeOfDataOut)
        return false;
    FILE *file = fopen(fileName, "rb");
    if (!file)
        return false;
    if (sizeOfHeader && header)
    {
        if (!fread(header, sizeOfHeader, 1, file))
        {
            fclose(file);
            file = nullptr;
            return false;
        }
    }
    if (fread(dataOut, sizeOfDataOut, 1, file))
    {
        fclose(file);
        file = nullptr;
        return true;
    }
    return false;
}
    bool loadHeaderFromFileBynary(const char *fileName, void *header, DWORD sizeOfHeader)
{
    if (fileName == nullptr)
        return false;
    FILE *file = fopen(fileName, "rb");
    if (!file)
        return false;
    if (sizeOfHeader && header)
    {
        if (fread(header, sizeOfHeader, 1, file))
        {
            fclose(file);
            file = nullptr;
            return true;
        }
        fclose(file);
        file = nullptr;
    }
    return false;
}
    void closeAllWindows()
    {
        std::vector<COM_BETWEEN_WINP *>::const_iterator i;
        i = COM_BETWEEN_WINP::lsComBetweenWinp.cbegin();
        while (i != COM_BETWEEN_WINP::lsComBetweenWinp.cend())
        {
            COM_BETWEEN_WINP *ptr = (*i);
            if (ptr && ptr->ptrWindow)
            {
                ptr->ptrWindow->run = false;
            }
            ++i;
        }
    }
}

    __AUX_MONITOR_SELECT::__AUX_MONITOR_SELECT()
    {
        indexCmbSelectedeMonitor = 0;
        idCmbSelectMonitor       = 0;
        idbntOk                  = 0;
        idChkAskAboutMonitor     = 0;
        askMeAgain               = true;
        monitor                  = nullptr;
    }
    void __AUX_MONITOR_SELECT::__0_onProcess(mbm::WINDOW *, mbm::DATA_EVENT &dataEvent)
    {
        if (dataEvent.getAsInt() == WM_QUIT)
            exit(0);
    }
    void __AUX_MONITOR_SELECT::__0_onPressOkMonitor(mbm::WINDOW *w, mbm::DATA_EVENT &)
    {
        __AUX_MONITOR_SELECT *__auxSelectMonitor = static_cast<__AUX_MONITOR_SELECT *>(w->getObjectContext(0));
        mbm::MONITOR_MANAGER *manMonitor         = static_cast<mbm::MONITOR_MANAGER *>(w->getObjectContext(1));
        if (__auxSelectMonitor->monitor && manMonitor)
        {
            mbm::MONITOR monitorOut;
            if (manMonitor->getMonitor(w->getSelectedIndex(__auxSelectMonitor->indexCmbSelectedeMonitor), &monitorOut))
                *__auxSelectMonitor->monitor = monitorOut;
        }
        w->run = false;
        w->closeWindow();
    }
    void __AUX_MONITOR_SELECT::__1_onCheckedDontAskAgain(mbm::WINDOW *w, mbm::DATA_EVENT &)
    {
        __AUX_MONITOR_SELECT *__auxSelectMonitor = static_cast<__AUX_MONITOR_SELECT *>(w->getObjectContext(0));
        if (__auxSelectMonitor)
            __auxSelectMonitor->askMeAgain = !w->getCheckBoxState(__auxSelectMonitor->idChkAskAboutMonitor);
    }


namespace mbm
{

    bool selectMonitor(mbm::MONITOR *monitorOut)
{
    MONITOR_MANAGER manMonitor;
    manMonitor.updateMonitors();
    if (monitorOut == nullptr)
        return false;
    if (manMonitor.getTotalMonitor() == 1)
    {
        if (manMonitor.getTotalMonitor())
        {
            mbm::MONITOR temp;
            if (!manMonitor.getMonitor(0, &temp))
                return false;
            *monitorOut = temp;
            return true;
        }
        else
            MessageBoxA(0, "Unexpected error!\nIt wasn't found monitors available!!!", "MBM", 0);
    }
    else
    {
        mbm::WINDOW w;
        FILE *      fp = fopen("plusWin.cfg", "rt");
        if (fp)
        {
            char strMonitor[MAX_PATH];
            int  index = 0;
            fscanf(fp, "%8s=", strMonitor);
            fscanf(fp, "%d", &index);
            fclose(fp);
            fp = nullptr;
            if (manMonitor.getMonitor(index, monitorOut))
                return true;
        }

        w.init("Monitor options", 400, 200, 0, 0, false, false, false, false, __AUX_MONITOR_SELECT::__0_onProcess, false,
               101);
        w.addLabel("Monitors available:", 10, 10, 380, 25);
        __AUX_MONITOR_SELECT __auxSelectMonitor;
        __auxSelectMonitor.monitor                  = monitorOut;
        __auxSelectMonitor.indexCmbSelectedeMonitor = w.addCombobox(10, 50, 380, 100);
        w.setObjectContext(&__auxSelectMonitor, 0);
        w.setObjectContext(&manMonitor, 1);
        DWORD s = manMonitor.getTotalMonitor();

        for (DWORD i = 0; i < s; ++i)
        {
            char         str[255];
            mbm::MONITOR temp;
            if (!manMonitor.getMonitor(i, &temp))
                return false;
            sprintf(str, "%d: %ld x %ld, frequency:%lu, position:%ld x %ld", (int)i + 1, temp.width, temp.height,
                    temp.frequency, temp.position.x, temp.position.y);
            w.addText(__auxSelectMonitor.indexCmbSelectedeMonitor, str);
        }

        w.setSelectedIndex(__auxSelectMonitor.indexCmbSelectedeMonitor, manMonitor.getIndexMainMonitor());
        manMonitor.getMonitor(manMonitor.getIndexMainMonitor(), monitorOut);
        __auxSelectMonitor.idbntOk = w.addButton("Ok", 320, 80, 60, 20, -1, __AUX_MONITOR_SELECT::__0_onPressOkMonitor);
        __auxSelectMonitor.idChkAskAboutMonitor =
            w.addCheckBox("Don't ask me again!", 10, 80, 200, 20, __AUX_MONITOR_SELECT::__1_onCheckedDontAskAgain);

        w.setCheckBox(false, __auxSelectMonitor.idChkAskAboutMonitor);
        w.askOnExit = false;
        w.hideConsoleWindow();
        w.enterLoop(nullptr);
        if (!__auxSelectMonitor.askMeAgain)
        {
            fp = fopen("plusWin.cfg", "wt");
            if (fp)
            {
                fprintf(fp, "monitor=%u\n", monitorOut->index);
                fclose(fp);
                fp = nullptr;
            }
        }
        w.run = false;
        w.closeWindow();
        w.doEvents();
        return true;
    }
    return false;
}

    bool LAYOUT::init(const char *nameApp, mbm::WINDOW & window, int adjustRendererWidth, int adjustRendererHeight,
                     const bool hasMenu, const bool leftToRight, const int idResourceIcon)
    {
        mbm::MONITOR monitor;
        if (!mbm::selectMonitor(&monitor))
            return false;
        xGroupRenderer = 10;
        yGroupRenderer = 10;
        widthWindow    = monitor.width;
        heightWindow   = monitor.height;
        widthRenderer  = monitor.width - 230 - adjustRendererWidth;
        heightRenderer = monitor.height - 90 - adjustRendererHeight;
        xComponent     = widthRenderer + 20;
        yComponent     = 10;
        position.x     = monitor.position.x;
        position.y     = monitor.position.y;
        middleAbs.x    = (widthWindow / 2) + position.x;
        middleAbs.y    = (heightWindow / 2) + position.y;

        if (hasMenu)
        {
            yComponent += 20;
            yGroupRenderer += 20;
            heightRenderer -= 20;
        }

        middleRenderer.x = widthRenderer / 2;
        middleRenderer.y = heightRenderer / 2;

        maxWidthComponente = widthWindow - widthRenderer - 25;
        if (leftToRight)
        {
            xGroupRenderer += maxWidthComponente + 10;
            xComponent = 10;
        }
        if (!window.init(nameApp, widthWindow, heightWindow, monitor.position.x, monitor.position.y, enableReziseWindow,
                         true, true, true, onEventWindow, false, idResourceIcon))
            return false;
        window.doEvents();

        idGroupRender = window.addGroupBox(0, xGroupRenderer, yGroupRenderer, widthRenderer, heightRenderer);
        hwndRenderer  = window.getHwnd(idGroupRender);
        window.doEvents();
        return true;
    }

    LAYOUT::LAYOUT()
    {
        xGroupRenderer     = 10;
        yGroupRenderer     = 10;
        withoutBorder      = 0;
        position.x         = 0;
        position.y         = 0;
        widthRenderer      = 0;
        heightRenderer     = 0;
        widthWindow        = 0;
        heightWindow       = 0;
        hwndRenderer       = 0;
        xComponent         = 0;
        yComponent         = 0;
        maxWidthComponente = 0;
        middleAbs.x        = 0;
        middleAbs.y        = 0;
        idGroupRender      = -1;
        middleRenderer.x   = 0;
        middleRenderer.y   = 0;
        enableReziseWindow = 0;
        onEventWindow      = nullptr;
    }
    LAYOUT::~LAYOUT()
    {
    }
    HWND LAYOUT::getHwndRenderer()
    {
        return hwndRenderer;
    }
    
}

    __DRAW_SPLASH::__DRAW_SPLASH(const int ID_IMAGE_RESOURCE, mbm::STATIC_IMAGE_RESOURCE *imageResource)
    {
        resource         = imageResource;
        enableEffectFade = false;
        rProgress        = 255;
        gProgress        = 0;
        bProgress        = 0;
        ID_IMAGE         = ID_IMAGE_RESOURCE;
        coutDownTimeOut  = 20;
    }
    __DRAW_SPLASH::~__DRAW_SPLASH()
    {
    }
    bool __DRAW_SPLASH::eraseBackGround(mbm::COMPONENT_INFO*)
    {
        return false;
    }

    bool __DRAW_SPLASH::render(mbm::COMPONENT_INFO &component)
    {
        if (bmpSplash.isLoaded())
        {
            bmpSplash.draw(component.hdc);
            if (bmpProgress.isLoaded())
            {
                const int _5PercentWidth = (int)((float)bmpProgress.getWidth() * 0.05f);
                const int realCount      = 20 - coutDownTimeOut;
                RECT      rect;
                rect.left   = 0;
                rect.top    = bmpSplash.getHeight() - bmpProgress.getHeight();
                rect.bottom = rect.top + bmpProgress.getHeight();
                rect.right  = realCount * _5PercentWidth;
                bmpProgress.draw(component.hdc, rect);
            }
        }
        else if (resource)
        {
            const int     realCount        = 20 - coutDownTimeOut;
            const int     _5PercenteHeight = (int)((float)resource->height * 0.05f);
            const int     _5PercentWidth   = (int)((float)resource->width * 0.05f);
            const DWORD   s                = resource->width * resource->height;
            const int     xMax             = resource->width;
            unsigned char lr, lg, lb;
            lr = lg = lb = 255;
            if (enableEffectFade && realCount < 10)
            {
                int prop = 255 / 10;
                prop     = (10 - realCount) * prop;
                for (DWORD i = 0, x = 0, y = 0; i < s; ++i)
                {
                    unsigned char *ptr = (unsigned char *)&resource->data[i];
                    unsigned char  b   = *ptr;
                    ptr++;
                    unsigned char g = *ptr;
                    ptr++;
                    unsigned char r = *ptr;
                    if (prop >= r)
                        r = (unsigned char)prop;
                    if (prop >= g)
                        g = (unsigned char)prop;
                    if (prop >= b)
                        b = (unsigned char)prop;
                    if (r != lr && g != lg && b != lb)
                    {
                        this->drawLine(x, y, xMax, y);
                        lr = r;
                        lg = g;
                        lb = b;
                    }
                    x++;
                    if (x >= resource->width)
                    {
                        y++;
                        x = 0;
                    }
                }
            }
            else
            {
                for (DWORD i = 0, x = 0, y = 0; i < s; ++i)
                {
                    unsigned char *ptr = (unsigned char *)&resource->data[i];
                    unsigned char  b   = *ptr;
                    ptr++;
                    unsigned char g = *ptr;
                    ptr++;
                    unsigned char r = *ptr;
                    if (r != lr && g != lg && b != lb)
                    {
                        this->drawLine(x, y, xMax, y);
                        lr = r;
                        lg = g;
                        lb = b;
                    }
                    x++;
                    if (x >= resource->width)
                    {
                        y++;
                        x = 0;
                    }
                }
            }
            this->drawRectangle(0, resource->height - _5PercenteHeight, realCount * _5PercentWidth, _5PercenteHeight);
        }
        return true;
    }
    void __DRAW_SPLASH::onTimeOutSplah(mbm::WINDOW *w, mbm::DATA_EVENT &)
    {
        __DRAW_SPLASH *draw = static_cast<__DRAW_SPLASH *>(w->getObjectContext(0));
        if (draw)
        {
            mbm::DRAW *graphWin = w->getGrafics(-1);
            if (graphWin)
            {
                draw->coutDownTimeOut--;
                if (draw->coutDownTimeOut < 0)
                {
                    int *id = static_cast<int *>(w->getObjectContext(1));
                    if (id)
                    {
                        w->killTimer(*id);
                    }
                }
                else
                {
                    return;
                }
            }
        }
        w->closeWindow();
    }
    

void __destroyMenu(void *extraParams)
{
    mbm::WINDOW::__MENU_DRAW *hmenu = static_cast<mbm::WINDOW::__MENU_DRAW *>(extraParams);
    if (hmenu)
    {
        for (unsigned int i = 0; i < mbm::WINDOW::lsAllMenus.size(); ++i)
        {
            mbm::WINDOW::__MENU_DRAW *pHmenu = mbm::WINDOW::lsAllMenus[i];
            if (pHmenu == hmenu)
                mbm::WINDOW::lsAllMenus[i] = nullptr;
        }
        delete hmenu;
    }
    hmenu = nullptr;
}

namespace mbm
{

    void splash(const DWORD timeMiliSec, STATIC_IMAGE_RESOURCE &imageResource, unsigned char rgbProgres[3],
                       unsigned char colorKeiyng[3])
    {
        mbm::WINDOW w;
        w.hideConsoleWindow();

        __DRAW_SPLASH drawSplash(0, &imageResource);
        if (rgbProgres)
        {
            drawSplash.rProgress = rgbProgres[0];
            drawSplash.gProgress = rgbProgres[1];
            drawSplash.bProgress = rgbProgres[2];
        }
        w.init(nullptr, imageResource.width, imageResource.height, 0, 0, 0, 0, 0, 0, 0, true);
        if (colorKeiyng)
        {
            SetWindowLong(w.getHwnd(), GWL_EXSTYLE, GetWindowLong(w.getHwnd(), GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(w.getHwnd(), RGB(colorKeiyng[0], colorKeiyng[1], colorKeiyng[2]), 0, LWA_COLORKEY);
        }
        w.setAlwaysOnTop(true);
        w.exitOnEsc                = false;
        drawSplash.coutDownTimeOut = 20;
        DWORD newTimerMiliSec      = (timeMiliSec / 20) + 1;
        int   id                   = w.addTimer(newTimerMiliSec, __DRAW_SPLASH::onTimeOutSplah, nullptr);
        w.setObjectContext(&drawSplash, 0);
        w.setObjectContext(&id, 1);
        w.enterLoop(nullptr);
    }
    void splash(const DWORD timeMiliSec, int ID_IMAGE_RESOURCE, unsigned char rgbProgres[3],
                       unsigned char colorKeiyng[3])
    {
        mbm::WINDOW w;
        w.hideConsoleWindow();
        mbm::MONITOR monitor;
        mbm::selectMonitor(&monitor);
        __DRAW_SPLASH drawSplash(ID_IMAGE_RESOURCE, nullptr);
        if (rgbProgres)
        {
            drawSplash.rProgress = rgbProgres[0];
            drawSplash.gProgress = rgbProgres[1];
            drawSplash.bProgress = rgbProgres[2];
        }
        w.setDrawer(&drawSplash);
        w.init(nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
        if (colorKeiyng)
        {
            SetWindowLong(w.getHwnd(), GWL_EXSTYLE, GetWindowLong(w.getHwnd(), GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(w.getHwnd(), RGB(colorKeiyng[0], colorKeiyng[1], colorKeiyng[2]), 0, LWA_COLORKEY);
        }
        drawSplash.bmpSplash.load(w.getHwnd(), ID_IMAGE_RESOURCE);
        w.setAlwaysOnTop(true);
        w.exitOnEsc                = false;
        drawSplash.coutDownTimeOut = 20;
        DWORD newTimerMiliSec      = (timeMiliSec / 20) + 1;
        int   id                   = w.addTimer(newTimerMiliSec, __DRAW_SPLASH::onTimeOutSplah, nullptr);
        w.setObjectContext(&drawSplash, 0);
        w.setObjectContext(&id, 1);
        RECT rect;
        rect.left = (monitor.width / 2) + (drawSplash.bmpSplash.getWidth() / 2) - drawSplash.bmpSplash.getWidth() +
                    monitor.position.x;
        rect.top = (monitor.height / 2) + (drawSplash.bmpSplash.getHeight() / 2) - drawSplash.bmpSplash.getHeight() +
                   monitor.position.y;
        rect.right  = drawSplash.bmpSplash.getWidth() + rect.left + monitor.position.x;
        rect.bottom = drawSplash.bmpSplash.getHeight() + rect.top + monitor.position.y;

        w.setSize(rect);
        w.enterLoop(nullptr);
    }
    void splash(const DWORD timeMiliSec, int ID_IMAGE_RESOURCE, int ID_IMAGE_PROGRESS,
                       unsigned char colorKeiyng[3])
    {
        mbm::WINDOW  w;
        mbm::MONITOR monitor;
        mbm::selectMonitor(&monitor);
        w.hideConsoleWindow();
        __DRAW_SPLASH drawSplash(ID_IMAGE_RESOURCE, nullptr);
        w.setDrawer(&drawSplash);
        w.init(nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, true);
        if (colorKeiyng)
        {
            SetWindowLong(w.getHwnd(), GWL_EXSTYLE, GetWindowLong(w.getHwnd(), GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(w.getHwnd(), RGB(colorKeiyng[0], colorKeiyng[1], colorKeiyng[2]), 0, LWA_COLORKEY);
        }
        drawSplash.bmpSplash.load(w.getHwnd(), ID_IMAGE_RESOURCE);
        drawSplash.bmpProgress.load(w.getHwnd(), ID_IMAGE_PROGRESS);
        w.setAlwaysOnTop(true);
        w.exitOnEsc                = false;
        drawSplash.coutDownTimeOut = 20;
        DWORD newTimerMiliSec      = (timeMiliSec / 20) + 1;
        int   id                   = w.addTimer(newTimerMiliSec, __DRAW_SPLASH::onTimeOutSplah, nullptr);
        w.setObjectContext(&drawSplash, 0);
        w.setObjectContext(&id, 1);
        RECT rect;
        rect.left = (monitor.width / 2) + (drawSplash.bmpSplash.getWidth() / 2) - drawSplash.bmpSplash.getWidth() +
                    monitor.position.x;
        rect.top = (monitor.height / 2) + (drawSplash.bmpSplash.getHeight() / 2) - drawSplash.bmpSplash.getHeight() +
                   monitor.position.y;
        rect.right  = drawSplash.bmpSplash.getWidth() + rect.left + monitor.position.x;
        rect.bottom = drawSplash.bmpSplash.getHeight() + rect.top + monitor.position.y;
        w.setSize(rect);
        w.enterLoop(nullptr);
    }
    void splash(const int widthWindow, const int heightWindow, const DWORD timeMiliSec, int ID_IMAGE_RESOURCE,
                       int ID_IMAGE_PROGRESS, unsigned char colorKeiyng[3])
    {
        mbm::WINDOW w;
        w.hideConsoleWindow();
        __DRAW_SPLASH drawSplash(ID_IMAGE_RESOURCE, nullptr);
        w.setDrawer(&drawSplash);
        w.init(nullptr, widthWindow, heightWindow, 0, 0, 0, 0, 0, 0, 0, true);
        if (colorKeiyng)
        {
            SetWindowLong(w.getHwnd(), GWL_EXSTYLE, GetWindowLong(w.getHwnd(), GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(w.getHwnd(), RGB(colorKeiyng[0], colorKeiyng[1], colorKeiyng[2]), 0, LWA_COLORKEY);
        }
        drawSplash.bmpSplash.load(w.getHwnd(), ID_IMAGE_RESOURCE);
        drawSplash.bmpProgress.load(w.getHwnd(), ID_IMAGE_PROGRESS);
        w.setAlwaysOnTop(true);
        w.exitOnEsc                = false;
        drawSplash.coutDownTimeOut = 20;
        DWORD newTimerMiliSec      = (timeMiliSec / 20) + 1;
        int   id                   = w.addTimer(newTimerMiliSec, __DRAW_SPLASH::onTimeOutSplah, nullptr);
        w.setObjectContext(&drawSplash, 0);
        w.setObjectContext(&id, 1);
        w.enterLoop(nullptr);
    }
}

const int __getTabStopPixelSize(HWND hwnd)
{
    mbm::COM_BETWEEN_WINP *ptrDest = mbm::getComBetweenWinp(hwnd);
    if (ptrDest)
        return ptrDest->getWindow()->getTabStopPixelSize();
    return 0;
}
std::vector<mbm::COM_BETWEEN_WINP *>    mbm::COM_BETWEEN_WINP::lsComBetweenWinp;
std::vector<HWND>                       mbm::WINDOW::lsDisabledRender;
std::vector<mbm::WINDOW::__MENU_DRAW *> mbm::WINDOW::lsAllMenus;
HHOOK                                   mbm::WINDOW::hookMsgProc = nullptr;

namespace mbm
{
    void __destroyOnExitAllListComBetweenWindows()
    {
        for (unsigned int i = 0, s = mbm::COM_BETWEEN_WINP::lsComBetweenWinp.size(); i < s; ++i)
        {
            mbm::COM_BETWEEN_WINP *ptr = mbm::COM_BETWEEN_WINP::lsComBetweenWinp[i];
            if (ptr)
            {
                delete ptr;
                mbm::COM_BETWEEN_WINP::lsComBetweenWinp[i] = nullptr;
            }
        }
    }
    const char *DATA_EVENT::format(bool v)
    {
        if (v)
            strcpy(_ret, "true");
        else
            strcpy(_ret, "false");
        return _ret;
    }
    const char *DATA_EVENT::format(int v)
    {
        sprintf_s(_ret, "%d", v);
        return _ret;
    }
    const char *DATA_EVENT::format(float v)
    {
        sprintf_s(_ret, "%f", v);
        return _ret;
    }
    const char *DATA_EVENT::format()
    {
        if (this->myString)
            return this->myString;
        mbm::COM_BETWEEN_WINP *ptr = mbm::getComBetweenWinp(this->idComponent);
        mbm::WINDOW *          w   = ptr->getWindow();
        WORD                   len = (WORD)w->getTextLength(ptr->getId());
        if (len)
        {
            len += 4;
            static std::string buffer;
            auto              text = new char[len];
            memset(text, 0, len);
            if (w->getText(ptr->getId(), text, len, -1))
            {
                buffer         = text;
                this->myString = buffer.c_str();
                delete[] text;
                return this->myString;
            }
            else
            {
                delete[] text;
                return "";
            }
        }
        else
        {
            return "";
        }
    }
    const int DATA_EVENT::getInt()
    {
        switch (this->type)
        {
            case WINPLUS_TYPE_NONE: return 0;
            case WINPLUS_TYPE_IMAGE: return 0;
            case WINPLUS_TYPE_WINDOW: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_WINDOW_MESSAGE_BOX: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_LABEL: return (*static_cast<int *>(data ? data : 0));
            case WINPLUS_TYPE_BUTTON: return 0;
            case WINPLUS_TYPE_BUTTON_TAB: return (static_cast<__TAB_GROUP_DESC *>(data)->index);
            case WINPLUS_TYPE_CHECK_BOX: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_RADIO_BOX: return static_cast<RADIO_GROUP *>(data)->checked ? 1 : 0;
            case WINPLUS_TYPE_COMBO_BOX: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_LIST_BOX: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_TEXT_BOX: return std::atoi(static_cast<EDIT_TEXT_DATA *>(data)->text);
            case WINPLUS_TYPE_SCROLL: return 0;
            case WINPLUS_TYPE_SPIN_INT: return static_cast<SPIN_PARAMSi *>(data)->currentPosition;
            case WINPLUS_TYPE_SPIN_FLOAT: return (int)static_cast<SPIN_PARAMSf *>(data)->currentPosition;
            case WINPLUS_TYPE_RICH_TEXT:
                return (*static_cast<int *>(data)); // inde line
            case WINPLUS_TYPE_CHILD_WINDOW: return 0;
            case WINPLUS_TYPE_GROUP_BOX: return (*static_cast<int *>(data ? data : 0));
            case WINPLUS_TYPE_PROGRESS_BAR: return (int)static_cast<PROGRESS_BAR_INFO *>(data)->position;
            case WINPLUS_TYPE_TIMER: return static_cast<TIMER *>(data)->times;
            case WINPLUS_TYPE_TRACK_BAR: return (int)static_cast<TRACK_BAR_INFO *>(data)->position;
            case WINPLUS_TYPE_STATUS_BAR: return 0;
            case WINPLUS_TYPE_MENU: return (static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu);
            case WINPLUS_TYPE_SUB_MENU: return (static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu);
            case WINPLUS_TYPE_GROUP_BOX_TAB: return (*static_cast<int *>(data ? data : 0));
            case WINPLUS_TYPE_TRY_ICON_MENU: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_TRY_ICON_SUB_MENU: return (*static_cast<int *>(data));
            case WINPLUS_TYPE_TOOL_TIP: return 0;
            case WINPLUS_TYPE_WINDOWNC: return (static_cast<__NC_BORDERS::__NC_BUTTONS *>(data)->isHoverClose) ? 1 : 0;
        }
        return 0;
    }
    const float DATA_EVENT::getFloat()
    {
        switch (this->type)
        {
            case WINPLUS_TYPE_NONE: return 0.0f;
            case WINPLUS_TYPE_IMAGE: return 0.0f;
            case WINPLUS_TYPE_WINDOW: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_WINDOW_MESSAGE_BOX: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_LABEL: return (float)(*static_cast<int *>(data ? data : 0));
            case WINPLUS_TYPE_BUTTON: return 0.0f;
            case WINPLUS_TYPE_BUTTON_TAB: return (float)(static_cast<__TAB_GROUP_DESC *>(data)->index);
            case WINPLUS_TYPE_CHECK_BOX: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_RADIO_BOX: return static_cast<RADIO_GROUP *>(data)->checked ? 1.0f : 0.0f;
            case WINPLUS_TYPE_COMBO_BOX: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_LIST_BOX: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_TEXT_BOX: return (float)atof(static_cast<EDIT_TEXT_DATA *>(data)->text);
            case WINPLUS_TYPE_SCROLL: return 0.0f;
            case WINPLUS_TYPE_SPIN_INT: return (float)static_cast<SPIN_PARAMSi *>(data)->currentPosition;
            case WINPLUS_TYPE_SPIN_FLOAT: return static_cast<SPIN_PARAMSf *>(data)->currentPosition;
            case WINPLUS_TYPE_RICH_TEXT:
                return (float)(*static_cast<int *>(data)); // index line
            case WINPLUS_TYPE_CHILD_WINDOW: return 0.0f;
            case WINPLUS_TYPE_GROUP_BOX: return (float)(*static_cast<int *>(data ? data : 0));
            case WINPLUS_TYPE_PROGRESS_BAR: return static_cast<PROGRESS_BAR_INFO *>(data)->position;
            case WINPLUS_TYPE_TIMER: return (float)static_cast<TIMER *>(data)->times;
            case WINPLUS_TYPE_TRACK_BAR: return static_cast<TRACK_BAR_INFO *>(data)->position;
            case WINPLUS_TYPE_STATUS_BAR: return 0.0f;
            case WINPLUS_TYPE_MENU: return (float)(static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu);
            case WINPLUS_TYPE_SUB_MENU: return (float)(static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu);
            case WINPLUS_TYPE_GROUP_BOX_TAB: return (float)(*static_cast<int *>(data ? data : 0));
            case WINPLUS_TYPE_TRY_ICON_MENU: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_TRY_ICON_SUB_MENU: return (float)(*static_cast<int *>(data));
            case WINPLUS_TYPE_TOOL_TIP: return 0.0f;
            case WINPLUS_TYPE_WINDOWNC: return (static_cast<__NC_BORDERS::__NC_BUTTONS *>(data)->isHoverClose) ? 1.0f : 0.0f;
        }
        return 0.0f;
    }
    const bool DATA_EVENT::getBool()
    {
        switch (this->type)
        {
            case WINPLUS_TYPE_NONE: return false;
            case WINPLUS_TYPE_IMAGE: return false;
            case WINPLUS_TYPE_WINDOW: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_WINDOW_MESSAGE_BOX: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_LABEL: return (*static_cast<int *>(data ? data : 0) != 0);
            case WINPLUS_TYPE_BUTTON: return false;
            case WINPLUS_TYPE_BUTTON_TAB: return (static_cast<__TAB_GROUP_DESC *>(data)->index) ? true : false;
            case WINPLUS_TYPE_CHECK_BOX: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_RADIO_BOX: return static_cast<RADIO_GROUP *>(data)->checked ? true : false;
            case WINPLUS_TYPE_COMBO_BOX: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_LIST_BOX: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_TEXT_BOX: return std::atoi(static_cast<EDIT_TEXT_DATA *>(data)->text) ? true : false;
            case WINPLUS_TYPE_SCROLL: return false;
            case WINPLUS_TYPE_SPIN_INT: return static_cast<SPIN_PARAMSi *>(data)->currentPosition ? true : false;
            case WINPLUS_TYPE_SPIN_FLOAT: return ((int)static_cast<SPIN_PARAMSf *>(data)->currentPosition) ? true : false;
            case WINPLUS_TYPE_RICH_TEXT: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_CHILD_WINDOW: return false;
            case WINPLUS_TYPE_GROUP_BOX: return (*static_cast<int *>(data ? data : 0) != 0);
            case WINPLUS_TYPE_PROGRESS_BAR: return ((int)static_cast<PROGRESS_BAR_INFO *>(data)->position) ? true : false;
            case WINPLUS_TYPE_TIMER: return (static_cast<TIMER *>(data)->times) ? true : false;
            case WINPLUS_TYPE_TRACK_BAR: return ((int)static_cast<TRACK_BAR_INFO *>(data)->position) ? true : false;
            case WINPLUS_TYPE_STATUS_BAR: return false;
            case WINPLUS_TYPE_MENU: return ((static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu) ? true : false);
            case WINPLUS_TYPE_SUB_MENU:
                return ((static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu) ? true : false);
            case WINPLUS_TYPE_GROUP_BOX_TAB: return (*static_cast<int *>(data ? data : 0) != 0);
            case WINPLUS_TYPE_TRY_ICON_MENU: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_TRY_ICON_SUB_MENU: return (*static_cast<int *>(data)) ? true : false;
            case WINPLUS_TYPE_TOOL_TIP: return false;
            case WINPLUS_TYPE_WINDOWNC: return (static_cast<__NC_BORDERS::__NC_BUTTONS *>(data)->isHoverClose) ? true : false;
        }
        return 0;
    }
    const char *DATA_EVENT::getString()
{
    switch (this->type)
    {
        case WINPLUS_TYPE_NONE: return this->format();
        case WINPLUS_TYPE_IMAGE: return this->format();
        case WINPLUS_TYPE_WINDOW: return this->format();
        case WINPLUS_TYPE_WINDOW_MESSAGE_BOX: return this->format();
        case WINPLUS_TYPE_LABEL: return this->format((int)(*static_cast<int *>(data ? data : 0)));
        case WINPLUS_TYPE_BUTTON: return this->format();
        case WINPLUS_TYPE_BUTTON_TAB: return this->format((static_cast<__TAB_GROUP_DESC *>(data)->index));
        case WINPLUS_TYPE_CHECK_BOX: return this->format((*static_cast<int *>(data)) ? true : false);
        case WINPLUS_TYPE_RADIO_BOX: return this->format(static_cast<RADIO_GROUP *>(data)->checked);
        case WINPLUS_TYPE_COMBO_BOX: return this->format();
        case WINPLUS_TYPE_LIST_BOX: return this->format((*static_cast<int *>(data)));
        case WINPLUS_TYPE_TEXT_BOX: return static_cast<EDIT_TEXT_DATA *>(data)->text;
        case WINPLUS_TYPE_SCROLL: return this->format();
        case WINPLUS_TYPE_SPIN_INT: return this->format(static_cast<SPIN_PARAMSi *>(data)->currentPosition);
        case WINPLUS_TYPE_SPIN_FLOAT: return this->format(static_cast<SPIN_PARAMSf *>(data)->currentPosition);
        case WINPLUS_TYPE_RICH_TEXT: return this->format();
        case WINPLUS_TYPE_CHILD_WINDOW: return this->format();
        case WINPLUS_TYPE_GROUP_BOX: return this->format((int)(*static_cast<int *>(data ? data : 0)));
        case WINPLUS_TYPE_PROGRESS_BAR: return this->format(static_cast<PROGRESS_BAR_INFO *>(data)->position);
        case WINPLUS_TYPE_TIMER: return this->format(static_cast<TIMER *>(data)->times);
        case WINPLUS_TYPE_TRACK_BAR: return this->format(static_cast<TRACK_BAR_INFO *>(data)->position);
        case WINPLUS_TYPE_STATUS_BAR: return this->format();
        case WINPLUS_TYPE_MENU: return this->format((static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu));
        case WINPLUS_TYPE_SUB_MENU:
            return this->format((static_cast<mbm::WINDOW::__MENU_DRAW *>(data)->indexClickedMenu));
        case WINPLUS_TYPE_GROUP_BOX_TAB: return this->format((int)(*static_cast<int *>(data ? data : 0)));
        case WINPLUS_TYPE_TRY_ICON_MENU: return this->format((*static_cast<int *>(data)));
        case WINPLUS_TYPE_TRY_ICON_SUB_MENU: return this->format((*static_cast<int *>(data)));
        case WINPLUS_TYPE_TOOL_TIP: return this->format();
        case WINPLUS_TYPE_WINDOWNC: return this->format(static_cast<__NC_BORDERS::__NC_BUTTONS *>(data)->isHoverClose);
    }
    return nullptr;
}

};
