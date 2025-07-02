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

#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "core-exports.h"
#include <stdint.h>
#include <cmath>
#include <limits>
#include <utility>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined _WIN32
	#pragma warning(push)
	#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

namespace mbm
{
struct API_IMPL VEC2
{
    union {
        struct
        {
            float x, y;
        };
        struct
        {
            float v[2];
        };
    };
    constexpr VEC2() noexcept : x(0.0f), y(0.0f)
    {
    }

    constexpr VEC2(const float x_, const float y_) noexcept : x(x_), y(y_)
    {
    }

    constexpr VEC2(const VEC2 &vet) noexcept : x(vet.x), y(vet.y)
    {
    }

    inline VEC2 &operator=(const VEC2 &vec)  noexcept
    {
        x = vec.x;
        y = vec.y;
        return *this;
    }

    inline VEC2 &operator+=(const VEC2 &vec) noexcept
    {
        x += vec.x;
        y += vec.y;
        return *this;
    }

    inline VEC2 &operator-=(const VEC2 &vec) noexcept
    {
        x -= vec.x;
        y -= vec.y;
        return *this;
    }

    inline VEC2 &operator*=(const VEC2 &vec) noexcept
    {
        x *= vec.x;
        y *= vec.y;
        return *this;
    }

    inline VEC2 &operator/=(const VEC2 &vec) noexcept
    {
        x /= vec.x;
        y /= vec.y;
        return *this;
    }

    inline VEC2 operator+(const VEC2 &vec) const  noexcept
    {
        return VEC2(x + vec.x, y + vec.y);
    }

    inline VEC2 operator-(const VEC2 &vec) const  noexcept
    {
        return VEC2(x - vec.x, y - vec.y);
    }

    inline VEC2 operator*(const float num) const  noexcept
    {
        return VEC2(x * num, y * num);
    }

    inline VEC2 operator/(const float num) const  noexcept
    {
        const float res = 1.0f / num;
        return VEC2(x * res, y * res);
    }

    inline float length() const noexcept
    {
        return static_cast<float>(sqrtf((x * x) + (y * y)));
    }

    inline operator float *()  noexcept
    {
        return &this->x;
    }
};

struct API_IMPL VEC3
{
    union {
        struct
        {
            float x, y, z;
        };
        struct
        {
            float v[3];
        };
    };

    constexpr VEC3() noexcept : x(0.0f), y(0.0f), z(0.0f)
    {
    }

    constexpr VEC3(const float *f) noexcept : x(f[0]), y(f[1]), z(f[2])
    {
    }

    constexpr VEC3(const float x_, const float y_, const float z_) noexcept : x(x_), y(y_), z(z_)
    {
    }

    constexpr VEC3(const VEC3 &vet) noexcept : x(vet.x), y(vet.y), z(vet.z)
    {
    }

    inline VEC3 &operator+=(const VEC3 &vec) noexcept
    {
        x += vec.x;
        y += vec.y;
        z += vec.z;
        return *this;
    }

    inline VEC3 &operator=(const VEC3 &vec) noexcept
    {
        x = vec.x;
        y = vec.y;
        z = vec.z;
        return *this;
    }

    inline VEC3 &operator-=(const VEC3 &vec) noexcept
    {
        x -= vec.x;
        y -= vec.y;
        z -= vec.z;
        return *this;
    }

    inline VEC3 &operator*=(const VEC3 &vec) noexcept
    {
        x *= vec.x;
        y *= vec.y;
        z *= vec.z;
        return *this;
    }

    inline VEC3 &operator/=(const VEC3 &vec) noexcept
    {
        x /= vec.x;
        y /= vec.y;
        z /= vec.z;
        return *this;
    }

    inline VEC3 operator+(const VEC3 &vec) const noexcept
    {
        return VEC3(x + vec.x, y + vec.y, z + vec.z);
    }

    inline VEC3 operator-(const VEC3 &vec) const noexcept
    {
        return VEC3(x - vec.x, y - vec.y, z - vec.z);
    }

    inline VEC3 operator*(const float num) const noexcept
    {
        return VEC3(x * num, y * num, z * num);
    }

    inline VEC3 operator/(const float num) const noexcept
    {
        const float res = 1.0f / num;
        return VEC3(x * res, y * res, z * res);
    }

    inline float length() const noexcept
    {
        return static_cast<float>(sqrtf((x * x) + (y * y) + (z * z)));
    }

    inline operator float *() noexcept
    {
        return static_cast<float *>(&this->x);
    }
};

struct API_IMPL COLOR
{
  public:
    constexpr COLOR() noexcept : r(0.0f), g(0.0f), b(0.0f), a(0.0f)
    {
    }

