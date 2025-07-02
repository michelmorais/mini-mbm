
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

#ifndef CAMERA_H
#define CAMERA_H

#include "primitives.h"

namespace mbm
{

    class API_IMPL CAMERA
    {
      public:
    
        CAMERA() noexcept;
        void calculateAzimuthFromCamera() noexcept;
        void updateNormalsRelativeCam() noexcept;
        void updateCam(const bool is3d, const float width, const float height);
    
        VEC3 position;   //(3d)
        VEC2 position2d; //(2d)
        VEC3 focus;      // 3d
        VEC3 up;
        MATRIX  matrixViewRayCastInverse; // Utilizado para converter ponto 2d para 3d
    
        MATRIX matrixProj;        // Matriz de projeção (3d)
        MATRIX matrixView;        // Matriz de view (3d)
        MATRIX matrixPerspective; // Matriz perspectiva (matrixProj x matrixView)(3d)
        MATRIX matrixBillboard;   // Matriz para utilização em billboards automaticamente atualizado a cada loop de render
    
        MATRIX matrixOrtho;         // Matriz orthographic (2d)
        MATRIX matrixView2d;        // Matriz de view (2d)
        MATRIX matrixPerspective2d; // Matriz perspectiva (matrixOrtho x matrixView2d)(2d)
    
        VEC3 normalForward;
        VEC3 normalRight;
        VEC3 normalBackward;
        VEC3 normalLeft;
        VEC3 normalUp;

        VEC3 scale2d;
        VEC3 scaleScreen2d;
        VEC2 expectedScreen;
        char    stretch[3];
        float   azimuthFromCamera;
        float   angleOfView;
        float zNear;
        float zFar;
        bool perfectPixel;
    };
}
#endif
