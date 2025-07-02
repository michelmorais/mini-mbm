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

#include <camera.h>
#include <gles-debug.h>
#include <platform/mismatch-platform.h>
#include <cmath>
#include <cstring>
#include <cmath>

namespace mbm
{
    CAMERA::CAMERA() noexcept :
        position          (0, 0, -100),
        position2d        (0, 0),
        focus             (0, 0, 0),
        up                (0, 1, 0),
        normalForward     (1, 1, 1),
        normalRight       (0, 0, 0),
        normalBackward    (0, 0, 0),
        normalLeft        (0, 0, 0),
        normalUp          (0, 0, 0),
        scale2d           (1, 1, 1),
        scaleScreen2d     (1, 1, 1),
        expectedScreen    (1, 1),
        azimuthFromCamera (0.0f),
        angleOfView       (110.0f),
        zNear(0.1f),
        zFar(1000.0f),
		perfectPixel(false)
    {
        memset(this->stretch, 0, sizeof(stretch));
    }
    
    void CAMERA::calculateAzimuthFromCamera() noexcept
    {
        float   x, auxTotal;
        VEC3 direction(focus - position);
        if (direction.x == 0.0f) //==0 //-V550
        {
            if (direction.z > 0)
                azimuthFromCamera = 0.0f;
            else
                azimuthFromCamera = 3.14159265358979323846f; // 180°
        }
        else if (direction.z == 0.0f) //==0 //-V550
        {
            if (direction.x > 0)
                azimuthFromCamera = 1.57079632679489661923f; // 90
            else
                azimuthFromCamera = 4.71238898270f; // 270°
        }
        else if (direction.x > 0) // 1° e 4° Quadrant |
        {
            if (direction.z > 0) // 1° Quadrant  |____
            {
                auxTotal          = direction.x + direction.z;
                x                 = direction.x / auxTotal;
                azimuthFromCamera = 1.570796327f * x;
            }    //                 _____
            else // 4° Quadrant  |
            {    //  |
                auxTotal          = (direction.x * -1) + direction.z;
                x                 = direction.z / auxTotal;
                x                 = 1.570796327f + (1.570796327f * x);
                azimuthFromCamera = x; // 135°
            }
        }
        else // 2° e 3° Quadrant
        {
            if (direction.z > 0) // 2° Quadrant |
            {                    //                    _____|
                auxTotal          = (direction.x * -1) + direction.z;
                x                 = direction.z / auxTotal;
                x                 = 4.71238898f + (1.570796327f * x);
                azimuthFromCamera = x; // 5.497787144f;//315°
            }
            else // 3° Quadrant          _____
            {    //          |
                //           |
                auxTotal          = direction.x + direction.z;
                x                 = direction.x / auxTotal;
                azimuthFromCamera = (1.570796327f * x) + 3.14159265358979323846f;
            }
        }
    }
    
    void CAMERA::updateNormalsRelativeCam() noexcept
    {
        VEC3 temp(focus - position);
        vec3Normalize(&normalForward, &temp);
        normalBackward.x = -normalForward.x;
        normalBackward.y = -normalForward.y;
        normalBackward.z = -normalForward.z;

        normalRight.x = sinf(this->azimuthFromCamera + static_cast<float>(M_PI_2));
        normalRight.y = normalForward.y;
        normalRight.z = cosf(this->azimuthFromCamera + static_cast<float>(M_PI_2));

        normalLeft.x = sinf(this->azimuthFromCamera - static_cast<float>(M_PI_2));
        normalLeft.y = normalBackward.y;
        normalLeft.z = cosf(this->azimuthFromCamera - static_cast<float>(M_PI_2));

        vec3Cross(&normalUp, &normalForward, &normalRight);
    }
    
    void CAMERA::updateCam(const bool is3d, const float width, const float height)
    {
		if (is3d)
        {
			VEC3 perfectPosition(this->position);
			VEC3 perfectFocus(this->focus);
			if (perfectPixel)
			{
				perfectPosition.x = std::round(this->position.x);
				perfectPosition.y = std::round(this->position.y);
				perfectPosition.z = std::round(this->position.z);

				perfectFocus.x = std::round(this->focus.x);
				perfectFocus.y = std::round(this->focus.y);
				perfectFocus.z = std::round(this->focus.z);
			}
            static VEC3 zero(0, 0, 0);
            const float    aspect = width / height;
            const float    scale  = 1.0f / tanf(this->angleOfView * 0.5f * static_cast<float>(M_PI / 180.0f));
            MatrixPerspectiveFovLH(&this->matrixProj, scale, aspect, zNear, zFar);
            MatrixLookAtLH(&this->matrixView, &perfectPosition, &perfectFocus, &this->up);
            MatrixLookAtLH(&this->matrixViewRayCastInverse, &zero, &this->normalForward, &this->up);
            MatrixMultiply(&this->matrixPerspective, &this->matrixView, &this->matrixProj);
            MatrixInverse(&this->matrixViewRayCastInverse, nullptr, &this->matrixViewRayCastInverse);
        }
        else
        {
			VEC2 perfectPosition(this->position2d);
			if (perfectPixel)
			{
				perfectPosition.x = std::round(this->position2d.x);
				perfectPosition.y = std::round(this->position2d.y);
			}
            const VEC3           posCam(-perfectPosition.x, -perfectPosition.y, 100);
            static const VEC3 angleDefault(0, 0, 0);
            MatrixIdentity(&this->matrixView2d);
            MatrixTranslationRotationScale(&this->matrixView2d, &posCam, &angleDefault, &this->scale2d);
            MatrixOrthoLH(&matrixOrtho, width, height, zNear, zFar);
            MatrixMultiply(&this->matrixPerspective2d, &matrixView2d, &matrixOrtho);
        }
    }
}
