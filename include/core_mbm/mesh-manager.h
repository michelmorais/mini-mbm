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

#ifndef MESH_MANAGER_GLES_H
#define MESH_MANAGER_GLES_H

#include "core-exports.h"
#include "primitives.h"
#include "header-mesh.h"
#include "physics.h"
#include <unordered_map>

namespace util 
{
    struct SUBSET;
}


namespace mbm
{
    class BUFFER_GL;
    class RENDERIZABLE_TO_TARGET;
    class SHADER;
    class MESH_MBM;
    struct IMAGE_RESOURCE;

    struct BUFFER_MESH
    {
        BUFFER_GL *pBufferGL;
        util::SUBSET *  subset;
        uint32_t    totalSubset;
        API_IMPL constexpr BUFFER_MESH() noexcept;
        API_IMPL virtual ~BUFFER_MESH();
        API_IMPL void release();
    };

#if defined USE_EDITOR_FEATURES

    class MESH_MBM_DEBUG
    {
      public:
        util::HEADER						   headerMain;
        util::HEADER_MESH					   headerMesh;
        INFO_PHYSICS                           infoPhysics;
        util::INFO_ANIMATION                   infoAnimation;
		util::INFO_DRAW_MODE			       info_mode;
        VEC2                                   zoomEditorSprite;
        util::TYPE_MESH                        typeMe;
        int                                    sizeCoordTexFrame_0;
        VEC2 *                                 coordTexFrame_0;
        VEC3                                   positionOffset;
        VEC3                                   angleDefault;
        std::vector<util::BUFFER_MESH_DEBUG *> buffer;
        std::string                            fileName;
        std::vector<int>                       lsBlendOperation;
        API_IMPL MESH_MBM_DEBUG() noexcept;
    
        API_IMPL virtual ~MESH_MBM_DEBUG();
        API_IMPL uint32_t addBuffer(const int stride = 3);
        API_IMPL uint32_t addSubset(uint32_t indexFrame);
        API_IMPL bool getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut, INFO_BOUND_FONT **datailFontOut,
                     std::vector<util::STAGE_PARTICLE> &lsStageParticle);
        API_IMPL static bool getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                                  util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut, 
                                  std::vector<util::STAGE_PARTICLE> & lsStageParticle);
        API_IMPL static const char* getValidExtension(const char* fileName,bool &isImage,bool &isMesh,bool &isUnknown);
		API_IMPL static std::string getExtension(const char* fileName);
        API_IMPL util::TYPE_MESH getType() noexcept;
        API_IMPL util::TYPE_MESH getType(const char *fileNamePath);
        API_IMPL void calculateNormals();
        API_IMPL void calculateUV();
        API_IMPL bool saveDebug(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, char *errorOut,const int lenErrorOut);
        API_IMPL bool loadDebugFromMemory(const MESH_MBM* meshMemory);
        API_IMPL bool loadDebug(const char *fileNamePath);
        API_IMPL bool check(char *error,const int lenError);
        API_IMPL void centralizeFrame(const int indexFrame, const int indexSubset);
        API_IMPL bool addIndex(const uint32_t indexFrame, const uint32_t indexSubset,
                            const uint16_t *newIndexPart, const uint32_t sizeArrayNewIndexPart,
                            char *strErrorOut);
        API_IMPL bool addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex);
        API_IMPL int addAnimation(const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut);
        API_IMPL bool updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError);
        API_IMPL const util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnim(const uint32_t index)const;
        API_IMPL void fixDefaultBoud();
        API_IMPL void release();
		API_IMPL void deleteExtraInfo();
		void *       extraInfo;
      private:
        void fillAtLeastOneBound();
        bool fillAnimation_2(const char *fileNamePath, FILE *fp);
        bool loadFromSplited(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                    VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                    uint16_t *indexArray, const int sizeArrayIndex, const int stride);
    
        bool saveAnimationHeaders(const char *fileOut, FILE **file);
        bool compressFile(const char *fileNameIn, char *stringStatus,const int lenStatus);
    };

