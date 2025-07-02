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

#ifndef GLES_DEPRECATED_H
#define GLES_DEPRECATED_H

#ifdef USE_DEPRECATED_2_MINOR

#include "primitives.h"
#include <vector>
#include <stdio.h>


namespace util 
{
    struct SUBSET;
    struct SUBSET_DEBUG;
    struct INFO_ANIMATION;
    struct HEADER_MESH;
}

namespace mbm 
{
    struct CUBE;
    struct TRIANGLE;
    struct SPHERE;
}

namespace deprecated_mbm
{
    enum TYPE_SUBSET_BOUNDING_SPRITE
    {
        FRAME_RECTANGLE = 1,
        FRAME_CIRCLE    = 2,
        FRAME_TRIANGLE  = 3,
        FRAME_POLYGON   = 4,
    };

    // 2.2.1: HEADER.typeApp == "Sprite mbm"
    struct DETAIL_HEADER_SPRITE // Header detalhes do sprite
    {
        int   totalDetail;   // informa o total de detail deste sprite
        float zoomEditor[2]; // Zoom x e y do editor
        DETAIL_HEADER_SPRITE() noexcept;
    };
    // 2.2.2
    struct DETAIL_SPRITE // detalhes do sprite
    {
        int   totalSubset;       // Total de subset para este frame
        float offSetPosition[2]; // Offset do frame
        DETAIL_SPRITE() noexcept;
    };

    // 2.2.3
    struct DETAIL_SPRITE_SUBSET // detalhes do subset
    {
        short type;    // Tipo do subset. 1: retangulo, 2 circulo, 3 Triângulo.
        short isBreak; // Indica se a forma esta livre para modificações, por exmplo o circulo ja foi esticado deixando de ser
                       // um circulo.
        int lenBounding; // Quantidade de indices das fronteiras
        DETAIL_SPRITE_SUBSET() noexcept;
    };

    uint32_t getSizeInfoColision(const uint32_t                sizePolygnon,
                                           const TYPE_SUBSET_BOUNDING_SPRITE currentType) noexcept;
    struct BOUNDING_SUBSET_SPRITE // para cada subset de uma animação havera um bounding de sprite
    {
        uint32_t                      sizeInfoColision; // Tamanho do array de pontos
        const TYPE_SUBSET_BOUNDING_SPRITE typeFrame;        // 1 retangulo, 2 circulo 3 triangulo 4 poligono
        mbm::VEC2 *                            infoColision;     // pontos que formam a colisão (carregado automaticamente).
        BOUNDING_SUBSET_SPRITE(const uint32_t sizePolygnon, const TYPE_SUBSET_BOUNDING_SPRITE currentType) noexcept;
        ~BOUNDING_SUBSET_SPRITE();
    
    };
    struct BOUNDING_SPRITE
    {
        const uint32_t                    indexFrame;
        const uint32_t                    indexAnimation;
        mbm::VEC2                             offset;
        std::vector<BOUNDING_SUBSET_SPRITE *> lsSubset;
        BOUNDING_SPRITE(const uint32_t initialFrame, const uint32_t newIndexAnimation) noexcept;
        ~BOUNDING_SPRITE();
    };

    struct INFO_SUBSET_SPRITE
    {
        std::vector<uint16_t> lsIndexBounds;
        int                             type;
        bool                            isBreak;
        INFO_SUBSET_SPRITE() noexcept;
    };

    struct INFO_FRAME_SPRITE
    {
        std::vector<INFO_SUBSET_SPRITE *> lsSubset;
        mbm::VEC2                              offSet;
        INFO_FRAME_SPRITE() noexcept;
        ~INFO_FRAME_SPRITE();
    };

    struct INFO_SPRITE
    {
        std::vector<INFO_FRAME_SPRITE *> lsSprite;   // Informações dos frames (1 por farme)
        std::vector<BOUNDING_SPRITE *>   lsBounding; // boundig de colisão para o sprite (tera um por animação)
        INFO_SPRITE() noexcept;
        ~INFO_SPRITE();
        void release();
        void fillPhysicsSprite(mbm::VEC3 *vertex, const uint32_t currentFrame, util::SUBSET *subsetArray, int typeMe,
                                      std::vector<mbm::CUBE *> &lsCube2Convert, std::vector<mbm::SPHERE *> &lsSphere2Convert,
                                      std::vector<mbm::TRIANGLE *> &lsTriangle2Convert);
        void fillPhysicsSprite(mbm::VEC3 *vertex, const uint32_t currentFrame,
                                      std::vector<util::SUBSET_DEBUG *> &subsetArray, int typeMe,
                                      std::vector<mbm::CUBE *> &lsCube2Convert, std::vector<mbm::SPHERE *> &lsSphere2Convert,
                                      std::vector<mbm::TRIANGLE *> &lsTriangle2Convert);
        void fillOldPhysicsSprite_2(int typeMe, util::INFO_ANIMATION &infoAnimation, int totalFrames);
        bool readBoundingSprite(FILE *fp, const char *fileNamePath, float *xZoomEditor, float *yZoomEditor);
      private:
        void convert(uint32_t indexFrame, std::vector<mbm::CUBE *> &lsCube2Convert,
                     std::vector<mbm::SPHERE *> &lsSphere2Convert, std::vector<mbm::TRIANGLE *> &lsTriangle2Convert);
    };

    bool fillAnimation_1(const char *fileNamePath, FILE *fp, util::HEADER_MESH *headerMesh,
                           util::INFO_ANIMATION *infoAnimation);
};

#endif
#endif
