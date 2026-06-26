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
        API_IMPL MESH_MBM_DEBUG();

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
        API_IMPL INFO_PHYSICS &getPhysicsInfo() noexcept;
        API_IMPL const INFO_PHYSICS &getPhysicsInfo() const noexcept;
        API_IMPL int getFileVersion() const noexcept;
        API_IMPL util::MATERIAL &getMaterial() noexcept;
        API_IMPL const util::MATERIAL &getMaterial() const noexcept;
        API_IMPL int16_t getHasNormal() const noexcept;
        API_IMPL void setHasNormal(const int16_t hasNormalMode) noexcept;
        API_IMPL int16_t getHasTexture() const noexcept;
        API_IMPL void setHasTexture(const int16_t hasTextureMode) noexcept;
        API_IMPL const char *getFilenameMesh() const noexcept;
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
        // Writes the v11 section/TLV format (docs/mesh-v11-format.md). Milestone 3 core slice only:
        // material+transform, static frames (path-referenced textures only), physics bounding volumes,
        // extra paths. Returns false with a message in errorOut (not a partial file) if the mesh has any
        // animations or is FONT/PARTICLE/TILE_MAP typed - those land in a later pass of this milestone.
        API_IMPL bool saveV11(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, char *errorOut,const int lenErrorOut);
        API_IMPL bool loadDebugFromMemory(const MESH_MBM* meshMemory);
        // Reads the v11 section/TLV format. Milestone 5: this is now the only mesh format core_mbm
        // reads/writes - v1-v10 support moved to the offline mesh_deprecated library (no longer
        // linked into core_mbm), per docs/mesh-v11-plan.md Scope Decision 1.
        API_IMPL bool loadV11(const char *fileNamePath);
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
      private:
        void fillAtLeastOneBound();
        bool fillInSubsetDebug(const MESH_MBM* meshMemory,
                               const int currentFrame,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
                               const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
                               util::HEADER_FRAME* headerFrame,
                               util::BUFFER_MESH_DEBUG* pBuffer);//need to be implemented by specific backend engine
        bool readDebugTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion);
        bool readFrameStaticV11Payload(FILE *fp, const util::BUFFER_MESH_DEBUG *frame0, util::BUFFER_MESH_DEBUG *&out,
                                       util::FRAME_HEADER_V11 &outFrameHeader);
        std::vector<std::string> getKnowPathsToExtraHeader();

        struct Impl;
        std::unique_ptr<Impl> impl;
    };


    class MESH_MBM
    {
        friend class MESH_MANAGER;
      public:
        API_IMPL VEC3 getPositionOffset() const noexcept;
        API_IMPL void setPositionOffset(const VEC3 &position) noexcept;
        API_IMPL VEC3 getAngleDefault() const noexcept;
        API_IMPL void setAngleDefault(const VEC3 &angle) noexcept;
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
        MESH_MBM();
        bool load(const char *fileNamePath);
        bool load(const char *fileNamePath, RENDERIZABLE *renderizable);
        // Reads the v11 section/TLV format. Milestone 5: this is now the only mesh format - it backs
        // load()/MESH_MANAGER::load() directly, since v1-v10 support moved to the offline
        // mesh_deprecated library (no longer linked into core_mbm).
        bool loadV11(const char *fileNamePath);
        bool readFrameStaticV11Payload(FILE *fp, const char *fileNamePath, const uint32_t currentFrame);
        bool readTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion);

        struct Impl;
        std::unique_ptr<Impl> impl;
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
