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
#include <map>
#include <memory>

namespace util 
{
    struct SUBSET;
}

namespace deprecated_mbm
{
  struct INFO_SPRITE;
}


namespace mbm
{
    class BUFFER_GL;
    class RENDERIZABLE;
    class RENDERIZABLE_TO_TARGET;
    class SHADER;
    class MESH_MBM;
    struct IMAGE_RESOURCE;

    struct BUFFER_MESH
    {
        BUFFER_GL *pBufferGL;
        util::SUBSET *  subset;
        uint32_t    totalSubset;
        constexpr BUFFER_MESH() noexcept;
        API_IMPL virtual ~BUFFER_MESH();
        API_IMPL void release();
        API_IMPL BUFFER_GL *getRenderBuffer() const noexcept;
        API_IMPL bool hasLoadedRenderBuffer() const noexcept;
        API_IMPL uint32_t getTotalSubsets() const noexcept;
        API_IMPL util::SUBSET *getSubset(const uint32_t indexSubset) const noexcept;
    };

    class MESH_MBM_DEBUG
    {
      public:
        util::HEADER						               headerMain;
        util::HEADER_MESH					             headerMesh;
        INFO_PHYSICS                           infoPhysics;
        util::INFO_ANIMATION                   infoAnimation;
        util::INFO_DRAW_MODE			             info_mode;
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
        API_IMPL void     removeSubset(uint32_t indexFrame, uint32_t indexSubset);
        API_IMPL uint32_t copyBufferFrom(MESH_MBM_DEBUG &src, uint32_t srcFrameIdx);
        API_IMPL uint32_t copySubsetFrom(uint32_t targetFrame, MESH_MBM_DEBUG &src, uint32_t srcFrame, uint32_t srcSubsetIdx);
        API_IMPL bool getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut, INFO_BOUND_FONT **datailFontOut,
                     std::vector<util::STAGE_PARTICLE> &lsStageParticle);
        API_IMPL static bool getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                                  util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut, 
                                  std::vector<util::STAGE_PARTICLE> & lsStageParticle, int *versionOut = nullptr);
        API_IMPL static const char* getValidExtension(const char* fileName,bool &isImage,bool &isMesh,bool &isUnknown);
        API_IMPL static std::string getExtension(const char* fileName);
        API_IMPL util::TYPE_MESH getMeshType() const noexcept;
        API_IMPL void setMeshType(const util::TYPE_MESH type) noexcept;
        API_IMPL util::TYPE_MESH getType() noexcept;
        API_IMPL util::TYPE_MESH getType(const char *fileNamePath);
        API_IMPL VEC3 getAngleDefault() const noexcept;
        API_IMPL void setAngleDefault(const VEC3 &angle) noexcept;
        API_IMPL VEC3 getPositionOffset() const noexcept;
        API_IMPL void setPositionOffset(const VEC3 &position) noexcept;
        API_IMPL unsigned int getModeDraw() const noexcept;
        API_IMPL void setModeDraw(const unsigned int modeDraw) noexcept;
        API_IMPL unsigned int getModeCullFace() const noexcept;
        API_IMPL void setModeCullFace(const unsigned int modeCullFace) noexcept;
        API_IMPL unsigned int getModeFrontFaceDirection() const noexcept;
        API_IMPL void setModeFrontFaceDirection(const unsigned int modeFrontFaceDirection) noexcept;
        API_IMPL void * getDetailInfo() const noexcept;
        API_IMPL void replaceDetailInfo(void *detailInfo) noexcept;
        API_IMPL uint32_t getTotalAnimationHeaders() const noexcept;
        API_IMPL util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnimationHeader(const uint32_t index) const noexcept;
        API_IMPL void appendAnimationHeader(util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead) noexcept;
        API_IMPL void clearBlendOperations() noexcept;
        API_IMPL void resizeBlendOperations(const uint32_t totalAnimations);
        API_IMPL void setBlendOperation(const uint32_t index, const int blendOperation);
        API_IMPL uint32_t getTotalFrames() const noexcept;
        API_IMPL util::BUFFER_MESH_DEBUG *getFrameBuffer(const uint32_t indexFrame) const noexcept;
        API_IMPL uint32_t getTotalSubsets(const uint32_t indexFrame) const noexcept;
        API_IMPL util::SUBSET_DEBUG *getSubset(const uint32_t indexFrame, const uint32_t indexSubset) const noexcept;
        API_IMPL bool hasIndexBuffer(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC3 *getPositionArray(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC3 *getNormalArray(const uint32_t indexFrame) const noexcept;
        API_IMPL VEC2 *getUvArray(const uint32_t indexFrame) const noexcept;
        API_IMPL uint16_t *getIndexArray(const uint32_t indexFrame) const noexcept;
        API_IMPL void calculateNormals();
        API_IMPL void calculateUV();
        API_IMPL void removeNormals();
        API_IMPL void addNormals();
        API_IMPL void removeBuffer(uint32_t indexFrame);
        API_IMPL void removeAnimation(uint32_t index);
        API_IMPL bool saveDebug(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, char *errorOut,const int lenErrorOut);
        API_IMPL bool loadDebugFromMemory(const MESH_MBM* meshMemory);
        API_IMPL bool loadDebug(const char *fileNamePath);
        API_IMPL bool check(char *error,const int lenError);
        API_IMPL void centralizeFrame(const int indexFrame, const int indexSubset);
        API_IMPL void rotateFrame(const int indexFrame, const int indexSubset, const float angleX, const float angleY, const float angleZ);
        API_IMPL void scaleFrame(const int indexFrame, const int indexSubset, const float sx, const float sy, const float sz);
        API_IMPL void translateFrame(const int indexFrame, const int indexSubset, const float dx, const float dy, const float dz);
        API_IMPL bool addIndex(const uint32_t indexFrame, const uint32_t indexSubset,
                            const uint16_t *newIndexPart, const uint32_t sizeArrayNewIndexPart,
                            char *strErrorOut, const int strErrorOutLen);
        API_IMPL bool addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex);
        API_IMPL int addAnimation(const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut, const int errorOutLen);
        API_IMPL bool updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                               const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError);
        API_IMPL const util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnim(const uint32_t index)const;
        API_IMPL void fixDefaultBoud();
        API_IMPL void release();
        API_IMPL void deleteExtraInfo();
        void *       extraInfo;
      private:
        bool loadDebugImpl(const char *fileNamePath, const bool allowLegacyDispatch);
        void fillAtLeastOneBound();
        bool fillInSubsetDebug(const MESH_MBM* meshMemory, 
                               const int currentFrame,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
                               util::HEADER_FRAME* headerFrame,
                               util::BUFFER_MESH_DEBUG* pBuffer);//need to be implemented by specific backend engine 
        std::vector<std::string> getKnowPathsToExtraHeader();
        bool fillAnimation_2(const char *fileNamePath, FILE *fp);
        bool readDebugTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion);
        bool loadFromSeparatedBuffers(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                    VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                    uint16_t *indexArray, const int sizeArrayIndex, const int stride,
                                    int fileVersion = CURRENT_VERSION_MBM_HEADER);
        bool loadDebugLegacyCompat(const char *fileNamePath);
      #if defined(MBM_ENABLE_MESH_LEGACY_V7)
        bool loadDebugLegacyDetailStep(FILE *fp, const char *fileNamePath, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite);
        bool loadDebugLegacyAnimationStep(FILE *fp, const char *fileNamePath, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite);
        static bool getInfoLegacyCompat(FILE *fp, const char *fileNamePath, const util::HEADER &headerMbmOut,
                                        util::HEADER_MESH &headerMeshMbmOut, util::INFO_DRAW_MODE &info_mode,
                                        util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut,
                                        std::vector<util::STAGE_PARTICLE> &lsStageParticle, int *versionOut);
        void fillDebugLegacyPhysicsIfNeeded(const int fileVersion, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite,
                    VEC3 *position, const uint32_t currentFrame, std::vector<util::SUBSET_DEBUG *> &subsetArray);
      #endif
    
        bool saveAnimationHeaders(const char *fileOut, FILE **file);
        bool compressFile(const char *fileNameIn, char *stringStatus,const int lenStatus);
    };


    class MESH_MBM
    {
        friend class MESH_MANAGER;
      public:
        VEC3                            positionOffset;
        VEC3                            angleDefault;
        util::MATERIAL                  material;
        INFO_PHYSICS                    infoPhysics;
        util::INFO_ANIMATION            infoAnimation;
        util::INFO_DRAW_MODE		        info_mode;
        
        API_IMPL BUFFER_MESH *getBuffer(const uint32_t index) const;
        API_IMPL TEXTURE *getTexture(const uint32_t indexFrame, const uint32_t indexSubset);
        API_IMPL bool setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                               const bool hasAlpha);
        API_IMPL const char *getFilenameMesh() const;
        API_IMPL INFO_PHYSICS &getPhysicsInfo() noexcept;
        API_IMPL const INFO_PHYSICS &getPhysicsInfo() const noexcept;
        API_IMPL void resetPhysicsInfo();
        API_IMPL void appendPhysicsCube(CUBE *cube) noexcept;
        API_IMPL void appendPhysicsSphere(SPHERE *sphere) noexcept;
        API_IMPL void appendPhysicsCubeComplex(CUBE_COMPLEX *cubeComplex) noexcept;
        API_IMPL void appendPhysicsTriangle(TRIANGLE *triangle) noexcept;
        API_IMPL util::INFO_ANIMATION &getAnimationInfo() noexcept;
        API_IMPL const util::INFO_ANIMATION &getAnimationInfo() const noexcept;
        API_IMPL uint32_t getTotalAnimations() const noexcept;
        API_IMPL util::INFO_ANIMATION::INFO_HEADER_ANIM *getAnimationHeader(const uint32_t index) const noexcept;
        API_IMPL virtual ~MESH_MBM();
        API_IMPL void release();
        API_IMPL void deleteExtraInfo();
        API_IMPL bool isLoaded() const;
        API_IMPL bool render(const uint32_t indexFrame,const SHADER *pShader,
                             const RENDERIZABLE *renderizableOwner = nullptr);
        API_IMPL bool renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                        VEC2 *uv,
                                        const RENDERIZABLE *renderizableOwner = nullptr);
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
        bool load(const char *fileNamePath, RENDERIZABLE *renderizable);
        bool loadImpl(const char *fileNamePath, const bool allowLegacyDispatch, RENDERIZABLE *renderizable);
        void invertMap(const bool u, const bool v, VEC2 *pTexture, const uint32_t arraySize);
        bool loadFromSeparatedBuffers(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                    VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                    uint16_t *indexArray, const int sizeArrayIndex, const int stride,
                                    int fileVersion = CURRENT_VERSION_MBM_HEADER);
        bool readTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion);
        bool fillAnimation_2(util::HEADER_MESH &headerMesh, const int version, const char *fileNamePath, FILE *fp);
        bool loadLegacyCompat(const char *fileNamePath, RENDERIZABLE *renderizable);
      #if defined(MBM_ENABLE_MESH_LEGACY_V7)
        bool loadLegacyDetailStep(FILE *fp, const char *fileNamePath, const util::HEADER &headerMain,
                deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite);
        bool loadLegacyAnimationStep(FILE *fp, const char *fileNamePath, const util::HEADER &headerMain,
                   util::HEADER_MESH &headerMesh, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite);
        void fillLegacyPhysicsIfNeeded(const int fileVersion, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite,
                     VEC3 *position, const uint32_t currentFrame, util::SUBSET *subsetArray);
      #endif

        BUFFER_MESH *               buffer;
        std::string                 fileName;
        VEC2                        zoomEditorSprite; // Zoom do editor de sprite
        util::TYPE_MESH             typeMe;
        int16_t                     hasNormTex[2];       // Indica se tem normal e textura vinda do arquivo
        uint8_t                     depthUberImage;      // Quando uber image esta no arquivo é setado esta variavel
        int                         sizeCoordTexFrame_0; // Tamanho do array das coordenadas de textura do frame 0
        VEC2 *                      coordTexFrame_0;     // Coordenadas de Textura do frame 0 (faz cópia para os outros frames)
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
        API_IMPL MESH_MBM *load(const char *fileName, RENDERIZABLE *renderizable);
        API_IMPL MESH_MBM *loadTrueTypeFont(const char *fileNameTtf, const float heightLetter, const short spaceWidth,const short spaceHeight,const bool saveTextureAsPng,TEXTURE ** texture_loaded);
        API_IMPL MESH_MBM *load(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer,const util::INFO_DRAW_MODE * info_mode);
        API_IMPL MESH_MBM *loadIndex(const char *nickName, float *pPosition, float *pNormal, float *pTexture,const uint32_t sizeVertexBuffer, uint16_t *index,const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode);
        API_IMPL MESH_MBM *loadDynamicIndex(const char *nickName, const uint32_t sizeVertexBuffer,uint16_t *index, const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode, const util::DYNAMIC_SHAPE & dynamic_shape_info);
        API_IMPL MESH_MBM *getIfExists(const char* fileName);
        API_IMPL static const char * typeClassName(const util::TYPE_MESH type) noexcept;
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
        MESH_MANAGER();
        virtual ~MESH_MANAGER();
    };
}

#endif
