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

#include <util-interface.h>
#include <algorithm>
#include <cr-static-local.h>
#include <cmath>
#include <ctime>
#include <cstring>
#include <functional>

namespace util
{

    float getByteProp() // 1 / 255
    {
        return (1.0f / 255.0f);
    }

    float radianToDegree(const float radian)
    {
        return radian * (180.0f / 3.141592654f);
    }

    float degreeToRadian(const float degree)
    {
        return  static_cast<float>(degree * 0.01745329251994329576924f);
    }

    const char* getDirSeparator()
    {
        #ifdef _WIN32
	        static const char _DIRSEPARATOR[2] = "\\";
        #else
	        static const char _DIRSEPARATOR[2] = "/";
        #endif
        return _DIRSEPARATOR;
    }

    const char  getCharDirSeparator()
    {
        #ifdef _WIN32
	        const char _DIRSEPARATOR = '\\';
        #else
	        const char _DIRSEPARATOR = '/';
        #endif
        return _DIRSEPARATOR;
    }

    const char *getPathFromFullPathName(const char *fileNamePath)
    {
        if (fileNamePath == nullptr)
            return nullptr;
        CR_DEFINE_STATIC_LOCAL(std::string,ret);
        ret.clear();
        std::vector<std::string> lsRet;
        util::split(lsRet, fileNamePath, util::getDirSeparator()[0]);
        if (lsRet.size())
        {
            const std::vector<std::string>::size_type  s = lsRet.size() - 1;
            for (std::vector<std::string>::size_type i = 0; i < s; ++i)
            {
                ret += lsRet[i];
                if ((i + 1) < s)
                    ret += util::getDirSeparator();
            }
        }
        return ret.c_str();
    }

    #if defined   _WIN32

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

    #endif

    void setRandomSeed()
    {
        static bool ST_Semente = false;
        if (!ST_Semente)
        {                                 // TODO: gerar um sed random aqui
            srand(static_cast<uint32_t>(time(nullptr))); // seed random number generator
            ST_Semente = true;
        }
    }

    float getHeightMaxWithInitialSpeed(const float gravity, const float speedInitial) noexcept
    {
        return ((speedInitial * speedInitial) / (gravity * 2.0f));
    }

    float getHeightWithTime(const float gravity, const float time) noexcept
    {
        return ((gravity / 2.0f) * (time * time));
    }

    float getTimeWithMaxHeight(const float gravity, const float heigth) noexcept
    {
        return (sqrtf((2.0f * heigth) / gravity));
    }

    float getSpeedWithTimeFall(const float gravity, const float time) noexcept
    {
        return (time * gravity);
    }

    float getSpeedWithHeight(const float gravity, const float heigth) noexcept
    {
        return (sqrtf(2.0f * gravity * heigth));
    }

    int getRandomInt(const int min, const int max) noexcept
    {
        return static_cast<int>(((static_cast<double>(rand()) / static_cast<double>(0x7fff + 1)) * (max - min + 1) + min));
    }

    char getRandomChar(const char min, const char max) noexcept
    {
        return static_cast<char>(((static_cast<double>(rand()) / static_cast<double>(0x7fff + 1)) * (max - min + 1) + min));
    }

    float getRandomFloat(const float min, const float max) noexcept
    {
        if (min >= max) // bad input
            return min;
        // get random float in [0, 1] interval
        float f = (rand() % 10000) * 0.0001f;
        // return float in [min, max] interval.
        return (f * (max - min)) + min;
    }

    uint32_t FloatToDWORD(float &Float) noexcept
    {   
        const auto* p = static_cast<const void*>(&Float);
        return *static_cast<const uint32_t *>(p);
    }

    //const mbm::COLOR COLOR_WHITE(mbm::COLOR(255, 255, 255));
    //const mbm::COLOR COLOR_BLACK(mbm::COLOR(0, 0, 0));
    //const mbm::COLOR COLOR_RED(mbm::COLOR(255, 0, 0));
    //const mbm::COLOR COLOR_GREEN(mbm::COLOR(0, 255, 0));
    //const mbm::COLOR COLOR_BLUE(mbm::COLOR(0, 0, 255));
    //const mbm::COLOR COLOR_YELLOW(mbm::COLOR(255, 255, 0));
    //const mbm::COLOR COLOR_OCEAN(mbm::COLOR(0, 255, 255));
    //const mbm::COLOR COLOR_MAGENT(mbm::COLOR(255, 0, 255));
    //const mbm::COLOR COLOR_ORANGE(mbm::COLOR(255, 128, 0));
    //const mbm::COLOR COLOR_EVENING(mbm::COLOR(255, 128, 30));
    //const mbm::COLOR COLOR_PURPLE(mbm::COLOR(128, 0, 255));
    //const mbm::COLOR COLOR_PURPLE_BLACK(mbm::COLOR(70, 0, 100));
    //const mbm::COLOR COLOR_ROSE(mbm::COLOR(255, 128, 128));
    //const mbm::COLOR COLOR_MOSS(mbm::COLOR(128, 128, 0));
    //const mbm::COLOR COLOR_GREEN_CLEAR(mbm::COLOR(128, 255, 128));
    //const mbm::COLOR COLOR_BLUE_MARINE(mbm::COLOR(50, 50, 128));
    //const mbm::COLOR COLOR_BLUE_POOL(mbm::COLOR(20, 167, 222));
    //const mbm::COLOR COLOR_MUSTARD(mbm::COLOR(234, 158, 19));
    //const mbm::COLOR COLOR_GRAY(mbm::COLOR(121, 121, 121));
    //const mbm::COLOR COR_VERDE_MEIO_TOM(mbm::COLOR(0, 128, 128));
    //const mbm::COLOR COLOR_RED_DARK(mbm::COLOR(114, 31, 31));
    //const mbm::COLOR COLOR_GRAY_CLEAR(mbm::COLOR(180, 180, 180));
    //const mbm::COLOR COLOR_GRAY_DARK(mbm::COLOR(50, 50, 50));
    //const mbm::COLOR COLOR_RED_OPAQUE(mbm::COLOR(164, 98, 98));
    //const mbm::COLOR COLOR_RED_WASHOUT(mbm::COLOR(105, 165, 136));
    //const mbm::COLOR COLOR_YELLOW_CLEAR(mbm::COLOR(255, 255, 128));
    //const mbm::COLOR COLOR_GREEN_BRIGHT(mbm::COLOR(0, 255, 128));
    //const mbm::COLOR COLOR_WINE(mbm::COLOR(128, 0, 64));
    //
    //const MATERIAL MATERIAL_WHITE (getMaterial(COLOR_WHITE, COLOR_WHITE, COLOR_WHITE, COLOR_BLACK, 1.0f));
    //const MATERIAL MATERIAL_RED   (getMaterial(COLOR_RED, COLOR_RED, COLOR_RED, COLOR_BLACK, 1.0f));
    //const MATERIAL MATERIAL_GREEN (getMaterial(COLOR_GREEN, COLOR_GREEN, COLOR_GREEN, COLOR_BLACK, 1.0f));
    //const MATERIAL MATERIAL_BLUE  (getMaterial(COLOR_BLUE, COLOR_BLUE, COLOR_BLUE, COLOR_BLACK, 1.0f));
    //const MATERIAL MATERIAL_YELLOW(getMaterial(COLOR_YELLOW, COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK, 1.0f));
    //const MATERIAL MATERIAL_BLACK (getMaterial(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, 1.0f));

