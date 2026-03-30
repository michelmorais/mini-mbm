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

#include <background.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <util.h>
#include <header-mesh.h>
#include <font.h>
#include <core_mbm/scene.h>

namespace mbm
{

    BACKGROUND::BACKGROUND(const SCENE *scene, const bool isBackGround3d)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_BACKGROUND, isBackGround3d, false)
    {
        this->isMajorScale      = true;
        this->howFar3d          = 800.0f;
        this->spaceXCharacter   = 0;
        this->spaceYCharacter   = 0;
        this->buffer            = nullptr;
        this->texture           = nullptr;
        this->type              = util::TYPE_MESH_UNKNOWN;
        this->mesh              = nullptr;
        this->lasIndexAnimation = 0xffffffff;
        mbm::DEVICE* device     = mbm::DEVICE::getInstance();
        this->position.z        = device->orderRender.getNextZOrderControl2dBackground();
        this->isFrontGround     = false;
        device->addRenderizable(this);
    }
    
    BACKGROUND::~BACKGROUND()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        this->release();
    }
    
    void BACKGROUND::release()
    {
        this->releaseAnimation();
        if (this->buffer)
            delete this->buffer;
        this->buffer                = nullptr;
        this->mesh                  = nullptr;
        this->indexCurrentAnimation = 0;
        this->text.clear();
    }
    
    const char * BACKGROUND::getFileName()
    {
        switch (this->type)
        {
            case util::TYPE_MESH_TEXTURE:
            {
                if (this->texture)
                    return this->texture->getFileNameTexture();
            }
            break;
            case util::TYPE_MESH_FONT:
            case util::TYPE_MESH_SPRITE:
            case util::TYPE_MESH_TILE_MAP:
            case util::TYPE_MESH_3D:
            {
                if (this->mesh)
                    return this->mesh->getFilenameMesh();
            }
            break;
            default: { return nullptr;
            }
        }
        return nullptr;
    }
    
    bool BACKGROUND::loadMesh(const char *fileNameMeshMbm) // sprite, mesh ou até mesmo font (para fonte utilize loadFont)
    {
        return this->load(fileNameMeshMbm);
    }
    
    bool BACKGROUND::loadFont(const char *fileNameMeshMbm, const char *newText) // font (para sprite, mesh utilize loadMesh)
    {
        if (this->load(fileNameMeshMbm) == false)
            return false;
        if (newText)
            this->text                = newText;
        this->fileName = "loadFont|";
        this->fileName += fileNameMeshMbm;
        this->fileName += "|";
        this->fileName += newText ? newText : "(null)";

        return true;
    }
    
    util::TYPE_MESH BACKGROUND::getType() const
    {
        return this->type;
    }
    
    TEXTURE * BACKGROUND::getTexture() const
    {
        return this->texture;
    }

    bool BACKGROUND::onRestoreDevice()
    {
        std::vector<std::string> result;
        util::split(result, this->fileName.c_str(), '|');
        if (result.size() <= 1)
            return false;
        if (result[0].compare("load") == 0 || result[0].compare("loadFont") == 0)
        {
			// TODO: check if this works correctly for not texture, checked only for texture
            if (result.size() < 2)
                return false;
            const bool               isFont = result[0].compare("loadFont") == 0;
            const char* fileName = result[1].c_str();
            std::vector<std::string> lsresult;
            util::split(lsresult, fileName, '.');
            if (lsresult.size() == 0)
                return false;
            //TODO: more types are needed here
            if ((lsresult[lsresult.size() - 1].compare("mbm") == 0) ||
                (lsresult[lsresult.size() - 1].compare("MBM") == 0))
            {
                this->mesh = MESH_MANAGER::getInstance()->load(fileName, this);
                if (this->mesh == nullptr)
                    return false;
            }
            else // Textura
            {
#if defined DEBUG
                PRINT_IF_DEBUG("Failed to restore [%s]", log_util::basename(this->fileName.c_str()));
#endif
                return false;
            }
            if (!this->setScale(this->isMajorScale))
                return false;
            // adicionamos as animações
            for (unsigned int i = 0; i < this->mesh->infoAnimation.lsHeaderAnim.size(); ++i)
            {
                util::INFO_ANIMATION::INFO_HEADER_ANIM* header = this->mesh->infoAnimation.lsHeaderAnim[i];
                if (!this->populateAnimationFromHeader(this->mesh, header->headerAnim, i))
                {
                    this->release();
                    PRINT_IF_DEBUG("error on add animation!!");
                    return false;
                }
            }
            // carregamos a textura do estagio 2
            this->populateTextureStage2FromMesh(this->mesh);
            if (isFont && this->text.size() == 0)
            {
                this->text = result[2];
            }
            this->lsAnimation[this->indexCurrentAnimation]->restartAnimation();
#if defined DEBUG
            PRINT_INFO_IF_DEBUG("background [%s] successfully restored", log_util::basename(fileName));
#endif
            // later the engine will fill in the animation state with onRestoreAnimationsState
            return true;
        }
        else if (result[0].compare("loadTexture") == 0)
        {
            if (result.size() != 3)
                return false;
            const char* fileName = result[1].c_str();
            const bool  alpha_color = result[2].compare("1") == 0;
            if (this->buffer)
                delete this->buffer;
            this->buffer = nullptr;
            this->texture = nullptr;
#if defined DEBUG
            PRINT_INFO_IF_DEBUG("background [%s] successfully restored", log_util::basename(fileName));
#endif
            return this->loadTexture(fileName, alpha_color);
            // later the engine will fill in the animation state with onRestoreAnimationsState
        }
        return true;
    }
    
    bool BACKGROUND::loadTexture(const char *fileNameMeshMbm, const bool hasAlpha)
    {
        return this->load(fileNameMeshMbm, hasAlpha);
    }
    
    void BACKGROUND::setFrontGround(const bool enable)
    {
        this->isFrontGround = enable;
        this->setScale(this->isMajorScale);
    }
    
    bool BACKGROUND::load(const char *fileName, const bool hasAlpha , const bool majorScale)
    {
        if (this->mesh || this->texture)
            return true;
        if (fileName == nullptr)
            return false;
        std::vector<std::string> lsresult;
        util::split(lsresult, fileName, '.');
        if (lsresult.size() == 0)
            return false;
        if ((lsresult[lsresult.size() - 1].compare("mbm") == 0) || 
            (lsresult[lsresult.size() - 1].compare("spt") == 0) ||
            (lsresult[lsresult.size() - 1].compare("msh") == 0))
        {
            this->mesh = MESH_MANAGER::getInstance()->load(fileName, this);
            if (this->mesh == nullptr)
                return false;
            this->type = this->mesh->getTypeMesh();
            if (!this->setScale(majorScale))
                return false;

            // adicionamos as animações
            for (unsigned int i = 0; i < this->mesh->infoAnimation.lsHeaderAnim.size(); ++i)
            {
                util::INFO_ANIMATION::INFO_HEADER_ANIM* header = this->mesh->infoAnimation.lsHeaderAnim[i];
                if (!this->populateAnimationFromHeader(this->mesh, header->headerAnim, i))
                {
                    this->release();
                    PRINT_IF_DEBUG("error on add animation!!");
                    return false;
                }
            }
            // carregamos a textura do estagio 2
            this->populateTextureStage2FromMesh(this->mesh);
            this->restartAnimation();
            this->fileName = "load|";
            this->fileName += fileName;
            this->updateAABB();
            return true;
        }
        else // Textura
        {
            if (this->buffer)
                delete this->buffer;
            this->buffer                  = nullptr;
            int                indexStart = 0;
            int                indexCount = 6;
            VEC3            _position[4];
            VEC2            uv[4];
            unsigned short int index[6] = {0, 1, 2, 2, 1, 3};
            this->texture               = TEXTURE_MANAGER::getInstance()->load(fileName, hasAlpha);
            if (this->texture == nullptr)
                return false;
            this->buffer = new BUFFER_GL();
            mbm::fillVertexQuadTexture(_position, uv, static_cast<float>(this->texture->getWidth()),
                                     static_cast<float>(this->texture->getHeight()));
            this->bound.halfDim.x = static_cast<float>(this->texture->getWidth()) * 0.5f;
            this->bound.halfDim.y = static_cast<float>(this->texture->getHeight()) * 0.5f;
            const bool ret = this->buffer->loadBuffer(_position, nullptr, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
            if (ret == false)
                return false;
			this->addAnimation();
            this->buffer->setTextureByStage(this->texture, 0, 0);
            bool useAlpha  = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
            this->type     = util::TYPE_MESH_TEXTURE;
            this->fileName = "loadTexture|";
            this->fileName += fileName;
            if (useAlpha)
                this->fileName += "|1";
            else
                this->fileName += "|0";

            this->restartAnimation();
            return true;
        }
    }
    
    bool BACKGROUND::setTexture(const MESH_MBM *_mesh,const char *fileNametexture, const unsigned int stage, const bool hasAlpha)
    {
        if (this->type == util::TYPE_MESH_TEXTURE)
        {
            if (stage == 0)
            {
                mbm::TEXTURE *newTex = mbm::TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
                if (newTex)
                {
                    this->texture = newTex;
                    if (this->buffer)
                        this->buffer->setTextureByStage(newTex, stage, 0);
                    return true;
                }
            }
            else
            {
                return ANIMATION_MANAGER::setTexture(_mesh, fileNametexture, stage, hasAlpha);//TODO check this
            }
        }
        else if (_mesh)
        {
            return ANIMATION_MANAGER::setTexture(_mesh, fileNametexture, stage, hasAlpha);//TODO check this
        }
        return false;
    }
    
    float BACKGROUND::getValueFromMinMaxValue(const float min, const float max, const float value_0_100_percent)
    {
        const float interval = max - min;
        return ((interval * value_0_100_percent) + min);
    }
    
    bool BACKGROUND::isOnFrustum()
    {
        if (this->type == util::TYPE_MESH_UNKNOWN)
            return false;
        if (this->lasIndexAnimation != this->indexCurrentAnimation)
        {
            if (!this->setScale(this->isMajorScale))
                return false;
        }
        if (this->indexCurrentAnimation >= this->lsAnimation.size())
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        const float w = device->getScaleBackBufferWidth() * 0.5f;
        const float h = device->getScaleBackBufferHeight() * 0.5f;
        if (this->is3D)
        {
            if (this->isFrontGround)
                device->transformeScreen2dToWorld3d_scaled(w, h, &this->position, 60);
            else
                device->transformeScreen2dToWorld3d_scaled(w, h, &this->position, this->howFar3d);
        }
        else
        {
            this->position.x = w;
            this->position.y = h;
            device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, this->position);
        }
        if (this->isRender2Texture)
            return false;
        return true; // always renderize
    }
    
    bool BACKGROUND::render()
    {
        if (this->alwaysRenderize)
        {
            if (this->isOnFrustum() == false)
                return false;
        }
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        ANIMATION *animation = this->getAnimation();
        if (animation)
            animation->updateAnimation(device->delta, this, this->onEndAnimation, this->onEndFx);
        else
            return false;
        switch (this->type)
        {
            case util::TYPE_MESH_TEXTURE:
            {
                if (this->is3D)
                {
                    device->setBillboard(&SHADER::modelView, &this->position, &this->scale);
                    MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective);
                }
                else
                {
                    const VEC3 positionWorld(this->position.x, this->position.y, this->position.z);
                    MatrixTranslationRotationScale(&SHADER::modelView, &positionWorld, &this->angle, &this->scale);
                    MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
                }
                this->blend.set(animation->blendState);
                animation->fx.shader.update(); // glUseProgram
                animation->fx.setBlendOp();

                if (animation->fx.textureOverrideStage2)
                    this->buffer->setTextureByStage(animation->fx.textureOverrideStage2, 1, 0);
                if (!animation->fx.shader.render(this->buffer))
                    return false;
                return true;
            }
            case util::TYPE_MESH_TILE_MAP:
            {
                //TODO
                return false;
            }
            break;
            case util::TYPE_MESH_SPRITE:
            {
                if (this->is3D)
                {
                    device->setBillboard(&SHADER::modelView, &this->position, &this->scale);
                    MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective);
                }
                else
                {
                    MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                    MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
                }
                this->blend.set(animation->blendState);
                animation->fx.shader.update(); // glUseProgram
                animation->fx.setBlendOp();
                if (animation->fx.textureOverrideStage2)
                {
                    if (!this->mesh->render(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,
                                            animation->fx.textureOverrideStage2))
                        return false;
                }
                else
                {
                    if (!this->mesh->render(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,0))
                        return false;
                }
                return true;
            }
            case util::TYPE_MESH_3D:
            {
                if (this->is3D)
                {
                    device->setBillboard(&SHADER::modelView, &this->position, &this->scale);
                    MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective);
                }
                else
                {
                    MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                    MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
                }
                this->blend.set(animation->blendState);
                animation->fx.shader.update(); // glUseProgram
                animation->fx.setBlendOp();
                if (animation->fx.textureOverrideStage2)
                {
                    if (!mesh->render(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader,animation->fx.textureOverrideStage2))
                        return false;
                }
                else
                {
                    if (!mesh->render(static_cast<unsigned int>(animation->indexCurrentFrame), &animation->fx.shader, nullptr))
                        return false;
                }
                return true;
            }
            case util::TYPE_MESH_FONT:
            {
                const std::string  textDraw(this->text);
                const auto s = static_cast<unsigned int>(textDraw.size());
                static VEC3     posTemp2d(0, 0, 0);

                if (this->is3D)
                    device->setBillboard(&SHADER::modelView, &posTemp2d, &this->scale);
                else
                    MatrixTranslationRotationScale(&SHADER::modelView, &posTemp2d, &this->angle, &this->scale);
                this->blend.set(animation->blendState);
                float curWidthLetter = 0;
                const INFO_BOUND_FONT * infoFont = this->mesh->getInfoFont();
                if(infoFont == nullptr)
                    return false;
                for (unsigned int i = 0; i < s; ++i)
                {
                    auto index = static_cast<unsigned char>(textDraw[i]);
                    switch (index)
                    {
                        case 194: // UTF8 - Without BOM
                        {
                            if ((i + 1) < s)
                            {
                                i++;
                                index = static_cast<unsigned char>(textDraw[i]);
                                index = TEXT_DRAW::withoutBOM2Map(index, 194);
                            }
                        }
                        break;
                        case 195: // UTF8 - Without BOM
                        {
                            if ((i + 1) < s)
                            {
                                i++;
                                index = static_cast<unsigned char>(textDraw[i]);
                                index = TEXT_DRAW::withoutBOM2Map(index, 195);
                            }
                        }
                        break;
                        default: {
                        }
                        break;
                    }
                    switch (index)
                    {
                        case '\n':
                        {
                            SHADER::modelView._41 = posTemp2d.x - (curWidthLetter * 0.5f);
                            SHADER::modelView._42 += static_cast<float>(infoFont->heightLetter + this->spaceYCharacter); // * 0.5f;
                        }
                        break;
                        case '\t': { SHADER::modelView._41 += (curWidthLetter * 8);
                        }
                        break;
                        default:
                        {
                            util::DETAIL_LETTER *detail = infoFont->letter[index].detail;
                            if (detail) // existe esta letra nesta fonte?
                            {
                                BUFFER_MESH *frame = this->mesh->getBuffer(detail->indexFrame);
                                if (frame)
                                {
                                    curWidthLetter = static_cast<float>(detail->widthLetter) * 0.5f;
                                    SHADER::modelView._41 += curWidthLetter;
                                    if (this->is3D)
                                        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView,
                                                       &device->camera.matrixPerspective);
                                    else
                                        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView,
                                                       &device->camera.matrixPerspective2d);
                                    animation->fx.shader.update(); // glUseProgram
                                    animation->fx.setBlendOp();
                                    if (animation->fx.textureOverrideStage2)
                                    {
                                        if (!this->mesh->render(static_cast<unsigned int>(detail->indexFrame), &animation->fx.shader,animation->fx.textureOverrideStage2))
                                            return false;
                                    }
                                    else
                                    {
                                        if (!this->mesh->render(static_cast<unsigned int>(detail->indexFrame), &animation->fx.shader, nullptr))
                                            return false;
                                    }
                                }
                            }
                            SHADER::modelView._41 += curWidthLetter + this->spaceXCharacter;
                        }
                    }
                }
                return true;
            }
            case util::TYPE_MESH_USER:
            case util::TYPE_MESH_UNKNOWN: return false;
            default: { return false;
            }
        }
    }
    
    bool BACKGROUND::setScale(const bool majorScale)
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        switch (this->type)
        {
            // Aqui buscamos preencher baseado no maior bounding box que representa o objeto como todo.
            // Isso porque sempre vamos renderizar o objeto então só precisamos de sua maior dimensão como todo (um simple
            // cubo, esfera ou circulo... nada mais)
            // Então o bounding do objeto que sera utiliado como backGround deve ser do tamanho exato do objeto ou maior
            // sempre.
            
            case util::TYPE_MESH_3D:
            {
                if (mesh == nullptr)
                    return false;
                unsigned int index = 0;
                if (mesh->infoPhysics.lsCube.size())
                {
                    if (this->indexCurrentAnimation < mesh->infoPhysics.lsCube.size())
                        index = this->indexCurrentAnimation;
                    if (index < mesh->infoPhysics.lsCube.size())
                    {
                        CUBE *cube            = mesh->infoPhysics.lsCube[index];
                        this->bound.halfDim.x = cube->halfDim.x;
                        this->bound.halfDim.y = cube->halfDim.y;
                        this->bound.halfDim.z = cube->halfDim.z;
                    }
                }
                else if (mesh->infoPhysics.lsSphere.size())
                {
                    if (this->indexCurrentAnimation < mesh->infoPhysics.lsSphere.size())
                        index = this->indexCurrentAnimation;
                    if (index < mesh->infoPhysics.lsSphere.size())
                    {
                        mbm::SPHERE *sphere   = mesh->infoPhysics.lsSphere[index];
                        this->bound.halfDim.x = sphere->ray;
                        this->bound.halfDim.y = sphere->ray;
                        this->bound.halfDim.z = sphere->ray;
                    }
                }
                this->indexCurrentAnimation = index;
            }
            break;
            
            case util::TYPE_MESH_FONT:
            {
                if (mesh == nullptr)
                    return false;
                const INFO_BOUND_FONT* infoFont = mesh->getInfoFont();
                if(infoFont == nullptr)
                    return false;
                for (auto & i : infoFont->letter)
                {
                    // encontramos a maior letra
                    util::DETAIL_LETTER *detail = i.detail;
                    if (detail)
                    {
                        if (static_cast<float>(detail->widthLetter) > this->bound.halfDim.x)
                            this->bound.halfDim.x = static_cast<float>(detail->widthLetter);
                        if (static_cast<float>(detail->heightLetter) > this->bound.halfDim.y)
                            this->bound.halfDim.y = static_cast<float>(detail->heightLetter);
                    }
                }
                this->bound.halfDim.z = 0.5f;
                this->bound.halfDim.x *= 0.5f;
                this->bound.halfDim.y *= 0.5f;
                this->spaceXCharacter = infoFont->spaceXCharacter;
                this->spaceYCharacter = infoFont->spaceYCharacter;
            }
            break;
            
            case util::TYPE_MESH_USER: { return false;}
            case util::TYPE_MESH_TILE_MAP:
            {
                //TODO
                return false;
            }
            break;
            case util::TYPE_MESH_SPRITE:
            {
                if (mesh == nullptr)
                    return false;
                unsigned int index = 0;
                // this->bound.halfDim.z    =   1.0f;
                // if(this->indexCurrentAnimation < this->mesh->infoSprite.lsBounding.size())
                //  index = this->indexCurrentAnimation;
                // if(index < this->mesh->infoSprite.lsBounding.size())
                //{
                //  deprecated_mbm::BOUNDING_SPRITE* bounding = this->mesh->infoSprite.lsBounding[index];
                //  for(unsigned int i=0,s = bounding->lsSubset.size(); i< s; ++i)
                //  {
                //      deprecated_mbm::BOUNDING_SUBSET_SPRITE* sub =  bounding->lsSubset[i];
                //      switch(sub->typeFrame)
                //      {
                //          case deprecated_mbm::FRAME_RECTANGLE://Temos uma informação indicando a largura e altura do
                // sprite
                //          {
                //              const float x   =   sub->infoColision[0].x * 0.5f;
                //              if(x > this->bound.halfDim.x)
                //                  this->bound.halfDim.x   =   x;
                //              const float y   =   sub->infoColision[0].y * 0.5f;
                //              if(y > this->bound.halfDim.y)
                //                  this->bound.halfDim.y   =   y;
                //          }
                //          break;
                //          case deprecated_mbm::FRAME_CIRCLE://Temos uma informação indicando o raio do circulo
                //          {
                //              const float ray = sub->infoColision[0].x ;
                //              if(ray > this->bound.halfDim.x)
                //                  this->bound.halfDim.x   =   ray;
                //              if(ray > this->bound.halfDim.y)
                //                  this->bound.halfDim.y   =   ray;
                //
                //          }
                //          break;
                //          case deprecated_mbm::FRAME_TRIANGLE://Temos 3 pontos de informação indicando o triangulo.
                // porem
                // informamos o bounding que é menos calculo e mais preciso
                //          {
                //              const float x   =   sub->infoColision[sub->sizeInfoColision].x * 0.5f;
                //              const float y   =   sub->infoColision[sub->sizeInfoColision].y * 0.5f;
                //              if(x > this->bound.halfDim.x)
                //                  this->bound.halfDim.x   =   x;
                //              if(y > this->bound.halfDim.y)
                //                  this->bound.halfDim.y   =   y;
                //          }
                //          break;
                //          case deprecated_mbm::FRAME_POLYGON://Temos informações para cada coordenadas do poligono porem
                // o
                // ultimo indice no array indica a largura e altura do bounding.
                //          {
                //              const float x   =   sub->infoColision[sub->sizeInfoColision].x * 0.5f;
                //              const float y   =   sub->infoColision[sub->sizeInfoColision].y * 0.5f;
                //              if(x > this->bound.halfDim.x)
                //                  this->bound.halfDim.x   =   x;
                //              if(y > this->bound.halfDim.y)
                //                  this->bound.halfDim.y   =   y;
                //          }
                //          break;
                //      }
                //  }
                //}
                this->indexCurrentAnimation = index;
            }
            break;
            
            case util::TYPE_MESH_TEXTURE:
            {
                if (this->texture == nullptr)
                    return false;
                this->bound.halfDim.x = static_cast<float>(this->texture->getWidth()) * 0.5f;
                this->bound.halfDim.y = static_cast<float>(this->texture->getHeight()) * 0.5f;
            }
            break;
            case util::TYPE_MESH_UNKNOWN: { return false;}
            default: { return false;}
                
        }
        this->lasIndexAnimation = this->indexCurrentAnimation;
        if (this->is3D)
        {
            VEC3 dimNear, dimFar;
            device->getDimFromFrustum(&dimNear, &dimFar);

            const float percX = (this->isFrontGround ? dimNear.x : dimFar.x) / (this->bound.halfDim.x * 2.0f);
            const float percY = (this->isFrontGround ? dimNear.y : dimFar.y) / (this->bound.halfDim.y * 2.0f);
            if (majorScale)
            {
                if (percX >= percY)
                {
                    this->scale.x = percX;
                    this->scale.y = percX; //-V537
                }
                else
                {
                    this->scale.x = percY; //-V537
                    this->scale.y = percY;
                }
            }
            else
            {
                if (percX <= percY)
                {
                    this->scale.x = percX;
                    this->scale.y = percX; //-V537
                }
                else
                {
                    this->scale.x = percY; //-V537
                    this->scale.y = percY;
                }
            }
        }
        else
        {
            const float w     = device->getScaleBackBufferWidth();
            const float h     = device->getScaleBackBufferHeight();
            const float percX = w / (this->bound.halfDim.x * 2.0f);
            const float percY = h / (this->bound.halfDim.y * 2.0f);
            if (majorScale)
            {
                if (percX >= percY)
                {
                    this->scale.x = percX;
                    this->scale.y = percX; //-V537
                }
                else
                {
                    this->scale.x = percY; //-V537
                    this->scale.y = percY;
                }
            }
            else
            {
                if (percX <= percY)
                {
                    this->scale.x = percX;
                    this->scale.y = percX; //-V537
                }
                else
                {
                    this->scale.x = percY; //-V537
                    this->scale.y = percY;
                }
            }
        }
        this->isMajorScale = majorScale;
        return true;
    }
    
    FVF_PROVIDE_BY_ENGINE BACKGROUND::getFvfFromBuffer() const noexcept
    {
        return (buffer && buffer->isLoadedBuffer()) ? buffer->fvf : FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    const mbm::INFO_PHYSICS * BACKGROUND::getInfoPhysics() const 
    {
        if (this->mesh)
            return &this->mesh->infoPhysics;
        return nullptr;
    }
    
    const MESH_MBM * BACKGROUND::getMesh() const 
    {
        if (this->type == util::TYPE_MESH_TEXTURE)
            return nullptr;
        return this->mesh;
    }

    FX*  BACKGROUND::getFx()const
    {
        auto * anim = getAnimation();
        if (anim)
            return &anim->fx;
        return nullptr;
    }

    ANIMATION_MANAGER*  BACKGROUND::getAnimationManager()
    {
        return this;
    }
    
    bool BACKGROUND::isLoaded() const 
    {
        return this->mesh != nullptr || this->texture != nullptr;
    }

};
