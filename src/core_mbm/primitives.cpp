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

#include <primitives.h>

namespace mbm
{
    void MatrixTranslationRotationScale(MATRIX *pout, const VEC3 *position, const VEC3 *angle,const VEC3 *scale)
    {
        static MATRIX matrixAux;
        MatrixIdentity(pout);

        MatrixRotationX(&matrixAux, angle->x);
        MatrixMultiply(pout, &matrixAux, pout);

        MatrixRotationY(&matrixAux, angle->y);
        MatrixMultiply(pout, &matrixAux, pout);
        
        MatrixRotationZ(&matrixAux, angle->z);
        MatrixMultiply(pout, &matrixAux, pout);

        
        MatrixScaling(&matrixAux, scale->x, scale->y, scale->z);
        MatrixMultiply(pout, &matrixAux, pout);

        pout->_41 = position->x;
        pout->_42 = position->y;
        pout->_43 = position->z;
    }

    MATRIX *MatrixIdentity(MATRIX *pout)
    {
        if (!pout)
            return nullptr;
        pout->m[0][1] = 0.0f;
        pout->m[0][2] = 0.0f;
        pout->m[0][3] = 0.0f;
        pout->m[1][0] = 0.0f;
        pout->m[1][2] = 0.0f;
        pout->m[1][3] = 0.0f;
        pout->m[2][0] = 0.0f;
        pout->m[2][1] = 0.0f;
        pout->m[2][3] = 0.0f;
        pout->m[3][0] = 0.0f;
        pout->m[3][1] = 0.0f;
        pout->m[3][2] = 0.0f;
        pout->m[0][0] = 1.0f;
        pout->m[1][1] = 1.0f;
        pout->m[2][2] = 1.0f;
        pout->m[3][3] = 1.0f;
        return pout;
    }

    MATRIX *MatrixAffineTransformation2D(MATRIX *out, float scaling, const VEC2 *rotationcenter, float rotation,
                                         const VEC2 *translation)
    {
        float tmp1, tmp2, s;

        s    = sinf(rotation / 2.0f);
        tmp1 = 1.0f - 2.0f * s * s;
        tmp2 = 2.0f * s * cosf(rotation / 2.0f);

        MatrixIdentity(out);
        out->m[0][0] = scaling * tmp1;
        out->m[0][1] = scaling * tmp2;
        out->m[1][0] = -scaling * tmp2;
        out->m[1][1] = scaling * tmp1;

        if (rotationcenter)
        {
            float x, y;

            x = rotationcenter->x;
            y = rotationcenter->y;

            out->m[3][0] = y * tmp2 - x * tmp1 + x;
            out->m[3][1] = -x * tmp2 - y * tmp1 + y;
        }

        if (translation)
        {
            out->m[3][0] += translation->x;
            out->m[3][1] += translation->y;
        }

        return out;
    }

    float MatrixDeterminant(const MATRIX *pm)
    {
        float t[3], v[4];

        t[0] = pm->m[2][2] * pm->m[3][3] - pm->m[2][3] * pm->m[3][2];
        t[1] = pm->m[1][2] * pm->m[3][3] - pm->m[1][3] * pm->m[3][2];
        t[2] = pm->m[1][2] * pm->m[2][3] - pm->m[1][3] * pm->m[2][2];
        v[0] = pm->m[1][1] * t[0] - pm->m[2][1] * t[1] + pm->m[3][1] * t[2];
        v[1] = -pm->m[1][0] * t[0] + pm->m[2][0] * t[1] - pm->m[3][0] * t[2];

        t[0] = pm->m[1][0] * pm->m[2][1] - pm->m[2][0] * pm->m[1][1];
        t[1] = pm->m[1][0] * pm->m[3][1] - pm->m[3][0] * pm->m[1][1];
        t[2] = pm->m[2][0] * pm->m[3][1] - pm->m[3][0] * pm->m[2][1];
        v[2] = pm->m[3][3] * t[0] - pm->m[2][3] * t[1] + pm->m[1][3] * t[2];
        v[3] = -pm->m[3][2] * t[0] + pm->m[2][2] * t[1] - pm->m[1][2] * t[2];

        return pm->m[0][0] * v[0] + pm->m[0][1] * v[1] + pm->m[0][2] * v[2] + pm->m[0][3] * v[3];
    }