    inline COLOR(const uint32_t dw) noexcept
    {
        constexpr float f = 1.0f / 255.0f;
        r                 = f * static_cast<float>(static_cast<uint8_t>((dw >> 16)));
        g                 = f * static_cast<float>(static_cast<uint8_t>((dw >> 8)));
        b                 = f * static_cast<float>(static_cast<uint8_t>((dw >> 0)));
        a                 = f * static_cast<float>(static_cast<uint8_t>((dw >> 24)));
    }

    constexpr COLOR(const float *pf) : r(pf[0]), g(pf[1]), b(pf[2]), a(pf[3])
    {
    }

    constexpr COLOR(const float fr, const float fg, const float fb, const float fa) noexcept : r(fr), g(fg), b(fb), a(fa)
    {
    }

    inline COLOR(const uint8_t UCr, const uint8_t UCg, const uint8_t UCb,
                 const uint8_t UCa) noexcept
    {
        constexpr float prop = 1.0f / 255.0f;
        r                    = prop * UCr;
        g                    = prop * UCg;
        b                    = prop * UCb;
        a                    = prop * UCa;
    }

	static const char* getStringHexColorFromColor(const COLOR &color,char * out_put_string,const int size_string_out) noexcept
	{
		snprintf(out_put_string,size_string_out,"#%x",(uint32_t)color);
		return out_put_string;
	}
	

    static COLOR getColorFromHexString(const char *stringAsColor) noexcept
    {
        COLOR color(0.0f, 0.0f, 0.0f, 0.0f);
        if (stringAsColor == nullptr)
            return color;
        int len = strlen(stringAsColor);
        if ((len == 9 && stringAsColor[0] == '#') || (len == 7 && stringAsColor[0] == '#'))
        {
            len = len - 1;
            stringAsColor++;
        }

        if (len == 8)
        {
            char alpha[3] = {0, 0, 0};
            alpha[0]      = *stringAsColor;
            stringAsColor++;
            alpha[1] = *stringAsColor;
            stringAsColor++;
            const int n = strtol(stringAsColor, nullptr, 16);
            color       = COLOR(n);
            color.a     = strtol(alpha, nullptr, 16) * 1.0f / 255.0f;
        }
        else if (len == 6)
        {
            const int n = strtol(stringAsColor, nullptr, 16);
            color       = COLOR(n);
            color.a     = 1.0f;
        }
        return color;
    }

    inline COLOR(const uint8_t UCr, const uint8_t UCg, const uint8_t UCb) noexcept
    {
        constexpr float prop = 1.0f / 255.0f;
        r                    = prop * UCr;
        g                    = prop * UCg;
        b                    = prop * UCb;
        a                    = 1;
    }

    inline void get(uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *alpha) const noexcept //(0 a 255)
    {
        *red   = static_cast<uint8_t>((r * 255.0f + 0.5f));
        *green = static_cast<uint8_t>((g * 255.0f + 0.5f));
        *blue  = static_cast<uint8_t>((b * 255.0f + 0.5f));
        *alpha = static_cast<uint8_t>((a * 255.0f + 0.5f));
    }

    inline void get(uint8_t *red, uint8_t *green, uint8_t *blue) const noexcept //(0 a 255)
    {
        *red   = static_cast<uint8_t>((r * 255.0f + 0.5f));
        *green = static_cast<uint8_t>((g * 255.0f + 0.5f));
        *blue  = static_cast<uint8_t>((b * 255.0f + 0.5f));
    }

    inline operator uint32_t() const noexcept
    {
        const uint32_t dwR = r >= 1.0f ? 0xff : r <= 0.0f ? 0x00 : static_cast<uint32_t>((r * 255.0f + 0.5f));
        const uint32_t dwG = g >= 1.0f ? 0xff : g <= 0.0f ? 0x00 : static_cast<uint32_t>((g * 255.0f + 0.5f));
        const uint32_t dwB = b >= 1.0f ? 0xff : b <= 0.0f ? 0x00 : static_cast<uint32_t>((b * 255.0f + 0.5f));
        const uint32_t dwA = a >= 1.0f ? 0xff : a <= 0.0f ? 0x00 : static_cast<uint32_t>((a * 255.0f + 0.5f));

        return (dwA << 24) | (dwR << 16) | (dwG << 8) | dwB;
    }

    inline operator float *() noexcept
    {
        return static_cast<float *>(&r);
    }

    inline operator const float *() const noexcept
    {
        return static_cast<const float *>(&r);
    }

