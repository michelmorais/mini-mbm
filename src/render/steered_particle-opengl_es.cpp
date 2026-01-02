/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include "steered_particle.h"
#include <core_mbm/texture-manager.h>
#include <core_mbm/header-mesh.h>
#include <core_mbm/gles-debug.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/scene.h>
#include <climits>

#if (defined _DEBUG || defined DEBUG_RESTORE)
    #include <core_mbm/log-util.h>
#endif


namespace mbm
{
    void STEERED_PARTICLE::release()
    {
        this->releaseAnimation();
        this->enableRender = false;
        if (this->vboIndexBuffer)
        {
            GLDeleteBuffers(1, &this->vboIndexBuffer);
        }
        this->vboIndexBuffer = 0;
        for (unsigned int i = 0; i < this->lsParticleGroup.size(); ++i)
        {
            FLUID_GROUP* pGroup = lsParticleGroup[i];
            delete pGroup;
        }
        lsParticleGroup.clear();
    }
    
    bool STEERED_PARTICLE::load(const char *fileNameTexture,
                                const COLOR *p_color,
                                const mbm::INFO_PHYSICS* const p_infoPhysics)
    {
        this->release();
        if(p_infoPhysics == nullptr)
            return false;
        if((p_infoPhysics != &this->infoPhysics) && (infoPhysics.clone(p_infoPhysics) == false))
            return false;
        const unsigned short int index[6]            = {0, 1, 2, 2, 1, 3};
        const unsigned int       sizeIndexBuffer     = sizeof(index);
        GLGenBuffers(1, &this->vboIndexBuffer);
        if (!this->vboIndexBuffer)
            return false;
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        GLBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeIndexBuffer, index, GL_STATIC_DRAW);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        this->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTexture, true);
        if (this->texture)
        {
            if (!this->createAnimationAndShader2Particle(p_color))
            {
                ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
                return false;
            }
            this->fileName        = fileNameTexture;
            this->enableRender    = true;
            this->alwaysRenderize = true;
            this->updateAABB();
            return true;
        }
        return false;
    }

    
    void STEERED_PARTICLE::onStop()
    {
        if (this->vboIndexBuffer)
        {
            GLDeleteBuffers(1, &this->vboIndexBuffer);
        }
        this->vboIndexBuffer = 0;
    }
    
    bool STEERED_PARTICLE::renderParticle(FLUID_GROUP * pGroup)
    {
        if(pGroup->totalParticleToRender == 0)
            return false;
        for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
        {
            const VEC3 * particle  = &pGroup->particle_positions[i];
            VEC3 *vertex           = &pGroup->vertex_particle[i * 4];
            pGroup->setVertex( particle, vertex);
        }
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        ANIMATION *  anim   = this->getAnimation();
        this->blend.set(anim->blendState);
        anim->updateAnimation(device->delta, this, nullptr, this->onEndFx);
        anim->fx.setBlendOp();
        anim->fx.shader.update();
        GLDisable(GL_DEPTH_TEST);
        GLActiveTexture(GL_TEXTURE0);
        if (this->texture)
        {
            GLBindTexture(GL_TEXTURE_2D, this->texture->idTexture);
        }
        else
        {
            GLBindTexture(GL_TEXTURE_2D, 0);
        }
        GLUniform1i(anim->fx.shader.samplerHandle0, 0);
        if (anim->fx.textureOverrideStage2)
        {
            glActiveTexture(GL_TEXTURE1);
            GLBindTexture(GL_TEXTURE_2D, anim->fx.textureOverrideStage2->idTexture);
            glUniform1i(anim->fx.shader.samplerHandle1, 1);
        }
        else
        {
            GLActiveTexture(GL_TEXTURE1);
            GLBindTexture(GL_TEXTURE_2D, 0);
        }
        
        GLUniformMatrix4fv(anim->fx.shader.mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        VAR_SHADER *var = anim->fx.fxPS->ptrCurrentShader
                                         ? anim->fx.fxPS->ptrCurrentShader->getVarByName("color")
                                         : nullptr;
        if(this->segmented)
        {
            if (var)
            {
                GLUniform4f(var->handleVar, this->shader_color.r, this->shader_color.g, this->shader_color.b, this->shader_color.a);
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float * vertex  = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                    const float * uv      = reinterpret_cast<float *>(&pGroup->uv[i * 4]);
                    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                    GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                    GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                    GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                    GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    //if(i != 168/2 && i != 168/2+1 && i != 168/2+2 && i != 168/2+4)
                    {
                        const float *vertex = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                        const float * uv      = reinterpret_cast<float *>(&pGroup->uv[i * 4]);
                        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                        GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                        GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                        GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                        GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                        GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                    }
                }
            }
        }
        else
        {
            if (var)
            {
                GLUniform4f(var->handleVar, this->shader_color.r, this->shader_color.g, this->shader_color.b, this->shader_color.a);
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float * vertex  = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                    const float * uv      = reinterpret_cast<float *>(pGroup->uv);
                    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                    GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                    GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                    GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                    GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float * vertex  = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                    const float * uv      = reinterpret_cast<float *>(pGroup->uv);
                    GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                    GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                    GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                    GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
        }
        
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        GLEnable(GL_DEPTH_TEST);
        return true;
    }
    
    bool STEERED_PARTICLE::loadParticleShader(const COLOR *p_color)
    {
        if(p_color)
        {
            this->shader_color = *p_color;
            const char *defaultCodePs_1 = "precision mediump float;\n"
                                        "uniform vec4 color;\n"
                                        "varying vec2 vTexCoord;\n"
                                        "uniform sampler2D sample0;\n"
                                        "void main()\n"
                                        "{\n"
                                        "  vec4 texColor = texture2D( sample0, vTexCoord );\n"
                                        "  gl_FragColor = color * texColor;\n"
                                        "}\n";
            std::string defaultCodePs(defaultCodePs_1);
            const char *defaultCodeVs = "attribute vec4 aPosition;"
                                        "attribute vec2 aTextCoord;"
                                        "uniform mat4 mvpMatrix;"
                                        "varying vec2 vTexCoord;"
                                        "void main()"
                                        "{"
                                        "     gl_Position = mvpMatrix * aPosition;"
                                        "     vTexCoord = aTextCoord;"
                                        "}";

            const char *fileNamePs = "__steered_particle.ps";
            const char *fileNameVs = "__steered_particle.vs";

            ANIMATION *  anim   = this->getAnimation();
            
            anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs.c_str(), TYPE_ANIMATION_GROWING);
            anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_GROWING);
            anim->fx.shader.releaseShader();
            if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
                return false;
            const float defaultVar[4] = {this->shader_color.r, this->shader_color.g, this->shader_color.b, this->shader_color.a};
            if (anim->fx.fxPS->ptrCurrentShader)
            {
                if(anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, defaultVar,anim->fx.shader.programObject) == false)
                {
                    PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "color", fileNamePs);
                }
                VAR_SHADER * colorVar = anim->fx.fxPS->ptrCurrentShader->getVarByName("color");
                if(colorVar)
                {
                    colorVar->set(defaultVar,defaultVar,1.0f);
                }
            }
        }
        else
        {
            const char *defaultCodePs = "precision mediump float;\n"
                                        "varying vec2 vTexCoord;\n"
                                        "uniform sampler2D sample0;\n"
                                        "void main()\n"
                                        "{\n"
                                        "  gl_FragColor = texture2D( sample0, vTexCoord );\n"
                                        "}\n";
            const char *defaultCodeVs = "attribute vec4 aPosition;"
                                        "attribute vec2 aTextCoord;"
                                        "uniform mat4 mvpMatrix;"
                                        "varying vec2 vTexCoord;"
                                        "void main()"
                                        "{"
                                        "     gl_Position = mvpMatrix * aPosition;"
                                        "     vTexCoord = aTextCoord;"
                                        "}";

            const char *fileNamePs = "__steered_particle.ps";
            const char *fileNameVs = "__steered_particle.vs";

            ANIMATION *  anim   = this->getAnimation();
            
            anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs, TYPE_ANIMATION_PAUSED);
            anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_PAUSED);
            anim->fx.shader.releaseShader();
            if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
                return false;
        }
        return true;
    }
}