    MATRIX *MatrixInverse(MATRIX *pout, float *pdeterminant, const MATRIX *pm)
    {
        float        det, t[3], v[16];
        uint32_t i, j;

        t[0] = pm->m[2][2] * pm->m[3][3] - pm->m[2][3] * pm->m[3][2];
        t[1] = pm->m[1][2] * pm->m[3][3] - pm->m[1][3] * pm->m[3][2];
        t[2] = pm->m[1][2] * pm->m[2][3] - pm->m[1][3] * pm->m[2][2];
        v[0] = pm->m[1][1] * t[0] - pm->m[2][1] * t[1] + pm->m[3][1] * t[2];
        v[4] = -pm->m[1][0] * t[0] + pm->m[2][0] * t[1] - pm->m[3][0] * t[2];

        t[0]  = pm->m[1][0] * pm->m[2][1] - pm->m[2][0] * pm->m[1][1];
        t[1]  = pm->m[1][0] * pm->m[3][1] - pm->m[3][0] * pm->m[1][1];
        t[2]  = pm->m[2][0] * pm->m[3][1] - pm->m[3][0] * pm->m[2][1];
        v[8]  = pm->m[3][3] * t[0] - pm->m[2][3] * t[1] + pm->m[1][3] * t[2];
        v[12] = -pm->m[3][2] * t[0] + pm->m[2][2] * t[1] - pm->m[1][2] * t[2];

        det = pm->m[0][0] * v[0] + pm->m[0][1] * v[4] + pm->m[0][2] * v[8] + pm->m[0][3] * v[12];
        if (det == 0.0f)
            return nullptr;
        if (pdeterminant)
            *pdeterminant = det;

        t[0] = pm->m[2][2] * pm->m[3][3] - pm->m[2][3] * pm->m[3][2];
        t[1] = pm->m[0][2] * pm->m[3][3] - pm->m[0][3] * pm->m[3][2];
        t[2] = pm->m[0][2] * pm->m[2][3] - pm->m[0][3] * pm->m[2][2];
        v[1] = -pm->m[0][1] * t[0] + pm->m[2][1] * t[1] - pm->m[3][1] * t[2];
        v[5] = pm->m[0][0] * t[0] - pm->m[2][0] * t[1] + pm->m[3][0] * t[2];

        t[0]  = pm->m[0][0] * pm->m[2][1] - pm->m[2][0] * pm->m[0][1];
        t[1]  = pm->m[3][0] * pm->m[0][1] - pm->m[0][0] * pm->m[3][1];
        t[2]  = pm->m[2][0] * pm->m[3][1] - pm->m[3][0] * pm->m[2][1];
        v[9]  = -pm->m[3][3] * t[0] - pm->m[2][3] * t[1] - pm->m[0][3] * t[2];
        v[13] = pm->m[3][2] * t[0] + pm->m[2][2] * t[1] + pm->m[0][2] * t[2];

        t[0] = pm->m[1][2] * pm->m[3][3] - pm->m[1][3] * pm->m[3][2];
        t[1] = pm->m[0][2] * pm->m[3][3] - pm->m[0][3] * pm->m[3][2];
        t[2] = pm->m[0][2] * pm->m[1][3] - pm->m[0][3] * pm->m[1][2];
        v[2] = pm->m[0][1] * t[0] - pm->m[1][1] * t[1] + pm->m[3][1] * t[2];
        v[6] = -pm->m[0][0] * t[0] + pm->m[1][0] * t[1] - pm->m[3][0] * t[2];

        t[0]  = pm->m[0][0] * pm->m[1][1] - pm->m[1][0] * pm->m[0][1];
        t[1]  = pm->m[3][0] * pm->m[0][1] - pm->m[0][0] * pm->m[3][1];
        t[2]  = pm->m[1][0] * pm->m[3][1] - pm->m[3][0] * pm->m[1][1];
        v[10] = pm->m[3][3] * t[0] + pm->m[1][3] * t[1] + pm->m[0][3] * t[2];
        v[14] = -pm->m[3][2] * t[0] - pm->m[1][2] * t[1] - pm->m[0][2] * t[2];

        t[0] = pm->m[1][2] * pm->m[2][3] - pm->m[1][3] * pm->m[2][2];
        t[1] = pm->m[0][2] * pm->m[2][3] - pm->m[0][3] * pm->m[2][2];
        t[2] = pm->m[0][2] * pm->m[1][3] - pm->m[0][3] * pm->m[1][2];
        v[3] = -pm->m[0][1] * t[0] + pm->m[1][1] * t[1] - pm->m[2][1] * t[2];
        v[7] = pm->m[0][0] * t[0] - pm->m[1][0] * t[1] + pm->m[2][0] * t[2];

        v[11] = -pm->m[0][0] * (pm->m[1][1] * pm->m[2][3] - pm->m[1][3] * pm->m[2][1]) +
                pm->m[1][0] * (pm->m[0][1] * pm->m[2][3] - pm->m[0][3] * pm->m[2][1]) -
                pm->m[2][0] * (pm->m[0][1] * pm->m[1][3] - pm->m[0][3] * pm->m[1][1]);

        v[15] = pm->m[0][0] * (pm->m[1][1] * pm->m[2][2] - pm->m[1][2] * pm->m[2][1]) -
                pm->m[1][0] * (pm->m[0][1] * pm->m[2][2] - pm->m[0][2] * pm->m[2][1]) +
                pm->m[2][0] * (pm->m[0][1] * pm->m[1][2] - pm->m[0][2] * pm->m[1][1]);

        det = 1.0f / det;

        for (i = 0; i < 4; i++)
            for (j            = 0; j < 4; j++)
                pout->m[i][j] = v[4 * i + j] * det;

        return pout;
    }