    // assignment operators
    inline COLOR &operator+=(const COLOR &c) noexcept
    {
        r += c.r;
        g += c.g;
        b += c.b;
        a += c.a;
        return *this;
    }

    inline COLOR &operator-=(const COLOR &c) noexcept
    {
        r -= c.r;
        g -= c.g;
        b -= c.b;
        a -= c.a;
        return *this;
    }

    inline COLOR &operator*=(const float f) noexcept
    {
        r *= f;
        g *= f;
        b *= f;
        a *= f;
        return *this;
    }

    inline COLOR &operator/=(const float f) noexcept
    {
        const float fInv = 1.0f / f;
        r *= fInv;
        g *= fInv;
        b *= fInv;
        a *= fInv;
        return *this;
    }

    // unary operators
    inline COLOR operator+() const noexcept
    {
        return *this;
    }

    inline COLOR operator-() const noexcept
    {
        return COLOR(-r, -g, -b, -a);
    }

    inline COLOR operator+(const COLOR &c) const noexcept
    {
        return COLOR(r + c.r, g + c.g, b + c.b, a + c.a);
    }

    inline COLOR operator-(const COLOR &c) const noexcept
    {
        return COLOR(r - c.r, g - c.g, b - c.b, a - c.a);
    }

    inline COLOR operator*(const float f) const noexcept
    {
        return COLOR(r * f, g * f, b * f, a * f);
    }

    inline COLOR operator/(const float f) const noexcept
    {
        const float fInv = 1.0f / f;
        return COLOR(r * fInv, g * fInv, b * fInv, a * fInv);
    }

    inline bool operator==(const COLOR &c) const noexcept
    {
        constexpr float EPSILON = std::numeric_limits<float>::epsilon();
        return fabsf(r - c.r) < EPSILON && fabsf(g - c.g) < EPSILON && fabsf(b - c.b) < EPSILON &&
               fabsf(a - c.a) < EPSILON;
    }

    inline bool operator!=(const COLOR &c) const noexcept
    {
        constexpr float EPSILON = std::numeric_limits<float>::epsilon();
        return fabsf(r - c.r) > EPSILON && fabsf(g - c.g) > EPSILON && fabsf(b - c.b) > EPSILON &&
               fabsf(a - c.a) > EPSILON;
    }