#endif
    class MESH_MBM
    {
        friend class MESH_MANAGER;
      public:
        VEC3                            positionOffset;
        VEC3                            angleDefault;
        util::MATERIAL_GLES             material;
        INFO_PHYSICS                    infoPhysics;
        util::INFO_ANIMATION            infoAnimation;
	util::INFO_DRAW_MODE		info_mode;
        
        API_IMPL BUFFER_MESH *getBuffer(const uint32_t index) const;
        API_IMPL TEXTURE *getTexture(const uint32_t indexFrame, const uint32_t indexSubset);
        API_IMPL bool setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                               const bool hasAlpha);
        API_IMPL const char *getFilenameMesh() const;
        API_IMPL virtual ~MESH_MBM();
        API_IMPL void release();
	      API_IMPL void deleteExtraInfo();
        API_IMPL bool isLoaded() const;
        API_IMPL bool render(const uint32_t indexFrame,const SHADER *pShader, const uint32_t idTexture1);
        API_IMPL bool renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                        VEC2 *uv, const uint32_t idTexture1);
        API_IMPL util::TYPE_MESH getTypeMesh() const;
        API_IMPL VEC2 getZoomEditorSprite() const;
        API_IMPL uint32_t getTotalFrame() const;
        API_IMPL uint32_t getTotalSubset(const uint32_t indexFrame) const;
	      API_IMPL const INFO_BOUND_FONT* getInfoFont()const;
	      const std::vector<util::STAGE_PARTICLE*>* getInfoParticle()const;
	      API_IMPL const util::BTILE_INFO* getInfoTile()const;
        API_IMPL const util::DYNAMIC_SHAPE* getInfoShape()const;
		
      private:
        MESH_MBM() noexcept;
        bool load(const char *fileNamePath);
        void invertMap(const bool u, const bool v, VEC2 *pTexture, const uint32_t arraySize);
        bool loadFromSplited(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                    VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                    uint16_t *indexArray, const int sizeArrayIndex, const int stride);
        bool fillAnimation_2(util::HEADER_MESH &headerMesh, const char *fileNamePath, FILE *fp);

        BUFFER_MESH *               buffer;
        std::string                 fileName;
        VEC2                        zoomEditorSprite; // Zoom do editor de sprite
        util::TYPE_MESH             typeMe;
        int16_t                     hasNormTex[2];       // Indica se tem normal e textura vinda do arquivo
        uint8_t                     depthUberImage;      // Quando uber image esta no arquivo é setado esta variavel
        int                         sizeCoordTexFrame_0; // Tamanho do array das coordenadas de textura do frame 0
        VEC2 *                      coordTexFrame_0;     // Coordenadas de Textura do frame 0 (faz cópiad para os outros frames)
        uint32_t                    totalFramesMesh;
        void *                      extraInfo;
    };

    class MESH_MANAGER
    {
      private:
        static MESH_MANAGER *instanceMeshManager;

      public:
    
        API_IMPL static MESH_MANAGER *getInstance();
        API_IMPL static void release();
        API_IMPL void fakeRelease(const char* fileName);
        API_IMPL MESH_MBM *load(const char *fileName);
        API_IMPL MESH_MBM *loadTrueTypeFont(const char *fileNameTtf, const float heightLetter, const short spaceWidth,const short spaceHeight,const bool saveTextureAsPng,TEXTURE ** texture_loaded);
        API_IMPL MESH_MBM *load(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer,const util::INFO_DRAW_MODE * info_mode);
        API_IMPL MESH_MBM *loadIndex(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer, uint16_t *index,const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode);
        API_IMPL MESH_MBM *loadDynamicIndex(const char *nickName, const uint32_t sizeVertexBuffer,uint16_t *index, const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode, const util::DYNAMIC_SHAPE & dynamic_shape_info);
        API_IMPL MESH_MBM *getIfExists(const char* fileName);
        API_IMPL static const char * typeClassName(const util::TYPE_MESH type) noexcept;
      private:
        std::unordered_map<std::string,MESH_MBM *> lsMeshes;
        std::vector<MESH_MBM *> lsFakeRelease;
        MESH_MANAGER() = default;
        virtual ~MESH_MANAGER();
    };
}

#endif
