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

#include <joystick-win32/joystick.h>

#ifndef SAFE_FREE
    #define SAFE_FREE(p)    { if(p) { HeapFree(hHeap, 0, p); (p) = nullptr; } }
#endif
#ifndef CHECK
    #define CHECK(exp)      { if(!(exp)) goto Error; }
#endif
#ifndef STRING_VAR_ARG
    #define STRING_VAR_ARG(...) #__VA_ARGS__
#endif

using namespace std;

namespace mbm
{

    JOYSTICK::JOYSTICK()
    {
        this->_b_enableContinousMove = false;
    }
    
    JOYSTICK::~JOYSTICK()
    {
        for (unsigned int i = 0; i < this->lsInfoJoystick.size(); ++i)
        {
            INFO_JOYSTICK* pJoystick = this->lsInfoJoystick[i];
            delete pJoystick;
        }
        this->lsInfoJoystick.clear();
    }
    
    bool JOYSTICK::initJoystick(mbm::WINDOW* win)
    {
        if (win)
        {
            JOYSTICK::_indexJoystickInstance = win->setObjectContext((void*)this,0xffffffff);
            if (win->setOnParserRawInput(onParseRawInput))
                return this->updateDevices();
        }
        return false;
    }
    
    void JOYSTICK::parseRawInput(PRAWINPUT pRawInput)
    {
        PHIDP_PREPARSED_DATA pPreparsedData = nullptr;
        HIDP_CAPS            Caps;
        PHIDP_BUTTON_CAPS    pButtonCaps = nullptr;
        PHIDP_VALUE_CAPS     pValueCaps = nullptr;
        USHORT               capsLength=0;
        UINT                 bufferSize=0;
        USAGE                usage[MAX_BUTTONS];
        ULONG                usageLength, value;
		HANDLE               hHeap = GetProcessHeap();
        LONG                 wasMoveZeroLocal = 0;
        //
        // Get the preparsed data block
        //
        int iIdMyJoystick = -1;
        INFO_JOYSTICK* pInfoJoystick = nullptr;
        for (unsigned int i = 0; i < this->lsInfoJoystick.size(); ++i)
        {
            pInfoJoystick = this->lsInfoJoystick[i];
            if (pInfoJoystick->hDevice.dwType == RIM_TYPEHID)
            {
                iIdMyJoystick++;
                if (pRawInput->header.hDevice == pInfoJoystick->hDevice.hDevice)
                {
                    break;
                }
            }
            else if (pInfoJoystick->hDevice.dwType == RIM_TYPEKEYBOARD)
            {
                if (pRawInput->header.hDevice == pInfoJoystick->hDevice.hDevice)
                {
                    if (pRawInput->data.keyboard.Message == WM_KEYDOWN || pRawInput->data.keyboard.Message == WM_SYSKEYDOWN)
                        fprintf(stderr, "\nKey: [%d]", pRawInput->data.keyboard.VKey);
                    //its all ok.... just return 
                    goto Error;
                }
            }
        }
        if (iIdMyJoystick == -1 || iIdMyJoystick >= (int)this->lsInfoJoystick.size())
            goto Error;

        CHECK(GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, nullptr, &bufferSize) == 0);
        CHECK((pPreparsedData = (PHIDP_PREPARSED_DATA)HeapAlloc(hHeap, 0, bufferSize)) != nullptr);
        CHECK((int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0);

        //
        // Get the joystick's capabilities
        //

        // Button caps
        CHECK(HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS)
        CHECK((pButtonCaps = (PHIDP_BUTTON_CAPS)HeapAlloc(hHeap, 0, sizeof(HIDP_BUTTON_CAPS)* Caps.NumberInputButtonCaps))!= nullptr);

        capsLength = Caps.NumberInputButtonCaps;
        CHECK(HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS)
            pInfoJoystick->numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;

        // Value caps
        CHECK((pValueCaps = (PHIDP_VALUE_CAPS)HeapAlloc(hHeap, 0, sizeof(HIDP_VALUE_CAPS)* Caps.NumberInputValueCaps))!= nullptr);
        capsLength = Caps.NumberInputValueCaps;
        CHECK(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS)

            //
            // Get the pressed buttons
            //

            usageLength = pInfoJoystick->numberOfButtons;
        CHECK(
            HidP_GetUsages(
            HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
            (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
            ) == HIDP_STATUS_SUCCESS);

        if (!pInfoJoystick->info)
        {
            this->onInfoDeviceJoystick(
                iIdMyJoystick + 1,
                pInfoJoystick->numberOfButtons,
                pInfoJoystick->name.c_str(),
                pInfoJoystick->extraInfo.c_str());
            pInfoJoystick->info = true;
        }
        ZeroMemory(pInfoJoystick->bButtonStates, sizeof(pInfoJoystick->bButtonStates));
        //button Clicked
        for (ULONG i = 0; i < usageLength; ++i)
        {
            int key = usage[i] - pButtonCaps->Range.UsageMin;
            pInfoJoystick->bButtonStates[key] = TRUE;
        }

        for (int i = 0; i < pInfoJoystick->numberOfButtons; ++i)
        {
            if (pInfoJoystick->bButtonStates[i])
            {
                switch (pInfoJoystick->bStateKey[i])
                {
                    case KEY_NONE:
                    {
                        this->onKeyDownJoystick(iIdMyJoystick + 1, i + 1);
                        pInfoJoystick->bStateKey[i] = KEY_DOWN;
                    }
                    break;
                    case KEY_DOWN:
                    {
                    }
                    break;
                    case KEY_UP:
                    {
                        this->onKeyDownJoystick(iIdMyJoystick + 1, i + 1);
                        pInfoJoystick->bStateKey[i] = KEY_DOWN;
                    }
                    break;
                }
            }
            else
            {
                switch (pInfoJoystick->bStateKey[i])
                {
                    case KEY_NONE:
                    {

                    }
                    break;
                    case KEY_DOWN:
                    {
                        this->onKeyUpJoystick(iIdMyJoystick + 1, i + 1);
                        pInfoJoystick->bStateKey[i] = KEY_UP;
                    }
                    break;
                    case KEY_UP:
                    {
                        pInfoJoystick->bStateKey[i] = KEY_UP;
                    }
                    break;
                }
            }
        }

        for (int i = 0; i < Caps.NumberInputValueCaps; ++i)
        {
            CHECK(
                HidP_GetUsageValue(
                HidP_Input, pValueCaps[i].UsagePage, 0, pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
                (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
                ) == HIDP_STATUS_SUCCESS);

            switch (pValueCaps[i].Range.UsageMin)
            {
                case 0x30:  // X-axis
                    pInfoJoystick->lAxisX = (LONG)value - 128;
                break;

                case 0x31:  // Y-axis
                    pInfoJoystick->lAxisY = (LONG)value - 128;
                break;

                case 0x32: // Z-axis
                    pInfoJoystick->lAxisZ = (LONG)value - 128;
                break;

                case 0x35: // Rotate-Z
                    pInfoJoystick->lAxisRz = (LONG)value - 128;
                break;

                case 0x39:  // Hat Switch
                    pInfoJoystick->lHat = value;
                break;
            }
        }
        if (pInfoJoystick->numberOfButtons != 19)//controle ps3 - o pd ja esta incluso nos demais botões !!
        {
            switch (pInfoJoystick->lHat)
            {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                {
                    const int i = pInfoJoystick->numberOfButtons + pInfoJoystick->lHat;
                    switch (pInfoJoystick->bStateKey[i])
                    {
                        case KEY_NONE:
                        {
                            this->onKeyDownJoystick(iIdMyJoystick + 1, i + 1);
                            pInfoJoystick->bStateKey[i] = KEY_DOWN;
                        }
                        break;
                        case KEY_DOWN:
                        {
                        }
                        break;
                        case KEY_UP:
                        {
                            this->onKeyDownJoystick(iIdMyJoystick + 1, i + 1);
                            pInfoJoystick->bStateKey[i] = KEY_DOWN;
                        }
                        break;
                    }
                }
                break;
                case 15:
                {
                    const int j = pInfoJoystick->numberOfButtons + pInfoJoystick->lHat;
                    for (int i = pInfoJoystick->numberOfButtons; i < j; ++i)
                    {
                        switch (pInfoJoystick->bStateKey[i])
                        {
                            case KEY_NONE:
                            {
                            }
                            break;
                            case KEY_DOWN:
                            {
                                this->onKeyUpJoystick(iIdMyJoystick + 1, i + 1);
                                pInfoJoystick->bStateKey[i] = KEY_UP;
                            }
                            break;
                            case KEY_UP:
                            {
                                pInfoJoystick->bStateKey[i] = KEY_UP;
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
        wasMoveZeroLocal = ((pInfoJoystick->lAxisX != -1) && (pInfoJoystick->lAxisX)) || ((pInfoJoystick->lAxisY != -1) && (pInfoJoystick->lAxisY)) || pInfoJoystick->lAxisZ || pInfoJoystick->lAxisRz;
        if (wasMoveZeroLocal || pInfoJoystick->wasMoveZero)
        {
            INFO_LAST_MOVE current;
            current.lAxisX = (int)((pInfoJoystick->lAxisX == -1) ? 0 : pInfoJoystick->lAxisX);
            current.lAxisY = (int)((pInfoJoystick->lAxisY == -1) ? 0 : pInfoJoystick->lAxisY);
            current.lAxisZ = (int)((pInfoJoystick->lAxisZ == -1) ? 0 : pInfoJoystick->lAxisZ);
            current.lAxisRz = (int)((pInfoJoystick->lAxisRz == -1) ? 0 : pInfoJoystick->lAxisRz);
            if (_b_enableContinousMove)
            {
                this->onMoveJoystick(iIdMyJoystick + 1,
                    (float)current.lAxisX,
                    (float)current.lAxisY,
                    (float)current.lAxisZ,
                    (float)current.lAxisRz);
                pInfoJoystick->infoLastMove = current;
            }
            else if (current.lAxisX != pInfoJoystick->infoLastMove.lAxisX ||
                current.lAxisY != pInfoJoystick->infoLastMove.lAxisY ||
                current.lAxisZ != pInfoJoystick->infoLastMove.lAxisZ ||
                current.lAxisRz != pInfoJoystick->infoLastMove.lAxisRz)
            {
                this->onMoveJoystick(iIdMyJoystick + 1,
                    (float)current.lAxisX,
                    (float)current.lAxisY,
                    (float)current.lAxisZ,
                    (float)current.lAxisRz);
                pInfoJoystick->infoLastMove = current;
            }
        }
        pInfoJoystick->wasMoveZero = wasMoveZeroLocal;
    Error:
        SAFE_FREE(pPreparsedData);
        SAFE_FREE(pButtonCaps);
        SAFE_FREE(pValueCaps);
    }
    
    bool JOYSTICK::needChangeDevices(PRAWINPUTDEVICELIST pRawInputDeviceList, UINT nDevices)
    {
        if (nDevices != this->lsInfoJoystick.size())
            return true;
        for (unsigned int i = 0; i < this->lsInfoJoystick.size(); ++i)
        {
            INFO_JOYSTICK* pJoystick = this->lsInfoJoystick[i];
            if (pJoystick->hDevice.dwType != pRawInputDeviceList[i].dwType)
                return true;
            if (pJoystick->hDevice.hDevice != pRawInputDeviceList[i].hDevice)
                return true;
        }
        return false;
    }
    
    bool JOYSTICK::updateDevices()
    {
        UINT nDevices = 0;
        int nResult = 0;
        PRAWINPUTDEVICELIST pRawInputDeviceList;
        if (GetRawInputDeviceList(nullptr, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0 || nDevices == 0)
            return false;
        pRawInputDeviceList = new RAWINPUTDEVICELIST[nDevices];
        if (GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST)) <= 0)
        {
            delete[] pRawInputDeviceList;
            return false;
        }
        if (needChangeDevices(pRawInputDeviceList, nDevices))
        {
            for (unsigned int i = 0; i < this->lsInfoJoystick.size(); ++i)
            {
                INFO_JOYSTICK* pJoystick = this->lsInfoJoystick[i];
                delete pJoystick;
            }
        }
        else
        {
            delete[] pRawInputDeviceList;
            return true;
        }
        this->lsInfoJoystick.clear();
        // Loop Through Device List
        for (UINT i = 0; i < nDevices; ++i)
        {
            auto  pJoystick = new INFO_JOYSTICK();
            memcpy(&pJoystick->hDevice, &pRawInputDeviceList[i], sizeof(RAWINPUTDEVICELIST));
            this->lsInfoJoystick.push_back(pJoystick);
            // Get Character Count For Device Name
            UINT nBufferSize = 0;
            nResult = GetRawInputDeviceInfoA(pRawInputDeviceList[i].hDevice, // Device
                RIDI_DEVICENAME,                // Get Device Name
                nullptr,                           // NO Buff, Want Count!
                &nBufferSize);                 // Char Count Here!

            // Got Device Name?
            if (nResult < 0)
            {
                // Error
                printf("ERR: Unable to get Device Name character count.. Moving to next device.\n");
                // Next
                continue;
            }

            // Allocate Memory For Device Name
            auto  strDeviceName = new char[nBufferSize + 1];

            // Get Name
            nResult = GetRawInputDeviceInfoA(pRawInputDeviceList[i].hDevice, // Device
                RIDI_DEVICENAME,                // Get Device Name
                strDeviceName,                   // Get Name!
                &nBufferSize);                 // Char Count

            // Got Device Name?
            if (nResult < 0)
            {
                // Error
                printf("ERR: Unable to get Device Name.. Moving to next device.\n");
                // Clean Up
                delete[] strDeviceName;
                // Next
                continue;
            }

            // Set Device Info & Buffer Size
            RID_DEVICE_INFO rdiDeviceInfo;
            rdiDeviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
            nBufferSize = rdiDeviceInfo.cbSize;

            // Get Device Info
            nResult = GetRawInputDeviceInfoA(pRawInputDeviceList[i].hDevice,
                RIDI_DEVICEINFO,
                &rdiDeviceInfo,
                &nBufferSize);

            // Got All Buffer?
            if (nResult < 0)
            {
                // Error
                printf("ERR: Unable to read Device Info.. Moving to next device.\n");
                // Next
                continue;
            }
            if (rdiDeviceInfo.dwType == RIM_TYPEHID)
            {
                char str[255] = "";
                pJoystick->name = strDeviceName;
                pJoystick->info = false;
                pJoystick->extraInfo.clear();
                sprintf(str, "Vendor Id:%u\n", rdiDeviceInfo.hid.dwVendorId);
                pJoystick->extraInfo += str;

                sprintf(str, "Product Id:%u\n", rdiDeviceInfo.hid.dwProductId);
                pJoystick->extraInfo += str;

                sprintf(str, "Version No:%u\n", rdiDeviceInfo.hid.dwVersionNumber);
                pJoystick->extraInfo += str;

                sprintf(str, "Usage for the device:%d\n", rdiDeviceInfo.hid.usUsage);
                pJoystick->extraInfo += str;

                sprintf(str, "Usage Page for the device:%d\n", rdiDeviceInfo.hid.usUsagePage);
                pJoystick->extraInfo += str;
            }
            else if (rdiDeviceInfo.dwType == RIM_TYPEKEYBOARD)
            {
                char str[255] = "";
                pJoystick->name = strDeviceName;
                pJoystick->info = false;
                pJoystick->extraInfo.clear();
                sprintf(str, "Vendor Id:%u\n", rdiDeviceInfo.hid.dwVendorId);
                pJoystick->extraInfo += str;

                sprintf(str, "Product Id:%u\n", rdiDeviceInfo.hid.dwProductId);
                pJoystick->extraInfo += str;

                sprintf(str, "Version No:%u\n", rdiDeviceInfo.hid.dwVersionNumber);
                pJoystick->extraInfo += str;

                sprintf(str, "Usage for the device:%d\n", rdiDeviceInfo.hid.usUsage);
                pJoystick->extraInfo += str;

                sprintf(str, "Usage Page for the device:%d\n", rdiDeviceInfo.hid.usUsagePage);
                pJoystick->extraInfo += str;
            }
            // Delete Name Memory!
            delete[] strDeviceName;
        }
        // Clean Up - Free Memory
        delete[] pRawInputDeviceList;
        return true;
    }
    
    int JOYSTICK::onParseRawInput(mbm::WINDOW* window, HRAWINPUT phRawInput)
    {
        JOYSTICK* that = static_cast<JOYSTICK*>(window->getObjectContext(JOYSTICK::_indexJoystickInstance));
        if (that && that->updateDevices())
        {
            PRAWINPUT pRawInput = 0;
            UINT      bufferSize = 0;
            HANDLE    hHeap = 0;
            const UINT ret = GetRawInputData(phRawInput, RID_INPUT, nullptr, &bufferSize, sizeof(RAWINPUTHEADER));
            if (ret)
                return -1;
            hHeap = GetProcessHeap();
            pRawInput = (PRAWINPUT)HeapAlloc(hHeap, 0, bufferSize);
            if (!pRawInput)
                return -1;
            const UINT bSizeCopied = GetRawInputData(phRawInput, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER));
            if (bSizeCopied != bufferSize)
                return -1;
            that->parseRawInput(pRawInput);
            HeapFree(hHeap, 0, pRawInput);
        }
        return 0;
    }
    
    unsigned int JOYSTICK::getTotalDevices()
    {
        unsigned int t = 0;
        for (unsigned int i = 0; i < this->lsInfoJoystick.size(); ++i)
        {
            INFO_JOYSTICK* pInfoJoystick = this->lsInfoJoystick[i];
            if (pInfoJoystick->hDevice.dwType == RIM_TYPEHID)
                t++;
        }
        return t;
    }
    
    JOYSTICK::INFO_LAST_MOVE::INFO_LAST_MOVE()
    {
        memset(this, 0, sizeof(*this));
    }
    JOYSTICK::INFO_JOYSTICK::INFO_JOYSTICK()
    {
        memset(&hDevice,0,sizeof(hDevice));
        this->wasMoveZero = 0;
        this->info = false;
        memset(&bButtonStates, 0, sizeof(bButtonStates));
        memset(&lAxisX, 0, sizeof(lAxisX));
        memset(&lAxisY, 0, sizeof(lAxisY));
        memset(&lAxisZ, 0, sizeof(lAxisZ));
        memset(&lAxisRz, 0, sizeof(lAxisRz));
        memset(&lHat, 0, sizeof(lHat));
        memset(&numberOfButtons, 0, sizeof(numberOfButtons));
        memset(&bStateKey, KEY_NONE, sizeof(bStateKey));
    }
    
    int JOYSTICK::_indexJoystickInstance = 0;
}