    MATRIX *MatrixLookAtLH(MATRIX *out, const VEC3 *eye, const VEC3 *at, const VEC3 *up)
    {
        VEC3 right, upn, vec;

        vec3Subtract(&vec, at, eye);
        Vec3Normalize(&vec, &vec);
        vec3Cross(&right, up, &vec);
        vec3Cross(&upn, &vec, &right);
        Vec3Normalize(&right, &right);
        Vec3Normalize(&upn, &upn);
        out->m[0][0] = right.x;
        out->m[1][0] = right.y;
        out->m[2][0] = right.z;
        out->m[3][0] = -vec3Dot(&right, eye);
        out->m[0][1] = upn.x;
        out->m[1][1] = upn.y;
        out->m[2][1] = upn.z;
        out->m[3][1] = -vec3Dot(&upn, eye);
        out->m[0][2] = vec.x;
        out->m[1][2] = vec.y;
        out->m[2][2] = vec.z;
        out->m[3][2] = -vec3Dot(&vec, eye);
        out->m[0][3] = 0.0f;
        out->m[1][3] = 0.0f;
        out->m[2][3] = 0.0f;
        out->m[3][3] = 1.0f;

        return out;
    }

    MATRIX *MatrixLookAtRH(MATRIX *out, const VEC3 *eye, const VEC3 *at, const VEC3 *up)
    {
        VEC3 right, upn, vec;

        vec3Subtract(&vec, at, eye);
        Vec3Normalize(&vec, &vec);
        vec3Cross(&right, up, &vec);
        vec3Cross(&upn, &vec, &right);
        Vec3Normalize(&right, &right);
        Vec3Normalize(&upn, &upn);
        out->m[0][0] = -right.x;
        out->m[1][0] = -right.y;
        out->m[2][0] = -right.z;
        out->m[3][0] = vec3Dot(&right, eye);
        out->m[0][1] = upn.x;
        out->m[1][1] = upn.y;
        out->m[2][1] = upn.z;
        out->m[3][1] = -vec3Dot(&upn, eye);
        out->m[0][2] = -vec.x;
        out->m[1][2] = -vec.y;
        out->m[2][2] = -vec.z;
        out->m[3][2] = vec3Dot(&vec, eye);
        out->m[0][3] = 0.0f;
        out->m[1][3] = 0.0f;
        out->m[2][3] = 0.0f;
        out->m[3][3] = 1.0f;

        return out;
    }