    //MATERIAL getMaterial(const mbm::COLOR &ambient, const mbm::COLOR &diffuse, const mbm::COLOR &specular,
    //                            const mbm::COLOR &emissive, const float &power) noexcept
    //{
    //    MATERIAL material;
    //    material.Ambient  = ambient;
    //    material.Diffuse  = diffuse;
    //    material.Specular = specular;
    //    material.Emissive = emissive;
    //    material.Power    = power;
    //    return material;
    //}
    //
    //void initMaterial(MATERIAL &material) noexcept
    //{
    //    material.Ambient  = (mbm::COLOR)mbm::COLOR(255, 255, 255);
    //    material.Diffuse  = (mbm::COLOR)mbm::COLOR(255, 255, 255);
    //    material.Specular = (mbm::COLOR)mbm::COLOR(255, 255, 255);
    //    material.Emissive = (mbm::COLOR)mbm::COLOR(0, 0, 0);
    //    material.Power    = 1.0f;
    //}



    void getAABB(const float halfDimIn[2], const float angleRadian, float *widthOut, float *heightOut) noexcept
    {
        const float corner_1_x = halfDimIn[0];
        const float corner_2_x = halfDimIn[0];
        const float corner_1_y = -halfDimIn[1];
        const float corner_2_y = halfDimIn[1];

        const float sin_o = sinf(angleRadian);
        const float cos_o = cosf(angleRadian);

        const float fCorner_a_x = corner_1_x * cos_o - corner_1_y * sin_o;
        const float fCorner_a_y = corner_1_x * sin_o + corner_1_y * cos_o;
        const float fCorner_b_x = corner_2_x * cos_o - corner_2_y * sin_o;
        const float fCorner_b_y = corner_2_x * sin_o + corner_2_y * cos_o;
        const float ex          = std::max(fabsf(fCorner_a_x), fabsf(fCorner_b_x));
        const float ey          = std::max(fabsf(fCorner_a_y), fabsf(fCorner_b_y));

        const float aabb_min_x = -ex;
        const float aabb_max_x = ex;
        const float aabb_min_y = -ey;
        const float aabb_max_y = ey;

        *widthOut  = aabb_max_x - aabb_min_x;
        *heightOut = aabb_max_y - aabb_min_y;
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
                    result.emplace_back(in, next);
                }
                else
                {
                    result.emplace_back(in);
                }
                in = (next ? next + 1 : "");
            }
        }
    }

	void base_64_decode(const std::string & str_encoded, std::string & result)
	{
		static const std::string base64_chars =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789+/";

		std::function<bool(uint8_t)> is_base64 = [](uint8_t c)->bool
		{
			return (isalnum(c) || (c == '+') || (c == '/'));
		};

		auto in_len = str_encoded.size();
		int i = 0;
		int j = 0;
		int in_ = 0;
		uint8_t char_array_4[4], char_array_3[3];
		
		while (in_len-- && (str_encoded[in_] != '=') && is_base64(str_encoded[in_]))
		{
			char_array_4[i++] = str_encoded[in_]; in_++;
			if (i == 4)
			{
				for (i = 0; i < 4; i++)
				{
					char_array_4[i] = static_cast<uint8_t>(base64_chars.find(char_array_4[i]));
				}
				char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
				char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
				char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

				for (i = 0; (i < 3); i++)
				{
					result += char_array_3[i];
				}
				i = 0;
			}
		}

		if (i)
		{
			for (j = i; j < 4; j++)
			{
				char_array_4[j] = 0;
			}

			for (j = 0; j < 4; j++)
			{
				char_array_4[j] = static_cast<uint8_t>(base64_chars.find(char_array_4[j]));
			}

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (j = 0; (j < i - 1); j++)
			{
				result += char_array_3[j];
			}
		}

	}
}