    float r, g, b, a;
};

API_IMPL inline void vec3Normalize(VEC3 *vectorOut, const VEC3 *vectorIn) noexcept
{
    const float len = static_cast<const float>(
        sqrtf((vectorIn->x * vectorIn->x) + (vectorIn->y * vectorIn->y) + (vectorIn->z * vectorIn->z)));
    if (vectorIn->x != 0.0f) //-V550
        vectorOut->x = vectorIn->x / len;
    else
        vectorOut->x = 0.0f;

    if (vectorIn->y != 0.0f) //-V550
        vectorOut->y = vectorIn->y / len;
    else
        vectorOut->y = 0.0f;

    if (vectorIn->z != 0.0f) //-V550
        vectorOut->z = vectorIn->z / len;
    else
        vectorOut->z = 0.0f;
}

API_IMPL inline void vec2Normalize(VEC2 *vectorOut, const VEC2 *vectorIn) noexcept
{
    const float len = static_cast<const float>(sqrtf((vectorIn->x * vectorIn->x) + (vectorIn->y * vectorIn->y)));
    if (vectorIn->x != 0.0f) //-V550
        vectorOut->x = vectorIn->x / len;
    else
        vectorOut->x = 0.0f;

    if (vectorIn->y != 0.0f) //-V550
        vectorOut->y = vectorIn->y / len;
    else
        vectorOut->y = 0.0f;
}

API_IMPL inline float vec2Dot(const VEC2 *vector1, const VEC2 *vector2) noexcept
{
    return vector1->x * vector2->x + vector1->y * vector2->y;
}

API_IMPL inline VEC3 *vec3Scale(VEC3 *pout, const VEC3 *pv, float s) noexcept
{
    if (!pout || !pv)
        return nullptr;
    pout->x = s * (pv->x);
    pout->y = s * (pv->y);
    pout->z = s * (pv->z);
    return pout;
}

API_IMPL inline VEC3 *vec3Subtract(VEC3 *pout, const VEC3 *pva, const VEC3 *pvb) noexcept
{
    if (!pout || !pva || !pvb)
        return nullptr;
    pout->x = pva->x - pvb->x;
    pout->y = pva->y - pvb->y;
    pout->z = pva->z - pvb->z;
    return pout;
}

API_IMPL inline VEC3 *vec3Lerp(VEC3 *pout, const VEC3 *pv1, const VEC3 *pv2, float s) noexcept
{
    if (!pout || !pv1 || !pv2)
        return nullptr;
    pout->x = (1 - s) * (pv1->x) + s * (pv2->x);
    pout->y = (1 - s) * (pv1->y) + s * (pv2->y);
    pout->z = (1 - s) * (pv1->z) + s * (pv2->z);
    return pout;
}

API_IMPL inline VEC3 *vec3Add(VEC3 *pout, const VEC3 *pv1, const VEC3 *pv2) noexcept
{
    if (!pout || !pv1 || !pv2)
        return nullptr;
    pout->x = pv1->x + pv2->x;
    pout->y = pv1->y + pv2->y;
    pout->z = pv1->z + pv2->z;
    return pout;
}

API_IMPL inline VEC3 *vec3Cross(VEC3 *pout, const VEC3 *pv1, const VEC3 *pv2) noexcept
{
    if (!pout || !pv1 || !pv2)
        return nullptr;
    const float t_x = (pv1->y) * (pv2->z) - (pv1->z) * (pv2->y);
    const float t_y = (pv1->z) * (pv2->x) - (pv1->x) * (pv2->z);
    const float t_z = (pv1->x) * (pv2->y) - (pv1->y) * (pv2->x);
    pout->x         = t_x;
    pout->y         = t_y;
    pout->z         = t_z;
    return pout;
}

API_IMPL inline float vec3Dot(const VEC3 *pv1, const VEC3 *pv2) noexcept
{
    if (!pv1 || !pv2)
        return 0.0f;
    return (pv1->x) * (pv2->x) + (pv1->y) * (pv2->y) + (pv1->z) * (pv2->z);
}

API_IMPL inline float vec3Length(const VEC3 *pv) noexcept
{
    if (!pv)
        return 0.0f;
    return static_cast<float>(sqrtf(pv->x * pv->x + pv->y * pv->y + pv->z * pv->z));
}

API_IMPL inline VEC3 *Vec3Normalize(VEC3 *pout, const VEC3 *pv) noexcept
{
    float norm;
    norm = vec3Length(pv);
    if (norm == 0.0f) //-V550
    {
        pout->x = 0.0f;
        pout->y = 0.0f;
        pout->z = 0.0f;
    }
    else
    {
        pout->x = pv->x / norm;
        pout->y = pv->y / norm;
        pout->z = pv->z / norm;
    }

    return pout;
}

struct API_IMPL MATRIX
{
    union {
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
        float p[16];
    };
};

API_IMPL void    MatrixTranslationRotationScale(MATRIX *pout, const VEC3 *position, const VEC3 *angle, const VEC3 *scale);
API_IMPL MATRIX *MatrixIdentity(MATRIX *pout);
API_IMPL MATRIX *MatrixAffineTransformation2D(MATRIX *pout, float scaling, const VEC2 *protationcenter, float rotation,
                                     const VEC2 *ptranslation);
API_IMPL float   MatrixDeterminant(const MATRIX *pm);
API_IMPL MATRIX *MatrixInverse(MATRIX *pout, float *pdeterminant, const MATRIX *pm);
API_IMPL MATRIX *MatrixLookAtLH(MATRIX *pout, const VEC3 *peye, const VEC3 *pat, const VEC3 *pup);
API_IMPL MATRIX *MatrixLookAtRH(MATRIX *pout, const VEC3 *peye, const VEC3 *pat, const VEC3 *pup);
API_IMPL MATRIX *MatrixMultiply(MATRIX *pout, const MATRIX *pm1, const MATRIX *pm2);
API_IMPL MATRIX *MatrixMultiplyTranspose(MATRIX *pout, const MATRIX *pm1, const MATRIX *pm2);
API_IMPL MATRIX *MatrixOrthoLH(MATRIX *pout, float w, float h, float zn, float zf);
API_IMPL MATRIX *MatrixOrthoOffCenterLH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf);
API_IMPL MATRIX *MatrixOrthoOffCenterRH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf);
API_IMPL MATRIX *MatrixOrthoRH(MATRIX *pout, float w, float h, float zn, float zf);
API_IMPL MATRIX *MatrixPerspectiveFovLH(MATRIX *pout, float fovy, float aspect, float zn, float zf);
API_IMPL MATRIX *MatrixPerspectiveFovRH(MATRIX *pout, float fovy, float aspect, float zn, float zf);
API_IMPL MATRIX *MatrixPerspectiveLH(MATRIX *pout, float w, float h, float zn, float zf);
API_IMPL MATRIX *MatrixPerspectiveOffCenterLH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf);
API_IMPL MATRIX *MatrixPerspectiveOffCenterRH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf);
API_IMPL MATRIX *MatrixPerspectiveRH(MATRIX *pout, float w, float h, float zn, float zf);
API_IMPL MATRIX *MatrixRotationAxis(MATRIX *pout, const VEC3 *pv, float angle);
API_IMPL MATRIX *MatrixRotationX(MATRIX *pout, float angle);
API_IMPL MATRIX *MatrixRotationY(MATRIX *pout, float angle);
API_IMPL MATRIX *MatrixRotationYawPitchRoll(MATRIX *pout, float yaw, float pitch, float roll);
API_IMPL MATRIX *MatrixRotationZ(MATRIX *pout, float angle);
API_IMPL MATRIX *MatrixScaling(MATRIX *pout, float sx, float sy, float sz);
API_IMPL MATRIX *MatrixTranslation(MATRIX *pout, float x, float y, float z);
API_IMPL MATRIX *MatrixTranspose(MATRIX *pout, const MATRIX *pm);