    MATRIX *MatrixMultiply(MATRIX *pout, const MATRIX *pm1, const MATRIX *pm2)
    {
        MATRIX out;
        int    i, j;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                out.m[i][j] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + pm1->m[i][2] * pm2->m[2][j] +
                              pm1->m[i][3] * pm2->m[3][j];
            }
        }

        *pout = out;
        return pout;
    }

    MATRIX *MatrixMultiplyTranspose(MATRIX *pout, const MATRIX *pm1, const MATRIX *pm2)
    {
        MATRIX temp;
        int    i, j;

        for (i = 0; i < 4; i++)
            for (j           = 0; j < 4; j++)
                temp.m[j][i] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + pm1->m[i][2] * pm2->m[2][j] +
                               pm1->m[i][3] * pm2->m[3][j];

        *pout = temp;
        return pout;
    }

    MATRIX *MatrixOrthoLH(MATRIX *pout, float w, float h, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f / w;
        pout->m[1][1] = 2.0f / h;
        pout->m[2][2] = 1.0f / (zf - zn);
        pout->m[3][2] = zn / (zn - zf);
        return pout;
    }

    MATRIX *MatrixOrthoOffCenterLH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f / (r - l);
        pout->m[1][1] = 2.0f / (t - b);
        pout->m[2][2] = 1.0f / (zf - zn);
        pout->m[3][0] = -1.0f - 2.0f * l / (r - l);
        pout->m[3][1] = 1.0f + 2.0f * t / (b - t);
        pout->m[3][2] = zn / (zn - zf);
        return pout;
    }

    MATRIX *MatrixOrthoOffCenterRH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f / (r - l);
        pout->m[1][1] = 2.0f / (t - b);
        pout->m[2][2] = 1.0f / (zn - zf);
        pout->m[3][0] = -1.0f - 2.0f * l / (r - l);
        pout->m[3][1] = 1.0f + 2.0f * t / (b - t);
        pout->m[3][2] = zn / (zn - zf);
        return pout;
    }

    MATRIX *MatrixOrthoRH(MATRIX *pout, float w, float h, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f / w;
        pout->m[1][1] = 2.0f / h;
        pout->m[2][2] = 1.0f / (zn - zf);
        pout->m[3][2] = zn / (zn - zf);
        return pout;
    }

    MATRIX *MatrixPerspectiveFovLH(MATRIX *pout, float fovy, float aspect, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 1.0f / (aspect * tanf(fovy / 2.0f));
        pout->m[1][1] = 1.0f / tanf(fovy / 2.0f);
        pout->m[2][2] = zf / (zf - zn);
        pout->m[2][3] = 1.0f;
        pout->m[3][2] = (zf * zn) / (zn - zf);
        pout->m[3][3] = 0.0f;
        return pout;
    }

    MATRIX *MatrixPerspectiveFovRH(MATRIX *pout, float fovy, float aspect, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 1.0f / (aspect * tanf(fovy / 2.0f));
        pout->m[1][1] = 1.0f / tanf(fovy / 2.0f);
        pout->m[2][2] = zf / (zn - zf);
        pout->m[2][3] = -1.0f;
        pout->m[3][2] = (zf * zn) / (zn - zf);
        pout->m[3][3] = 0.0f;
        return pout;
    }

    MATRIX *MatrixPerspectiveLH(MATRIX *pout, float w, float h, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f * zn / w;
        pout->m[1][1] = 2.0f * zn / h;
        pout->m[2][2] = zf / (zf - zn);
        pout->m[3][2] = (zn * zf) / (zn - zf);
        pout->m[2][3] = 1.0f;
        pout->m[3][3] = 0.0f;
        return pout;
    }

    MATRIX *MatrixPerspectiveOffCenterLH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f * zn / (r - l);
        pout->m[1][1] = -2.0f * zn / (b - t);
        pout->m[2][0] = -1.0f - 2.0f * l / (r - l);
        pout->m[2][1] = 1.0f + 2.0f * t / (b - t);
        pout->m[2][2] = -zf / (zn - zf);
        pout->m[3][2] = (zn * zf) / (zn - zf);
        pout->m[2][3] = 1.0f;
        pout->m[3][3] = 0.0f;
        return pout;
    }

    MATRIX *MatrixPerspectiveOffCenterRH(MATRIX *pout, float l, float r, float b, float t, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f * zn / (r - l);
        pout->m[1][1] = -2.0f * zn / (b - t);
        pout->m[2][0] = 1.0f + 2.0f * l / (r - l);
        pout->m[2][1] = -1.0f - 2.0f * t / (b - t);
        pout->m[2][2] = zf / (zn - zf);
        pout->m[3][2] = (zn * zf) / (zn - zf);
        pout->m[2][3] = -1.0f;
        pout->m[3][3] = 0.0f;
        return pout;
    }

    MATRIX *MatrixPerspectiveRH(MATRIX *pout, float w, float h, float zn, float zf)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = 2.0f * zn / w;
        pout->m[1][1] = 2.0f * zn / h;
        pout->m[2][2] = zf / (zn - zf);
        pout->m[3][2] = (zn * zf) / (zn - zf);
        pout->m[2][3] = -1.0f;
        pout->m[3][3] = 0.0f;
        return pout;
    }

    MATRIX *MatrixReflect(MATRIX *pout, const PLANE *pplane)
    {
        PLANE Nplane;

        PlaneNormalize(&Nplane, pplane);
        MatrixIdentity(pout);
        pout->m[0][0] = 1.0f - 2.0f * Nplane.a * Nplane.a;
        pout->m[0][1] = -2.0f * Nplane.a * Nplane.b;
        pout->m[0][2] = -2.0f * Nplane.a * Nplane.c;
        pout->m[1][0] = -2.0f * Nplane.a * Nplane.b;
        pout->m[1][1] = 1.0f - 2.0f * Nplane.b * Nplane.b;
        pout->m[1][2] = -2.0f * Nplane.b * Nplane.c;
        pout->m[2][0] = -2.0f * Nplane.c * Nplane.a;
        pout->m[2][1] = -2.0f * Nplane.c * Nplane.b;
        pout->m[2][2] = 1.0f - 2.0f * Nplane.c * Nplane.c;
        pout->m[3][0] = -2.0f * Nplane.d * Nplane.a;
        pout->m[3][1] = -2.0f * Nplane.d * Nplane.b;
        pout->m[3][2] = -2.0f * Nplane.d * Nplane.c;
        return pout;
    }

    MATRIX *MatrixRotationAxis(MATRIX *out, const VEC3 *v, float angle)
    {
        VEC3 nv;
        float   sangle, cangle, cdiff;

        Vec3Normalize(&nv, v);
        sangle = sinf(angle);
        cangle = cosf(angle);
        cdiff  = 1.0f - cangle;

        out->m[0][0] = cdiff * nv.x * nv.x + cangle;
        out->m[1][0] = cdiff * nv.x * nv.y - sangle * nv.z;
        out->m[2][0] = cdiff * nv.x * nv.z + sangle * nv.y;
        out->m[3][0] = 0.0f;
        out->m[0][1] = cdiff * nv.y * nv.x + sangle * nv.z;
        out->m[1][1] = cdiff * nv.y * nv.y + cangle;
        out->m[2][1] = cdiff * nv.y * nv.z - sangle * nv.x;
        out->m[3][1] = 0.0f;
        out->m[0][2] = cdiff * nv.z * nv.x - sangle * nv.y;
        out->m[1][2] = cdiff * nv.z * nv.y + sangle * nv.x;
        out->m[2][2] = cdiff * nv.z * nv.z + cangle;
        out->m[3][2] = 0.0f;
        out->m[0][3] = 0.0f;
        out->m[1][3] = 0.0f;
        out->m[2][3] = 0.0f;
        out->m[3][3] = 1.0f;

        return out;
    }

    MATRIX *MatrixRotationX(MATRIX *pout, float angle)
    {
        MatrixIdentity(pout);
        pout->m[1][1] = cosf(angle);
        pout->m[2][2] = cosf(angle);
        pout->m[1][2] = sinf(angle);
        pout->m[2][1] = -sinf(angle);
        return pout;
    }

    MATRIX *MatrixRotationY(MATRIX *pout, float angle)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = cosf(angle);
        pout->m[2][2] = cosf(angle);
        pout->m[0][2] = -sinf(angle);
        pout->m[2][0] = sinf(angle);
        return pout;
    }

    MATRIX *MatrixRotationYawPitchRoll(MATRIX *out, float yaw, float pitch, float roll)
    {
        float sroll, croll, spitch, cpitch, syaw, cyaw;

        sroll  = sinf(roll);
        croll  = cosf(roll);
        spitch = sinf(pitch);
        cpitch = cosf(pitch);
        syaw   = sinf(yaw);
        cyaw   = cosf(yaw);

        out->m[0][0] = sroll * spitch * syaw + croll * cyaw;
        out->m[0][1] = sroll * cpitch;
        out->m[0][2] = sroll * spitch * cyaw - croll * syaw;
        out->m[0][3] = 0.0f;
        out->m[1][0] = croll * spitch * syaw - sroll * cyaw;
        out->m[1][1] = croll * cpitch;
        out->m[1][2] = croll * spitch * cyaw + sroll * syaw;
        out->m[1][3] = 0.0f;
        out->m[2][0] = cpitch * syaw;
        out->m[2][1] = -spitch;
        out->m[2][2] = cpitch * cyaw;
        out->m[2][3] = 0.0f;
        out->m[3][0] = 0.0f;
        out->m[3][1] = 0.0f;
        out->m[3][2] = 0.0f;
        out->m[3][3] = 1.0f;

        return out;
    }

    MATRIX *MatrixRotationZ(MATRIX *pout, float angle)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = cosf(angle);
        pout->m[1][1] = cosf(angle);
        pout->m[0][1] = sinf(angle);
        pout->m[1][0] = -sinf(angle);
        return pout;
    }

    MATRIX *MatrixScaling(MATRIX *pout, float sx, float sy, float sz)
    {
        MatrixIdentity(pout);
        pout->m[0][0] = sx;
        pout->m[1][1] = sy;
        pout->m[2][2] = sz;
        return pout;
    }

    MATRIX *MatrixTranslation(MATRIX *pout, float x, float y, float z)
    {
        MatrixIdentity(pout);
        pout->m[3][0] = x;
        pout->m[3][1] = y;
        pout->m[3][2] = z;
        return pout;
    }

    MATRIX *MatrixTranspose(MATRIX *pout, const MATRIX *pm)
    {
        const MATRIX m = *pm;
        int          i, j;

        for (i = 0; i < 4; i++)
            for (j            = 0; j < 4; j++)
                pout->m[i][j] = m.m[j][i];

        return pout;
    }

    PLANE *PlaneFromPointNormal(PLANE *pout, const VEC3 *pvpoint, const VEC3 *pvnormal)
    {
        pout->a = pvnormal->x;
        pout->b = pvnormal->y;
        pout->c = pvnormal->z;
        pout->d = -vec3Dot(pvpoint, pvnormal);
        return pout;
    }

    PLANE *PlaneFromPoints(PLANE *pout, const VEC3 *pv1, const VEC3 *pv2, const VEC3 *pv3)
    {
        VEC3 edge1, edge2, normal, Nnormal;

        edge1.x = 0.0f;
        edge1.y = 0.0f;
        edge1.z = 0.0f;
        edge2.x = 0.0f;
        edge2.y = 0.0f;
        edge2.z = 0.0f;
        vec3Subtract(&edge1, pv2, pv1);
        vec3Subtract(&edge2, pv3, pv1);
        vec3Cross(&normal, &edge1, &edge2);
        Vec3Normalize(&Nnormal, &normal);
        PlaneFromPointNormal(pout, pv1, &Nnormal);
        return pout;
    }

    VEC3 *PlaneIntersectLine(VEC3 *pout, const PLANE *pp, const VEC3 *pv1, const VEC3 *pv2)
    {
        VEC3 direction, normal;
        float   dot, temp;

        normal.x    = pp->a;
        normal.y    = pp->b;
        normal.z    = pp->c;
        direction.x = pv2->x - pv1->x;
        direction.y = pv2->y - pv1->y;
        direction.z = pv2->z - pv1->z;
        dot         = vec3Dot(&normal, &direction);
        if (dot == 0.0f)
            return nullptr;
        temp    = (pp->d + vec3Dot(&normal, pv1)) / dot;
        pout->x = pv1->x - temp * direction.x;
        pout->y = pv1->y - temp * direction.y;
        pout->z = pv1->z - temp * direction.z;
        return pout;
    }

    PLANE *PlaneNormalize(PLANE *out, const PLANE *p)
    {
        float norm;

        norm = sqrtf(p->a * p->a + p->b * p->b + p->c * p->c);
        if (norm != 0.0f)
        {
            out->a = p->a / norm;
            out->b = p->b / norm;
            out->c = p->c / norm;
            out->d = p->d / norm;
        }
        else
        {
            out->a = 0.0f;
            out->b = 0.0f;
            out->c = 0.0f;
            out->d = 0.0f;
        }

        return out;
    }

    PLANE *PlaneTransform(PLANE *pout, const PLANE *pplane, const MATRIX *pm)
    {
        const PLANE plane = *pplane;

        pout->a = pm->m[0][0] * plane.a + pm->m[1][0] * plane.b + pm->m[2][0] * plane.c + pm->m[3][0] * plane.d;
        pout->b = pm->m[0][1] * plane.a + pm->m[1][1] * plane.b + pm->m[2][1] * plane.c + pm->m[3][1] * plane.d;
        pout->c = pm->m[0][2] * plane.a + pm->m[1][2] * plane.b + pm->m[2][2] * plane.c + pm->m[3][2] * plane.d;
        pout->d = pm->m[0][3] * plane.a + pm->m[1][3] * plane.b + pm->m[2][3] * plane.c + pm->m[3][3] * plane.d;
        return pout;
    }

    PLANE *PlaneTransformArray(PLANE *out, uint32_t outstride, const PLANE *in, uint32_t instride,const MATRIX *matrix, uint32_t elements)
    {
        for (uint32_t i = 0; i < elements; ++i)
        {
            PlaneTransform( static_cast<PLANE *>(out + outstride * i), static_cast<const PLANE *>(in + instride * i), matrix);
        }
        return out;
    }

    VEC3 *vec3TransformCoord(VEC3 *pout, const VEC3 *pv, MATRIX *pm)
    {
        const float norm = pm->_14 * pv->x + pm->_24 * pv->y + pm->_34 * pv->z + pm->_44;
        const float x    = (pm->_11 * pv->x + pm->_21 * pv->y + pm->_31 * pv->z + pm->_41) / norm;
        const float y    = (pm->_12 * pv->x + pm->_22 * pv->y + pm->_32 * pv->z + pm->_42) / norm;
        const float z    = (pm->_13 * pv->x + pm->_23 * pv->y + pm->_33 * pv->z + pm->_43) / norm;
        pout->x          = x;
        pout->y          = y;
        pout->z          = z;
        return pout;
    }

    COLOR colorFromRGB(const uint8_t &r, const uint8_t &g, const uint8_t &b)
    {
        return {r, g, b};
    }

    uint32_t colorFromD3DXCOLOR(COLOR &color, uint8_t &r, uint8_t &g, uint8_t &b)
    {
        const auto c2 = static_cast<uint32_t>(color);
        r                     = static_cast<uint8_t>((c2 & 0x00ff0000) >> 16);
        g                     = static_cast<uint8_t>((c2 & 0x0000ff00) >> 8);
        b                     = static_cast<uint8_t>((c2 & 0x000000ff));
        return c2;
    }

    float calcAzimuth(const float ax, const float ay)
    {
        float azimuth = 0;
        if (ax == 0.0f) //==0 //-V550
        {
            if (ay > 0)
                azimuth = 0.0f;
            else
                azimuth = 3.14159265358979323846f; // 180°
        }
        else if (ay == 0.0f) //==0 //-V550
        {
            if (ax > 0)
                azimuth = 1.57079632679489661923f; // 90
            else
                azimuth = 4.71238898270f; // 270°
        }
        else if (ax > 0) // 1° e 4° Quadrant |
        {
            if (ay > 0) // 1° Quadrant  |____
            {
                const  float y          = ax + ay;
                const  float x          = ax / y;
                azimuth = 1.570796327f * x;
            }    //                 _____
            else // 4° Quadrant  |
            {    //  |
                float y           = (ax * -1) + ay;
                float x           = ay / y;
                x                 = 1.570796327f + (1.570796327f * x);
                azimuth = x; // 135°
            }
        }
        else // 2° e 3° Quadrant
        {
            if (ay > 0) // 2° Quadrant |
            {                    //                    _____|
                const float y     = (ax * -1) + ay;
                float x           = ay / y;
                x                 = 4.71238898f + (1.570796327f * x);
                azimuth = x; // 5.497787144f;//315°
            }
            else // 3° Quadrant          _____
            {    //          |
                //           |
                const float y     = ax + ay;
                const float x     = ax / y;
                azimuth = (1.570796327f * x) + 3.14159265358979323846f;
            }
        }
        return azimuth;
    }
}

