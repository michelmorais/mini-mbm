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


#ifdef USE_DEPRECATED_2_MINOR

#include <deprecated.h>
#include <renderizable.h>
#include <header-mesh.h>
#include <cstring>
#include <cfloat>
#include <util-interface.h>
#include <shapes.h>


namespace deprecated_mbm
{

    DETAIL_HEADER_SPRITE::DETAIL_HEADER_SPRITE() noexcept
    {
        totalDetail   = 0;
        zoomEditor[0] = 1.0f;
        zoomEditor[1] = 1.0f;
    }

    DETAIL_SPRITE::DETAIL_SPRITE() noexcept
    {
        totalSubset       = 0;
        offSetPosition[0] = 0;
        offSetPosition[1] = 0;
    }

    DETAIL_SPRITE_SUBSET::DETAIL_SPRITE_SUBSET() noexcept
    {
        type        = 1;
        isBreak     = 0;
        lenBounding = 0;
    }

    uint32_t getSizeInfoColision(const uint32_t                sizePolygnon,
                                           const TYPE_SUBSET_BOUNDING_SPRITE currentType) noexcept
    {
        if (sizePolygnon)
        {
            switch (currentType)
            {
                case FRAME_RECTANGLE: // criamos tres informações indicando largura, altura e o centro
                {
                    return 2;
                }
                case FRAME_CIRCLE: // criamos duas informações indicando o raio do circulo e o centro
                {
                    return 2;
                }
                case FRAME_TRIANGLE: // criamos 3 pontos de informação indicando o triangulo
                {
                    return 3;
                }
                case FRAME_POLYGON: // Criamos informações para guardar coordenadas do poligono
                {
                    return sizePolygnon;
                }
            }
        }
        return 0;
    }

    BOUNDING_SUBSET_SPRITE::BOUNDING_SUBSET_SPRITE(const uint32_t sizePolygnon, const TYPE_SUBSET_BOUNDING_SPRITE currentType)noexcept
        : sizeInfoColision(getSizeInfoColision(sizePolygnon, currentType)), typeFrame(currentType)
    {
        this->infoColision = nullptr;
        if (sizePolygnon)
        {
            switch (typeFrame)
            {
                case FRAME_RECTANGLE: // criamos uma informação indicando a largura, altura e o centro do retangulo
                {
                    this->infoColision = new mbm::VEC2[2];
                }
                break;
                case FRAME_CIRCLE: // criamos uma informação indicando o raio do circulo
                {
                    this->infoColision = new mbm::VEC2[2];
                }
                break;
                case FRAME_TRIANGLE: // criamos 3 pontos de informação indicando o triangulo
                {
                    this->infoColision =
                        new mbm::VEC2[3 + 1]; //+1 para o bounding como retangulo para varificar se esta no frustum
                }
                break;
                case FRAME_POLYGON: // Criamos informações para guardar coordenadas do poligono
                {
                    this->infoColision =
                        new mbm::VEC2[sizePolygnon + 1]; //+1 para o bounding como retangulo para varificar se esta no frustum
                    this->infoColision[sizePolygnon].x = 0.0f;
                    this->infoColision[sizePolygnon].y = 0.0f;
                }
                break;
            }
            memset(static_cast<void*>(this->infoColision), 0, sizeof(mbm::VEC2) * sizeInfoColision);
        }
    }
    
    BOUNDING_SUBSET_SPRITE::~BOUNDING_SUBSET_SPRITE()
    {
        if (this->infoColision)
            delete[] this->infoColision;
        this->infoColision = nullptr;
    }
    

    BOUNDING_SPRITE::BOUNDING_SPRITE(const uint32_t initialFrame, const uint32_t newIndexAnimation)
    noexcept : indexFrame(initialFrame), indexAnimation(newIndexAnimation), offset(0, 0)
    {
    }
    
    BOUNDING_SPRITE::~BOUNDING_SPRITE()
    {
        for (auto b : lsSubset)
        {
            delete b;
        }
        lsSubset.clear();
    }

    INFO_SUBSET_SPRITE::INFO_SUBSET_SPRITE() noexcept
    {
        type    = 3;
        isBreak = false;
    }

    INFO_FRAME_SPRITE::INFO_FRAME_SPRITE() noexcept : offSet(0,0) 
    {
        
    }
    
    INFO_FRAME_SPRITE::~INFO_FRAME_SPRITE()
    {
        for (auto sub : lsSubset)
        {
            delete sub;
        }
        lsSubset.clear();
    }
    

    INFO_SPRITE::INFO_SPRITE() noexcept
    = default;
    
    INFO_SPRITE::~INFO_SPRITE()
    {
        this->release();
    }
    
    void INFO_SPRITE::release()
    {
        for (auto frame : lsSprite)
        {
            if (frame)
                delete frame;
        }
        lsSprite.clear();

        for (auto b : lsBounding)
        {
            delete b;
        }
        lsBounding.clear();
    }
    