struct API_IMPL PLANE
{
    float a, b, c, d;
    constexpr PLANE() noexcept : a(0.0f), b(0.0f), c(0.0f), d(0.0f)
    {
    }

    constexpr PLANE(const float *pf) : a(pf[0]), b(pf[1]), c(pf[2]), d(pf[3])
    {
    }

    constexpr PLANE(float fa, float fb, float fc, float fd) noexcept : a(fa), b(fb), c(fc), d(fd)
    {
    }

    inline operator float *() noexcept
    {
        return static_cast<float *>(&a);
    }

    inline operator const float *() noexcept
    {
        return static_cast<const float *>(&a);
    }

    inline PLANE operator+() const noexcept
    {
        return *this;
    }

    inline PLANE operator-() const noexcept
    {
        return PLANE(-a, -b, -c, -d);
    }

    inline bool operator==(const PLANE &pl) const noexcept
    {
        return a == pl.a && b == pl.b && c == pl.c && d == pl.d; //-V550
    }

    inline bool operator!=(const PLANE &pl) const noexcept
    {
        return a != pl.a || b != pl.b || c != pl.c || d != pl.d; //-V550
    }
};

API_IMPL PLANE *PlaneFromPointNormal(PLANE *pout, const VEC3 *pvpoint, const VEC3 *pvnormal);
API_IMPL PLANE *PlaneFromPoints(PLANE *pout, const VEC3 *pv1, const VEC3 *pv2, const VEC3 *pv3);
API_IMPL VEC3 * PlaneIntersectLine(VEC3 *pout, const PLANE *pp, const VEC3 *pv1, const VEC3 *pv2);
API_IMPL PLANE *PlaneNormalize(PLANE *pout, const PLANE *pp);
API_IMPL PLANE *PlaneTransform(PLANE *pout, const PLANE *pplane, const MATRIX *pm);
API_IMPL PLANE *PlaneTransformArray(PLANE *pout, uint32_t outstride, const PLANE *pplane, uint32_t pstride,
                           const MATRIX *pm, uint32_t n);

API_IMPL inline float PlaneDotCoord(const PLANE *pP, const VEC3 *pV) noexcept
{
    return pP->a * pV->x + pP->b * pV->y + pP->c * pV->z + pP->d;
}

API_IMPL inline float PlaneDotNormal(const PLANE *pP, const VEC3 *pV) noexcept
{
    return pP->a * pV->x + pP->b * pV->y + pP->c * pV->z;
}

API_IMPL inline PLANE *PlaneScale(PLANE *pOut, const PLANE *pP, float s) noexcept
{
    pOut->a = pP->a * s;
    pOut->b = pP->b * s;
    pOut->c = pP->c * s;
    pOut->d = pP->d * s;
    return pOut;
}

API_IMPL MATRIX *MatrixReflect(MATRIX *pout, const PLANE *pplane);

API_IMPL VEC3 *vec3TransformCoord(VEC3 *pout, const VEC3 *pv, MATRIX *pm);

API_IMPL COLOR colorFromRGB(const uint8_t &r, const uint8_t &g, const uint8_t &b);

API_IMPL uint32_t colorFromD3DXCOLOR(mbm::COLOR &color, uint8_t &r, uint8_t &g, uint8_t &b);

API_IMPL float calcAzimuth(const float ax, const float ay);

}

#if defined _WIN32
	#pragma warning(pop) // nonstandard extension used : nameless struct/union
#endif

#endif