    void INFO_SPRITE::fillPhysicsSprite(mbm::VEC3 *vertex, const uint32_t currentFrame, util::SUBSET *subsetArray, int typeMe,
                                  std::vector<mbm::CUBE *> &lsCube2Convert, std::vector<mbm::SPHERE *> &lsSphere2Convert,
                                  std::vector<mbm::TRIANGLE *> &lsTriangle2Convert)
    {
        if (typeMe == util::TYPE_MESH_SPRITE)
        {
            mbm::VEC2 boundMax(-FLT_MAX, -FLT_MAX);
            mbm::VEC2 boundMin(FLT_MAX, FLT_MAX);
            for (auto bounding : lsBounding)
            {
                if (bounding->indexFrame == currentFrame)
                {
                    INFO_FRAME_SPRITE *infoFrame = lsSprite[bounding->indexFrame];
                    const auto s         = static_cast<const uint32_t>(bounding->lsSubset.size());
                    // Para cada subset guardamos a informação do bounding
                    for (uint32_t j = 0; j < s; ++j)
                    {
                        util::SUBSET *          pSubset       = &subsetArray[j];
                        BOUNDING_SUBSET_SPRITE *sub           = bounding->lsSubset[j];
                        INFO_SUBSET_SPRITE *    infoSubSprite = infoFrame->lsSubset[j];
                        switch (sub->typeFrame)
                        {
                            // Para o sprite guardamos a informação largura e altura do retangulo no indice 0 e centro no
                            // indice 1
                            case FRAME_RECTANGLE:
                            {
                                const uint16_t EC =
                                    infoSubSprite->lsIndexBounds[0] + static_cast<const uint16_t>(pSubset->vertexStart);
                                const uint16_t DC =
                                    infoSubSprite->lsIndexBounds[1] + static_cast<const uint16_t>(pSubset->vertexStart);
                                const uint16_t EB =
                                    infoSubSprite->lsIndexBounds[2] + static_cast<const uint16_t>(pSubset->vertexStart);
                                sub->infoColision[0].x = vertex[DC].x - vertex[EC].x; // width
                                sub->infoColision[0].y = vertex[EC].y - vertex[EB].y; // height

                                sub->infoColision[1].x = ((sub->infoColision[0].x * 0.5f) + vertex[EC].x); // centro x
                                sub->infoColision[1].y = ((sub->infoColision[0].y * 0.5f) + vertex[EB].y); // centro y
                            }
                            break;
                            case FRAME_CIRCLE: // criamos uma informação indicando o raio do circulo
                            {
                                const auto a = static_cast<const uint16_t>(pSubset->vertexStart);
                                const uint16_t b = 1 + static_cast<const uint16_t>(pSubset->vertexStart);
                                mbm::VEC2  p(vertex[b].x - vertex[a].x, vertex[b].y - vertex[a].y);
                                const float              ray = p.length();

                                sub->infoColision[1].x = vertex[a].x; // centro
                                sub->infoColision[1].y = vertex[a].y; // centro

                                if ((vertex[a].x) > boundMax.x)
                                    boundMax.x = (vertex[a].x);
                                if ((vertex[a].y) > boundMax.y)
                                    boundMax.y = vertex[a].y;

                                if ((vertex[a].x) < boundMin.x)
                                    boundMin.x = vertex[a].x;
                                if ((vertex[a].y) < boundMin.y)
                                    boundMin.y         = vertex[a].y;
                                sub->infoColision[0].x = ray;
                                sub->infoColision[0].y = ray;
                            }
                            break;
                            case FRAME_TRIANGLE: // criamos 3 pontos de informação indicando o triangulo
                            {
                                mbm::VEC2 maxB(-999999, -999999);
                                mbm::VEC2 minB(999999, 999999);
                                for (uint32_t k = 0; k < 3; ++k)
                                { // os bounds de um triangulo sao 0,1 e o ultimo indice do vertex.
                                    const uint32_t index =
                                        infoSubSprite->lsIndexBounds[k] + static_cast<const uint16_t>(pSubset->vertexStart);
                                    sub->infoColision[k].x = vertex[index].x;
                                    sub->infoColision[k].y = vertex[index].y;
                                    if (sub->infoColision[k].x > maxB.x)
                                        maxB.x = sub->infoColision[k].x;
                                    if (sub->infoColision[k].y > maxB.y)
                                        maxB.y = sub->infoColision[k].y;

                                    if (sub->infoColision[k].x < minB.x)
                                        minB.x = sub->infoColision[k].x;
                                    if (sub->infoColision[k].y < minB.y)
                                        minB.y = sub->infoColision[k].y;
                                }
                                if (maxB.x > boundMax.x)
                                    boundMax.x = maxB.x;
                                if (maxB.y > boundMax.y)
                                    boundMax.y = maxB.y;

                                if (minB.x < boundMin.x)
                                    boundMin.x = minB.x;
                                if (minB.y < boundMin.y)
                                    boundMin.y = minB.y;
                                if (s > 1 && (j + 1) == s)
                                {
                                    sub->infoColision[sub->sizeInfoColision].x = boundMax.x - boundMin.x;
                                    sub->infoColision[sub->sizeInfoColision].y = boundMax.y - boundMin.y;
                                }
                                else
                                {
                                    sub->infoColision[sub->sizeInfoColision].x = maxB.x - minB.x;
                                    sub->infoColision[sub->sizeInfoColision].y = maxB.y - minB.y;
                                }
                            }
                            break;
                            case FRAME_POLYGON: // Criamos informações para guardar coordenadas do poligono
                            {
                                mbm::VEC2 maxB(-999999, -999999);
                                mbm::VEC2 minB(999999, 999999);
                                if (sub->infoColision)
                                    delete[] sub->infoColision;
                                sub->infoColision     = new mbm::VEC2[pSubset->vertexCount + 1];
                                sub->sizeInfoColision = static_cast<uint32_t>(pSubset->vertexCount);
                                memset(static_cast<void*>(sub->infoColision), 0, sizeof(mbm::VEC2) * (sub->sizeInfoColision + 1));
                                for (int k = 0; k < pSubset->vertexCount; ++k)
                                {
                                    const uint32_t index = static_cast<const uint32_t>(pSubset->vertexStart) + static_cast<uint32_t>(k);
                                    if (vertex[index].x > maxB.x)
                                        maxB.x = vertex[index].x;
                                    if (vertex[index].y > maxB.y)
                                        maxB.y = vertex[index].y;

                                    if (vertex[index].x < minB.x)
                                        minB.x = vertex[index].x;
                                    if (vertex[index].y < minB.y)
                                        minB.y = vertex[index].y;
                                }
                                // info bounding box
                                sub->infoColision[sub->sizeInfoColision].x = maxB.x - minB.x;
                                sub->infoColision[sub->sizeInfoColision].y = maxB.y - minB.y;
                                // nao importa o tipo vamos copiar tudo
                                for (int k = 0; k < pSubset->vertexCount; ++k)
                                {
                                    const uint32_t index = static_cast<uint32_t>(pSubset->vertexStart) + static_cast<uint32_t>(k);
                                    sub->infoColision[k].x   = vertex[index].x;
                                    sub->infoColision[k].y   = vertex[index].y;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            this->convert(currentFrame, lsCube2Convert, lsSphere2Convert, lsTriangle2Convert);
        }
    }
    
    void INFO_SPRITE::fillPhysicsSprite(mbm::VEC3 *vertex, const uint32_t currentFrame,
                                  std::vector<util::SUBSET_DEBUG *> &subsetArray, int typeMe,
                                  std::vector<mbm::CUBE *> &lsCube2Convert, std::vector<mbm::SPHERE *> &lsSphere2Convert,
                                  std::vector<mbm::TRIANGLE *> &lsTriangle2Convert)
    {
        if (typeMe == util::TYPE_MESH_SPRITE)
        {
            mbm::VEC2 boundMax(-FLT_MAX, -FLT_MAX);
            mbm::VEC2 boundMin(FLT_MAX, FLT_MAX);
            for (auto bounding : lsBounding)
            {
                if (bounding->indexFrame == currentFrame)
                {
                    INFO_FRAME_SPRITE *infoFrame = lsSprite[bounding->indexFrame];
                    const auto s         = static_cast<uint32_t>(bounding->lsSubset.size());
                    // Para cada subset guardamos a informação do bounding
                    for (uint32_t j = 0; j < s; ++j)
                    {
                        util::SUBSET_DEBUG *    pSubset       = subsetArray[j];
                        BOUNDING_SUBSET_SPRITE *sub           = bounding->lsSubset[j];
                        INFO_SUBSET_SPRITE *    infoSubSprite = infoFrame->lsSubset[j];
                        switch (sub->typeFrame)
                        {
                            // Para o sprite guardamos a informação largura e altura do retangulo no indice 0 e centro no
                            // indice 1
                            case FRAME_RECTANGLE:
                            {
                                const uint16_t EC =
                                    infoSubSprite->lsIndexBounds[0] + static_cast<const uint16_t>(pSubset->vertexStart);
                                const uint16_t DC =
                                    infoSubSprite->lsIndexBounds[1] + static_cast<const uint16_t>(pSubset->vertexStart);
                                const uint16_t EB =
                                    infoSubSprite->lsIndexBounds[2] + static_cast<const uint16_t>(pSubset->vertexStart);
                                sub->infoColision[0].x = vertex[DC].x - vertex[EC].x; // width
                                sub->infoColision[0].y = vertex[EC].y - vertex[EB].y; // height

                                sub->infoColision[1].x = ((sub->infoColision[0].x * 0.5f) + vertex[EC].x); // centro x
                                sub->infoColision[1].y = ((sub->infoColision[0].y * 0.5f) + vertex[EB].y); // centro y
                            }
                            break;
                            case FRAME_CIRCLE: // criamos uma informação indicando o raio do circulo
                            {
                                const auto a = static_cast<const uint16_t>(pSubset->vertexStart);
                                const uint16_t b = 1 + static_cast<const uint16_t>(pSubset->vertexStart);
                                mbm::VEC2                     p(vertex[b].x - vertex[a].x, vertex[b].y - vertex[a].y);
                                const float              ray = p.length();

                                sub->infoColision[1].x = vertex[a].x; // centro
                                sub->infoColision[1].y = vertex[a].y; // centro

                                if ((vertex[a].x) > boundMax.x)
                                    boundMax.x = (vertex[a].x);
                                if ((vertex[a].y) > boundMax.y)
                                    boundMax.y = vertex[a].y;

                                if ((vertex[a].x) < boundMin.x)
                                    boundMin.x = vertex[a].x;
                                if ((vertex[a].y) < boundMin.y)
                                    boundMin.y         = vertex[a].y;
                                sub->infoColision[0].x = ray;
                                sub->infoColision[0].y = ray;
                            }
                            break;
                            case FRAME_TRIANGLE: // criamos 3 pontos de informação indicando o triangulo
                            {
                                mbm::VEC2 maxB(-999999, -999999);
                                mbm::VEC2 minB(999999, 999999);
                                for (uint32_t k = 0; k < 3; ++k)
                                { // os bounds de um triangulo sao 0,1 e o ultimo indice do vertex.
                                    const uint32_t index =
                                        infoSubSprite->lsIndexBounds[k] + static_cast<const uint16_t>(pSubset->vertexStart);
                                    sub->infoColision[k].x = vertex[index].x;
                                    sub->infoColision[k].y = vertex[index].y;
                                    if (sub->infoColision[k].x > maxB.x)
                                        maxB.x = sub->infoColision[k].x;
                                    if (sub->infoColision[k].y > maxB.y)
                                        maxB.y = sub->infoColision[k].y;

                                    if (sub->infoColision[k].x < minB.x)
                                        minB.x = sub->infoColision[k].x;
                                    if (sub->infoColision[k].y < minB.y)
                                        minB.y = sub->infoColision[k].y;
                                }
                                if (maxB.x > boundMax.x)
                                    boundMax.x = maxB.x;
                                if (maxB.y > boundMax.y)
                                    boundMax.y = maxB.y;

                                if (minB.x < boundMin.x)
                                    boundMin.x = minB.x;
                                if (minB.y < boundMin.y)
                                    boundMin.y = minB.y;
                                if (s > 1 && (j + 1) == s)
                                {
                                    sub->infoColision[sub->sizeInfoColision].x = boundMax.x - boundMin.x;
                                    sub->infoColision[sub->sizeInfoColision].y = boundMax.y - boundMin.y;
                                }
                                else
                                {
                                    sub->infoColision[sub->sizeInfoColision].x = maxB.x - minB.x;
                                    sub->infoColision[sub->sizeInfoColision].y = maxB.y - minB.y;
                                }
                            }
                            break;
                            case FRAME_POLYGON: // Criamos informações para guardar coordenadas do poligono
                            {
                                mbm::VEC2 maxB(-999999, -999999);
                                mbm::VEC2 minB(999999, 999999);
                                if (sub->infoColision)
                                    delete[] sub->infoColision;
                                sub->infoColision     = new mbm::VEC2[pSubset->vertexCount + 1];
                                sub->sizeInfoColision = static_cast<uint32_t>(pSubset->vertexCount);
                                memset(static_cast<void*>(sub->infoColision), 0, sizeof(mbm::VEC2) * (sub->sizeInfoColision + 1));
                                for (int k = 0; k < pSubset->vertexCount; ++k)
                                {
                                    const auto index = static_cast<uint32_t>(pSubset->vertexStart + k);
                                    if (vertex[index].x > maxB.x)
                                        maxB.x = vertex[index].x;
                                    if (vertex[index].y > maxB.y)
                                        maxB.y = vertex[index].y;

                                    if (vertex[index].x < minB.x)
                                        minB.x = vertex[index].x;
                                    if (vertex[index].y < minB.y)
                                        minB.y = vertex[index].y;
                                }
                                // info bounding box
                                sub->infoColision[sub->sizeInfoColision].x = maxB.x - minB.x;
                                sub->infoColision[sub->sizeInfoColision].y = maxB.y - minB.y;
                                // nao importa o tipo vamos copiar tudo
                                for (int k = 0; k < pSubset->vertexCount; ++k)
                                {
                                    const auto index = static_cast<uint32_t>(pSubset->vertexStart + k);
                                    sub->infoColision[k].x   = vertex[index].x;
                                    sub->infoColision[k].y   = vertex[index].y;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            this->convert(currentFrame, lsCube2Convert, lsSphere2Convert, lsTriangle2Convert);
        }
    }
    
    void INFO_SPRITE::fillOldPhysicsSprite_2(int typeMe, util::INFO_ANIMATION &infoAnimation, int totalFrames) // version <= 2
    {
        // preenchemos as possiveis fisicas caso sprite
        if (typeMe == util::TYPE_MESH_SPRITE)
        {
            for (uint32_t i = 0; i < infoAnimation.lsHeaderAnim.size(); ++i)
            {
                util::INFO_ANIMATION::INFO_HEADER_ANIM *infoAnim = infoAnimation.lsHeaderAnim[i];
                util::HEADER_ANIMATION *     head     = infoAnim->headerAnim;
                if (head && head->initialFrame < totalFrames && 
                    this->lsSprite[static_cast<std::vector<INFO_FRAME_SPRITE *>::size_type>(head->initialFrame)])
                {
                    
                    INFO_FRAME_SPRITE *infoFrame = this->lsSprite[static_cast<std::vector<INFO_FRAME_SPRITE *>::size_type>(head->initialFrame)];
                    auto   bounding  = new BOUNDING_SPRITE(static_cast<uint32_t>(head->initialFrame), i);
                    if (head->initialFrame < static_cast<int>(this->lsSprite.size()))
                        bounding->offset = this->lsSprite[static_cast<std::vector<INFO_FRAME_SPRITE *>::size_type>(head->initialFrame)]->offSet;
                    this->lsBounding.push_back(bounding);
                    for (auto subset : infoFrame->lsSubset)
                    {
                        auto type   = static_cast<TYPE_SUBSET_BOUNDING_SPRITE>(subset->type);
                        if (subset->isBreak)
                            type = FRAME_POLYGON;
                        auto subBounding = new BOUNDING_SUBSET_SPRITE(static_cast<uint32_t>(subset->lsIndexBounds.size()), type);
                        bounding->lsSubset.push_back(subBounding);
                    }
                }
            }
        }
    }
    
    bool INFO_SPRITE::readBoundingSprite(FILE *fp, const char *fileNamePath, float *xZoomEditor, float *yZoomEditor)
    {
        deprecated_mbm::DETAIL_HEADER_SPRITE detailHeader;
        if (!fread(&detailHeader, sizeof(deprecated_mbm::DETAIL_HEADER_SPRITE), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_SPRITE [%s]", fileNamePath);
        *xZoomEditor = detailHeader.zoomEditor[0];
        *yZoomEditor = detailHeader.zoomEditor[1];
        for (int i = 0; i < detailHeader.totalDetail; ++i)
        {
            deprecated_mbm::DETAIL_SPRITE detailSprite;
            if (!fread(&detailSprite, sizeof(deprecated_mbm::DETAIL_SPRITE), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_SPRITE [%s]", fileNamePath);
            auto infoFrameSprite = new deprecated_mbm::INFO_FRAME_SPRITE();
            lsSprite.push_back(infoFrameSprite);
            infoFrameSprite->offSet.x = detailSprite.offSetPosition[0];
            infoFrameSprite->offSet.y = detailSprite.offSetPosition[1];
            for (int j = 0; j < detailSprite.totalSubset; ++j)
            {
                deprecated_mbm::DETAIL_SPRITE_SUBSET detailSubset;
                if (!fread(&detailSubset, sizeof(deprecated_mbm::DETAIL_SPRITE_SUBSET), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_SPRITE_SUBSET [%s]", fileNamePath);
                auto infoSubset = new deprecated_mbm::INFO_SUBSET_SPRITE();
                infoFrameSprite->lsSubset.push_back(infoSubset);
                infoSubset->type    = detailSubset.type;
                infoSubset->isBreak = detailSubset.isBreak ? true : false;
                if (detailSubset.lenBounding)
                {
                    auto indexTemp = new uint16_t[detailSubset.lenBounding];
                    if (!fread(indexTemp, sizeof(uint16_t) * static_cast<unsigned long>(detailSubset.lenBounding), 1, fp))
                    {
                        delete [] indexTemp;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load index boundig sprite [%s]", fileNamePath);
                    }
                    for (int k = 0; k < detailSubset.lenBounding; ++k)
                    {
                        infoSubset->lsIndexBounds.push_back(indexTemp[k]);
                    }
                    delete[] indexTemp;
                }
            }
        }
        return true;
    }
    
    void INFO_SPRITE::convert(uint32_t indexFrame, std::vector<mbm::CUBE *> &lsCube2Convert,
                 std::vector<mbm::SPHERE *> &lsSphere2Convert, std::vector<mbm::TRIANGLE *> &lsTriangle2Convert)
    {
        if (indexFrame == 0 && indexFrame < this->lsBounding.size() && indexFrame < this->lsSprite.size())
        {
            deprecated_mbm::INFO_FRAME_SPRITE *infoFrame  = this->lsSprite[indexFrame];
            deprecated_mbm::BOUNDING_SPRITE *  bounding   = this->lsBounding[indexFrame];
            const std::vector<BOUNDING_SUBSET_SPRITE *>::size_type sizeSubset = infoFrame->lsSubset.size();
            for (std::vector<BOUNDING_SUBSET_SPRITE *>::size_type j = 0; j < sizeSubset; ++j)
            {
                deprecated_mbm::BOUNDING_SUBSET_SPRITE * sub = bounding->lsSubset[j];
                switch (sub->typeFrame)
                {
                    case deprecated_mbm::FRAME_RECTANGLE: // Temos uma informação indicando a largura e altura do sprite
                    {
                        auto cube = new mbm::CUBE();
                        lsCube2Convert.push_back(cube);
                        cube->halfDim.x   = sub->infoColision[0].x * 0.5f;
                        cube->halfDim.y   = sub->infoColision[0].y * 0.5f;
                        cube->halfDim.z   = 1.0f;
                        cube->absCenter.x = sub->infoColision[1].x;
                        cube->absCenter.y = sub->infoColision[1].y;
                    }
                    break;
                    case deprecated_mbm::FRAME_CIRCLE: // Temos uma informação indicando o raio do circulo
                    {
                        auto sphere = new mbm::SPHERE();
                        lsSphere2Convert.push_back(sphere);
                        sphere->ray          = sub->infoColision[0].x;
                        sphere->absCenter[0] = sub->infoColision[1].x;
                        sphere->absCenter[1] = sub->infoColision[1].y;
                        sphere->absCenter[2] = 0;
                    }
                    break;
                    case deprecated_mbm::FRAME_TRIANGLE: // Temos 3 pontos de informação indicando o triangulo. porem
                                                         // informamos o bounding que é menos calculo e mais preciso
                        {
                            auto triangle = new mbm::TRIANGLE();
                            lsTriangle2Convert.push_back(triangle);
                            triangle->point[0].x = sub->infoColision[0].x;
                            triangle->point[0].y = sub->infoColision[0].y;
                            triangle->point[0].z = 0.0f;

                            triangle->point[1].x = sub->infoColision[1].x;
                            triangle->point[1].y = sub->infoColision[1].y;
                            triangle->point[1].z = 0.0f;

                            triangle->point[2].x = sub->infoColision[2].x;
                            triangle->point[2].y = sub->infoColision[2].y;
                            triangle->point[2].z = 0.0f;
                        }
                        break;
                    case deprecated_mbm::FRAME_POLYGON: // Temos informações para cada coordenadas do poligono porem o
                                                        // ultimo indice no array indica a largura e altura do bounding.
                        { // para poligono dynamic a box2d limita em até 8 pontos ou (b2_maxPolygonVertices) e utiliza o
                            // algoritimo Gift wrapping algorithm para
                            // montar o poligono... entrão é imprevisivel sua fisica...
                            // ao invez disso a engine converte tudo em triangulo entao vai cominando os diversos
                            // triangulos
                            deprecated_mbm::INFO_SUBSET_SPRITE *infoSubset = infoFrame->lsSubset[j];
                            switch (infoSubset->type)
                            {
                                case deprecated_mbm::FRAME_RECTANGLE:
                                {
                                    const uint32_t s = sub->sizeInfoColision;
                                    for (uint32_t k = 0; (k + 3) <= s; k += 3)
                                    {
                                        const uint32_t a        = k;
                                        const uint32_t b        = k + 1;
                                        const uint32_t c        = k + 2;
                                        auto     triangle = new mbm::TRIANGLE();
                                        lsTriangle2Convert.push_back(triangle);
                                        triangle->point[0].x = sub->infoColision[a].x;
                                        triangle->point[0].y = sub->infoColision[a].y;
                                        triangle->point[0].z = 0.0f;

                                        triangle->point[1].x = sub->infoColision[b].x;
                                        triangle->point[1].y = sub->infoColision[b].y;
                                        triangle->point[1].z = 0.0f;

                                        triangle->point[2].x = sub->infoColision[c].x;
                                        triangle->point[2].y = sub->infoColision[c].y;
                                        triangle->point[2].z = 0.0f;
                                    }
                                }
                                break;
                                case deprecated_mbm::FRAME_CIRCLE: // a engine converte tudo em triangulo entao vai
                                                                   // cominando
                                                                   // os diversos triangulos
                                    {
                                        const uint32_t s = sub->sizeInfoColision;
                                        for (uint32_t k = 1; k < s; ++k)
                                        {
                                            const uint32_t a = k;
                                            const uint32_t b = (k + 1) == s ? 1 : k + 1; // o ultimo é o primeiro

                                            auto triangle = new mbm::TRIANGLE();
                                            lsTriangle2Convert.push_back(triangle);
                                            triangle->point[0].x = sub->infoColision[0].x;
                                            triangle->point[0].y = sub->infoColision[0].y;
                                            triangle->point[0].z = 0.0f;

                                            triangle->point[1].x = sub->infoColision[a].x;
                                            triangle->point[1].y = sub->infoColision[a].y;
                                            triangle->point[1].z = 0.0f;

                                            triangle->point[2].x = sub->infoColision[b].x;
                                            triangle->point[2].y = sub->infoColision[b].y;
                                            triangle->point[2].z = 0.0f;
                                        }
                                    }
                                    break;
                                case deprecated_mbm::FRAME_TRIANGLE: // Temos tudo em triangulo entao vai cominando
                                {
                                    /* Exemplo de organizacao:
                                    um triangulo com 5 elementos tera 15 indices
                                        0 é a origem e os seguintes são 1 e 1 + 1
                                    vejamos:
                                    0, 1, 2, - 0, 2, 3, - 0, 3, 4, - 0, 4, 5, - 0, 5, 6.*/
                                    const uint32_t s = sub->sizeInfoColision - 1;
                                    for (uint32_t k = 1; k < s; ++k)
                                    {
                                        const uint32_t a        = k;
                                        const uint32_t b        = a + 1;
                                        auto     triangle = new mbm::TRIANGLE();
                                        lsTriangle2Convert.push_back(triangle);
                                        triangle->point[0].x = sub->infoColision[0].x;
                                        triangle->point[0].y = sub->infoColision[0].y;
                                        triangle->point[0].z = 0.0f;

                                        triangle->point[1].x = sub->infoColision[a].x;
                                        triangle->point[1].y = sub->infoColision[a].y;
                                        triangle->point[1].z = 0.0f;

                                        triangle->point[2].x = sub->infoColision[b].x;
                                        triangle->point[2].y = sub->infoColision[b].y;
                                        triangle->point[2].z = 0.0f;
                                    }
                                }
                                break;
                                case deprecated_mbm::FRAME_POLYGON:
                                {
                                    // impossible uhahuah
                                }
                                break;
                            }
                        }
                        break;
                }
            }
        }
    }



    bool fillAnimation_1(const char *fileNamePath, FILE *fp, util::HEADER_MESH *headerMesh,
                           util::INFO_ANIMATION *infoAnimation)
    {
        struct INFO_MUSIC_EFFECT
        {
            char *fileNameMusic;
            bool  loop;
            
            INFO_MUSIC_EFFECT(const int len)
            {
                loop = false;
                if (len)
                    this->fileNameMusic = new char[len];
                else
                    this->fileNameMusic = nullptr;
            }
            
            ~INFO_MUSIC_EFFECT()
            {
                this->release();
            }
            
            void release()
            {
                if (this->fileNameMusic)
                    delete[] this->fileNameMusic;
                this->fileNameMusic = nullptr;
            }
            
        };
        
        struct old_HEADER_ANIMATION_MBM // Header das animações. contem os nomes e indicam quando começam e quando terminam
        {
            char  nameAnimation[32]; // 32 bytes para o nome da animação (31 + null)
            int   initialFrame;      // Frame inicial para esta animação
            int   finalFrame;        // Frame final para esta animação
            float timeBetweenFrame;  // Tempo entre frames da animação
            int   typeAnimation;     // Tipo da animação
            int   lenMusicFileName;  // Comprimento da string do arquivo de musica ou efeito sonoro para a animação.
            int   loopMusic;         // Flag que indica se esta em loop a musica ou efeito sonoro para a animação.
            int   lenPS; // Efeito Pixel shader. 0 nenhum. Indica o tamanho da string + nullptr do arquivo Pixel shader.
            int   lenVS; // Efeito Vertex shader. 0 nenhum. Indica o tamanho da string + nullptr do arquivo Vertex shader.
            
            old_HEADER_ANIMATION_MBM() noexcept
            {
                memset(nameAnimation, 0, sizeof(nameAnimation));
                initialFrame     = 0;
                finalFrame       = 0;
                timeBetweenFrame = 0.0f;
                typeAnimation    = 0;
                lenPS            = 0;
                lenVS            = 0;
                lenMusicFileName = 0;
                loopMusic        = 0;
            }
        };
        
        struct old_HEADER_DATA_SHADER_INFO
        {
            int       sizeArrayVar;  // Tamanho do array das variaveis do Shader
            float     timeAnimation; // Tempo da animação
            int16_t typeAnimation; // 0 - 6
            int16_t
                lenTextureStage2; // Quando ha textura no segundo estaigio esta variavel informa o tamanho da string + nullptr
            //--------------------------------------------------------------------------------------------
            old_HEADER_DATA_SHADER_INFO() noexcept
            {
                sizeArrayVar     = 0;
                typeAnimation    = 0;
                lenTextureStage2 = 0;
                timeAnimation    = 0;
            }
        };
        
        struct old_SHADER_INFO
        {
            char *                      fileNameShader;
            old_HEADER_DATA_SHADER_INFO infoShader;
            //--------------------------------------------------------------------------------------------
            old_SHADER_INFO(const uint16_t sizeFileName)
            {
                if (sizeFileName)
                    fileNameShader = new char[sizeFileName];
                else
                    fileNameShader = nullptr;
            }
            //--------------------------------------------------------------------------------------------
            ~old_SHADER_INFO()
            {
                if (fileNameShader)
                    delete[] fileNameShader;
                fileNameShader = nullptr;
            }
            //--------------------------------------------------------------------------------------------
        };
        
        struct old_INFO_SHADER_PS_VS
        {
            
            char * typeVars;
            float *min;
            float *max;
            char * textureFileNameStage2; // Textura stage 2
            int    lenVars;
            
            old_INFO_SHADER_PS_VS(const int sizeArrayFloatBy4, const int sizeFileNameTexture)
            {
                min                   = nullptr;
                max                   = nullptr;
                textureFileNameStage2 = nullptr;
                if (sizeArrayFloatBy4)
                {
                    min = new float[sizeArrayFloatBy4];
                    max = new float[sizeArrayFloatBy4];
                }
                if (sizeArrayFloatBy4)
                    lenVars = sizeArrayFloatBy4 / 4;
                else
                    lenVars = 0;
                if (lenVars)
                    typeVars = new char[lenVars];
                else
                    typeVars = nullptr;
                if (sizeFileNameTexture)
                    textureFileNameStage2 = new char[sizeFileNameTexture];
            }
            
            ~old_INFO_SHADER_PS_VS()
            {
                if (min)
                    delete[] min;
                if (max)
                    delete[] max;
                if (textureFileNameStage2)
                    delete[] textureFileNameStage2;
                if (typeVars)
                    delete[] typeVars;

                min                   = nullptr;
                max                   = nullptr;
                typeVars              = nullptr;
                textureFileNameStage2 = nullptr;
            }
        };
        
        // Versão 1.0
        // 4 header anim -- Todas as animações -----------------------------------------------------------
        // this->infoAnimation.totalAnimation   =   headerMesh->totalAnimation;
        for (int i = 0; i < headerMesh->totalAnimation; ++i)
        {
            old_HEADER_ANIMATION_MBM headerAnim;
            if (!fread(&headerAnim, sizeof(old_HEADER_ANIMATION_MBM), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load animation 's mesh [%s]", fileNamePath);
            auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            auto      realAnim = new util::HEADER_ANIMATION();
            infoAnimation->lsHeaderAnim.push_back(infoHead);
            infoHead->headerAnim = realAnim;

            realAnim->finalFrame       = headerAnim.finalFrame;
            realAnim->initialFrame     = headerAnim.initialFrame;
            realAnim->timeBetweenFrame = headerAnim.timeBetweenFrame;
            realAnim->hasShaderEffect  = (headerAnim.lenPS || headerAnim.lenVS) ? 1 : 0;
            realAnim->typeAnimation    = headerAnim.typeAnimation;
            memcpy(realAnim->nameAnimation, headerAnim.nameAnimation, sizeof(realAnim->nameAnimation));
            // Efeito sonoro da animação ------------------------------------------------------------------------
            if (headerAnim.lenMusicFileName)
            {
                INFO_MUSIC_EFFECT infoMusic(headerAnim.lenMusicFileName);
                if (!fread(infoMusic.fileNameMusic, static_cast<unsigned long>(headerAnim.lenMusicFileName), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load o nome da musica efeito sonoro da animacao [%s]",
                                             fileNamePath);
            }
            util::INFO_FX *infoStepShader = nullptr;
            if (headerAnim.lenPS || headerAnim.lenVS)
            {
                infoStepShader             = new util::INFO_FX();
                infoStepShader->blendOperation = 1;
                infoHead->effetcShader = infoStepShader;
            }
            // Pixel Shader -------------------------------------------------------------------------------
            if (headerAnim.lenPS)
            {
                old_SHADER_INFO dataInfo(static_cast<unsigned short>(headerAnim.lenPS));
                if (!fread(dataInfo.fileNameShader, static_cast<std::size_t>(headerAnim.lenPS), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);
                if (!fread(&dataInfo.infoShader, sizeof(old_HEADER_DATA_SHADER_INFO), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info header shader [%s]", fileNamePath);
                old_INFO_SHADER_PS_VS    dataMinMax(dataInfo.infoShader.sizeArrayVar, dataInfo.infoShader.lenTextureStage2);
                auto dataInfoNew = new util::INFO_SHADER_DATA(
                    dataInfo.infoShader.sizeArrayVar, headerAnim.lenPS, dataInfo.infoShader.lenTextureStage2);
                infoStepShader->dataPS     = dataInfoNew; //-V522
                dataInfoNew->timeAnimation = dataInfo.infoShader.timeAnimation;
                dataInfoNew->typeAnimation = dataInfo.infoShader.typeAnimation;
                strcpy(dataInfoNew->fileNameShader, dataInfo.fileNameShader);

                if (dataInfo.infoShader.sizeArrayVar)
                {
                    if (!fread(dataMinMax.typeVars, static_cast<std::size_t>(dataInfoNew->lenVars), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    if (!fread(dataMinMax.min, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    if (!fread(dataMinMax.max, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    memcpy(dataInfoNew->typeVars, dataMinMax.typeVars, static_cast<std::size_t>(dataMinMax.lenVars));
                    memcpy(dataInfoNew->min, dataMinMax.min, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar));
                    memcpy(dataInfoNew->max, dataMinMax.max, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar));
                }
                else
                {
                    dataInfoNew->fileNameShader = nullptr;
                    dataInfoNew->max            = nullptr;
                    dataInfoNew->min            = nullptr;
                }
                if (dataInfo.infoShader.lenTextureStage2)
                {
                    if (!fread(dataMinMax.textureFileNameStage2,static_cast<std::size_t>(dataInfo.infoShader.lenTextureStage2), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    strcpy(dataInfoNew->fileNameTextureStage2, dataMinMax.textureFileNameStage2);
                }
                else
                {
                    dataInfoNew->fileNameTextureStage2 = nullptr;
                }
            }
            // Vertex Shader -------------------------------------------------------------------------------
            if (headerAnim.lenVS)
            {
                old_SHADER_INFO dataInfo(static_cast<unsigned short>(headerAnim.lenVS));
                if (!fread(dataInfo.fileNameShader, static_cast<std::size_t>(headerAnim.lenVS), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);
                if (!fread(&dataInfo.infoShader, sizeof(old_HEADER_DATA_SHADER_INFO), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header info shader [%s]", fileNamePath);
                old_INFO_SHADER_PS_VS    dataMinMax(dataInfo.infoShader.sizeArrayVar, dataInfo.infoShader.lenTextureStage2);
                auto dataInfoNew = new util::INFO_SHADER_DATA(
                    dataInfo.infoShader.sizeArrayVar, headerAnim.lenVS, dataInfo.infoShader.lenTextureStage2);
                infoStepShader->dataVS             = dataInfoNew;
                dataInfoNew->timeAnimation         = dataInfo.infoShader.timeAnimation;
                dataInfoNew->typeAnimation         = dataInfo.infoShader.typeAnimation;
                dataInfoNew->fileNameTextureStage2 = nullptr;
                strcpy(dataInfoNew->fileNameShader, dataInfo.fileNameShader);
                if (dataInfo.infoShader.sizeArrayVar)
                {
                    if (!fread(dataMinMax.typeVars, static_cast<std::size_t>(dataInfoNew->lenVars), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    if (!fread(dataMinMax.min, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    if (!fread(dataMinMax.max, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    memcpy(dataInfoNew->typeVars, dataMinMax.typeVars, static_cast<std::size_t>(dataMinMax.lenVars));
                    memcpy(dataInfoNew->min, dataMinMax.min, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar));
                    memcpy(dataInfoNew->max, dataMinMax.max, sizeof(float) * static_cast<std::size_t>(dataInfo.infoShader.sizeArrayVar));
                }
                else
                {
                    dataInfoNew->fileNameShader = nullptr;
                    dataInfoNew->max            = nullptr;
                    dataInfoNew->min            = nullptr;
                }
            }
        }
        return true;
    }
};
#endif

